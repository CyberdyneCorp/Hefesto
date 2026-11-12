// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl implementation of script::Interpreter (design D4/D5; capability
// hefesto-scripting "Embedded sandboxed Tcl on all platforms", "One
// interpreter per open project", "Host-independent scripting binding").
//
// Each project gets a full Tcl 8.6 interpreter, set up in this order:
//   1. Tcl_CreateInterp, with the sandbox session current on this thread so
//      the filesystem layer already applies the policy's roots;
//   2. the synthesized `env` array (D-31);
//   3. Tcl's library from the embedded table: init.tcl evaluated from
//      memory, `tcl_library` = kVirtualLibraryRoot, `auto_path` and the
//      Tcl-module path limited to the embedded tree;
//   4. the restricted commands (sandbox_commands.cpp).
// Every call into Tcl makes the session current (ActiveSession). A top-level
// evaluation applies the policy's limits and can be interrupted from another
// thread; its result carries the SCRIPT-SANDBOX-* diagnostics of the denials
// it caused, also those a `catch` absorbed (marked caught, with their
// DeniedOperation events published when it ends).
//
// Progress (script/progress.hpp): with a ProgressReporting handler set, a
// top-level evaluation arms Tcl's time limit at "now + interval" (or the
// policy's deadline when sooner). Tcl checks it every few commands and
// bytecode instructions and while `after` sleeps; the limit handler moves it
// one interval on, so the evaluation never stops for it, and reports the
// file and line being executed. Only the policy's own deadline is left in
// place, which then stops the evaluation with SCRIPT-TCL-003.

#include "hefesto/script/tcl/tcl_interpreter.hpp"

#include <tcl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/progress.hpp"
#include "hefesto/script/tcl/embedded_library.hpp"
#include "hefesto/script/tcl/value_conversion.hpp"
#include "script/tcl/sandbox_runtime.hpp"

// Tcl's internal stub-table function (tclIntDecls.h, not in tcl.h) that
// clears an interpreter's cancellation flags.
extern "C" int TclResetCancellation(Tcl_Interp* interp, int force);

namespace hefesto::script {

namespace {

using tcl_detail::ActiveSession;
using tcl_detail::as_view;
using tcl_detail::SandboxSession;
using tcl_detail::tcl_length;

constexpr std::string_view kTclError = "SCRIPT-TCL-001";
constexpr std::string_view kInterrupted = "SCRIPT-TCL-002";
constexpr std::string_view kLimitExceeded = "SCRIPT-TCL-003";

Tcl_Obj* new_string(std::string_view text) {
  return Tcl_NewStringObj(text.data(), tcl_length(text));
}

// `time` + `span` (Tcl's wall-clock time of the limits). The span is counted
// in Tcl_Time's own field type: that is std::chrono's rep on Linux (long) but
// not on macOS (long long), so an explicit cast is useless on one of them.
Tcl_Time later(Tcl_Time time, std::chrono::milliseconds span) {
  using TclMilliseconds = std::chrono::duration<decltype(Tcl_Time::sec), std::milli>;
  const auto milliseconds = std::chrono::duration_cast<TclMilliseconds>(span).count();
  time.sec += milliseconds / 1000;
  time.usec += (milliseconds % 1000) * 1000;
  if (time.usec >= 1000000) {
    time.sec += 1;
    time.usec -= 1000000;
  }
  return time;
}

bool earlier(const Tcl_Time& a, const Tcl_Time& b) {
  return a.sec < b.sec || (a.sec == b.sec && a.usec < b.usec);
}

// Tcl's process-wide initialization, once: the executable name (none), the
// encoding search path under the embedded root, and the sandbox's filesystem
// layer.
void initialize_process() {
  static std::once_flag once;
  std::call_once(once, [] {
    Tcl_FindExecutable(nullptr);
    Tcl_Obj* path = Tcl_NewListObj(0, nullptr);
    Tcl_ListObjAppendElement(nullptr, path, new_string(std::string{kVirtualLibraryRoot} + "/encoding"));
    Tcl_SetEncodingSearchPath(path);
    tcl_detail::register_filesystem();
  });
}

// Removes the Tcl-module roots that tm.tcl derives from the executable and
// the environment (relative to the working directory here, since the
// executable name is empty), keeping the embedded tree only.
constexpr std::string_view kLimitModulePaths = R"tcl(
if {![catch {auto_load ::tcl::tm::UnknownHandler} loaded] && $loaded} {
    foreach path [::tcl::tm::path list] {
        if {![string match /__hefesto_embedded__/* $path]} {
            ::tcl::tm::path remove $path
        }
    }
}
)tcl";

class TclInterpreter;

// A NativeCommand owned by its Tcl command (deleted with it).
struct NativeBinding {
  const TclInterpreter* owner;
  NativeCommand command;
};

class TclInterpreter final : public Interpreter {
 public:
  TclInterpreter(const SandboxPolicy& policy, ScriptEvents* events, const ProgressReporting* progress)
      : interp_(Tcl_CreateInterp()),
        session_(std::make_unique<SandboxSession>(interp_, policy, events)),
        limits_(policy.limits),
        progress_(progress) {
    try {
      const ActiveSession active{*session_};
      tcl_detail::synthesize_environment(interp_, policy);
      initialize_library();
      tcl_detail::install_sandbox_commands(*session_, policy);
      limit_module_paths();
      Tcl_LimitAddHandler(interp_, TCL_LIMIT_TIME, on_time_limit, this, nullptr);
      session_->reset();
    } catch (...) {
      Tcl_DeleteInterp(interp_);
      throw;
    }
  }

  ~TclInterpreter() override {
    const ActiveSession active{*session_};
    Tcl_DeleteInterp(interp_);
  }

  TclInterpreter(const TclInterpreter&) = delete;
  TclInterpreter& operator=(const TclInterpreter&) = delete;

  cmd::CommandResult evaluate(std::string_view script, cmd::Origin origin) override {
    Evaluation evaluation{*this, origin, ""};
    const int code = Tcl_EvalEx(interp_, script.data(), tcl_length(script), TCL_EVAL_GLOBAL);
    return evaluation.finish(code, "", "");
  }

  cmd::CommandResult source(const platform::Path& path) override {
    Evaluation evaluation{*this, cmd::Origin::script, path.string()};
    Tcl_Obj* file = new_string(path.string());
    Tcl_IncrRefCount(file);
    const int code = Tcl_FSEvalFileEx(interp_, file, nullptr);
    Tcl_DecrRefCount(file);
    return evaluation.finish(code, "source", path.string());
  }

  bool set_var(std::string_view name, const cmd::Value& value, std::string* error) override {
    const Access access{*this};
    const std::string variable{name};
    if (Tcl_SetVar2Ex(interp_, variable.c_str(), nullptr, to_tcl(value), TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) !=
        nullptr) {
      return true;
    }
    if (error != nullptr) {
      *error = Tcl_GetStringResult(interp_);
    }
    Tcl_ResetResult(interp_);
    return false;
  }

  std::optional<cmd::Value> get_var(std::string_view name) const override {
    const Access access{*this};
    const std::string variable{name};
    Tcl_Obj* value = Tcl_GetVar2Ex(interp_, variable.c_str(), nullptr, TCL_GLOBAL_ONLY);
    if (value == nullptr) {
      Tcl_ResetResult(interp_);
      return std::nullopt;
    }
    return from_tcl(value);
  }

  void define_command(std::string_view name, NativeCommand command) override {
    const std::string qualified{name};
    auto owned = std::make_unique<NativeBinding>(NativeBinding{this, std::move(command)});
    Tcl_CreateObjCommand(interp_, qualified.c_str(), call_native, owned.release(), delete_native);
  }

  bool has_command(std::string_view name) const override {
    const std::string qualified{name};
    Tcl_CmdInfo info;
    return Tcl_GetCommandInfo(interp_, qualified.c_str(), &info) != 0;
  }

  std::string info_script() const override {
    const Access access{*this};
    Tcl_InterpState saved = Tcl_SaveInterpState(interp_, TCL_OK);
    std::string script;
    if (Tcl_EvalEx(interp_, "::tcl::info::script", -1, 0) == TCL_OK) {
      script = Tcl_GetStringResult(interp_);
    }
    static_cast<void>(Tcl_RestoreInterpState(interp_, saved));
    return script;
  }

  void request_interrupt() noexcept override {
    // Tcl_CancelEval is the one Tcl call allowed from another thread. An
    // interrupt with no evaluation running is ignored (Tcl would otherwise
    // cancel the next one); TCL_CANCEL_UNWIND keeps `catch` from absorbing it.
    if (depth_.load() > 0) {
      interrupted_.store(true);
      static_cast<void>(Tcl_CancelEval(interp_, nullptr, nullptr, TCL_CANCEL_UNWIND));
    }
  }

  [[nodiscard]] cmd::Origin origin() const noexcept { return origin_; }

  [[nodiscard]] diag::SourceLocation location() const {
    const ActiveSession active{*session_};
    return tcl_detail::current_location(interp_);
  }

 private:
  // A call into Tcl that is not an evaluation (variables, `info script`):
  // the session is current, and denials caused by variable traces are
  // reported at once instead of being left for the next evaluation.
  class Access {
   public:
    explicit Access(const TclInterpreter& owner)
        : session_(*owner.session_), active_(session_), mark_(session_.pending()) {}
    ~Access() { session_.report_unclaimed("", "", mark_); }
    Access(const Access&) = delete;
    Access& operator=(const Access&) = delete;

   private:
    SandboxSession& session_;
    ActiveSession active_;
    std::size_t mark_;
  };

  // One evaluation (nested ones too); the outermost applies the limits.
  class Evaluation {
   public:
    Evaluation(TclInterpreter& owner, cmd::Origin origin, std::string_view script)
        : owner_(owner),
          active_(*owner.session_),
          saved_origin_(owner.origin_),
          diagnostics_mark_(owner.session_->diagnostic_count()),
          pending_mark_(owner.session_->pending()),
          top_(owner.depth_.fetch_add(1) == 0) {
      owner_.origin_ = origin;
      if (top_) {
        owner_.interrupted_.store(false);
        owner_.session_->begin_deferral();
        owner_.start_progress(origin, script);
        owner_.apply_limits();
      }
    }

    ~Evaluation() {
      if (top_) {
        Tcl_LimitTypeReset(owner_.interp_, TCL_LIMIT_COMMANDS);
        owner_.disarm_time_limit();
        // Tcl_EvalEx leaves an unwinding cancellation set at level 0.
        static_cast<void>(TclResetCancellation(owner_.interp_, 1));
        owner_.session_->end_deferral(std::nullopt);  // finish() did unless it threw
      }
      owner_.origin_ = saved_origin_;
      owner_.depth_.fetch_sub(1);
      if (top_) {
        owner_.end_progress();
      }
    }

    Evaluation(const Evaluation&) = delete;
    Evaluation& operator=(const Evaluation&) = delete;

    // The result of an evaluation that returned `code`; `operation` names
    // the denials no guarded command claimed, `file` is the sourced file.
    cmd::CommandResult finish(int code, std::string_view operation, std::string_view file) {
      Tcl_Interp* interp = owner_.interp_;
      SandboxSession& session = *owner_.session_;
      session.report_unclaimed(operation, file, pending_mark_);
      if (top_) {
        const bool failed = code == TCL_ERROR;
        session.end_deferral(failed ? std::optional<std::string>{Tcl_GetStringResult(interp)} : std::nullopt);
      }
      auto sandbox = session.diagnostics_since(diagnostics_mark_);
      cmd::CommandResult result;
      if (code == TCL_OK || code == TCL_RETURN) {
        result = cmd::CommandResult::success(from_tcl(Tcl_GetObjResult(interp)));
      } else {
        result = cmd::CommandResult::failure(error_diagnostic(code, file));
      }
      for (auto& diagnostic : sandbox) {
        result.diagnostics.push_back(std::move(diagnostic));
      }
      return result;
    }

   private:
    diag::Diagnostic error_diagnostic(int code, std::string_view file) const {
      Tcl_Interp* interp = owner_.interp_;
      std::string message = Tcl_GetStringResult(interp);
      if (code == TCL_BREAK || code == TCL_CONTINUE) {
        message = code == TCL_BREAK ? "invoked \"break\" outside of a loop" : "invoked \"continue\" outside of a loop";
      }
      std::string_view error_code = kTclError;
      if (top_ && owner_.interrupted_.load()) {
        error_code = kInterrupted;
        message = "script evaluation interrupted (" + message + ")";
      } else if (top_ && Tcl_LimitExceeded(interp) != 0) {
        error_code = kLimitExceeded;
      }
      return diag::make(error_code, std::move(message), diag::SourceLocation{std::string{file}, error_line(code)});
    }

    std::uint32_t error_line(int code) const {
      Tcl_Interp* interp = owner_.interp_;
      Tcl_Obj* options = Tcl_GetReturnOptions(interp, code);
      Tcl_IncrRefCount(options);
      Tcl_Obj* key = new_string("-errorline");
      Tcl_IncrRefCount(key);
      Tcl_Obj* value = nullptr;
      int line = 0;
      if (Tcl_DictObjGet(nullptr, options, key, &value) != TCL_OK || value == nullptr ||
          Tcl_GetIntFromObj(nullptr, value, &line) != TCL_OK) {
        line = 0;
      }
      Tcl_DecrRefCount(key);
      Tcl_DecrRefCount(options);
      return line > 0 ? static_cast<std::uint32_t>(line) : 0U;
    }

    TclInterpreter& owner_;
    ActiveSession active_;
    cmd::Origin saved_origin_;
    std::size_t diagnostics_mark_;
    std::size_t pending_mark_;
    bool top_;
  };

  static int call_native(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    const auto& binding = *static_cast<const NativeBinding*>(data);
    cmd::Invocation invocation;
    invocation.name = std::string{as_view(objv[0])};
    for (int i = 1; i < objc; ++i) {
      invocation.args.emplace_back(as_view(objv[i]));
    }
    invocation.origin = binding.owner->origin();
    cmd::CommandResult result;
    try {
      result = binding.command(invocation);
    } catch (const std::exception& error) {
      Tcl_SetObjResult(interp, new_string(error.what()));
      return TCL_ERROR;
    }
    if (!result.ok()) {
      const std::string message = result.diagnostics.empty() ? "command failed" : result.diagnostics.front().text;
      Tcl_SetObjResult(interp, new_string(message));
      return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, to_tcl(result.value));
    return TCL_OK;
  }

  static void delete_native(ClientData data) { std::unique_ptr<NativeBinding> owned{static_cast<NativeBinding*>(data)}; }

  // ---- progress (script/progress.hpp) ----

  // A top-level evaluation starts: takes the project's handler as it is now.
  void start_progress(cmd::Origin origin, std::string_view script) {
    handler_ = progress_ != nullptr ? progress_->handler : ProgressHandler{};
    interval_ = std::max(progress_ != nullptr ? progress_->interval : std::chrono::milliseconds{0},
                         std::chrono::milliseconds{1});
    progress_state_ = ScriptProgress{};
    progress_state_.script = std::string{script};
    progress_state_.origin = origin;
    started_ = std::chrono::steady_clock::now();
    reported_ = false;
  }

  // The top-level evaluation ended: the last report, when there were others.
  void end_progress() noexcept {
    if (reported_ && handler_) {
      progress_state_.done = true;
      progress_state_.elapsed = std::chrono::steady_clock::now() - started_;
      call_handler();
    }
    handler_ = {};
    reported_ = false;
  }

  // Tcl's time limit was reached (TCL_LIMIT_TIME handler).
  static void on_time_limit(ClientData data, Tcl_Interp* /*interp*/) {
    static_cast<TclInterpreter*>(data)->time_limit_reached();
  }

  void time_limit_reached() {
    Tcl_Time now;
    Tcl_GetTime(&now);
    if (depth_.load() == 0 || !handler_ || (has_deadline_ && !earlier(now, deadline_))) {
      return;  // the policy's own limit: the evaluation stops (SCRIPT-TCL-003)
    }
    // Moving the limit on clears Tcl's "exceeded" state, so the location
    // query below and the rest of the script run.
    Tcl_Time limit = next_time_limit(now);
    Tcl_LimitSetTime(interp_, &limit);
    if (in_handler_) {
      return;
    }
    progress_state_.location = tcl_detail::current_location(interp_);
    progress_state_.elapsed = std::chrono::steady_clock::now() - started_;
    reported_ = true;
    call_handler();
  }

  // The handler runs the front end's frame; nothing it throws crosses Tcl.
  void call_handler() noexcept {
    in_handler_ = true;
    try {
      handler_(progress_state_);
    } catch (...) {
      static_cast<void>(0);  // a failing frame must not stop the script
    }
    in_handler_ = false;
  }

  // Ends the time limit of a top-level evaluation. Tcl keeps the limit's
  // timer and time after Tcl_LimitTypeReset, and `after N` in a later
  // evaluation without a time limit then busy-waits for N ms (AfterDelay
  // sees an expired limit time); moving the time far ahead avoids that.
  void disarm_time_limit() {
    if (Tcl_LimitTypeEnabled(interp_, TCL_LIMIT_TIME) != 0) {
      Tcl_Time now;
      Tcl_GetTime(&now);
      Tcl_Time far = later(now, std::chrono::hours{24 * 365});
      Tcl_LimitSetTime(interp_, &far);
    }
    Tcl_LimitTypeReset(interp_, TCL_LIMIT_TIME);
  }

  // The next moment the limit handler runs: one interval on, or the
  // policy's deadline when that comes first.
  [[nodiscard]] Tcl_Time next_time_limit(const Tcl_Time& now) const {
    if (!handler_) {
      return deadline_;
    }
    const Tcl_Time tick = later(now, interval_);
    return has_deadline_ && earlier(deadline_, tick) ? deadline_ : tick;
  }

  // The policy's limits for one top-level evaluation (Tcl resource limits:
  // the command limit is absolute, so it is set from the current count).
  void apply_limits() {
    if (limits_.command_limit > 0) {
      Tcl_Obj* word = new_string("::tcl::info::cmdcount");
      Tcl_IncrRefCount(word);
      Tcl_WideInt count = 0;
      if (Tcl_EvalObjv(interp_, 1, &word, 0) != TCL_OK ||
          Tcl_GetWideIntFromObj(nullptr, Tcl_GetObjResult(interp_), &count) != TCL_OK) {
        count = 0;
      }
      Tcl_DecrRefCount(word);
      Tcl_ResetResult(interp_);
      const Tcl_WideInt limit = count + static_cast<Tcl_WideInt>(std::min<std::uint64_t>(limits_.command_limit, INT_MAX));
      Tcl_LimitSetCommands(interp_, static_cast<int>(std::min<Tcl_WideInt>(limit, INT_MAX)));
      Tcl_LimitTypeSet(interp_, TCL_LIMIT_COMMANDS);
    }
    Tcl_Time now;
    Tcl_GetTime(&now);
    has_deadline_ = limits_.time_limit.count() > 0;
    if (has_deadline_) {
      deadline_ = later(now, limits_.time_limit);
    }
    if (has_deadline_ || handler_) {
      Tcl_Time limit = next_time_limit(now);
      Tcl_LimitSetTime(interp_, &limit);
      Tcl_LimitTypeSet(interp_, TCL_LIMIT_TIME);
    }
  }

  // init.tcl from the embedded table, with the library root virtual and the
  // package search limited to it (no host Tcl file is read). A failure is a
  // build defect (the table is generated from the pinned sources).
  void initialize_library() {
    const std::string root{kVirtualLibraryRoot};
    Tcl_SetVar2Ex(interp_, "tcl_library", nullptr, new_string(root), TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp_, "tcl_pkgPath", nullptr, Tcl_NewObj(), TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp_, "auto_path", nullptr, new_string(root), TCL_GLOBAL_ONLY);
    // Sourced through the filesystem layer (not evaluated from a string) so
    // that its procedures' frames name the embedded file (in_tcl_library).
    Tcl_Obj* init = new_string(root + "/init.tcl");
    Tcl_IncrRefCount(init);
    const int code = Tcl_FSEvalFileEx(interp_, init, "utf-8");
    Tcl_DecrRefCount(init);
    if (code != TCL_OK) {
      throw std::runtime_error(std::string{"embedded init.tcl failed: "} + Tcl_GetStringResult(interp_));
    }
    // init.tcl appends the executable's ../lib and tcl_pkgPath; keep the root only.
    Tcl_SetVar2Ex(interp_, "auto_path", nullptr, new_string(root), TCL_GLOBAL_ONLY);
    Tcl_ResetResult(interp_);
  }

  void limit_module_paths() {
    if (Tcl_EvalEx(interp_, kLimitModulePaths.data(), tcl_length(kLimitModulePaths), TCL_EVAL_GLOBAL) != TCL_OK) {
      throw std::runtime_error(std::string{"embedded tm.tcl failed: "} + Tcl_GetStringResult(interp_));
    }
    Tcl_ResetResult(interp_);
  }

  Tcl_Interp* interp_;
  std::unique_ptr<SandboxSession> session_;
  SandboxLimits limits_;
  const ProgressReporting* progress_;
  cmd::Origin origin_ = cmd::Origin::script;
  std::atomic<int> depth_{0};
  std::atomic<bool> interrupted_{false};
  // The running top-level evaluation (command thread only).
  bool has_deadline_ = false;
  Tcl_Time deadline_{};
  ProgressHandler handler_;
  std::chrono::milliseconds interval_{50};
  ScriptProgress progress_state_;
  std::chrono::steady_clock::time_point started_{};
  bool reported_ = false;
  bool in_handler_ = false;
};

}  // namespace

std::unique_ptr<Interpreter> create_project_interpreter(editor::EditorState& editor, const SandboxPolicy& policy) {
  initialize_process();
  return std::make_unique<TclInterpreter>(policy, &editor.script.events, &editor.script.progress);
}

ScriptLocation current_script_location(const Interpreter& interpreter) {
  const auto* tcl = dynamic_cast<const TclInterpreter*>(&interpreter);
  return tcl != nullptr ? tcl->location() : ScriptLocation{};
}

}  // namespace hefesto::script
