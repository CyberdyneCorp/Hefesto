// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-session CELL state (capability cell-io-safety: the `locking`
// setting, the file host, pending timestamp mismatches, crash backups and
// their timer, recovery), held by editor::EditorState::cell_session. The
// lock service (FileLockService, hefesto/cell/io_safety.hpp) stays a pure
// host mechanism.
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "hefesto/cell/host.hpp"
#include "hefesto/cell/timestamps.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::cell {

/// Host timer behind `crashbackups` (ui-gui-managers "Crash backup timer").
/// The core keeps the schedule; a front end owns the clock. When the timer
/// fires, the front end calls cell::run_backup_timer (hefesto/cell/crash.hpp)
/// on the thread that runs commands. Headless sessions install none: the
/// schedule is kept as state only.
class BackupTimerHost {
 public:
  BackupTimerHost() = default;
  BackupTimerHost(const BackupTimerHost&) = delete;
  BackupTimerHost& operator=(const BackupTimerHost&) = delete;
  virtual ~BackupTimerHost() = default;

  /// Arms the one-shot timer to fire once after `delay_ms` (> 0),
  /// replacing a pending one.
  virtual void schedule(std::int64_t delay_ms) = 0;
  /// Cancels the pending timer, if any.
  virtual void cancel() = 0;
};

/// Per-session CELL state (editor::EditorState::cell_session). Front ends
/// install `host`, `timer`, `confirm_recovery` and `import_magic_backups`
/// before running commands; replacing EditorState::locks keeps it.
struct CellSession {
  CellSession();
  ~CellSession();
  CellSession(const CellSession&) = delete;
  CellSession& operator=(const CellSession&) = delete;

  bool locking = true;                    // the `locking` setting
  std::unique_ptr<FileHost> host;         // nullptr: the local disk
  TimestampLog stamps;                    // pending mismatches
  // Session crash backup target; empty: none chosen yet. A plain Path, not
  // std::optional: GCC 14 reports a bogus -Wmaybe-uninitialized on
  // optional<path>::emplace.
  platform::Path backup_file;
  bool backup_explicit = false;           // backup_file was named by `crash save FILE`
  /// "Crash backup location and session lock"; empty: the platform
  /// directory from the path variables (crash_backup_directory).
  platform::Path backup_directory;
  std::string session_id;                 // 32 lowercase hex digits, set on first backup
  std::optional<std::int64_t> backup_interval_ms = 60000;  // nullopt: unset (`crashbackups disable`)
  enum class BackupTimer : std::uint8_t { stopped, running, disabled } backup_timer = BackupTimer::stopped;
  std::unique_ptr<BackupTimerHost> timer;  // nullptr: no host clock (headless)
  /// "Crash recovery" confirmation `Recover from backup file <file>?`:
  /// true answers `read`. Unset: `read` (the default answer, headless).
  /// Must not capture references that can dangle: it lives with the session.
  std::function<bool(const platform::Path& backup)> confirm_recovery;
  /// "Import of Magic crash backups": also offer `$TMPDIR/MAG*` files
  /// (desktop front ends set it; needs FileHost::process_running).
  bool import_magic_backups = false;
  std::set<const db::CellDef*> renamed_duplicates;  // Q-CELL-24 warning on save
  /// The host to use: `host` or the local disk.
  [[nodiscard]] FileHost& file_host();

 private:
  std::unique_ptr<FileHost> local_;
};

}  // namespace hefesto::cell
