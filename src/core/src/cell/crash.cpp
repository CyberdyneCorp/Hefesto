// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Crash backups, recovery candidates and archive records (cell-io-safety).
// Ported in part from Magic 8.3.694 database/DBio.c (DBFileRecovery,
// DBReadBackup, DBWriteBackup) and tcltk/tools.tcl (magic::makecrashbackup);
// Magic is (c) the Regents of the University of California, distributed
// under its permissive license (THIRD_PARTY_NOTICES.md). The session lock
// file and the private directory replace Magic's /tmp and SIGCONT probe
// (decision D-8).

#include "hefesto/cell/crash.hpp"

#include <algorithm>
#include <cstdint>
#include <random>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::cell {

namespace {

std::string strip_mag(std::string_view location) {
  if (location.size() > 4 && location.ends_with(".mag")) {
    location.remove_suffix(4);
  }
  return std::string{location};
}

diag::Status read_record(CellIoContext& ctx, const BackupEntry& entry, const BackupReadOptions& options) {
  db::CellDef* def = ctx.library.find(entry.cell);
  if (def == nullptr) {
    auto created = ctx.library.create(entry.cell, ctx.tech->plane_count());
    if (!created) {
      return created.error();
    }
    def = created.value();
  }
  def->flags().not_found = false;
  def->flags().available = true;
  if (options.dereference) {
    def->flags().dereference = true;
  }
  ReadOptions read;
  read.ignore_tech = true;
  read.dereference = options.dereference;
  if (auto status = read_mag(ctx, *def, entry.mag_text, read); !status) {
    return status;
  }
  if (def->file_path().empty() && entry.location.find('/') != std::string::npos) {
    def->set_file_path(strip_mag(entry.location));  // Q-CELL-32
  }
  if (options.mark_modified) {
    def->flags().modified = true;  // Q-CELL-33
  }
  flag_mismatched_parents(*def);
  db::recompute_bbox(*def, ctx.magic_compat);
  return diag::Done{};
}

/// The liveness test of "Crash backup location and session lock": the lock
/// is free when this session can take it (and then gives it back).
bool lock_is_free(FileLockService& locks, const platform::Path& lock) {
  if (locks.holds(lock)) {
    return false;  // this session's own lock
  }
  const auto got = locks.acquire(lock);
  if (!got.ok() || !got.value()) {
    return false;
  }
  locks.release(lock);
  return true;
}

void scan_hefesto_backups(platform::FileSystem& fs, FileHost& host, FileLockService& locks, const RecoveryScan& scan,
                          std::vector<RecoveryCandidate>& out) {
  constexpr std::string_view kSuffix = ".backup";
  const auto listed = fs.list(scan.directory);
  if (!listed) {
    return;
  }
  for (const platform::Path& path : listed.value()) {
    const std::string name = path.filename().string();
    if (name.size() <= kSuffix.size() || !name.ends_with(kSuffix)) {
      continue;
    }
    const std::string id = name.substr(0, name.size() - kSuffix.size());
    const platform::Path lock = scan.directory / (id + ".lock");
    if (id == scan.own_session || !host.owned_by_current_user(path) ||
        (fs.exists(lock) && !lock_is_free(locks, lock))) {
      continue;
    }
    out.push_back(RecoveryCandidate{path, lock, false, host.change_time(path).value_or(0)});
  }
}

void scan_magic_backups(platform::FileSystem& fs, FileHost& host, const platform::Path& directory,
                        std::vector<RecoveryCandidate>& out) {
  const auto listed = fs.list(directory);
  if (!listed) {
    return;
  }
  for (const platform::Path& path : listed.value()) {
    const auto pid = magic_backup_pid(path.filename().string());
    if (!pid || host.is_directory(path) || !host.owned_by_current_user(path)) {
      continue;
    }
    const auto running = host.process_running(*pid);
    if (running && !*running) {
      out.push_back(RecoveryCandidate{path, {}, true, host.change_time(path).value_or(0)});
    }
  }
}

}  // namespace

CellSession& cell_session(editor::EditorState& ed) {
  if (!ed.locks) {
    ed.locks = make_in_process_lock_service();
  }
  return ed.locks->session();
}

std::string make_session_id() {
  static constexpr std::string_view kHex = "0123456789abcdef";
  std::random_device rd;
  std::string id;
  id.reserve(32);
  for (int word = 0; word < 4; ++word) {
    std::uint32_t bits = rd();
    for (int nibble = 0; nibble < 8; ++nibble) {
      id.push_back(kHex[bits & 0xFU]);
      bits >>= 4U;
    }
  }
  return id;
}

diag::Outcome<std::string> read_backup(CellIoContext& ctx, std::string_view text, const BackupReadOptions& options) {
  BackupFile parsed;
  const diag::Status split = parse_backup_records(text, options.kind, options.file_name, parsed);
  for (const BackupEntry& entry : parsed.entries) {
    if (auto read = read_record(ctx, entry, options); !read) {
      return read.error();
    }
  }
  if (!split) {
    return split.error();
  }
  return parsed.root_cell;
}

std::optional<std::string> write_backup(const CellIoContext& ctx,
                                        const std::function<bool(const db::CellDef&)>& include,
                                        std::string_view root_cell) {
  std::vector<BackupEntry> entries;
  WriteOptions options;
  options.backup = true;
  ctx.library.for_each([&](db::CellDef& def) {
    if (!include(def)) {
      return;
    }
    const std::string recorded = def.file_path().generic_string();
    entries.push_back(BackupEntry{recorded.empty() ? def.name() : recorded, def.name(), write_mag(ctx, def, options)});
  });
  if (entries.empty()) {
    return std::nullopt;
  }
  return render_backup_file(std::move(entries), root_cell);
}

std::vector<RecoveryCandidate> find_recovery_candidates(platform::FileSystem& fs, FileHost& host,
                                                        FileLockService& locks, const RecoveryScan& scan) {
  std::vector<RecoveryCandidate> out;
  scan_hefesto_backups(fs, host, locks, scan, out);
  if (scan.magic_directory) {
    scan_magic_backups(fs, host, *scan.magic_directory, out);
  }
  std::stable_sort(out.begin(), out.end(), [](const RecoveryCandidate& a, const RecoveryCandidate& b) {
    return a.change_time > b.change_time;
  });
  return out;
}

void end_crash_session(editor::EditorState& ed, platform::FileSystem& fs) {
  CellSession& s = cell_session(ed);
  if (s.session_id.empty() || s.backup_directory.empty()) {
    return;
  }
  if (s.timer) {
    s.timer->cancel();
  }
  (void)fs.remove(s.backup_directory / (s.session_id + ".backup"));
  const platform::Path lock = s.backup_directory / (s.session_id + ".lock");
  ed.locks->release(lock);
  (void)fs.remove(lock);
  s.backup_file.reset();
  s.backup_explicit = false;
  s.session_id.clear();
}

void run_backup_timer(cmd::Dispatcher& dispatcher) {
  editor::EditorState* ed = dispatcher.editor();
  if (ed == nullptr) {
    return;
  }
  CellSession& s = cell_session(*ed);
  if (s.backup_timer != CellSession::BackupTimer::running) {
    return;  // stopped after the host armed the timer
  }
  const auto result = dispatcher.execute(cmd::Invocation{"crash", {"save"}, cmd::Origin::internal, std::nullopt});
  if (!result.ok()) {
    const std::string text = result.diagnostics.empty() ? std::string{} : result.diagnostics.back().text;
    dispatcher.report(diag::make(codes::kBackupTimer, "Crash backup save error:  " + text));
  }
  if (s.backup_interval_ms && *s.backup_interval_ms > 0) {
    if (s.timer) {
      s.timer->schedule(*s.backup_interval_ms);
    }
    return;
  }
  s.backup_timer = CellSession::BackupTimer::stopped;
  dispatcher.report(diag::make(codes::kBackupTimer,
                               s.backup_interval_ms ? "Warning:  Crash backups halted due to interval of " +
                                                          std::to_string(*s.backup_interval_ms)
                                                    : "Warning:  Crash backups halted due to no interval being set."));
}

}  // namespace hefesto::cell
