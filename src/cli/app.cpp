// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "app.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "hefesto/app/startup.hpp"
#include "hefesto/cell/crash.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/command_log.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/platform/session_log.hpp"
#include "hefesto/platform_net/http_fetcher.hpp"
#include "hefesto/platform_posix/file_host.hpp"
#include "hefesto/platform_posix/lock_service.hpp"
#include "hefesto/platform_posix/process_runner.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/pdk/startup.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/tools/auxiliary.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/version.hpp"
#include "interactive.hpp"
#include "options.hpp"
#include "tcl_lines.hpp"

#include <unistd.h>

namespace hefesto::cli {

namespace {

namespace codes = diag::codes;
using platform::Path;

/// The Magic release this Hefesto build is compatible with (app-startup
/// "Tcl launcher options": --version names it).
constexpr std::string_view kMagicCompatVersion = "8.3.694";
/// Its source revision (`--commit` in --magic mode prints it, as Magic does).
constexpr std::string_view kMagicCompatCommit = "fc5daa9e2856b89a0a3056f296ded1fd4474e39e";

void print_line(std::FILE* stream, std::string_view text) {
  std::fwrite(text.data(), 1, text.size(), stream);
  std::fputc('\n', stream);
  std::fflush(stream);
}

/// Prints each diagnostic to a stream as text or JSON lines.
class StreamSink final : public diag::DiagnosticSink {
 public:
  StreamSink(std::FILE* stream, diag::Format format) : stream_(stream), format_(format) {}

  void emit(const diag::Diagnostic& diagnostic) override {
    const auto text = diag::render(diagnostic, format_);
    if (!text) {
      return;
    }
    const std::scoped_lock lock{mutex_};
    print_line(stream_, *text);
  }

 private:
  std::FILE* stream_;
  diag::Format format_;
  std::mutex mutex_;
};

/// Counts the commands that failed and remembers the last error text that
/// was reported, so the CLI can fail the run on a quiet command failure
/// (Magic's convention: the script goes on) and print a Tcl error only when
/// the failing command has not reported it already.
class RunMonitor final : public diag::DiagnosticSink, public cmd::CommandObserver {
 public:
  void emit(const diag::Diagnostic& diagnostic) override {
    if (diagnostic.severity == diag::Severity::error) {
      const std::scoped_lock lock{mutex_};
      last_error_ = diagnostic.text;
    }
  }

  void on_command(const cmd::Invocation& /*invocation*/, const cmd::CommandResult& result) override {
    if (!result.ok()) {
      const std::scoped_lock lock{mutex_};
      ++failures_;
    }
  }

  [[nodiscard]] std::size_t failures() const {
    const std::scoped_lock lock{mutex_};
    return failures_;
  }

  /// True when `text` is the last error a command reported since forget().
  [[nodiscard]] bool reported(const std::string& text) const {
    const std::scoped_lock lock{mutex_};
    return last_error_ && *last_error_ == text;
  }

  void forget() {
    const std::scoped_lock lock{mutex_};
    last_error_.reset();
  }

 private:
  mutable std::mutex mutex_;
  std::size_t failures_ = 0;
  std::optional<std::string> last_error_;
};

/// A failure that is a Tcl error in Magic's interpreter: a raised usage error
/// or a dispatch failure (unknown or ambiguous command name).
bool raises_tcl_error(const cmd::CommandResult& result) {
  return result.raises || std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                                      [](const diag::Diagnostic& d) {
                                        return d.code == codes::kCmdDispatch || d.code == ui::codes::kAmbiguous;
                                      });
}

/// --diagnostics=magic: the console of `magic -dnull -noconsole` running a
/// golden reference script (tools/golden/README.md). Commands print nothing
/// themselves; the script's `puts` prints their Tcl results. As in Magic's
/// Tcl dispatcher (scripting-api "Command result and error conventions"), a
/// failed command's diagnostics are TxError text on stderr, except when the
/// command set an explicit result (`tech load`), which replaces its last
/// diagnostic, or raised a Tcl error, whose message becomes the result: that
/// diagnostic is then the Tcl result, not console text. Other diagnostics are
/// TxError (stderr) or, for informational ones, TxPrintf (stdout) text.
class MagicConsole final : public diag::DiagnosticSink, public cmd::CommandObserver {
 public:
  MagicConsole(std::FILE* out, std::FILE* err) : out_(out), err_(err) {}
  MagicConsole(const MagicConsole&) = delete;
  MagicConsole& operator=(const MagicConsole&) = delete;
  ~MagicConsole() override { flush(); }

  void emit(const diag::Diagnostic& diagnostic) override {
    if (!diag::shown_in(diagnostic, diag::Format::magic)) {
      return;  // a Hefesto-only diagnostic: Magic prints nothing
    }
    const std::scoped_lock lock{mutex_};
    flush_pending();
    if (diagnostic.severity == diag::Severity::error) {
      pending_ = diagnostic;  // may turn out to be a command's result
      has_pending_ = true;
      return;
    }
    const bool printf_like = diagnostic.severity == diag::Severity::info || diagnostic.severity == diag::Severity::debug;
    std::FILE* stream = printf_like ? out_ : err_;
    const std::string text = diag::format_magic(diagnostic);
    if (diagnostic.no_newline) {
      std::fwrite(text.data(), 1, text.size(), stream);  // the next output ends the line
      std::fflush(stream);
      return;
    }
    print_line(stream, text);
  }

  /// Set while a Tcl chunk runs: its commands have Tcl results even when
  /// their origin is `cli` (-c text, standard input).
  void set_in_tcl(bool on) noexcept { in_tcl_ = on; }

  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) override {
    const std::scoped_lock lock{mutex_};
    if (!in_tcl_ && !invocation.source && invocation.origin != cmd::Origin::script) {
      flush_pending();  // commands of the startup sequence itself have no Tcl result
      return;
    }
    const bool is_result = !result.ok() && has_pending_ && !result.diagnostics.empty() &&
                           pending_ == result.diagnostics.back() && (raises_tcl_error(result) || !result.value.is_none());
    if (is_result) {
      has_pending_ = false;  // it is the command's Tcl result (or error), not TxError text
    }
    flush_pending();
  }

  void flush() {
    const std::scoped_lock lock{mutex_};
    flush_pending();
  }

 private:
  void flush_pending() {
    if (has_pending_) {
      print_line(err_, diag::format_magic(pending_));
      has_pending_ = false;
    }
  }

  std::FILE* out_;
  std::FILE* err_;
  bool in_tcl_ = false;
  // A plain member plus a flag, not std::optional: GCC 14 reports a bogus
  // -Wmaybe-uninitialized when an optional<Diagnostic> is assigned.
  diag::Diagnostic pending_;
  bool has_pending_ = false;
  std::mutex mutex_;
};

std::optional<std::string> read_stdin() {
  std::string text{std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
  if (std::cin.bad()) {
    return std::nullopt;
  }
  return text;
}

// The session state of one CLI run.
class Session {
 public:
  Session(const Options& options, std::FILE* out, std::FILE* err)
      : options_(options),
        out_(out),
        err_(err),
        services_(platform::PortableOptions{.storage_root = platform::default_storage_dir(), .pdk_roots = {}}),
        sink_(err, options.diagnostics),
        magic_console_(out, err) {
    log_warning_ = open_log();
    services_.set_process_runner(std::make_unique<platform_posix::PosixProcessRunner>());
    install_pdk_store();
    // cell-io-safety: POSIX locks and the crash-backup file host, before any command.
    editor_.locks = platform_posix::make_lock_service();
    editor_.cell_session.host = platform_posix::make_file_host();
    // hefesto-security "Consent before implicit script execution": batch mode
    // runs a PDK's scripts only for --trust-pdk (or a version the store trusts).
    editor_.script.pdk_trust.set_consent_host(&consent_);
    cmd::register_all_commands(
        registry_, pdk::CommandHosts{platform_net::http_fetcher_factory(install_host_->environment("GITHUB_TOKEN")), {}, {}});
    command_log_ = std::make_unique<cmd::CommandLog>(logger_);
    dispatcher_ = std::make_unique<cmd::Dispatcher>(registry_, services_.view(), project_, script_session_);
    dispatcher_->set_editor(&editor_);
    dispatcher_->set_typed_abbreviations(false);  // scripts and -c lines: exact Tcl names (Q-UI-41)
    dispatcher_->set_default_point(db::Point{100, 100});  // Magic's txCurrentPoint (Q-UI-43)
    ui::install_defaults(editor_);  // the tools and default key bindings (Magic's startup files)
    dispatcher_->add_observer(*command_log_);
    dispatcher_->add_observer(monitor_);
    if (options.diagnostics == diag::Format::magic) {
      dispatcher_->add_sink(magic_console_);
      dispatcher_->add_observer(magic_console_);
    } else {
      dispatcher_->add_sink(sink_);
    }
    dispatcher_->add_sink(monitor_);
  }

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  // The interpreter's commands refer to the dispatcher, which is destroyed
  // before the editor: close the project's interpreter first.
  ~Session() { script::close_project_interpreter(editor_); }

  int run(const app::StartupOptions& startup) {
    log(diag::Severity::info, "session start hefesto-cli " + std::string{version()});
    if (log_warning_) {
      dispatcher_->report(*log_warning_);
    }
    run_startup(startup);
    const bool ok = script_session_.quit_requested || run_sources();
    magic_console_.flush();
    cell::end_crash_session(editor_, services_.files());  // normal exit: drop this session's backup and lock
    // `quit N` sets the status (app-startup "quit command and exit
    // status"); a plain `quit` keeps the failure status of earlier commands.
    const int status = script_session_.exit_status != 0 ? script_session_.exit_status : (ok ? kExitOk : kExitFailure);
    log(diag::Severity::info, "session end exit=" + std::to_string(status));
    return status;
  }

 private:
  // hefesto-files-and-pdks: the session's PdkStore resolves through the PDK
  // manager's installs (pinned, enabled, <root>/<name>, PDKPATH) under the root
  // of --pdk-root, PDK_ROOT or ~/.ciel. Before the dispatcher takes its view.
  void install_pdk_store() {
    // hefesto-licensing D-13: in-process HTTPS (URLSession on macOS, libcurl on Linux).
    platform_net::HttpFetcherOptions fetch;
    fetch.github_token = install_host_->environment("GITHUB_TOKEN");
    pdk_fetcher_ = platform_net::make_http_fetcher(std::move(fetch));
    pdk_manager_ = std::make_unique<pdk::PdkManager>(services_.files(), *install_host_, *pdk_fetcher_,
                                                     pdk::Catalog::builtin());
    pdk_manager_->set_root(pdk_manager_->resolve_root(options_.pdk_root));
    services_.set_pdk_store(pdk::make_managed_pdk_store(*pdk_manager_, *install_host_));
  }

  // The sink of this session's diagnostic format.
  diag::DiagnosticSink& console_sink() {
    if (options_.diagnostics == diag::Format::magic) {
      return magic_console_;
    }
    return sink_;
  }

  // Installs the rotating session log; a warning when it cannot be opened.
  std::optional<diag::Diagnostic> open_log() {
    if (options_.no_log) {
      return std::nullopt;
    }
    const Path dir = options_.log_dir.value_or(platform::default_storage_dir() / "logs");
    platform::SessionLog log = platform::install_session_log(services_, dir, "hefesto-cli", codes::kCliLog);
    logger_ = log.logger;
    return log.warning;
  }

  void log(diag::Severity level, const std::string& message) {
    if (logger_ != nullptr) {
      logger_->write(level, message);
    }
  }

  // app-startup: settings layers, startup files, technology, file arguments.
  void run_startup(const app::StartupOptions& startup) {
    if (startup.technology) {
      log(diag::Severity::info, "tech " + *startup.technology);
    }
    const auto result =
        app::run_startup(*dispatcher_, project_, services_.files(), startup, environment(), console_sink());
    if (!result) {
      dispatcher_->report(result.error());
    }
  }

  // The CLI's console in the project's interpreter: `puts` to stdout and
  // stderr writes to this session's streams (in order with the diagnostics);
  // other channels go to Tcl's own `puts`, kept as ::hefesto::tcl_puts.
  // The globals the `magic` launcher's Tcl shell starts with (`argv`,
  // `argc`, and `batch_mode` under `-dnull -noconsole`).
  void set_launcher_variables(script::Interpreter& interp) const {
    const tools::LauncherVariables vars = tools::launcher_variables(options_.magic_args);
    cmd::Value::List words(vars.argv.begin(), vars.argv.end());
    static_cast<void>(interp.set_var("::argc", cmd::Value{static_cast<std::int64_t>(words.size())}));
    static_cast<void>(interp.set_var("::argv", cmd::Value{std::move(words)}));
    if (vars.batch_mode) {
      static_cast<void>(interp.set_var("::batch_mode", cmd::Value{"true"}));
    }
  }

  void install_console(script::Interpreter& interp) {
    const auto renamed = interp.evaluate("namespace eval ::hefesto {}; rename ::puts ::hefesto::tcl_puts");
    if (!renamed.ok()) {
      return;  // keep Tcl's own puts
    }
    interp.define_command("::puts", [this, &interp](const cmd::Invocation& inv) { return console_puts(interp, inv); });
  }

  // `puts ?-nonewline? ?channelId? string` (Tcl's argument rules).
  cmd::CommandResult console_puts(script::Interpreter& interp, const cmd::Invocation& inv) {
    const auto& a = inv.args;
    const bool nonewline = (a.size() == 2 || a.size() == 3) && a.front() == "-nonewline";
    const std::size_t first = nonewline ? 1 : 0;
    if (a.size() - first == 0 || a.size() - first > 2) {
      return cmd::CommandResult::failure(
          diag::make("SCRIPT-TCL-001", R"(wrong # args: should be "puts ?-nonewline? ?channelId? string")"));
    }
    const std::string channel = a.size() - first == 2 ? a[first] : "stdout";
    if (channel != "stdout" && channel != "stderr") {
      std::string script = "::hefesto::tcl_puts";
      for (const auto& word : a) {
        script += ' ';
        script += script::quote_list_element(word);
      }
      return interp.evaluate(script);
    }
    const std::string& text = a.back();
    std::FILE* stream = channel == "stdout" ? out_ : err_;
    dispatcher_->flush_running();  // the running command's messages so far come first
    magic_console_.flush();        // a pending TxError line comes first
    std::fwrite(text.data(), 1, text.size(), stream);
    if (!nonewline) {
      std::fputc('\n', stream);
    }
    std::fflush(stream);
    return cmd::CommandResult::success();
  }

  app::StartupEnvironment environment() {
    app::StartupEnvironment env;
    env.host = install_host_.get();
    env.install_root = app::resolve_install_root(install_host_->environment("CAD_ROOT"), Path{});
    env.home = install_host_->home_directory();
    std::error_code ec;
    env.working_directory = std::filesystem::current_path(ec);
    env.grant_working_directory = true;  // a batch run's files live in its working directory (S-11 limits it)
    env.display_available = false;       // NULL graphics unless -d names a display
    env.script_limits.time_limit = std::chrono::seconds{options_.script_time_limit};  // S-14
    env.project_folder = options_.project;
    if (!env.project_folder && services_.files().exists(env.working_directory / std::string{app::kProjectFileName})) {
      env.project_folder = env.working_directory;
    }
    env.pdk_root = options_.pdk_root;
    env.user_preferences = platform::default_storage_dir() / "preferences.json";
    env.recent_projects = platform::default_storage_dir() / "recent-projects.json";
    env.configure_interpreter = [this](script::Interpreter& interp) {
      install_console(interp);
      if (options_.magic_mode) {
        set_launcher_variables(interp);
        // Magic's Tcl build has no readline `history` (CmdHistory needs
        // USE_READLINE): `history` is Tcl's own, as in `magic -dnull`.
        static_cast<void>(interp.evaluate("catch {rename ::magic::history {}}"));
      }
    };
    return env;
  }

  // One source of Tcl input and how the CLI runs it.
  struct TclSource {
    std::string name;        // "-c", the file name or "-" (error locations)
    bool echo;               // print each chunk's non-empty result (a console)
    bool keep_going;         // a Tcl error does not stop the source (tclsh's loop)
    bool count_failures;     // a quiet command failure fails the run
    /// `cli` for text the user typed (-c, standard input), `script` for a
    /// script file: a file's engine commands see only the sandbox's granted
    /// roots and never run a process (security findings S-01, S-02).
    cmd::Origin origin = cmd::Origin::script;
  };

  // How each source runs: `-c` text and script files print each command's
  // result and stop at a Tcl error, like `source` (a quiet command failure
  // still fails the run, Hefesto's CLI status convention); in --magic mode
  // standard input is Magic's console without a terminal (tclsh's
  // non-interactive loop: an error prints its message and the next command
  // runs, nothing else is echoed, the status comes from `quit`).
  [[nodiscard]] TclSource tcl_source(const ScriptSource& source) const {
    using Kind = ScriptSource::Kind;
    const cmd::Origin origin = source.kind == Kind::file ? cmd::Origin::script : cmd::Origin::cli;
    if (source.kind == Kind::inline_text) {
      return {"-c", true, false, true, origin};
    }
    if (options_.magic_mode) {
      return {source.value, false, true, false, origin};
    }
    return {source.value, true, false, true, origin};
  }

  // Runs every source in order; stops at an aborted script or `quit`.
  // In --magic mode without -c or scripts, commands come from standard input.
  // With --interactive (or --magic on a terminal), the prompt follows.
  bool run_sources() {
    std::vector<ScriptSource> sources = options_.sources;
    const bool interactive = options_.interactive || (options_.magic_mode && sources.empty() && ::isatty(0) == 1);
    if (options_.magic_mode && sources.empty() && !interactive) {
      sources.push_back({ScriptSource::Kind::standard_input, "-"});
    }
    bool ok = true;
    for (const auto& source : sources) {
      const auto run = run_source(source);
      ok = ok && run.ok();
      if (run.aborted || run.quit) {
        return ok;
      }
    }
    if (interactive) {
      ok = run_prompt() && ok;
    }
    return ok;
  }

  // The interactive prompt: each complete command is evaluated as typed at
  // a console (its result printed, an error reported, the next one read).
  // The background checker runs only here, never in batch mode (m3-drc
  // design D8).
  bool run_prompt() {
    const TclSource console{"-", true, true, true};
    const std::size_t failures = monitor_.failures();
    const InteractiveRun run = run_interactive(*dispatcher_, editor_, 0, out_, true, [&](const TclChunk& chunk) {
      SourceRun ignored;
      return evaluate_chunk(*editor_.script.interp, chunk, console, ignored);
    });
    return run.ok && monitor_.failures() == failures;
  }

  // The result of running one source.
  struct SourceRun {
    std::size_t failures = 0;  // Tcl errors, plus quiet command failures where they count
    bool aborted = false;      // stopped by a Tcl error or an unreadable source
    bool quit = false;         // stopped by `quit`

    [[nodiscard]] bool ok() const noexcept { return failures == 0 && !aborted; }
  };

  SourceRun run_source(const ScriptSource& source) {
    const auto text =
        source.kind == ScriptSource::Kind::inline_text ? std::optional<std::string>{source.value} : load_source(source);
    if (!text) {
      return SourceRun{0, true, false};
    }
    return run_tcl(*editor_.script.interp, *text, tcl_source(source));
  }

  // Evaluates `text` one complete command at a time (see TclSource).
  SourceRun run_tcl(script::Interpreter& interp, std::string_view text, const TclSource& source) {
    const std::size_t failures = monitor_.failures();
    SourceRun run;
    ChunkReader reader{interp, text};
    while (!script_session_.quit_requested) {
      const auto chunk = reader.next();
      if (!chunk) {
        break;
      }
      if (!evaluate_chunk(interp, *chunk, source, run) && !source.keep_going) {
        run.aborted = true;
        break;
      }
    }
    if (source.count_failures) {
      run.failures += monitor_.failures() - failures;
    }
    run.quit = script_session_.quit_requested;
    return run;
  }

  // Evaluates one chunk; prints its result or reports its Tcl error. False
  // on a Tcl error (`quit` unwinds the evaluation: not an error).
  bool evaluate_chunk(script::Interpreter& interp, const TclChunk& chunk, const TclSource& source, SourceRun& run) {
    monitor_.forget();
    magic_console_.set_in_tcl(true);
    const auto result = interp.evaluate(chunk.text, source.origin);
    magic_console_.set_in_tcl(false);
    if (script_session_.quit_requested) {
      return true;
    }
    if (result.ok()) {
      print_result(result.value, source.echo);
      return true;
    }
    ++run.failures;
    report_tcl_error(result, located_error(result, source.name, chunk.line));
    return false;
  }

  void print_result(const cmd::Value& value, bool echo) {
    if (!echo || value.is_none()) {
      return;
    }
    const std::string text = value.to_string();
    if (!text.empty()) {
      magic_console_.flush();
      print_line(out_, text);
    }
  }

  // A Tcl error: tclsh prints its message (the Magic console, nothing
  // else); the other formats report it as a located diagnostic unless the
  // failing command reported the same error already, then the sandbox's
  // diagnostics of the evaluation.
  void report_tcl_error(const cmd::CommandResult& result, const diag::Diagnostic& error) {
    if (options_.diagnostics == diag::Format::magic) {
      magic_console_.flush();
      print_line(err_, error.text);
      return;
    }
    if (!monitor_.reported(error.text)) {
      dispatcher_->report(error);
    }
    for (std::size_t i = 1; i < result.diagnostics.size(); ++i) {
      dispatcher_->report(result.diagnostics[i]);
    }
  }

  std::optional<std::string> load_source(const ScriptSource& source) {
    if (source.kind == ScriptSource::Kind::standard_input) {
      auto text = read_stdin();
      if (!text) {
        dispatcher_->report(diag::make(codes::kCliScript, "cannot read script from standard input"));
      }
      return text;
    }
    auto text = services_.files().read(source.value);
    if (!text) {
      dispatcher_->report(diag::make(codes::kCliScript, "cannot read script: " + text.error().text));
      return std::nullopt;
    }
    return std::move(text).value();
  }

  const Options& options_;
  std::FILE* out_;
  std::FILE* err_;
  platform::PortableServices services_;
  platform::Logger* logger_ = nullptr;
  std::optional<diag::Diagnostic> log_warning_;
  settings::ProjectSettings project_;
  cmd::ScriptSession script_session_{project_};
  cmd::CommandRegistry registry_;
  script::BatchConsentHost consent_{options_.trust_pdk};  // the editor's trust store points at it
  editor::EditorState editor_;
  std::unique_ptr<pdk::InstallHost> install_host_ = pdk::make_local_install_host();
  std::unique_ptr<pdk::Fetcher> pdk_fetcher_;
  std::unique_ptr<pdk::PdkManager> pdk_manager_;  // outlives every use of the store (the dispatcher dies first)
  StreamSink sink_;
  std::unique_ptr<cmd::CommandLog> command_log_;
  RunMonitor monitor_;
  MagicConsole magic_console_;
  std::unique_ptr<cmd::Dispatcher> dispatcher_;
};

int report_usage_error(const std::string& text, std::FILE* err) {
  print_line(err, diag::format_text(diag::make(codes::kCliUsage, text)));
  print_line(err, "try 'hefesto-cli --help'");
  return kExitUsage;
}

// --help, --version, --diagnostic-codes; nullopt when none was given.
std::optional<int> run_informational(const Options& options, std::FILE* out) {
  if (options.show_help) {
    std::fputs(usage_text().c_str(), out);
    return kExitOk;
  }
  if (options.show_version) {
    print_line(out, "hefesto-cli " + std::string{version()} + " (Magic " + std::string{kMagicCompatVersion} +
                        " compatible)");
    return kExitOk;
  }
  if (options.show_codes) {
    std::fputs(diag::render_code_table_markdown(options.codes_area).c_str(), out);
    return kExitOk;
  }
  return std::nullopt;
}

// --strip-reflibs <dir> (scripting-auxiliary-tools "Reference-library
// stripper"): strips every `.mag` file of the directory, printing Magic's
// error text for each file it cannot rewrite.
int run_strip_reflibs(const Options& options, std::FILE* out, std::FILE* err) {
  platform::LocalFileSystem files;
  const auto problems = tools::strip_reflibs(files, *options.strip_reflibs);
  for (const auto& d : problems) {
    // Magic's script prints its error on stdout (strip_reflibs.tcl:23).
    if (options.diagnostics == diag::Format::magic) {
      print_line(out, diag::format_magic(d));
    } else if (const auto text = diag::render(d, options.diagnostics)) {
      print_line(err, *text);
    }
  }
  return problems.empty() ? kExitOk : kExitFailure;
}

// --magic mode: Magic's command line as its script host classifies it
// (scripting-pdk-integration "Command-line options in the script host";
// magic.tcl.in's argument loop, with Q-SCRIPT-37's spellings): `--version`,
// `--commit` and `--prefix` print and exit 0, `-r`/`-re`/`-recover` (and
// `-rcover`) request recovery, `-now...` is dropped, and the rest goes to
// the engine's option parser in order (an option missing its argument
// passes the empty string, as `lindex` does).
script::ScriptHostCommandLine script_host_line(const Options& options) {
  const auto cad_root = app::resolve_install_root(pdk::make_local_install_host()->environment("CAD_ROOT"), Path{});
  const script::ScriptHostInfo info{std::string{kMagicCompatVersion}, std::string{kMagicCompatCommit},
                                    cad_root.string()};
  return script::classify_command_line(options.magic_args, info);
}

// The startup options: Magic's command line in --magic mode (`host`, the
// script host's classification), else -T and the layout files. A Magic
// option or display error prints Magic's text and fails with status 1
// (app-startup "Command-line options").
std::optional<app::StartupOptions> startup_options(const Options& options, const script::ScriptHostCommandLine& host,
                                                   std::FILE* err) {
  app::StartupOptions startup;
  if (options.magic_mode) {
    const std::vector<std::string_view> words(host.engine_args.begin(), host.engine_args.end());
    auto parsed = app::parse_startup_options(words);
    auto display = parsed ? app::resolve_display(parsed.value().display_type, false)
                          : diag::Outcome<app::DisplayChoice>{parsed.error()};
    if (!display) {
      print_line(err, display.error().text);
      return std::nullopt;
    }
    startup = std::move(parsed).value();
    startup.recover = startup.recover || host.recover;
  } else {
    startup.technology = options.tech;
    for (const auto& file : options.files) {
      startup.files.push_back(app::classify_file_argument(file));
    }
  }
  startup.session_settings.insert(startup.session_settings.end(), options.settings.begin(), options.settings.end());
  return startup;
}

}  // namespace

int run(std::span<const std::string_view> args, std::FILE* out, std::FILE* err) {
  const ParsedArgs parsed = parse_args(args);
  if (parsed.error) {
    return report_usage_error(*parsed.error, err);
  }
  if (const auto status = run_informational(parsed.options, out)) {
    return *status;
  }
  if (parsed.options.strip_reflibs) {
    return run_strip_reflibs(parsed.options, out, err);
  }
  const script::ScriptHostCommandLine host =
      parsed.options.magic_mode ? script_host_line(parsed.options) : script::ScriptHostCommandLine{};
  if (host.print_and_exit) {
    print_line(out, *host.print_and_exit);
    return kExitOk;
  }
  const auto startup = startup_options(parsed.options, host, err);
  if (!startup) {
    return kExitFailure;
  }
  Session session{parsed.options, out, err};
  return session.run(*startup);
}

}  // namespace hefesto::cli
