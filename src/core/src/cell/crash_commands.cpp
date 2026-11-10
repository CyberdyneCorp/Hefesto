// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `crash` and `archive` commands (cmd-hierarchy "Crash backup command",
// "Archive command"; cell-io-safety). Follows Magic 8.3.694
// commands/CmdCD.c (CmdCrash), commands/CmdAB.c (CmdArchive) and
// database/DBio.c (DBWriteBackup, DBFileRecovery); Magic is (c) the
// Regents of the University of California, distributed under its
// permissive license (THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <vector>

#include "crash_commands.hpp"
#include "hefesto/cell/crash.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "io_session.hpp"

namespace hefesto::cell::detail {

namespace {

using cmd::CommandResult;

constexpr std::uint32_t kPrivateDirectory = 0700U;

void emit_note(const IoSession& s, std::string text) {
  s.emit(codes::kBackupNote, std::move(text));
}

/// A failed backup or archive read. read_mag has already emitted its format
/// error, so returning it again would publish it twice; Magic prints it once
/// (Magic ref: database/DBio.c:1301-1302).
CommandResult backup_read_failure(diag::Diagnostic error) {
  if (error.code == codes::kFormatError) {
    return CommandResult{cmd::Status::failed, {}, {}, false, std::nullopt};
  }
  return CommandResult::failure(std::move(error));
}

/// The root cell of the first layout window ("" when there is none).
std::string first_window_root(const editor::EditorState& ed) {
  std::string root;
  view::WindowId first = std::numeric_limits<view::WindowId>::max();
  ed.windows.for_each([&](view::LayoutWindow& w) {
    if (w.id() < first && w.root() != nullptr) {
      first = w.id();
      root = w.root()->name();
    }
  });
  return root;
}

/// The platform crash backup directory ("Crash backup location and session
/// lock"); empty when the home directory is unknown.
platform::Path platform_backup_directory(const PathVariables& vars) {
#if defined(__APPLE__)
  constexpr bool kApple = true;
#else
  constexpr bool kApple = false;
#endif
  const std::string home = vars.home().value_or("");
  const auto xdg = vars.get("XDG_STATE_HOME");
  if (home.empty() && (kApple || !xdg || xdg->empty())) {
    return {};
  }
  return crash_backup_directory(kApple, xdg, home);
}

platform::Path backup_directory(IoSession& s) {
  return s.session.backup_directory.empty() ? platform_backup_directory(s.io.vars) : s.session.backup_directory;
}

/// The first backup of the session creates the private directory and the
/// session lock file, held for the session's lifetime.
diag::Status open_session_files(IoSession& s) {
  CellSession& cs = s.session;
  if (cs.session_id.empty()) {
    const platform::Path dir = backup_directory(s);
    if (dir.empty()) {
      return diag::make(codes::kWriteFailed, "Cannot locate the crash backup directory (HOME is not set).");
    }
    if (auto made = s.io.fs.create_directories(dir); !made) {
      return made.error();
    }
    (void)s.io.host.set_permissions(dir, kPrivateDirectory);
    const std::string id = make_session_id();
    const platform::Path lock = dir / (id + ".lock");
    if (auto written = s.io.fs.write_atomic(lock, ""); !written) {
      return written.error();
    }
    (void)s.ed.locks->acquire(lock);
    cs.session_id = id;
    cs.backup_directory = dir;
  }
  if (cs.backup_file.empty()) {
    cs.backup_file = cs.backup_directory / (cs.session_id + ".backup");
  }
  return diag::Done{};
}

CommandResult crash_save(IoSession& s, const std::optional<std::string>& file) {
  CellSession& cs = s.session;
  if (file) {
    cs.backup_explicit = !file->empty();
    cs.backup_file = platform::Path{*file};  // an empty name clears the target
  }
  const auto text = write_backup(s.io, crash_backup_includes, first_window_root(s.ed));
  if (!text) {
    return CommandResult::success();  // nothing qualifies: no file is created
  }
  if (auto opened = open_session_files(s); !opened) {
    return CommandResult::failure(opened.error());
  }
  const platform::Path target = cs.backup_file;
  const bool existed = s.io.fs.exists(target);
  if (auto written = s.io.fs.write_atomic(target, *text); !written) {
    return fail(codes::kWriteFailed, "Cannot write crash backup file " + target.generic_string());
  }
  if (!existed || file) {
    emit_note(s, "Created database crash recovery file " + target.generic_string());  // Q-CELL-29
  }
  return CommandResult::success();
}

/// "End record": the root cell is loaded into every layout window.
void show_root(IoSession& s, const std::string& root) {
  db::CellDef* def = root.empty() ? nullptr : s.ed.cells.find(root);
  if (def == nullptr) {
    return;
  }
  expand_all(s.io, *def);
  s.ed.windows.for_each([def](view::LayoutWindow& w) { w.set_root(def); });
  if (s.ed.windows.size() > 0) {
    s.ed.edit_cell = def->flags().no_edit ? nullptr : def;
  }
}

/// The backup to recover: the named file, else the newest candidate
/// (empty when there is none).
std::vector<RecoveryCandidate> choose_backup(IoSession& s, const std::optional<std::string>& file) {
  if (file) {
    RecoveryCandidate named{*file, {}, false, 0};
    if (file->ends_with(".backup")) {
      named.lock = file->substr(0, file->size() - 7) + ".lock";
    }
    return {named};
  }
  RecoveryScan scan{backup_directory(s), s.session.session_id, std::nullopt};
  if (s.session.import_magic_backups) {
    const std::string tmp = s.io.vars.get("TMPDIR").value_or("");
    scan.magic_directory.emplace(tmp.empty() ? std::string{"/tmp"} : tmp);
  }
  auto candidates = find_recovery_candidates(s.io.fs, s.io.host, *s.ed.locks, scan);
  candidates.resize(std::min<std::size_t>(candidates.size(), 1));
  return candidates;
}

CommandResult crash_recover(IoSession& s, const std::optional<std::string>& file) {
  if (!s.session.backup_file.empty()) {
    return fail(codes::kBackupRead, "Error:  Backup file in use for current session.");
  }
  const auto chosen_list = choose_backup(s, file);
  if (chosen_list.empty()) {
    return CommandResult::success();
  }
  const RecoveryCandidate& chosen = chosen_list.front();
  if (s.session.confirm_recovery && !s.session.confirm_recovery(chosen.backup)) {
    return CommandResult::success();  // `cancel`
  }
  const std::string name = chosen.backup.generic_string();
  auto data = s.io.fs.read(chosen.backup);
  if (!data) {
    return fail(codes::kBackupRead, "Cannot open backup file \"" + name + "\"");
  }
  BackupReadOptions options;
  options.file_name = name;
  options.mark_modified = true;
  const auto root = read_backup(s.io, data.value(), options);
  if (!root) {
    return backup_read_failure(root.error());
  }
  (void)s.io.fs.remove(chosen.backup);
  if (!chosen.lock.empty()) {
    (void)s.io.fs.remove(chosen.lock);
  }
  show_root(s, root.value());
  emit_note(s, "Recovered from backup file " + name);
  return CommandResult::success();
}

CommandResult crash_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.empty()) {
    return CommandResult::success();  // `crash` alone does nothing
  }
  static constexpr std::array<std::string_view, 2> kOptions = {"save", "recover"};
  const int option = lookup(inv.args[0], kOptions);
  if (option < 0 || inv.args.size() > 2) {
    return fail(codes::kBadUsage, "Usage: crash save|recover [filename]");
  }
  const std::optional<std::string> file = inv.args.size() == 2 ? std::optional{inv.args[1]} : std::nullopt;
  return with_session(ctx, [&](IoSession& s) { return option == 0 ? crash_save(s, file) : crash_recover(s, file); });
}

// --- archive --------------------------------------------------------------------

CommandResult archive_write(IoSession& s, const std::string& file, bool include_read_only) {
  const std::string root = first_window_root(s.ed);
  const auto text =
      write_backup(s.io, [include_read_only](const db::CellDef& def) { return archive_includes(def, include_read_only); },
                   root);
  if (auto written = s.io.fs.write_atomic(file, text.value_or(render_backup_file({}, root))); !written) {
    return fail(codes::kWriteFailed, "Cannot write archive file " + file);
  }
  emit_note(s, "Created database archive file " + file);
  return CommandResult::success();
}

/// Q-CELL-34: the name as given first, then with `.mag`, through Path.
std::optional<std::string> read_archive_text(IoSession& s, const std::string& file) {
  LookupContext lookup_ctx{s.io.fs, s.io.paths, s.io.vars, s.io.sink};
  auto found = search_file(lookup_ctx, file, s.io.paths.path, nullptr);
  if (!found) {
    found = search_file(lookup_ctx, file + ".mag", s.io.paths.path, nullptr);
  }
  if (!found) {
    return std::nullopt;
  }
  auto data = s.io.fs.read(found->path);
  if (!data) {
    return std::nullopt;
  }
  auto text = maybe_decompress(std::move(data).value());
  return text ? std::optional{std::move(text).value()} : std::nullopt;
}

CommandResult archive_read(IoSession& s, const std::string& file, bool dereference) {
  const auto text = read_archive_text(s, file);
  if (!text) {
    return fail(codes::kBackupRead, "Cannot open archive file \"" + file + "\"");
  }
  BackupReadOptions options;
  options.kind = "archive";
  options.file_name = file;
  options.dereference = dereference;
  const auto root = read_backup(s.io, *text, options);
  if (!root) {
    return backup_read_failure(root.error());
  }
  show_root(s, root.value());
  return CommandResult::success();
}

CommandResult archive_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  static constexpr std::array<std::string_view, 4> kModes = {"write", "writeall", "read", "readref"};
  const int mode = inv.args.size() == 2 ? lookup(inv.args[0], kModes) : -1;
  if (mode < 0) {
    return fail(codes::kBadUsage, "Usage: archive write|writeall|read|readref filename");  // Q-CMDEDIT-34: stop
  }
  const std::string& file = inv.args[1];
  return with_session(ctx, [&](IoSession& s) {
    return mode < 2 ? archive_write(s, file, mode == 1) : archive_read(s, file, mode == 2);
  });
}

}  // namespace

bool register_crash_commands(cmd::CommandRegistry& registry) {
  const std::array<cmd::CommandSpec, 2> specs = {{
      {"crash", "crash save|recover [filename]",
       "Writes a crash backup of the modified cells, or recovers the newest abandoned one.", crash_cmd},
      {"archive", "archive write|writeall|read|readref filename",
       "Writes every cell to one archive file, or reads one back.", archive_cmd},
  }};
  bool ok = true;
  for (const cmd::CommandSpec& spec : specs) {
    ok = !registry.add(spec).has_value() && ok;
  }
  return ok;
}

}  // namespace hefesto::cell::detail
