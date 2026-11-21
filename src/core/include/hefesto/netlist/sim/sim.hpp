// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The sim netlister, Magic's `ext2sim` / `exttosim` (capability netlist-sim;
// openspec/changes/m7-extraction/design.md D4, D5, D15): `.sim`, `.al` and
// `.nodes` in MIT, LBL and SU formats. SimSettings holds the
// session-sticky options with Magic's defaults. The command matches its
// first word with Magic's table and the first-word rule of
// netlist/first_word.hpp.
//
// Scaffold state (tasks.md 3.3, 3.5): every option but `help` and the run
// fail with CORE-NOTIMPL-008 until SIM lands them.
//
// Owner: INTEGRATION (this header and the registration); SIM (the
// implementation).

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::netlist::sim {

/// `format MIT|LBL|SU`, in Magic's order (sim_formats).
enum class SimFormat : std::uint8_t { mit, lbl, su };
/// `merge none|conservative|aggressive` (Q-NETLIST-14).
enum class SimMerge : std::uint8_t { none, conservative, aggressive };

struct SimSettings {
  bool alias = false;   // write aliases to `<cell>.al`
  bool labels = false;  // write `<cell>.nodes`
  SimFormat format = SimFormat::mit;
  double cthresh = 2.0;       // fF
  std::int64_t rthresh = 10;  // ohms
  SimMerge merge = SimMerge::none;
  bool extresist = false;

  friend bool operator==(const SimSettings&, const SimSettings&) = default;
};

/// One run's arguments.
struct SimRun {
  std::vector<std::string> args;
};

[[nodiscard]] cmd::CommandResult run(editor::EditorState& editor, const SimRun& request);

/// cmdExtToSimOption, in Magic's order: [run], alias, labels, default,
/// format, rthresh, cthresh, merge, extresist, help.
[[nodiscard]] std::span<const std::string_view> sim_options() noexcept;

/// Registers `ext2sim` and `exttosim` in the layout client.
bool register_sim_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::netlist::sim
