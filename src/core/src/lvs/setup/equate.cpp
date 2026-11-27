// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `equate classes` (lvs-setup-file "equate classes"). Q-LVS-29 (fix, both
// netgen-compat modes): the fresh identity of `-unique` is the name plus
// uppercase letters drawn from the seeded generator, never a random suffix.
// `equate nodes|elements` belong to a comparison in progress and are not
// setup commands.
//
// Owner: LVSSETUP.

#include <algorithm>
#include <array>

#include "hefesto/lvs/net/names.hpp"
#include "runtime.hpp"

namespace hefesto::lvs::setup {

namespace {

constexpr std::array<std::string_view, 4> kKinds{"nodes", "elements", "classes", "pins"};
constexpr std::string_view kUsage = "equate [-unique] classes <cellA> <cellB>";

bool exists_in(const Runtime& runtime, int side, std::string_view name) {
  for (const auto& netlist : runtime.netlists()) {
    if (netlist.side == side && netlist.circuit->db.find_cell(name, netlist.local)) {
      return true;
    }
  }
  return false;
}

bool exists_anywhere(const Runtime& runtime, std::string_view name) {
  return exists_in(runtime, 1, name) || exists_in(runtime, 2, name);
}

/// Q-LVS-29: `name` plus letters A-Z from the seeded generator until no
/// cell of that name exists. A taken name gets another letter appended
/// after 26 draws, so the search always ends.
std::string fresh_identity(Runtime& runtime, const std::string& name) {
  std::string base = name;
  for (;;) {
    for (int draw = 0; draw < 26; ++draw) {
      const auto letter = static_cast<char>('A' + static_cast<int>(runtime.next_random() % 26U));
      std::string candidate = base + letter;
      if (!exists_anywhere(runtime, candidate)) {
        return candidate;
      }
    }
    base += 'A';
  }
}

EquatedClass record(const Target& a, const Target& b, bool wildcard) {
  EquatedClass out;
  out.first = SetupCellRef{a.side, a.get().name};
  out.second = SetupCellRef{b.side, b.get().name};
  out.wildcard = wildcard;
  return out;
}

/// Whether `-unique` needs a fresh identity: either name also exists in the
/// other netlist under a different name than its partner.
bool needs_fresh_identity(const Runtime& runtime, const EquatedClass& pair) {
  const bool same = names_equal(pair.first.cell, pair.second.cell, true);
  return !same && (exists_in(runtime, pair.second.circuit, pair.first.cell) ||
                   exists_in(runtime, pair.first.circuit, pair.second.cell));
}

void declare(Runtime& runtime, EquatedClass pair, bool unique) {
  if (unique && needs_fresh_identity(runtime, pair)) {
    pair.identity = fresh_identity(runtime, pair.first.cell);
  }
  SetupState& state = runtime.state();
  const bool first_is_1 = pair.first.circuit == 1;
  state.equated_classes.emplace_back(first_is_1 ? pair.first.cell : pair.second.cell,
                                     first_is_1 ? pair.second.cell : pair.first.cell);
  runtime.print("Device classes " + pair.first.cell + " and " + pair.second.cell + " are equivalent.");
  state.equated.push_back(std::move(pair));
}

cmd::CommandResult classes(Runtime& runtime, std::span<const std::string> args, bool unique) {
  auto a = runtime.resolve(args[0]);
  auto b = runtime.resolve(args[1]);
  if (!a || !b) {
    runtime.print("Cell to equate does not exist.");
    return command_error("0");  // the message is printed; the Tcl error's result is 0
  }
  const bool wildcard = a.value().wildcard || b.value().wildcard;
  if (!wildcard) {
    const Target& ta = a.value().targets.front();
    const Target& tb = b.value().targets.front();
    if (ta.circuit == tb.circuit && ta.number == tb.number) {
      runtime.print("Cells to equate are in the same netlist.");
      return command_error("0");
    }
    declare(runtime, record(ta, tb, false), unique);
    return cmd::CommandResult::success(1);
  }
  // A wildcard: the pair applies while the compare queue is built; the
  // first resolved cell of each side names it.
  const Target& ta = a.value().targets.front();
  const Target* tb = &b.value().targets.front();
  for (const auto& t : b.value().targets) {
    if (t.side != ta.side) {
      tb = &t;
      break;
    }
  }
  declare(runtime, record(ta, *tb, true), unique);
  return cmd::CommandResult::success(1);
}

}  // namespace

cmd::CommandResult pin_lists(Runtime& runtime, std::span<const std::string> args);

cmd::CommandResult equate_command(Runtime& runtime, const Words& words) {
  const bool unique = !words.empty() && words[0] == "-unique";
  const std::span<const std::string> rest{words.begin() + (unique ? 1 : 0), words.end()};
  if (rest.empty()) {
    return wrong_args(kUsage);
  }
  const auto kind = match_keyword(rest[0], kKinds);
  if (!kind) {
    return command_error(bad_option("option", rest[0], kKinds));
  }
  if (*kind != 2) {
    return command_error("equate " + std::string{kKinds.at(*kind)} +
                         ": no comparison is in progress during setup evaluation");
  }
  if (rest.size() == 3) {
    return classes(runtime, rest.subspan(1), unique);
  }
  if (rest.size() == 5 && !unique) {
    auto renamed = pin_lists(runtime, rest.subspan(1));
    if (!renamed.ok() || renamed.value != cmd::Value{1}) {
      return renamed;
    }
    const std::array<std::string, 2> cells{rest[1], rest[3]};
    return classes(runtime, cells, false);
  }
  return wrong_args(kUsage);
}

}  // namespace hefesto::lvs::setup
