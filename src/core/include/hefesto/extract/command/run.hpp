// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The extraction run driver (capability extract-commands;
// openspec/changes/m7-extraction/design.md D5, D12, D18): `extract`'s
// incremental check, the bottom-up stack and the three locking phases
// (prepare under the exclusive lock, extract under the shared lock, finish
// under the exclusive lock). ensure_extracted() is M9's entry: it brings a
// hierarchy up to date as bare `extract` does (ExtIncremental), never as
// `extract all`.
//
// Scaffold state (tasks.md 3.5): both entry points report CORE-NOTIMPL-008
// until EXTCMD lands them.
//
// Owner: INTEGRATION (this header); EXTCMD (the implementation).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::editor {
struct EditorState;
}
namespace hefesto::mask {
struct Progress;
}

namespace hefesto::extract {

/// Which cells a run extracts.
enum class RunMode : std::uint8_t {
  incremental,  // bare `extract`: stale or missing cells and their parents
  all,          // `extract all`: every cell of the root's tree
  cell,         // `extract cell NAME`: the selected cell into NAME.ext
  parents,      // `extract parents`: the selected cell and every user of it
};

struct RunRequest {
  RunMode mode = RunMode::incremental;
  std::string root;                        // the root cell; empty = the window's root
  std::optional<std::string> output_name;  // `extract cell NAME`
};

/// What a run did.
struct RunReport {
  std::vector<std::string> extracted;  // cells written, in extraction order
  std::int32_t errors = 0;
  std::int32_t warnings = 0;
  bool cancelled = false;
};

/// `extract`'s driver; prints Magic's progress and summary lines.
[[nodiscard]] cmd::CommandResult run(editor::EditorState& editor, const RunRequest& request,
                                     const mask::Progress* progress);

/// M9's entry (D18): extracts the stale or missing cells of `cell`'s tree
/// and their parents.
[[nodiscard]] diag::Outcome<RunReport> ensure_extracted(editor::EditorState& editor, std::string_view cell,
                                                        const mask::Progress* progress);

}  // namespace hefesto::extract
