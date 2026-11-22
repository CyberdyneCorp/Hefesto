// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SPICE netlister, Magic's `ext2spice` / `exttospice` (capabilities
// netlist-spice-options, netlist-spice-devices, netlist-spice-hierarchy;
// openspec/changes/m7-extraction/design.md D4, D5, D14, D18). run() is the
// command's run path; render() produces a netlist in memory with one-run
// settings and leaves EditorState::netlist.spice untouched (M9's entry).
// The command matches its first word with Magic's table and the
// first-word rule of Q-NETLIST-13 (netlist/first_word.hpp).
//
// Scaffold state (tasks.md 3.3, 3.5): every option but `help` and the run
// fail with CORE-NOTIMPL-008, and render() reports it, until SPICE lands
// them.
//
// Owner: INTEGRATION (this header and the registration); SPICE (the
// implementation).

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/netlist/spice/settings.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::netlist::spice {

/// One run's arguments (the words after the first-word rule).
struct SpiceRun {
  std::vector<std::string> args;
};

[[nodiscard]] cmd::CommandResult run(editor::EditorState& editor, const SpiceRun& request);

[[nodiscard]] diag::Outcome<std::string> render(editor::EditorState& editor, std::string_view cell,
                                                const SpiceSettings& settings);

/// cmdExtToSpcOption, in Magic's order: [run], default, format, rthresh,
/// cthresh, merge, extresist, resistor, scale, short, subcircuits,
/// hierarchy, blackbox, renumber, global, lvs, help.
[[nodiscard]] std::span<const std::string_view> spice_options() noexcept;

/// Registers `ext2spice` and `exttospice` in the layout client.
bool register_spice_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::netlist::spice
