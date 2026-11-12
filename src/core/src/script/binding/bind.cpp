// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// bind_registry: the registration of the engine's commands in `magic::`,
// the Tcl-side setup Magic's startup script does before `magic::startup`
// (generic helpers, renamed built-ins, the unknown handler, the global
// import) and the binding's own hooks (see bind.hpp). Ported from Magic
// 8.3.694 (fc5daa9e) tcltk/magic.tcl.in:23-156,285-332 and
// tcltk/tclmagic.c:665-951,1603-1673, Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).

#include "hefesto/script/binding/bind.hpp"

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "binding_internal.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/binding/drcstate.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/binding/variables.hpp"
#include "hefesto/script/binding/window_commands.hpp"
#include "hefesto/script/pdk/helper_origins.hpp"

namespace hefesto::script {

namespace {

using detail::Binding;

constexpr std::string_view kNetgenCode = "SCRIPT-BIND-003";
constexpr std::string_view kXcircuitCode = "SCRIPT-BIND-004";
constexpr std::string_view kIrsimCode = "SCRIPT-BIND-005";

// magic.tcl.in before the import: the background-DRC hook, the console
// stubs of batch mode (added separately), the cross-application flags, the
// `echo` helper, auto_noexec, the unknown handler and the renamed
// built-ins. Each rename is skipped when its target exists and caught when
// the built-in is absent (the sandbox removes `load`'s native loader; Tk's
// `label` and `grid` do not exist without Tk).
constexpr std::string_view kPrelude = R"tcl(
proc magic::drcstate {option} {
   # (Null proc---the shell or a script redefines it)
}
set UsingIRSIM 0
set UsingXCircuit 0
set UsingNetgen 0
foreach ::hefesto::_i [namespace children] {
   switch $::hefesto::_i {
      ::irsim { set UsingIRSIM 1 }
      ::xcircuit { set UsingXCircuit 1 }
      ::netgen { set UsingNetgen 1 }
   }
}
unset -nocomplain ::hefesto::_i
proc echo {args} {
   puts stdout $args
}
set auto_noexec 1
if {[lsearch [info commands] tcl_unknown] < 0} {rename unknown tcl_unknown}
proc unknown { args } {
   # CAD tools special:
   # Check for commands which were renamed to tcl_(command)
   set cmd [lindex $args 0]
   if {[lsearch [info commands] tcl_$cmd] >= 0} {
      set arglist [concat tcl_$cmd [lrange $args 1 end]]
      set ret [catch {eval $arglist} result]
      if {$ret == 0} {
         return $result
      } else {
         return -code $ret -errorcode $::errorCode $result
      }
   }
   return [eval [concat tcl_unknown $args]]
}
foreach {::hefesto::_from ::hefesto::_to} {clock orig_clock flush tcl_flush load tcl_load array tcl_array label tcl_label grid tcl_grid} {
   if {[lsearch [info commands] $::hefesto::_to] < 0} {catch {rename $::hefesto::_from $::hefesto::_to}}
}
unset -nocomplain ::hefesto::_from ::hefesto::_to
if {[lsearch [info commands] tcl_cd] < 0} {catch {rename cd tcl_cd}}
proc cd {args} {
   if {[catch {pwd} olddir]} {set olddir {}}
   set code [catch {uplevel 1 [list tcl_cd {*}$args]} result options]
   if {$code} {
      return -options $options $result
   }
   if {$olddir ne {} && ![catch {pwd} newdir]} {
      ::hefesto::_rebase_cell_paths $olddir $newdir
   }
   return $result
}
)tcl";

// magic.tcl.in's console stubs, defined when no console owns the terminal.
constexpr std::string_view kBatchStubs = R"tcl(
proc magic::suspendout {} {}
proc magic::resumeout {} {}
proc magic::dialog {} {}
proc magic::consolegeometry {} {}
proc magic::consolefocus {} {}
)tcl";

// magic.tcl.in pushnamespace / popnamespace and the import itself.
constexpr std::string_view kImport = R"tcl(
proc pushnamespace { name } {
   set y [namespace eval ${name} info commands ::${name}::*]
   set z [info commands]
   foreach v $y {
      regsub -all {\*} $v {\\*} i
      set x [namespace tail $i]
      if {[lsearch $z $x] < 0} {
         namespace import $i
      }
   }
}
proc popnamespace { name } {
   set z [info commands]
   set l [expr [string length ${name}] + 5]
   while {[set v [lsearch $z ${name}_tcl_*]] >= 0} {
      set y [lindex $z $v]
      set w [string range $y $l end]
      interp alias {} ::$w {}
      rename ::$y ::$w
      puts "Info: replacing ::$w with ::$y"
   }
   namespace forget ::${name}::*
}
namespace eval magic namespace export *
pushnamespace magic
)tcl";

/// The six Magic commands whose names collide with Tcl/Tk built-ins and the
/// built-ins' new names (_tcl_dispatch's conflicts/resolutions tables).
struct Conflict {
  std::string_view name;
  std::string_view builtin;
};
constexpr std::array<Conflict, 6> kConflicts{{{"clockwise", "orig_clock"},
                                              {"flush", "tcl_flush"},
                                              {"load", "tcl_load"},
                                              {"label", "tcl_label"},
                                              {"array", "tcl_array"},
                                              {"grid", "tcl_grid"}}};

// _tcl_dispatch's conflict rule for the global `<name>`: the renamed
// built-in first (in the caller's frame, so `array set a` inside a
// procedure sets the local array); its result when it succeeds; for `load`
// its error when the file's last component has an extension other than
// `mag` and `gz`; otherwise the Magic command.
std::string conflict_proc(const Conflict& c) {
  std::string body = "proc ::" + std::string{c.name} + " {args} {\n" +
                     "   if {![catch {uplevel 1 [list ::" + std::string{c.builtin} +
                     " {*}$args]} result]} {return $result}\n";
  if (c.name == "load") {
    body +=
        "   if {[llength $args] >= 1} {\n"
        "      set tail [lindex [split [lindex $args 0] /] end]\n"
        "      set dot [string last . $tail]\n"
        "      if {$dot >= 0 && [string range $tail [expr {$dot + 1}] end] ni {mag gz}} {\n"
        "         return -code error $result\n"
        "      }\n"
        "   }\n";
  }
  body += "   uplevel 1 [list ::magic::" + std::string{c.name} + " {*}$args]\n}\n";
  return body;
}

// The conflict procedures of the colliding commands that exist; a
// colliding built-in that does not exist here (Tk's `label` and `grid`)
// can never succeed, so its Magic command is simply imported.
std::string conflict_procs(const Binding& b) {
  std::string script;
  for (const Conflict& c : kConflicts) {
    if (b.registry.find(c.name) == nullptr) {
      continue;
    }
    if (c.name == "load" || b.interp.has_command("::" + std::string{c.builtin})) {
      script += conflict_proc(c);
    }
  }
  return script;
}

// True for the commands Magic implements as Tcl procedures defined after
// the import (toolkit, managers, readspice, ...): only in kScriptClient.
// `tag` is a C command of tclmagic and imported like the window commands.
bool is_procedure(const cmd::CommandRegistry& registry, const std::string& name) {
  if (name == "tag" || registry.find(name, cmd::kScriptClient) == nullptr) {
    return false;
  }
  for (const std::string& client : registry.clients()) {
    if (client != cmd::kScriptClient && registry.find(name, client) != nullptr) {
      return false;
    }
  }
  return true;
}

// The Tcl name of a registry command: `::magic::<name>`, except Hefesto's
// own namespaced commands (`hefesto::strict`), which keep their namespace.
std::string tcl_name(const std::string& name) {
  return name.find("::") != std::string::npos ? "::" + name : "::magic::" + name;
}

void define_commands(const Binding& b, bool procedures) {
  for (const std::string& name : b.registry.names()) {
    if (is_procedure(b.registry, name) != procedures) {
      continue;
    }
    if (name == "tag") {
      continue;  // tclmagic's own command, defined natively below
    }
    if (procedures) {
      b.interp.define_command(tcl_name(name), [b, name](const cmd::Invocation& call) {
        return detail::dispatch_procedure(b, name, call);
      });
    } else {
      b.interp.define_command(tcl_name(name), [b, name](const cmd::Invocation& call) {
        return detail::dispatch_command(b, name, call);
      });
    }
  }
}

// `magic::tag` (AddCommandTag): a plain Tcl command, not dispatched.
void define_tag(const Binding& b) {
  b.interp.define_command("::magic::tag", [b](const cmd::Invocation& call) {
    editor::EditorState* editor = b.editor();
    if (editor == nullptr) {
      return detail::tcl_error({});
    }
    cmd::CommandResult result = tag_command(editor->script.tags, call.args);
    return result.ok() ? result : detail::tcl_error(result.diagnostics.front().text);
  });
}

// The value words of `magic::*flags` (_magic_flags).
std::optional<bool> flag_value(std::string_view word) {
  for (const std::string_view on : {"on", "yes", "true", "1"}) {
    if (word == on) {
      return true;
    }
  }
  for (const std::string_view off : {"off", "no", "false", "0"}) {
    if (word == off) {
      return false;
    }
  }
  return std::nullopt;
}

// Tcl_GetIndexFromObj over the flag names (exact case, unique prefix).
std::optional<std::size_t> flag_index(std::string_view word) {
  constexpr std::array<std::string_view, 6> kFlags{"debug", "recover", "silent", "window", "console", "printf"};
  std::optional<std::size_t> found;
  for (std::size_t i = 0; i < kFlags.size(); ++i) {
    if (kFlags[i] == word) {
      return i;
    }
    if (!word.empty() && kFlags[i].starts_with(word)) {
      if (found) {
        return std::nullopt;
      }
      found = i;
    }
  }
  return found;
}

// `magic::*flags flag ?value?`, `magic::display`, `magic::initialize` and
// `magic::startup` (tclmagic.c:665-951). Initialization and startup have
// already happened when the binding is installed: both succeed; PDKINT
// redefines `magic::startup` to run the startup files.
void define_tclmagic_commands(const Binding& b) {
  auto flags = std::make_shared<std::array<bool, 6>>();
  const editor::EditorState* editor = b.editor();
  (*flags)[4] = editor != nullptr && editor->script.output.console;
  b.interp.define_command("::magic::*flags", [flags](const cmd::Invocation& call) {
    if (call.args.empty() || call.args.size() > 2) {
      return detail::tcl_error("wrong # args: should be \"*flags flag ?value?\"");
    }
    const auto index = flag_index(call.args[0]);
    if (!index) {
      return detail::tcl_error("bad flag \"" + call.args[0] +
                               "\": must be debug, recover, silent, window, console, or printf");
    }
    if (call.args.size() == 1) {
      return cmd::CommandResult::success(cmd::Value{std::int64_t{(*flags)[*index] ? 1 : 0}});
    }
    const auto value = flag_value(call.args[1]);
    if (!value) {
      return detail::tcl_error("bad value \"" + call.args[1] + "\"");
    }
    (*flags)[*index] = *value;
    return cmd::CommandResult::success();
  });
  b.interp.define_command("::magic::display", [](const cmd::Invocation&) {
    return cmd::CommandResult::success(cmd::Value{"NULL"});
  });
  for (const std::string_view name : {"::magic::initialize", "::magic::startup"}) {
    b.interp.define_command(name, [](const cmd::Invocation&) { return cmd::CommandResult::success(); });
  }
}

// `irsim`, `xcircuit`, `netgen` (decision D-34): diagnostics, no file is
// sourced (the packages load native libraries, which the sandbox denies).
void define_application_helpers(const Binding& b) {
  struct Helper {
    std::string_view name;
    std::string_view code;
    std::string_view text;
  };
  constexpr std::array<Helper, 3> kHelpers{{
      {"::netgen", kNetgenCode,
       "\"netgen\" is not available in Hefesto; use the built-in \"lvs\" command to compare netlists."},
      {"::xcircuit", kXcircuitCode, "\"xcircuit\" is not available in Hefesto."},
      {"::irsim", kIrsimCode, "\"irsim\" is not available here (desktop only, capability irsim-interface)."},
  }};
  for (const Helper& h : kHelpers) {
    b.interp.define_command(h.name, [b, h](const cmd::Invocation&) {
      b.dispatcher.report(diag::make(h.code, std::string{h.text}));
      return cmd::CommandResult::success();
    });
  }
  b.interp.define_command("::hefesto::_rebase_cell_paths", [b](const cmd::Invocation& call) {
    if (b.editor() != nullptr && call.args.size() == 2) {
      detail::rebase_cell_paths(*b.editor(), call.args[0], call.args[1]);
    }
    return cmd::CommandResult::success();
  });
}

// A diagnostic sink that reports through the dispatcher (to its sinks and
// the logger), owned by a hidden command so it lives as long as the
// interpreter that refers to it.
class ReportingSink final : public diag::DiagnosticSink {
 public:
  explicit ReportingSink(cmd::Dispatcher& dispatcher) : dispatcher_(dispatcher) {}
  void emit(const diag::Diagnostic& diagnostic) override { dispatcher_.report(diagnostic); }

 private:
  cmd::Dispatcher& dispatcher_;
};

cmd::CommandResult evaluate_all(Interpreter& interp, std::initializer_list<std::string_view> scripts) {
  for (const std::string_view script : scripts) {
    cmd::CommandResult result = interp.evaluate(script);
    if (!result.ok()) {
      return result;
    }
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult install_hooks(const Binding& b) {
  editor::EditorState* editor = b.editor();
  if (editor == nullptr) {
    return cmd::CommandResult::success();
  }
  cmd::Dispatcher* dispatcher = &b.dispatcher;
  cmd::CommandResult traced = VariableTraces::install(
      b.interp, *editor, [dispatcher](const diag::Diagnostic& d) { dispatcher->report(d); });
  if (!traced.ok()) {
    return traced;
  }
  install_drc_state(b.interp, *editor);
  // scripting-api "Script-defined procedures in the engine namespace"
  // (Q-SCRIPT-41): PDKINT's origin tracker hears every `proc` of a
  // `::magic::` procedure from a PDK script; its SCRIPT-PDK-003 warning is
  // reported through the dispatcher.
  auto warnings = std::make_shared<ReportingSink>(b.dispatcher);
  b.interp.define_command("::hefesto::_helper_warning_sink", [warnings](const cmd::Invocation&) {
    return cmd::CommandResult::success();
  });
  return install_helper_origin_hook(b.interp, editor->script.helper_origins, editor->script.events, warnings.get());
}

}  // namespace

cmd::CommandResult bind_registry(Interpreter& interp, const cmd::CommandRegistry& registry,
                                 cmd::Dispatcher& dispatcher) {
  const Binding b{interp, registry, dispatcher};
  define_commands(b, false);
  define_tag(b);
  define_tclmagic_commands(b);
  define_application_helpers(b);
  static_cast<void>(interp.evaluate("namespace eval ::hefesto {}"));
  const editor::EditorState* editor = b.editor();
  const bool console = editor != nullptr && editor->script.output.console;
  cmd::CommandResult setup = evaluate_all(interp, {kPrelude, console ? std::string_view{} : kBatchStubs});
  if (setup.ok()) {
    setup = evaluate_all(interp, {conflict_procs(b), kImport});
  }
  if (!setup.ok()) {
    return setup;
  }
  define_commands(b, true);
  return install_hooks(b);
}

cmd::CommandResult register_window_commands(Interpreter& interp, cmd::Dispatcher& dispatcher,
                                            std::span<const std::string> window_paths) {
  const Binding b{interp, dispatcher.registry(), dispatcher};
  for (const std::string& path : window_paths) {
    interp.define_command(path, [b, path](const cmd::Invocation& call) {
      return detail::dispatch_window(b, path, call);
    });
  }
  return cmd::CommandResult::success();
}

}  // namespace hefesto::script
