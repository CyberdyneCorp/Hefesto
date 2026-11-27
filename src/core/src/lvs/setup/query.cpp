// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The query and output commands of setup files (lvs-setup-file "Query and
// output commands available to setup files"): `format`, `cells`, `ports`
// and the diagnostic commands, whose text is informative. Q-LVS-16 (fix,
// both netgen-compat modes): a width of zero or less is rejected and the
// widths are left unchanged.
//
// Owner: LVSSETUP.

#include <algorithm>
#include <array>
#include <set>

#include "runtime.hpp"

namespace hefesto::lvs::setup {

namespace {

constexpr std::array<std::string_view, 12> kDiagnostic{"nodes",   "elements", "instances", "contents",
                                                       "describe", "leaves",   "log",       "readnet",
                                                       "readlib",  "writenet", "canonical", "help"};

/// The netlists a `cells` query covers: those of `reference`, or all.
diag::Outcome<std::vector<const Netlist*>> netlists_of(Runtime& runtime, const std::string* reference) {
  std::vector<const Netlist*> out;
  if (reference == nullptr) {
    for (const auto& netlist : runtime.netlists()) {
      out.push_back(&netlist);
    }
    return out;
  }
  auto cells = runtime.resolve(*reference);
  if (!cells) {
    return cells.error();
  }
  for (const auto& target : cells.value().targets) {
    const Netlist* netlist = runtime.netlist_of(*target.circuit, target.cell);
    if (netlist != nullptr && std::find(out.begin(), out.end(), netlist) == out.end()) {
      out.push_back(netlist);
    }
  }
  return out;
}

struct CellsQuery {
  bool list = false;
  bool all = false;
  bool top = false;
  const std::string* reference = nullptr;
};

std::vector<std::string> cell_names(const Netlist& netlist, const CellsQuery& query) {
  std::vector<std::string> names;
  const auto& db = netlist.circuit->db;
  for (CellId id = 0; id < db.cells.size(); ++id) {
    const Cell& cell = db.cells[id];
    if (cell.file != netlist.local) {
      continue;
    }
    const bool is_top = cell.flags.top || id == netlist.top;
    if ((query.top && !is_top) || (!query.all && is_primitive(cell.device_class))) {
      continue;
    }
    names.push_back(cell.name);
  }
  return names;
}

diag::Outcome<Target> one_cell(Runtime& runtime, const Words& words, std::string_view usage) {
  if (words.size() != 1) {
    return wrong_args(usage).diagnostics.front();
  }
  auto cells = runtime.resolve(words[0]);
  if (!cells) {
    return cells.error();
  }
  return cells.value().targets.front();
}

cmd::CommandResult print_lines(Runtime& runtime, const std::vector<std::string>& lines) {
  for (const auto& line : lines) {
    runtime.print(line);
  }
  return cmd::CommandResult::success();
}

std::vector<std::string> describe(const Cell& cell) {
  std::size_t globals = 0;
  for (const auto& net : cell.nets) {
    globals += net.global ? 1U : 0U;
  }
  return {"Cell " + cell.name + " (" + std::string{device_class_name(cell.device_class)} + "): " +
          std::to_string(cell.ports.size()) + " ports, " + std::to_string(cell.nets.size()) + " nets (" +
          std::to_string(globals) + " global), " + std::to_string(cell.instances.size()) + " instances"};
}

std::vector<std::string> cell_lines(const NetlistDb& db, const Cell& cell, std::string_view name) {
  std::vector<std::string> lines;
  if (name == "nodes") {
    for (const auto& net : cell.nets) {
      lines.push_back(net.name);
    }
  } else if (name == "elements" || name == "instances") {
    for (const auto& instance : cell.instances) {
      lines.push_back(instance.name);
    }
  } else if (name == "contents") {
    for (const auto& instance : cell.instances) {
      lines.push_back(instance.name + " " + db.cells.at(instance.cell).name);
    }
  } else if (name == "leaves") {
    std::set<std::string> leaves;
    for (const auto& instance : cell.instances) {
      if (is_primitive(db.cells.at(instance.cell).device_class)) {
        leaves.insert(db.cells.at(instance.cell).name);
      }
    }
    lines.assign(leaves.begin(), leaves.end());
  } else {
    lines = describe(cell);
  }
  return lines;
}

}  // namespace

std::span<const std::string_view> diagnostic_command_names() noexcept {
  return kDiagnostic;
}

cmd::CommandResult format_command(Runtime& runtime, const Words& words) {
  SetupState& state = runtime.state();
  if (words.empty()) {
    return cmd::CommandResult::success(cmd::Value::List{state.column_left, state.column_right});
  }
  if (words.size() > 2) {
    return wrong_args("format [<width1> [<width2>]]");
  }
  const auto left = parse_integer(words[0]);
  const auto right = words.size() == 2 ? parse_integer(words[1]) : left;
  if (!left || !right) {
    return command_error("expected integer but got \"" + (left ? words[1] : words[0]) + "\"");
  }
  if (*left <= 0 || *right <= 0) {
    return cmd::CommandResult::success("Column width cannot be zero or less");  // Q-LVS-16: not applied
  }
  constexpr std::int64_t kMaxWidth = 4096;  // a report line stays bounded
  state.column_left = static_cast<int>(std::min(*left, kMaxWidth));
  state.column_right = static_cast<int>(std::min(*right, kMaxWidth));
  return cmd::CommandResult::success();
}

cmd::CommandResult cells_command(Runtime& runtime, const Words& words) {
  CellsQuery query;
  for (const auto& word : words) {
    if (word == "list") {
      query.list = true;
    } else if (word == "-all") {
      query.all = true;
    } else if (word == "-top") {
      query.top = true;
    } else if (query.reference == nullptr) {
      query.reference = &word;
    } else {
      return wrong_args("cells [list] [-all] [-top] [<valid_cellname>]");
    }
  }
  auto netlists = netlists_of(runtime, query.reference);
  if (!netlists) {
    return command_error(netlists.error().text);
  }
  std::vector<std::string> names;
  for (const Netlist* netlist : netlists.value()) {
    auto more = cell_names(*netlist, query);
    names.insert(names.end(), more.begin(), more.end());
  }
  if (query.list) {
    cmd::Value::List out(names.begin(), names.end());
    return cmd::CommandResult::success(std::move(out));
  }
  return print_lines(runtime, names);
}

cmd::CommandResult ports_command(Runtime& runtime, const Words& words) {
  auto target = one_cell(runtime, words, "ports <valid_cellname>");
  if (!target) {
    return command_error(target.error().text);
  }
  std::vector<std::string> names;
  for (const auto& port : target.value().get().ports) {
    names.push_back(port.name);
  }
  return print_lines(runtime, names);
}

cmd::CommandResult diagnostic_command(Runtime& runtime, std::string_view name, const Words& words) {
  if (name == "log") {
    return cmd::CommandResult::success();  // the report log starts with the comparison (lvs-reports)
  }
  if (name == "help") {
    return print_lines(runtime, std::vector<std::string>(kDiagnostic.begin(), kDiagnostic.end()));
  }
  if (name == "readnet" || name == "readlib" || name == "writenet") {
    return command_error(std::string{name} + ": not available while a setup file is evaluated");
  }
  auto target = one_cell(runtime, words, std::string{name} + " <valid_cellname>");
  if (!target) {
    return command_error(target.error().text);
  }
  const Target& t = target.value();
  if (name == "canonical") {
    return cmd::CommandResult::success(cmd::Value::List{t.get().name, static_cast<std::int64_t>(t.number)});
  }
  return print_lines(runtime, cell_lines(t.circuit->db, t.get(), name));
}

}  // namespace hefesto::lvs::setup
