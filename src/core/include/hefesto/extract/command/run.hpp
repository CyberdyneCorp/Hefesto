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
// M7 R4 (EXTCMD, tasks.md 9.2): run() calls prepare_run(), extract_run()
// and finish_run() in sequence, each taking its lock (re-entrant for the
// command thread, which holds the exclusive lock already); a host that runs
// the extract phase on a worker calls the three itself.
//   - prepare: loads the subtree (ExtAll, ExtIncremental: DBCellReadArea),
//     brings the timestamps up to date (incremental), builds the stack
//     (extDefListFunc / extDefParentFunc: bottom-up, "don't use" cells
//     dropped), resolves and opens each `.ext` output (extract::AtomicOutput,
//     Q-EXTRACT-27), runs the temporary unique pass and swaps the isolated
//     substrate planes in (extPrepSubstrate), buffering the messages so the
//     console order stays Magic's;
//   - extract: extract_cell() for each cell, each file committed after its
//     last record (abandoned on cancel or error, the previous file intact);
//   - finish: reverts every temporary unique label (Q-EXTRACT-6, the top
//     cell included), restores the substrate planes, adds the feedback and
//     prints the totals.
//
// Owner: INTEGRATION (this header); EXTCMD (the implementation).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/node/unique.hpp"
#include "hefesto/gds/write/subtree.hpp"

namespace hefesto::editor {
struct EditorState;
}
namespace hefesto::mask {
struct Progress;
}
namespace hefesto::diag {
class DiagnosticSink;
}
namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::extract {

/// Which cells a run extracts.
enum class RunMode : std::uint8_t {
  incremental,  // bare `extract`: stale or missing cells and their parents
  all,          // `extract all`: every cell of the root's tree
  cell,         // `extract cell NAME`: the selected cell into NAME.ext
  parents,      // `extract parents`: the selected cell and every user of it
  showparents,  // added in M7 R4: `extract showparents` (print, no extraction)
};

struct RunRequest {
  RunMode mode = RunMode::incremental;
  std::string root;                        // the root cell; empty = the window's root
  std::optional<std::string> output_name;  // `extract cell NAME`
  /// Added in M7 R4: Magic's console messages; nullptr = not reported.
  diag::DiagnosticSink* sink = nullptr;
  /// Added in M7 R4: the file system the `.ext` files are read and written
  /// on; nullptr = the local disk.
  platform::FileSystem* files = nullptr;
  /// Added in M7 R4: DBCellRead for the unread cells of the subtree and
  /// DBFixMismatch (gds::command_subtree_loader); empty = nothing is read.
  gds::SubtreeLoader loader;
  /// Added in M7 R4: the time DBUpdateStamps stamps modified cells with
  /// (incremental runs; seconds); 0 = the system clock.
  std::int64_t now = 0;
  /// Added in M7 R4: the directory relative output names resolve against
  /// (Q-EXTRACT-11: the project folder on a sandboxed host); nullopt = the
  /// process's current directory (desktop hosts, Magic's behaviour).
  std::optional<std::string> current_directory;
  /// Added in M7 R4: the `$NAME` substrate variable lookup (the project's
  /// Tcl globals); empty = no variable is defined.
  std::function<std::optional<std::string>(std::string_view)> variable;
};

/// What a run did.
struct RunReport {
  std::vector<std::string> extracted;  // cells written, in extraction order
  std::int32_t errors = 0;
  std::int32_t warnings = 0;
  bool cancelled = false;
};

/// Added in M7 R4: the state a run carries between its phases.
class PreparedRun {
 public:
  PreparedRun();
  ~PreparedRun();
  PreparedRun(PreparedRun&&) noexcept;
  PreparedRun& operator=(PreparedRun&&) noexcept;
  PreparedRun(const PreparedRun&) = delete;
  PreparedRun& operator=(const PreparedRun&) = delete;

  struct State;
  [[nodiscard]] State& state() noexcept { return *state_; }

 private:
  std::unique_ptr<State> state_;
};

/// Added in M7 R4: the prepare phase (exclusive lock). Fails with the
/// command's message (no style, unnamed root, unreadable subtree); an empty
/// stack is a success whose run writes nothing.
[[nodiscard]] diag::Outcome<PreparedRun> prepare_run(editor::EditorState& editor, const RunRequest& request,
                                                     const mask::Progress* progress);
/// Added in M7 R4: the extract phase (shared lock): writes the files.
void extract_run(editor::EditorState& editor, PreparedRun& run);
/// Added in M7 R4: the finish phase (exclusive lock); returns the report.
RunReport finish_run(editor::EditorState& editor, PreparedRun& run);

/// `extract`'s driver; prints Magic's progress and summary lines.
[[nodiscard]] cmd::CommandResult run(editor::EditorState& editor, const RunRequest& request,
                                     const mask::Progress* progress);

/// M9's entry (D18): extracts the stale or missing cells of `cell`'s tree
/// and their parents.
[[nodiscard]] diag::Outcome<RunReport> ensure_extracted(editor::EditorState& editor, std::string_view cell,
                                                        const mask::Progress* progress);
/// Added in M7 R4: the same with the file system, subtree loader, clock and
/// variable lookup of `base` (its mode, root and sink are replaced). The
/// form above reads no unread cell and writes on the local disk.
[[nodiscard]] diag::Outcome<RunReport> ensure_extracted(editor::EditorState& editor, std::string_view cell,
                                                        const mask::Progress* progress, RunRequest base);

/// Added in M7 R4: `extract unique [OPT]` (ExtUnique): permanently renames
/// the duplicate labels of every cell of `root`'s tree, bottom-up
/// (`notopports`: `all` for every cell but the root), turning option
/// `unique` off with Magic's warning first.
[[nodiscard]] cmd::CommandResult run_unique(editor::EditorState& editor, const RunRequest& request, UniqueMode mode);

}  // namespace hefesto::extract
