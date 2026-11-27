// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `permute` (lvs-setup-file "permute command"; Q-LVS-5 fixed in both
// netgen-compat modes: a cell-wide `forget` leaves an empty pair list).
//
// Owner: LVSSETUP.

#include <algorithm>
#include <array>

#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/net/permute.hpp"
#include "runtime.hpp"

namespace hefesto::lvs::setup {

namespace {

constexpr std::array<std::string_view, 7> kKeywords{"transistors", "resistors", "capacitors", "inductors",
                                                    "default",     "forget",    "pins"};
enum Keyword : std::size_t { kTransistors, kResistors, kCapacitors, kInductors, kDefault, kForget, kPins };

bool same_pin(const Cell& cell, std::string_view a, std::string_view b) {
  return names_equal(a, b, cell.flags.case_insensitive);
}

bool has_pair(const Cell& cell, std::string_view a, std::string_view b) {
  return std::any_of(cell.permutes.begin(), cell.permutes.end(), [&](const PinPair& pair) {
    return (same_pin(cell, pair.first, a) && same_pin(cell, pair.second, b)) ||
           (same_pin(cell, pair.first, b) && same_pin(cell, pair.second, a));
  });
}

/// Declares (a, b) on `cell` when both pins exist and the pair is new.
void declare_if_present(Cell& cell, std::string_view a, std::string_view b) {
  if (port_position(cell, a) && port_position(cell, b) && !has_pair(cell, a, b)) {
    cell.permutes.push_back(PinPair{std::string{a}, std::string{b}});
  }
}

void declare_groups(Cell& cell, PermuteGroups groups) {
  const DeviceClass c = cell.device_class;
  if (groups.transistors && is_transistor(c)) {
    declare_if_present(cell, "drain", "source");
  }
  if (groups.resistors && is_resistor(c)) {
    declare_if_present(cell, "end_a", "end_b");
  }
  if (groups.inductors && c == DeviceClass::inductor) {
    declare_if_present(cell, "end_a", "end_b");
  }
  if (groups.capacitors && is_capacitor(c)) {
    declare_if_present(cell, "top", "bottom");
  }
}

/// `permute [pins] <c> <p1> <p2>` on one target.
void permute_pins(Runtime& runtime, const Target& target, std::string_view p1, std::string_view p2) {
  Cell& cell = target.get();
  for (const std::string_view pin : {p1, p2}) {
    if (!port_position(cell, pin)) {
      runtime.print("No such pin " + std::string{pin} + " in model " + cell.name);
      // stderr, as the capture of NG-SET-04 shows.
      runtime.print_error("Unable to permute model " + cell.name + " pins " + std::string{p1} + ", " +
                          std::string{p2} + ".");
      return;
    }
  }
  if (!has_pair(cell, p1, p2)) {
    cell.permutes.push_back(PinPair{std::string{p1}, std::string{p2}});
  }
  runtime.print("Model " + cell.name + " pin " + std::string{p1} + " == " + std::string{p2});
}

/// `permute forget [<c> [<p1> <p2>]]`.
cmd::CommandResult forget(Runtime& runtime, std::span<const std::string> args) {
  if (args.empty()) {
    for (Circuit* circuit : runtime.circuits()) {
      for (auto& cell : circuit->db.cells) {
        cell.permutes.clear();
      }
    }
    return cmd::CommandResult::success();
  }
  if (args.size() != 1 && args.size() != 3) {
    return wrong_args("permute forget [<valid_cellname> [<pin1> <pin2>]]");
  }
  auto resolved = runtime.resolve(args[0]);
  if (!resolved) {
    return command_error(resolved.error().text);
  }
  for (const auto& target : resolved.value().targets) {
    Cell& cell = target.get();
    if (args.size() == 1) {
      cell.permutes.clear();  // Q-LVS-5: the list is emptied, never left dangling
      runtime.print("No permutations on circuit " + cell.name);
      continue;
    }
    std::erase_if(cell.permutes, [&](const PinPair& pair) {
      return (same_pin(cell, pair.first, args[1]) && same_pin(cell, pair.second, args[2])) ||
             (same_pin(cell, pair.first, args[2]) && same_pin(cell, pair.second, args[1]));
    });
    runtime.print("Model " + cell.name + " pin " + args[1] + " != " + args[2]);
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult pins(Runtime& runtime, std::span<const std::string> args) {
  if (args.size() != 3) {
    return wrong_args("permute [pins] <valid_cellname> <pin1> <pin2>");
  }
  auto resolved = runtime.resolve(args[0]);
  if (!resolved) {
    runtime.print("No such model " + args[0]);
    runtime.print_error("Unable to permute model " + args[0] + " pins " + args[1] + ", " + args[2] + ".");
    return cmd::CommandResult::success();
  }
  for (const auto& target : resolved.value().targets) {
    permute_pins(runtime, target, args[1], args[2]);
  }
  return cmd::CommandResult::success();
}

PermuteGroups groups_of(std::size_t keyword) {
  PermuteGroups groups;
  groups.transistors = keyword == kTransistors || keyword == kDefault;
  groups.resistors = keyword == kResistors || keyword == kDefault;
  groups.inductors = keyword == kInductors || keyword == kDefault;
  groups.capacitors = keyword == kCapacitors;
  return groups;
}

}  // namespace

bool is_transistor(DeviceClass c) noexcept {
  return c == DeviceClass::nmos || c == DeviceClass::pmos || c == DeviceClass::nmos4 || c == DeviceClass::pmos4 ||
         c == DeviceClass::fet;
}

bool is_resistor(DeviceClass c) noexcept {
  return c == DeviceClass::resistor || c == DeviceClass::res3;
}

bool is_capacitor(DeviceClass c) noexcept {
  return c == DeviceClass::capacitor || c == DeviceClass::cap3 || c == DeviceClass::ecap;
}

void apply_permute_groups(Runtime& runtime, PermuteGroups groups) {
  for (Circuit* circuit : runtime.circuits()) {
    for (auto& cell : circuit->db.cells) {
      declare_groups(cell, groups);
    }
  }
}

cmd::CommandResult permute_command(Runtime& runtime, const Words& words) {
  if (words.empty()) {
    apply_permute_groups(runtime, groups_of(kDefault));
    return cmd::CommandResult::success();
  }
  // Three words are always `<c> <p1> <p2>`, so a cell named like a keyword
  // prefix (`d`, `pin`) is still a cell.
  const auto keyword = words.size() == 3 ? std::nullopt : match_keyword(words[0], kKeywords);
  const std::span<const std::string> rest{words.begin() + 1, words.end()};
  if (!keyword) {
    return pins(runtime, words);
  }
  if (*keyword == kForget) {
    return forget(runtime, rest);
  }
  if (*keyword == kPins) {
    return pins(runtime, rest);
  }
  if (!rest.empty()) {
    return wrong_args("permute [transistors|resistors|capacitors|inductors|default]");
  }
  apply_permute_groups(runtime, groups_of(*keyword));
  return cmd::CommandResult::success();
}

}  // namespace hefesto::lvs::setup
