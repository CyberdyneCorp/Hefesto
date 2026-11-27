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
// State (tasks.md 11.1-11.5, 11.7): every option is implemented; the run
// and render() read and flatten through extflat::parse_common_args, read
// and flatten (EXTFLAT, tasks.md 10.x) and write flat output with
// write_flat (writer.hpp); hierarchical output (`hierarchy on`, task 11.6)
// with write_hier (hier_writer.cpp).
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
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::netlist::spice {

/// One run's arguments (the words after the first-word rule).
struct SpiceRun {
  std::vector<std::string> args;
};

/// The run with the session's settings and magic-compat setting; writes
/// `<cell>.spice` (or the `-o` file) in the current directory.
[[nodiscard]] cmd::CommandResult run(editor::EditorState& editor, const SpiceRun& request);

/// The netlist of `cell` with one-run settings (M9's entry, design D18):
/// no file is written and EditorState::netlist is not changed.
[[nodiscard]] diag::Outcome<std::string> render(editor::EditorState& editor, std::string_view cell,
                                                const SpiceSettings& settings);

/// render() reading through `files`, the session's file system (M9 passes
/// it so a sandboxed host keeps its project-root scoping; security review
/// M7-S4). The overload above reads the local file system.
[[nodiscard]] diag::Outcome<std::string> render(editor::EditorState& editor, platform::FileSystem& files,
                                                std::string_view cell, const SpiceSettings& settings);

/// cmdExtToSpcOption, in Magic's order: [run], default, format, rthresh,
/// cthresh, merge, extresist, resistor, scale, short, subcircuits,
/// hierarchy, blackbox, renumber, global, lvs, help.
[[nodiscard]] std::span<const std::string_view> spice_options() noexcept;

/// Registers `ext2spice` and `exttospice` in the layout client.
bool register_spice_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::netlist::spice
