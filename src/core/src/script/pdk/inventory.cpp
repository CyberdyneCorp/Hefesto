// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK command inventory (scripting-pdk-integration "PDK command and
// variable inventory") and the command-trace hook (tasks.md 6.8).

#include "hefesto/script/pdk/inventory.hpp"

#include <algorithm>
#include <array>

namespace hefesto::script {

namespace {

// The framework procedures every verified PDK calls (same for all three).
std::vector<std::string_view> framework() {
  return {"gencell",         "add_entry",           "add_checkbox",          "add_selectlist",
          "add_message",     "add_dependency",      "add_toolkit_menu",      "add_toolkit_command",
          "add_toolkit_separator", "netlist_to_layout", "suspendall",        "resumeall",
          "pushbox",         "popbox",              "addcommandentry",       "quit",
          "closewrapper",    "gencell_create",      "gencell_change"};
}

const std::array<PdkInventory, 3>& inventories() {
  static const std::array<PdkInventory, 3> kTable = {
      PdkInventory{"sky130",
                   {"addpath", "array", "box", "cif", "drc", "erase", "expand", "ext2spice", "getcell", "label",
                    "paint", "port", "property", "quit", "random", "scalegrid", "select", "snap", "source", "tech",
                    "windownames", "cellname", "path"},
                   framework()},
      PdkInventory{"gf180mcu",
                   {"addpath", "array", "box", "cif", "drc", "erase", "expand", "getcell", "label", "paint", "port",
                    "property", "quit", "random", "scalegrid", "select", "snap", "source", "tech", "windownames",
                    "cellname", "path", "gds", "save"},
                   framework()},
      PdkInventory{"sg13g2",
                   {"array", "box", "cellname", "cif", "copy", "delete", "drc", "erase", "expand", "ext2spice", "gds",
                    "getcell", "label", "load", "paint", "path", "polygon", "port", "property", "quit", "random",
                    "scalegrid", "select", "source", "tag", "tech", "units", "view", "windownames", "wire"},
                   framework()},
  };
  return kTable;
}

std::string bare_name(std::string_view name) {
  if (name.starts_with("::")) {
    name.remove_prefix(2);
  }
  if (name.starts_with("magic::")) {
    name.remove_prefix(7);
  }
  return std::string{name};
}

constexpr std::string_view kTraceScript = R"tcl(
namespace eval ::hefesto::pdk {}
proc ::hefesto::pdk::traced {command op} {
    ::hefesto::pdk::trace_record [lindex $command 0]
}
foreach name $::hefesto::pdk::trace_names {
    foreach candidate [list $name ::magic::$name] {
        if {[info commands $candidate] eq ""} {
            continue
        }
        # A global name counts only as the engine command's import, not as
        # a Tcl built-in of the same name (`trace`, `flush`).
        if {$candidate eq $name && [info commands ::magic::$name] ne ""
                && [namespace origin $name] ne "::magic::$name"} {
            continue
        }
        trace add execution $candidate enter ::hefesto::pdk::traced
    }
}
unset -nocomplain name candidate
)tcl";

}  // namespace

bool PdkInventory::covers(std::string_view name) const {
  return std::find(engine_commands.begin(), engine_commands.end(), name) != engine_commands.end() ||
         std::find(framework_procedures.begin(), framework_procedures.end(), name) != framework_procedures.end();
}

std::span<const PdkInventory> pdk_inventories() {
  return inventories();
}

const PdkInventory* pdk_inventory(std::string_view family) {
  if (family == "ihp-sg13" || family == "ihp-sg13g2") {
    family = "sg13g2";
  }
  for (const auto& inventory : inventories()) {
    if (inventory.family == family) {
      return &inventory;
    }
  }
  return nullptr;
}

std::vector<std::string> missing_commands(const Interpreter& interp, const PdkInventory& inventory) {
  std::vector<std::string> out;
  const auto check = [&](std::string_view name) {
    const std::string bare{name};
    if (!interp.has_command("::magic::" + bare) && !interp.has_command("::" + bare)) {
      out.push_back(bare);
    }
  };
  std::for_each(inventory.engine_commands.begin(), inventory.engine_commands.end(), check);
  std::for_each(inventory.framework_procedures.begin(), inventory.framework_procedures.end(), check);
  return out;
}

std::vector<std::string> uncovered_commands(const std::set<std::string>& traced, const PdkInventory& inventory) {
  std::vector<std::string> out;
  for (const std::string& name : traced) {
    if (!inventory.covers(name)) {
      out.push_back(name);
    }
  }
  return out;
}

cmd::CommandResult CommandTrace::install(Interpreter& interp, std::span<const std::string> names) {
  interp.define_command("::hefesto::pdk::trace_record", [this](const cmd::Invocation& inv) {
    if (!inv.args.empty()) {
      names_.insert(bare_name(inv.args.front()));
    }
    return cmd::CommandResult::success();
  });
  cmd::Value::List list;
  for (const std::string& name : names) {
    list.emplace_back(name);
  }
  if (std::string error; !interp.set_var("::hefesto::pdk::trace_names", cmd::Value{std::move(list)}, &error)) {
    return cmd::CommandResult::failure(diag::make("SCRIPT-TCL-001", error));
  }
  return interp.evaluate(kTraceScript, cmd::Origin::internal);
}

}  // namespace hefesto::script
