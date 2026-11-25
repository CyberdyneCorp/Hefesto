// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Launcher confirmation once per document (capability
// schematic-symbols-and-libraries "Script execution permission";
// Q-SCHEM-25), and the command mapping of a confirmed launcher (capability
// schematic-simulation-integration "Simulation launchers", SIMINT).
//
// Owner: SYMLIB (permission); SIMINT (the launcher mapping).

#include "hefesto/schem/tcl/permission.hpp"

#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::schem {

namespace {

constexpr std::string_view kSkippedCode = "SCHEM-SIM-003";

// Installed before a launcher runs: `xschem` and `simulate` go through
// ::hefesto_launcher::run, which records a command that is not supported (or
// not available until M9) and goes on; the `sim` array is saved.
constexpr std::string_view kPrologue = R"tcl(
namespace eval ::hefesto_launcher {
  variable skipped {}
  variable had_sim [array exists ::sim]
  variable sim [array get ::sim]
  variable own_simulate [expr {[info commands ::simulate] eq {}}]
  proc skippable {message} {
    expr {[string match {xschem command '*' is not supported} $message] ||
          [string match {not implemented yet: *} $message]}
  }
  proc run {args} {
    if {[catch {uplevel #0 $args} result options]} {
      if {[skippable $result]} {
        variable skipped
        lappend skipped $result
        return {}
      }
      return -options $options $result
    }
    return $result
  }
}
if {[info commands ::xschem] ne {}} {
  rename ::xschem ::hefesto_launcher::xschem
}
proc ::xschem {args} {
  if {[info commands ::hefesto_launcher::xschem] eq {}} {
    lappend ::hefesto_launcher::skipped "xschem command '[lindex $args 0]' is not supported"
    return {}
  }
  ::hefesto_launcher::run ::hefesto_launcher::xschem {*}$args
}
if {$::hefesto_launcher::own_simulate} {
  proc ::simulate {args} {
    if {[info commands ::magic::schematic] eq {}} {
      lappend ::hefesto_launcher::skipped "not implemented yet: simulate (M9)"
      return {}
    }
    ::hefesto_launcher::run ::magic::schematic simulate {*}$args
  }
}
)tcl";

// Run after the launcher, whatever it did: the commands and the `sim` array
// as they were; the skipped commands, one per line, as the result.
constexpr std::string_view kEpilogue = R"tcl(
catch {rename ::xschem {}}
catch {rename ::hefesto_launcher::xschem ::xschem}
catch {if {$::hefesto_launcher::own_simulate} {rename ::simulate {}}}
catch {
  unset -nocomplain ::sim
  if {$::hefesto_launcher::had_sim} {array set ::sim $::hefesto_launcher::sim}
}
set ::hefesto_launcher_skipped {}
catch {set ::hefesto_launcher_skipped [join $::hefesto_launcher::skipped \n]}
catch {namespace delete ::hefesto_launcher}
set ::hefesto_launcher_skipped
)tcl";

void add_skipped(cmd::CommandResult& result, const std::string& skipped) {
  std::size_t start = 0;
  while (start < skipped.size()) {
    std::size_t end = skipped.find('\n', start);
    if (end == std::string::npos) end = skipped.size();
    if (end > start) {
      result.diagnostics.push_back(diag::make(
          kSkippedCode, "Launcher command skipped: " + skipped.substr(start, end - start) + "; the launcher went on"));
    }
    start = end + 1;
  }
}

}  // namespace

bool LauncherPermission::allow(DocumentId document, std::string_view tclcommand, const ConfirmLauncher& ask) {
  if (confirmed(document)) return true;
  if (!ask || !ask(tclcommand)) return false;
  confirmed_.insert(document);
  return true;
}

cmd::CommandResult run_launcher(script::Interpreter& sandbox, LauncherPermission& permission, DocumentId document,
                                std::string_view tclcommand, const ConfirmLauncher& ask) {
  if (!permission.allow(document, tclcommand, ask)) {
    return cmd::CommandResult::failure(diag::make(
        "SCHEM-TCL-002", "Launcher command not run: it was not confirmed for this document: " + std::string{tclcommand}));
  }
  (void)sandbox.evaluate(kPrologue, cmd::Origin::script);
  cmd::CommandResult result = sandbox.evaluate(tclcommand, cmd::Origin::script);
  const cmd::CommandResult skipped = sandbox.evaluate(kEpilogue, cmd::Origin::script);
  add_skipped(result, skipped.ok() ? skipped.value.to_string() : std::string{});
  (void)sandbox.evaluate("unset -nocomplain ::hefesto_launcher_skipped", cmd::Origin::script);
  return result;
}

}  // namespace hefesto::schem
