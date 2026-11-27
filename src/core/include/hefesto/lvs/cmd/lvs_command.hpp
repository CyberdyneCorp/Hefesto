// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lvs` command on netlist files (capability lvs-command "lvs command
// syntax", "Setup file and input resolution", "Execution model", "Headless
// LVS"; design D16, "As landed (LVSHIER, LVSREP, LVSCMD; M9 R3)"): argument
// parsing, the resolution of netlist and setup paths (Q-LVS-31), the run on
// the background worker with its cancel flag, and the headless exit status.
// The layout source (`lvs -layout`, after M7's G3 and MAPS) and `.sch`
// sources (after M8's G4) fail with CORE-NOTIMPL-009.
//
// Owner: LVSCMD (implementation team of the LVS clean room, design D2).

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/hier/run.hpp"
#include "hefesto/lvs/session.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/task_executor.hpp"

namespace hefesto::lvs {

/// The words of one `lvs` call ("lvs command syntax").
struct LvsArguments {
  bool layout = false;  // `lvs -layout <cell> <schematic> ...`
  std::string name1;
  std::string name2;
  std::string setup = "setup.tcl";
  std::string log = "comp.out";
  bool list = false;
  bool json = false;
  bool blackbox = false;
  std::optional<std::string> noflatten;  // the value of `-noflatten=`
};

/// Parses the arguments after `lvs`; LVS-CMD-002 on a usage error. Options
/// are matched exactly and may appear anywhere.
[[nodiscard]] diag::Outcome<LvsArguments> parse_lvs_arguments(const std::vector<std::string>& args);

/// A netlist name: `{file cell}`, or a single file name (the cell is then
/// the file's top cell; `cell` is empty).
struct NetlistName {
  std::string file;
  std::string cell;
};
[[nodiscard]] diag::Outcome<NetlistName> parse_netlist_name(std::string_view word);

/// Whether the log name turns logging off (it contains `nolog`).
[[nodiscard]] bool logging_off(std::string_view log) noexcept;

/// The places a setup name is looked for, in order ("Setup file and input
/// resolution"): as given, then, for a name with no directory part, the
/// active PDK's `libs.tech/netgen/` folder under each PDK store root,
/// trying `<PDK>_setup.tcl` when the name is the bare PDK name.
[[nodiscard]] std::vector<platform::Path> setup_candidates(std::string_view name, std::string_view active_pdk,
                                                           const std::vector<platform::Path>& pdk_roots);
/// The first candidate that exists; nullopt when none does.
[[nodiscard]] std::optional<platform::Path> resolve_setup(platform::FileSystem& files, std::string_view name,
                                                          std::string_view active_pdk,
                                                          const std::vector<platform::Path>& pdk_roots);

/// The `-noflatten=` value: the lines of a file when it names one (lines
/// whose first element is `#` are skipped), else a Tcl list. LVS-CMD-010
/// past 65536 entries or an entry longer than 4096 bytes.
[[nodiscard]] diag::Outcome<std::vector<std::string>> parse_noflatten(std::string_view value,
                                                                      platform::FileSystem& files);

/// Maps the setup file's state onto the run options. The setup commands
/// that write into the databases (permute, property, model, ...) need
/// nothing here; the hierarchy inputs (equate classes, flatten prohibit,
/// model blackbox, property topology and parallel mode, format widths) come
/// from the typed SetupState.
void apply_setup(const SetupState& setup, RunOptions& options);

/// `<k>/<n> <cell1> vs <cell2>` (k 1-based; "Execution model").
[[nodiscard]] std::string progress_text(std::string_view cell1, std::string_view cell2, std::size_t index,
                                        std::size_t count);

/// The exit status of "Headless LVS": 0 clean, 1 any other completed
/// comparison (mismatch or cancelled), 2 inputs or setup unreadable.
[[nodiscard]] int headless_status(const LvsResult& result) noexcept;

/// One LVS run on the background worker. start() refuses a second run of
/// the same document (LVS-CMD-008); the run's cancel flag is the session's.
struct LvsJob {
  std::string document;
  Circuit first;
  Circuit second;
  SetupState setup;
  RunOptions options;
  /// Called on the worker thread for each pair ("Execution model").
  std::function<void(const std::string&)> on_progress;
};

/// Runs `job` on `tasks` and waits for it; records the result, the input
/// stamps and the headless status in `session`. Thread use: the caller's
/// thread blocks; run_lvs runs on a task of `tasks`.
[[nodiscard]] diag::Outcome<LvsResult> run_job(platform::TaskExecutor& tasks, Session& session, LvsJob job,
                                               std::vector<Session::InputStamp> stamps);

/// Requests cancellation of the document's run in progress; false when
/// none is running.
bool cancel_run(Session& session, std::string_view document);

/// The stamp of one input file now (its modification time).
[[nodiscard]] Session::InputStamp stamp_input(platform::FileSystem& files, const platform::Path& path);

}  // namespace hefesto::lvs
