// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Crash backups, recovery and archives (capability cell-io-safety: "Crash
// backup policy", "Crash recovery", "Crash backup location and session
// lock", "Import of Magic crash backups", "Backup and archive reading",
// "Archive files"; cmd-hierarchy "Crash backup command", "Archive command";
// ui-gui-managers "Crash backup timer").
//
// Front-end contract:
// - Install a FileLockService that locks between processes
//   (platform_posix::make_lock_service on Linux/macOS) and, for the
//   ownership/ctime/liveness tests, a FileHost (platform_posix::
//   make_file_host) before running commands: `editor.locks = ...;
//   cell_session(editor).host = ...;`.
// - `-r` at startup runs the `crash recover` command (app startup does).
// - A desktop sets `cell_session(editor).import_magic_backups = true`, may
//   set `confirm_recovery` to ask `Recover from backup file <file>?`, and
//   installs a BackupTimerHost in `timer`; when it fires, call
//   run_backup_timer on the command thread.
// - On normal exit call end_crash_session; on a termination request run
//   `crash save` and exit without it (the backup stays for recovery).
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/io_safety.hpp"
#include "hefesto/cell/mag_io.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::cmd {
class Dispatcher;
}
namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cell {

/// The per-session CELL state of `ed`, kept in its lock service (an
/// in-process one is created when the session has none).
[[nodiscard]] CellSession& cell_session(editor::EditorState& ed);

/// A front end's BackupTimerHost fired: runs `crash save` through
/// `dispatcher` (whose editor is the session), reports a failure as
/// `Crash backup save error:  <msg>`, then reschedules when the interval is
/// set and > 0, or warns that backups halted.
void run_backup_timer(cmd::Dispatcher& dispatcher);

/// Normal session end ("Normal exit"): removes the session backup (unless
/// `crash save FILE` named it) and the session lock file, and releases the
/// lock. A session that never wrote a backup has nothing to remove.
void end_crash_session(editor::EditorState& ed, platform::FileSystem& fs);

/// A new session identifier: 128 random bits as 32 lowercase hex digits.
[[nodiscard]] std::string make_session_id();

struct BackupReadOptions {
  std::string_view kind = "backup";  // "backup" or "archive" (messages)
  std::string_view file_name;        // for messages
  bool dereference = true;           // every cell dereferenced
  bool mark_modified = false;        // Q-CELL-33: true for crash backups
};

/// "Backup and archive reading" of file text: every record's cell is found
/// or created, marked available and read (technology ignored); its parents
/// are flagged and its box recomputed. A record without a recorded path
/// gets the location as path when it has a directory (Q-CELL-32). An error
/// keeps the cells read so far. Returns the end record's root cell ("" for
/// none).
[[nodiscard]] diag::Outcome<std::string> read_backup(CellIoContext& ctx, std::string_view text,
                                                     const BackupReadOptions& options);

/// Renders the cells `include` selects as a backup or archive file
/// (records sorted by name); nullopt when no cell qualifies.
[[nodiscard]] std::optional<std::string> write_backup(const CellIoContext& ctx,
                                                      const std::function<bool(const db::CellDef&)>& include,
                                                      std::string_view root_cell);

/// Where recovery looks and what it skips.
struct RecoveryScan {
  platform::Path directory;                     // the crash backup directory
  std::string own_session;                      // this session's id (never offered)
  std::optional<platform::Path> magic_directory;  // $TMPDIR or /tmp, when importing
};

/// The recovery candidates, newest status-change time first (ties: path
/// order): `<id>.backup` files owned by the user whose `<id>.lock` is
/// missing or can be locked, and Magic `MAG<pid>.*` files owned by the
/// user whose process is not running (only when the host can tell).
[[nodiscard]] std::vector<RecoveryCandidate> find_recovery_candidates(platform::FileSystem& fs, FileHost& host,
                                                                      FileLockService& locks,
                                                                      const RecoveryScan& scan);

}  // namespace hefesto::cell
