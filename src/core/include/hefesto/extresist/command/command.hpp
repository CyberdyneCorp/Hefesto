// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extresist` command (capability extresist-command;
// openspec/changes/m7-extraction/design.md D4, D5, D16, D18): Magic's
// CmdExtResis option table and lookup, `Ambiguous option: <w>` and
// `Unknown option: <w>`, `help`, and the run. run() is also M9's entry for
// `rc` simulation netlists.
//
// Scaffold state (tasks.md 3.3, 3.5): every option but `help` and the run
// fail with CORE-NOTIMPL-008 until RESCMD lands them.
//
// Owner: INTEGRATION (this header and the registration); RESCMD (the
// option parsers and run()).

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"

namespace hefesto::editor {
struct EditorState;
}
namespace hefesto::mask {
struct Progress;
}

namespace hefesto::extresist {

/// One run (bare `extresist`, `all`, `cell`, `geometry`, `fasthenry`).
struct ResistRun {
  bool all = false;
  std::optional<std::string> cell;  // `extresist cell`: the selected cell
  bool geometry = false;
  bool fasthenry = false;
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
