// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The sim netlister, Magic's `ext2sim` / `exttosim` (capability netlist-sim;
// openspec/changes/m7-extraction/design.md D4, D5, D15): `.sim`, `.al` and
// `.nodes` in MIT, LBL and SU formats. SimSettings holds the
// session-sticky options with Magic's defaults. The command matches its
// first word with Magic's table and the first-word rule of
// netlist/first_word.hpp (Q-NETLIST-49).
//
// run() is CmdExtToSim's run path: the run flags (EFArgs and simParseArgs),
// EFReadFile and EFFlatBuild through extflat::read and extflat::flatten,
// then write_sim() (netlist/sim/writer.hpp) and the files. Each run opens
// and closes its own files (Q-NETLIST-52) and its `-o`, `-a` and `-l`
// names apply to that run only.
//
// Owner: INTEGRATION (the registration); SIM (this header and the
// implementation).

#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::platform {
class FileSystem;
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
  double cthresh = 2.0;       // fF; infinity for `cthresh infinite`
  std::int64_t rthresh = 10;  // ohms; INFINITE_THRESHOLD for `rthresh infinite`
  SimMerge merge = SimMerge::none;
  bool extresist = false;
  // Run flags Magic keeps in globals, so they stay set for later runs with
  // magic-compat ON (simParseArgs): -B, -F, -J hier|flat.
  bool no_attrs = false;        // esNoAttrs
  bool dev_nodes_only = false;  // esDevNodesOnly
  bool hier_ap = false;         // esHierAP

  friend bool operator==(const SimSettings&, const SimSettings&) = default;
};

/// One run's arguments and its host: the console and error lines go to
/// `emit` (NETLIST-SIM-001 for TxPrintf, -002 for TxError), the files to
/// `files` (the local file system when null). `window_cell` is the root
/// cell of the current window, used when the arguments name no cell.
struct SimRun {
  std::vector<std::string> args;
  bool magic_compat = true;
  platform::FileSystem* files = nullptr;
  std::function<void(diag::Diagnostic)> emit;
  std::string window_cell;
};

[[nodiscard]] cmd::CommandResult run(editor::EditorState& editor, const SimRun& request);

/// cmdExtToSimOption, in Magic's order: [run], alias, labels, default,
/// format, rthresh, cthresh, merge, extresist, help.
[[nodiscard]] std::span<const std::string_view> sim_options() noexcept;

/// The text of `ext2sim help`: Magic's usage loop, one `    <entry>` line
/// per option; with magic-compat OFF it ends with the note that sim2simp is
/// not provided and `ext2sim merge` covers it (Q-NETLIST-48).
[[nodiscard]] std::string sim_help(bool magic_compat);

/// Registers `ext2sim` and `exttosim` in the layout client.
bool register_sim_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::netlist::sim
