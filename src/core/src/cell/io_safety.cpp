// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell file safety (cell-io-safety): the in-process lock service, crash
// backup and archive records, backup policy and locations. Ported in part
// from Magic 8.3.694 database/DBio.c (DBReadBackup, DBWriteBackup,
// dbWriteBackupFunc); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).

#include "hefesto/cell/io_safety.hpp"

#include <algorithm>
#include <map>
#include <mutex>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/util/record_order.hpp"

namespace hefesto::cell {

FileHost& CellSession::file_host() {
  if (host) {
    return *host;
  }
  if (!local_) {
    local_ = make_local_file_host();
  }
  return *local_;
}

// --- in-process locks -------------------------------------------------------

struct LockRegistry {
  std::mutex mutex;
  std::map<platform::Path, const void*> owners;  // file -> holding service
};

std::shared_ptr<LockRegistry> make_lock_registry() {
  return std::make_shared<LockRegistry>();
}

namespace {

class InProcessLockService final : public FileLockService {
 public:
  explicit InProcessLockService(std::shared_ptr<LockRegistry> registry) : registry_(std::move(registry)) {}
  ~InProcessLockService() override {
    if (!registry_) {
      return;
    }
    const std::scoped_lock guard{registry_->mutex};
    std::erase_if(registry_->owners, [this](const auto& entry) { return entry.second == this; });
  }
  InProcessLockService(const InProcessLockService&) = delete;
  InProcessLockService& operator=(const InProcessLockService&) = delete;

  diag::Outcome<bool> acquire(const platform::Path& file, std::string* owner) override {
    if (!registry_) {
      held_.insert(file);
      return true;
    }
    const std::scoped_lock guard{registry_->mutex};
    const auto [it, inserted] = registry_->owners.try_emplace(file, this);
    if (!inserted && it->second != this) {
      if (owner != nullptr) {
        *owner = "another process";
      }
      return false;
    }
    held_.insert(file);
    return true;
  }

  void release(const platform::Path& file) override {
    held_.erase(file);
    if (!registry_) {
      return;
    }
    const std::scoped_lock guard{registry_->mutex};
    if (const auto it = registry_->owners.find(file); it != registry_->owners.end() && it->second == this) {
      registry_->owners.erase(it);
    }
  }

  [[nodiscard]] bool holds(const platform::Path& file) const override { return held_.contains(file); }

 private:
  std::shared_ptr<LockRegistry> registry_;
  std::set<platform::Path> held_;
};

}  // namespace

std::unique_ptr<FileLockService> make_in_process_lock_service(std::shared_ptr<LockRegistry> registry) {
  return std::make_unique<InProcessLockService>(std::move(registry));
}

// --- backup and archive records ----------------------------------------------

std::string backup_cell_name(std::string_view location) {
  if (location.ends_with(".mag")) {  // Q-CELL-31: only a trailing .mag
    location.remove_suffix(4);
  }
  if (const std::size_t slash = location.rfind('/'); slash != std::string_view::npos) {
    location.remove_prefix(slash + 1);
  }
  while (!location.empty() && (location.front() == ' ' || location.front() == '\t')) {
    location.remove_prefix(1);
  }
  return std::string{location};
}

namespace {

struct LineCursor {
  std::string_view text;
  std::size_t pos = 0;

  /// The next line without its LF (CR kept out); false at the end.
  bool next(std::string_view& line) {
    if (pos >= text.size()) {
      return false;
    }
    const std::size_t nl = text.find('\n', pos);
    const std::size_t end = nl == std::string_view::npos ? text.size() : nl;
    line = text.substr(pos, end - pos);
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    pos = end + 1;
    return true;
  }
};

diag::Diagnostic backup_error(std::string text) {
  return diag::make(codes::kBackupRead, std::move(text));
}

/// The `.mag` body of one record: lines through `<< end >>`; false when the
/// text ends first.
bool read_body(LineCursor& cur, std::string& body) {
  std::string_view line;
  while (cur.next(line)) {
    body.append(line);
    body.push_back('\n');
    if (line.starts_with("<< end >>")) {
      return true;
    }
  }
  return false;
}

}  // namespace

diag::Outcome<BackupFile> parse_backup_file(std::string_view text, std::string_view kind, std::string_view file_name) {
  const std::string type{kind};
  const std::string name{file_name};
  LineCursor cur{text};
  std::string_view line;
  if (!cur.next(line)) {
    return backup_error("Bad " + type + " file " + name + "; can't read!");
  }
  BackupFile out;
  while (!line.starts_with("end")) {
    if (!line.starts_with("file")) {
      return backup_error("Error in " + type + " file " + name + "; expected keyword \"file\", got \"" +
                          std::string{line} + "\"!");
    }
    BackupEntry entry;
    std::string_view location = line.substr(4);
    while (!location.empty() && (location.front() == ' ' || location.front() == '\t')) {
      location.remove_prefix(1);
    }
    entry.location = std::string{location};
    entry.cell = backup_cell_name(location);
    if (entry.cell.empty()) {
      return backup_error("Error in " + type + " file " + name + "; empty cell name!");
    }
    const bool complete = read_body(cur, entry.mag_text);
    out.entries.push_back(std::move(entry));
    if (!complete || !cur.next(line)) {
      return backup_error("Error in " + type + " file " + name + "; partial restore only!");
    }
  }
  if (line.size() > 4 && line[3] == ' ') {
    out.root_cell = std::string{line.substr(4)};
  }
  return out;
}

std::string render_backup_file(std::vector<BackupEntry> entries, std::string_view root_cell) {
  std::stable_sort(entries.begin(), entries.end(), [](const BackupEntry& a, const BackupEntry& b) {
    return util::compare_natural(a.cell, b.cell) < 0;
  });
  std::string out;
  for (const BackupEntry& e : entries) {
    out += "file " + e.location + "\n" + e.mag_text;
  }
  out += "end";
  if (!root_cell.empty()) {
    out += " " + std::string{root_cell};
  }
  return out + "\n";
}

bool crash_backup_includes(const db::CellDef& def) {
  const db::CellFlags& f = def.flags();
  return f.modified && f.available && !f.no_edit && !f.internal && !f.not_found;
}

bool archive_includes(const db::CellDef& def, bool include_read_only) {
  const db::CellFlags& f = def.flags();
  return f.available && !f.internal && !f.not_found && (include_read_only || !f.no_edit);
}

platform::Path crash_backup_directory(bool apple, const std::optional<std::string>& xdg_state_home,
                                      const std::string& home) {
  if (apple) {
    return platform::Path{home} / "Library" / "Application Support" / "Hefesto" / "CrashBackups";
  }
  if (xdg_state_home && !xdg_state_home->empty()) {
    return platform::Path{*xdg_state_home} / "hefesto" / "crash-backups";
  }
  return platform::Path{home} / ".local" / "state" / "hefesto" / "crash-backups";
}

std::optional<std::int64_t> magic_backup_pid(std::string_view file_name) {
  if (!file_name.starts_with("MAG")) {
    return std::nullopt;
  }
  file_name.remove_prefix(3);
  const std::size_t dot = file_name.find('.');
  const std::string_view digits = file_name.substr(0, dot);
  if (digits.empty() || digits.size() > 18 ||
      !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) {
    return std::nullopt;
  }
  std::int64_t pid = 0;
  for (const char c : digits) {
    pid = pid * 10 + (c - '0');
  }
  return pid;
}

}  // namespace hefesto::cell
