// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell file safety (capability cell-io-safety): advisory locks between
// sessions, crash backup and archive files and crash recovery candidates.
// The per-session CELL state is cell::CellSession (hefesto/cell/session.hpp),
// held by editor::EditorState.
// The lock primitive is a host mechanism behind FileLockService (Linux and
// macOS: platform_posix::make_lock_service, POSIX fcntl locks; tests and
// headless runs: the in-process default). Desktop parts of hefesto-files-and-pdks
// "Locking and recovery replacements".
// (Magic ref: utils/flock.c, database/DBio.c:1113-1333, database/DBio.c:6382-6533)
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/host.hpp"
#include "hefesto/cell/session.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::cell {

/// Advisory file locks between sessions: a pure host mechanism with no
/// session state (that is CellSession, held by editor::EditorState).
class FileLockService {
 public:
  FileLockService() = default;
  FileLockService(const FileLockService&) = delete;
  FileLockService& operator=(const FileLockService&) = delete;
  virtual ~FileLockService() = default;

  /// Takes the session's exclusive advisory lock. Returns false (not an
  /// error) when another session holds it: the cell opens read-only.
  /// `owner` receives the holder's description when known ("pid 4242").
  [[nodiscard]] virtual diag::Outcome<bool> acquire(const platform::Path& file, std::string* owner = nullptr) = 0;
  virtual void release(const platform::Path& file) = 0;
  /// True while this session holds the lock on `file`.
  [[nodiscard]] virtual bool holds(const platform::Path& file) const = 0;
};

/// Portable default: locks only within this process (a session never
/// conflicts with itself), shared by every session of the process through
/// `registry` when given, so two sessions in one process see each other's
/// locks (used by tests of "Two sessions on one file").
struct LockRegistry;
[[nodiscard]] std::shared_ptr<LockRegistry> make_lock_registry();
[[nodiscard]] std::unique_ptr<FileLockService> make_in_process_lock_service(
    std::shared_ptr<LockRegistry> registry = nullptr);

/// One record of a crash backup or archive file ("Crash backup file format").
struct BackupEntry {
  std::string location;  // the `file` line's location
  std::string cell;      // the cell name derived from it (Q-CELL-31)
  std::string mag_text;  // the `.mag` body, `magic` through `<< end >>`
};
struct BackupFile {
  std::vector<BackupEntry> entries;
  std::string root_cell;  // text after `end `, empty if none
};

/// Cell name of a backup location: a trailing `.mag` removed (Q-CELL-31),
/// the part after the last `/`, leading whitespace skipped.
[[nodiscard]] std::string backup_cell_name(std::string_view location);

/// Splits a backup or archive file into records. Errors (CELL-BACKUP-001)
/// carry Magic's texts; `kind` is "backup" or "archive".
[[nodiscard]] diag::Outcome<BackupFile> parse_backup_file(std::string_view text, std::string_view kind,
                                                          std::string_view file_name);
/// As parse_backup_file, but keeps the records split before an error in
/// `out` ("A body read failure aborts the whole archive read, keeping the
/// cells read so far"; the record before a "partial restore" is complete).
[[nodiscard]] diag::Status parse_backup_records(std::string_view text, std::string_view kind,
                                                std::string_view file_name, BackupFile& out);

/// Renders records (sorted by cell name with the total natural order) and
/// the end record.
[[nodiscard]] std::string render_backup_file(std::vector<BackupEntry> entries, std::string_view root_cell);

/// "Crash backup policy": the cells a crash backup includes.
[[nodiscard]] bool crash_backup_includes(const db::CellDef& def);
/// "Archive files": the cells `archive write` (or `writeall` when
/// `include_read_only`) includes.
[[nodiscard]] bool archive_includes(const db::CellDef& def, bool include_read_only);

/// "Crash backup location and session lock": the platform directory.
/// `xdg_state_home` and `home` come from the path variables.
[[nodiscard]] platform::Path crash_backup_directory(bool apple, const std::optional<std::string>& xdg_state_home,
                                                    const std::string& home);

/// A recovery candidate ("Crash recovery", "Import of Magic crash backups").
struct RecoveryCandidate {
  platform::Path backup;
  platform::Path lock;  // empty for Magic backups
  bool magic = false;
  std::int64_t change_time = 0;  // FileHost::change_time, 0 when unknown
};
/// Magic's backup naming: `MAG<pid>.<suffix>`; the pid or nullopt.
[[nodiscard]] std::optional<std::int64_t> magic_backup_pid(std::string_view file_name);

}  // namespace hefesto::cell
