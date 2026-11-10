// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CELL commands' session binding, including automatic technology
// discovery (cell-library-resolution "Automatic technology discovery").
// Follows Magic 8.3.694 database/DBio.c:223-320 and 597-750 (DBCellRead's
// technology switch); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).

#include "io_session.hpp"

#include <cctype>

#include "hefesto/cell/crash.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/tech/commands.hpp"

namespace hefesto::cell::detail {

int lookup(std::string_view word, std::span<const std::string_view> table) {
  int match = -1;
  int count = 0;
  for (std::size_t i = 0; i < table.size(); ++i) {
    const std::string_view entry = table[i];
    if (word.empty() || word.size() > entry.size()) {
      continue;
    }
    bool prefix = true;
    for (std::size_t k = 0; k < word.size() && prefix; ++k) {
      prefix = std::tolower(static_cast<unsigned char>(word[k])) == std::tolower(static_cast<unsigned char>(entry[k]));
    }
    if (prefix && word.size() == entry.size()) {
      return static_cast<int>(i);
    }
    if (prefix) {
      match = static_cast<int>(i);
      ++count;
    }
  }
  return count == 1 ? match : -1;
}

cmd::CommandResult fail(std::string_view code, std::string text) {
  return cmd::CommandResult::failure(diag::make(code, std::move(text)));
}

namespace {

/// The session's CELL state, with the in-process lock service installed
/// when the session has none (crash backups hold a session lock).
CellSession& prepared_session(editor::EditorState& editor) {
  if (!editor.locks) {
    editor.locks = make_in_process_lock_service();
  }
  return editor.cell_session;
}

}  // namespace

IoSession::IoSession(cmd::CommandContext& cmd_ctx, editor::EditorState& editor)
    : ed(editor),
      session(prepared_session(editor)),
      command(cmd_ctx),
      io{cmd_ctx.services().files,
         cmd_ctx.services().clock,
         editor.cells,
         editor.technology.get(),
         editor.grid,
         editor.search_paths,
         editor.path_variables,
         cmd_ctx.sink(),
         session.file_host(),
         session.stamps,
         session.locking ? editor.locks.get() : nullptr,
         &editor.fonts,
         cmd_ctx.project().compat.magic_compat,
         editor.property_compat,
         &session.renamed_duplicates,
         {},
         {},
         {}} {
  // Both callbacks live in `io`, a member: they never outlive this object.
  io.rescale_database = [this](std::int64_t n, std::int64_t d) {
    // dbReadOpen's finer-grid read: DRCTechScale(1, d) before DBScaleEverything(d, 1).
    drc::rescale_rules(ed, static_cast<std::int32_t>(d), static_cast<std::int32_t>(n));
    (void)tech::scale_layout(ed, n, d);
    return true;
  };
  io.load_technology = [this](std::string_view name) { return discover_technology(name); };
  io.on_read = [this](db::CellDef& def, bool format_error) {
    if (ed.drc.worker) {
      ed.drc.worker->bump_generation();  // a re-read invalidates a finished square
    }
    if (format_error) {
      drc::check_this(ed, def, std::nullopt);  // dbCellReadDef's badfile path
    }
  };
}

void IoSession::emit(std::string_view code, std::string text) const {
  io.sink.emit(diag::make(code, std::move(text)));
}

void IoSession::process_mismatches() {
  if (session.stamps.empty()) {
    return;
  }
  // DBFixMismatch prints its banner before it reads the cells it fixes
  // (Magic ref: database/DBtimestmp.c:120-195).
  emit(codes::kStampMismatch, "Processing timestamp mismatches.");
  const MismatchReport report = cell::process_mismatches(session.stamps, [this](db::CellDef& def) {
    ReadOptions opts;
    opts.ignore_tech = true;
    (void)read_cell(io, def, opts);
  });
  for (const RecheckArea& r : report.rechecks) {
    drc::check_this(ed, *r.parent, r.area);  // DBFixMismatch, TT_CHECKSUBCELL
  }
  const std::vector<std::string> lines = report.messages();
  for (std::size_t i = 1; i < lines.size(); ++i) {  // lines[0] is the banner
    emit(codes::kStampMismatch, lines[i]);
  }
}

// Step 3 (the `T.magicrc` startup script) waits for the Tcl binding (M4):
// the script is not run, so step 6 always applies, as in app startup
// (tasks.md 4.3 deviation). Step 7 has no `tech load` callbacks to run yet.
const tech::Technology* IoSession::discover_technology(std::string_view name) {
  const std::string tech_name{name};
  const auto dir = locate_technology_dir(io.fs, io.host, io.vars, tech_name);
  if (dir) {
    ed.search_paths.system.push_back(dir->generic_string());
  }
  emit(codes::kTechLoading, "Loading technology " + tech_name);
  const std::string target = dir ? (*dir / (tech_name + ".tech")).generic_string() : tech_name;
  tech::SessionLoadOptions options;
  options.magic_compat = io.magic_compat;
  options.precheck = false;
  if (auto loaded = tech::load_session_technology(ed, command.services(), target, options, io.sink); !loaded) {
    emit(codes::kTechLoadFailed, "Error in loading technology file");
    return nullptr;
  }
  if (dir) {
    discover_libraries(dir->generic_string());
  }
  return ed.technology.get();
}

void IoSession::discover_libraries(const std::string& tech_dir) {
  const auto root = libs_ref_root(tech_dir);
  if (!root) {
    return;
  }
  const auto found = discover_library_paths(io.fs, io.host, *root);
  if (found.empty()) {
    return;
  }
  auto& library = ed.search_paths.library;
  library.insert(library.end(), found.begin(), found.end());
  emit(codes::kLibraryPath, "Cell path is now \"" + join_search_path(library) + "\"");
}

bool read_for_expansion(cmd::CommandContext& ctx, db::CellDef& def) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return false;
  }
  IoSession s{ctx, *editor.value()};
  ReadOptions opts;
  opts.ignore_tech = true;
  const bool ok = read_cell(s.io, def, opts).ok() || def.flags().available;
  s.process_mismatches();
  return ok;
}

void show_in_window(editor::EditorState& ed, db::CellDef& def) {
  if (view::LayoutWindow* w = ed.windows.current()) {
    w->set_root(&def);
  }
  const bool editable = !def.flags().no_edit;
  set_edit_frame(ed, EditFrame{editable ? &def : nullptr, editable ? &def : nullptr, db::kIdentity});
}

}  // namespace hefesto::cell::detail
