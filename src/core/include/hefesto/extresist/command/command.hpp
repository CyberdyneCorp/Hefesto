// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extresist` command (capability extresist-command;
// openspec/changes/m7-extraction/design.md D4, D5, D16, D18): Magic's
// CmdExtResis option table and lookup, `Ambiguous option: <w>` and
// `Unknown option: <w>`, `help`, and the run. run() is also M9's entry for
// `rc` simulation netlists.
//
// The options and the run flow are RESCMD's (tasks.md 14.1-14.3); the
// network of each net is RESNET's (extresist/network/network.hpp).
//
// Owner: INTEGRATION (this header and the registration); RESCMD (the
// option parsers and run()).

#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::editor {
struct EditorState;
}
namespace hefesto::mask {
struct Progress;
}
namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::extresist {

/// One run (bare `extresist`, `all`, `cell`, `geometry`, `fasthenry`).
struct ResistRun {
  bool all = false;
  std::optional<std::string> cell;  // `extresist cell`: the selected cell
  bool geometry = false;
  bool fasthenry = false;
  // Added by RESCMD (M7 R4): the run's environment.
  bool magic_compat = true;
  /// Where `.ext` files are read and the outputs written; nullptr: the
  /// local file system.
  platform::FileSystem* files = nullptr;
  /// The console (TxPrintf and TxError lines); unset: silent.
  std::function<void(diag::Diagnostic)> emit;
};

[[nodiscard]] cmd::CommandResult run(editor::EditorState& editor, const ResistRun& request,
                                     const mask::Progress* progress);

/// cmdExtresisCmd, in Magic's order: all, threshold, minresist, mindelay,
/// tolerance, simplify, extout, lumped, silent, debug, skip, force,
/// ignore, include, box, cell, blackbox, fasthenry, geometry, stats, help.
[[nodiscard]] std::span<const std::string_view> extresist_options() noexcept;

/// Registers `extresist` in the layout client.
bool register_extresist_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::extresist
