// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The procedures xschem provides to `tcleval` values, written from the table
// "Procedures that xschem provides" of capability
// schematic-symbols-and-libraries "tcleval values" (sources: the public
// xschem manual, the SCHEM-NL-07 capture, the Apache-2.0 `from_eng` of the
// sky130 tree; Q-SCHEM-8). Plain Tcl for the pure ones, native commands for
// the ones that reach the host (abs_sym_path, textwindow, write_data).
//
// Owner: SYMLIB.

#include "hefesto/schem/tcl/procedures.hpp"

#include <memory>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::schem {

namespace {

// `ev`, `ev7` and `ev0` share ev_format: the words joined with single spaces
// are an `expr` expression; an integer result prints as the integer, any
// other number with %.<digits>g, and an expression that fails or is not a
// number gives the text unchanged. `to_eng` and `tabulate` are informative
// until the M9 captures pin their exact text (SCHEM-SIM-02).
constexpr std::string_view kProcedures = R"tcl(
namespace eval ::hefesto::schem {
  proc ev_format {digits words} {
    set text [join $words " "]
    if {[catch {expr $text} value]} { return $text }
    if {[string is entier -strict $value]} { return $value }
    if {[string is double -strict $value]} { return [format %.${digits}g $value] }
    return $text
  }
}
proc ::ev {args} { ::hefesto::schem::ev_format @EV@ $args }
proc ::ev0 {args} { ::hefesto::schem::ev_format @EV@ $args }
proc ::ev7 {args} { ::hefesto::schem::ev_format @EV7@ $args }
proc ::to_eng {args} {
  set n [string trim [join $args " "]]
  if {![string is double -strict $n]} { error "to_eng: not a number: $n" }
  if {$n == 0} { return 0 }
  set e [expr {int(floor(log10(abs(double($n))) / 3.0)) * 3}]
  if {$e < -18} { set e -18 } elseif {$e > 12} { set e 12 }
  set suffix [dict get {-18 a -15 f -12 p -9 n -6 u -3 m 0 {} 3 k 6 meg 9 g 12 t} $e]
  return [format %.4g [expr {$n / pow(10, $e)}]]$suffix
}
proc ::from_eng {text} {
  if {![regexp -nocase {^\s*([-+]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:e[-+]?[0-9]+)?)(.*)$} $text -> number rest]} {
    error "from_eng: not a number: $text"
  }
  set rest [string tolower [string trimleft $rest]]
  set scale 1
  if {[string match meg* $rest]} {
    set scale 1e6
  } else {
    set scales {a 1e-18 f 1e-15 p 1e-12 n 1e-9 u 1e-6 m 1e-3 k 1e3 g 1e9 t 1e12}
    set first [string index $rest 0]
    if {[dict exists $scales $first]} { set scale [dict get $scales $first] }
  }
  return [expr {$number * $scale}]
}
proc ::sim_is_ngspice {} { return 1 }
proc ::sim_is_xyce {} { return 0 }
proc ::tabulate {text {separator " "}} {
  set rows {}
  foreach line [split $text \n] { lappend rows [split $line $separator] }
  set widths {}
  foreach row $rows {
    for {set i 0} {$i < [llength $row]} {incr i} {
      set w [string length [lindex $row $i]]
      if {$i >= [llength $widths]} { lappend widths $w } elseif {$w > [lindex $widths $i]} { lset widths $i $w }
    }
  }
  set out {}
  foreach row $rows {
    set cells {}
    for {set i 0} {$i < [llength $row]} {incr i} { lappend cells [format %-*s [lindex $widths $i] [lindex $row $i]] }
    lappend out [string trimright [join $cells " "]]
  }
  return [join $out \n]
}
namespace eval ::ngspice {
  proc get_node {args} { return {} }
  proc get_current {args} { return {} }
  proc get_voltage {args} { return {} }
}
)tcl";

std::string procedure_script() {
  std::string script{kProcedures};
  for (const auto& [marker, digits] : {std::pair{std::string_view{"@EV7@"}, kEv7Digits},
                                       std::pair{std::string_view{"@EV@"}, kEvDigits}}) {
    for (std::size_t at = script.find(marker); at != std::string::npos; at = script.find(marker, at)) {
      script.replace(at, marker.size(), std::to_string(digits));
    }
  }
  return script;
}

cmd::CommandResult usage(std::string_view text) {
  return cmd::CommandResult::failure(diag::make("SCHEM-TCL-001", std::string{text}));
}

cmd::CommandResult write_data(const cmd::Invocation& invocation, platform::FileSystem* files) {
  if (invocation.args.size() != 2) return usage("wrong # args: should be \"write_data data file\"");
  if (files == nullptr) return usage("write_data: writing files is not allowed here");
  const diag::Status written = files->write_atomic(invocation.args[1], invocation.args[0]);
  if (!written.ok()) return usage("write_data: " + written.error().text);
  return cmd::CommandResult::success();
}

}  // namespace

const std::vector<std::string>& xschem_procedure_names() {
  static const std::vector<std::string> names{
      "ev",       "ev7",        "ev0",       "to_eng",           "from_eng",
      "sim_is_ngspice", "sim_is_xyce", "tabulate", "ngspice::get_node", "ngspice::get_current",
      "ngspice::get_voltage", "abs_sym_path", "textwindow", "write_data"};
  return names;
}

void install_xschem_procedures(script::Interpreter& interp, ProcedureHooks hooks) {
  (void)interp.evaluate(procedure_script());
  auto shared = std::make_shared<ProcedureHooks>(std::move(hooks));
  interp.define_command("::abs_sym_path", [shared](const cmd::Invocation& invocation) {
    if (invocation.args.size() != 1) return usage("wrong # args: should be \"abs_sym_path ref\"");
    const std::string& reference = invocation.args[0];
    return cmd::CommandResult::success(
        cmd::Value{shared->abs_sym_path ? shared->abs_sym_path(reference) : reference});
  });
  interp.define_command("::textwindow", [shared](const cmd::Invocation& invocation) {
    if (shared->textwindow && !invocation.args.empty()) shared->textwindow(invocation.args[0]);
    return cmd::CommandResult::success();
  });
  interp.define_command("::write_data", [shared](const cmd::Invocation& invocation) {
    return write_data(invocation, shared->files);
  });
}

}  // namespace hefesto::schem
