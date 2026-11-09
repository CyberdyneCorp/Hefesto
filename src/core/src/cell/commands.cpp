// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CELL commands: load, save, writeall, path, addpath, locking,
// crashbackups (the forms of tests/golden/CELL/CASES.md). Follows Magic
// 8.3.694 dbwind/DBWprocs.c (DBWloadWindow), commands/CmdRS.c (CmdSave),
// commands/CmdSubrs.c (cmdSaveCell), commands/CmdTZ.c (CmdWriteall),
// commands/CmdLQ.c (CmdPath, CmdLocking), commands/CmdAB.c (CmdAddPath)
// and tcltk/tools.tcl (magic::crashbackups).

#include "hefesto/cell/commands.hpp"

#include <array>
#include <cctype>
#include <optional>
#include <span>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/io_safety.hpp"
#include "hefesto/cell/mag_io.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/tech/commands.hpp"

namespace hefesto::cell {

namespace {

using cmd::CommandResult;
using cmd::Value;

CommandResult fail(std::string_view code, std::string text) {
  return CommandResult::failure(diag::make(code, std::move(text)));
}

CommandResult usage(std::string text) {
  return fail(codes::kBadUsage, std::move(text));
}

/// Magic's Lookup(): case-insensitive unique prefix, exact match wins.
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

/// The per-session CELL state lives in the lock service (io_safety.hpp).
CellSession& cell_session(editor::EditorState& ed) {
  if (!ed.locks) {
    ed.locks = make_in_process_lock_service();
  }
  return ed.locks->session();
}

bool property_compat(const editor::EditorState& ed) {
  return ed.property_compat;
}

/// A CellIoContext bound to one command's editor session.
struct IoSession {
  IoSession(cmd::CommandContext& cmd_ctx, editor::EditorState& editor)
      : ed(editor),
        session(cell_session(editor)),
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
           property_compat(editor),
           &session.renamed_duplicates,
           {},
           {}} {
    io.rescale_database = [this](std::int64_t n, std::int64_t d) {
      (void)tech::scale_layout(ed, n, d);
      return true;
    };
  }
  IoSession(const IoSession&) = delete;
  IoSession& operator=(const IoSession&) = delete;

  /// The safe point after a command: pending timestamp mismatches.
  void process_mismatches() {
    const MismatchReport report = cell::process_mismatches(session.stamps, [this](db::CellDef& def) {
      ReadOptions opts;
      opts.ignore_tech = true;
      (void)read_cell(io, def, opts);
    });
    for (const std::string& line : report.messages()) {
      io.sink.emit(diag::make(codes::kStampMismatch, line));
    }
  }

  editor::EditorState& ed;
  CellSession& session;
  CellIoContext io;
};

template <typename Body>
CommandResult with_session(cmd::CommandContext& ctx, Body&& body) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  IoSession s{ctx, *editor.value()};
  CommandResult result = body(s);
  s.process_mismatches();
  return result;
}

// --- load -------------------------------------------------------------------

struct LoadArgs {
  std::optional<std::string> name;
  ReadOptions options;
};

std::optional<LoadArgs> parse_load(const std::vector<std::string>& args) {
  LoadArgs out;
  static constexpr std::array<std::string_view, 6> kOptions = {"-force", "-dereference", "-silent",
                                                               "-quiet",  "-nowindow",    "-fail"};
  for (const std::string& a : args) {
    if (a.empty() || a.front() != '-') {
      if (out.name) {
        return std::nullopt;
      }
      out.name = a;
      continue;
    }
    switch (lookup(a, kOptions)) {
      case 0:
        out.options.ignore_tech = true;
        break;
      case 1:
        out.options.dereference = true;
        break;
      case -1:
        return std::nullopt;
      default:
        break;  // verbosity and window options do not change the database
    }
  }
  return out;
}

void show_in_window(editor::EditorState& ed, db::CellDef& def) {
  if (view::LayoutWindow* w = ed.windows.current()) {
    w->set_root(&def);
  }
  ed.edit_cell = def.flags().no_edit ? nullptr : &def;
}

CommandResult load_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const auto args = parse_load(inv.args);
  if (!args) {
    return usage("Usage: load [name [-force] [-dereference] [-silent] [-nowindow] [-fail]]");
  }
  return with_session(ctx, [&](IoSession& s) {
    db::CellDef* def = nullptr;
    if (!args->name) {
      def = s.ed.cells.find("(UNNAMED)");
      if (def == nullptr) {
        auto made = s.ed.cells.create("(UNNAMED)", s.ed.technology->plane_count());
        if (!made) {
          return CommandResult::failure(made.error());
        }
        def = made.value();
        def->flags().available = true;
      }
    } else {
      auto loaded = load_cell(s.io, *args->name, args->options);
      if (!loaded) {
        return CommandResult::failure(loaded.error());
      }
      def = loaded.value();
    }
    show_in_window(s.ed, *def);
    return CommandResult::success();
  });
}

// --- save -------------------------------------------------------------------

std::string base_name(std::string_view name) {
  if (const std::size_t slash = name.rfind('/'); slash != std::string_view::npos) {
    name.remove_prefix(slash + 1);
  }
  if (name.ends_with(".mag")) {
    name.remove_suffix(4);
  }
  return std::string{name};
}

db::CellDef* save_target(IoSession& s) {
  if (s.ed.edit_cell != nullptr) {
    return s.ed.edit_cell;
  }
  const view::LayoutWindow* w = s.ed.windows.current();
  return w != nullptr ? w->root() : nullptr;
}

CommandResult save_as(IoSession& s, db::CellDef& def, const std::string& name) {
  const bool script = name.size() > 4 && name.ends_with(".tcl");
  if (!script) {
    def.flags().no_edit = false;  // "save as" writes a new file (Q-CELL-37)
  }
  if (auto saved = save_cell(s.io, def, name); !saved) {
    return CommandResult::failure(saved.error());
  }
  const std::string base = base_name(name);
  if (!script && base != def.name()) {
    if (auto renamed = s.ed.cells.rename(def, base); !renamed) {
      return CommandResult::failure(renamed.error());
    }
  }
  return CommandResult::success();
}

CommandResult save_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.size() > 1) {
    return usage("Usage: save [file]");
  }
  return with_session(ctx, [&](IoSession& s) {
    db::CellDef* def = save_target(s);
    if (def == nullptr) {
      return fail(codes::kNoCell, "There is no cell to save.");
    }
    if (inv.args.empty() && def->name() == "(UNNAMED)") {
      return fail(codes::kBadUsage, "Must specify name for cell (UNNAMED).");
    }
    s.process_mismatches();
    update_timestamps(s.ed.cells, def, stamp_from_time(s.io.clock.wall_now()));
    if (!inv.args.empty()) {
      return save_as(s, *def, inv.args[0]);
    }
    if (auto saved = save_cell(s.io, *def); !saved) {
      return CommandResult::failure(saved.error());
    }
    return CommandResult::success();
  });
}

// --- writeall ---------------------------------------------------------------

bool needs_write(const db::CellDef& def) {
  const db::CellFlags& f = def.flags();
  return !f.internal && f.available && (f.modified || f.child_bboxes_changed || f.child_timestamps_changed);
}

void write_one(IoSession& s, db::CellDef& def) {
  if (def.flags().no_edit) {
    if (def.flags().modified) {
      s.io.sink.emit(diag::make(codes::kWriteRefused,
                                "Cell '" + def.name() + "' is not writeable;  modified cell not written."));
    }
    return;
  }
  if (auto saved = save_cell(s.io, def); !saved) {
    s.io.sink.emit(saved.error());
  }
}

CommandResult writeall_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  std::vector<std::string> names = inv.args;
  if (!names.empty() && lookup(names.front(), std::array<std::string_view, 2>{"force", "noupdate"}) >= 0) {
    names.erase(names.begin());  // non-interactive: every write is forced
  }
  return with_session(ctx, [&](IoSession& s) {
    s.process_mismatches();
    std::vector<db::CellDef*> cells;
    for (const std::string& n : names) {
      db::CellDef* def = s.ed.cells.find(n);
      if (def == nullptr) {
        return fail(codes::kNoCell, "No cell \"" + n + "\" found.");
      }
      cells.push_back(def);
    }
    const std::int64_t now = stamp_from_time(s.io.clock.wall_now());
    if (cells.empty()) {
      update_timestamps(s.ed.cells, nullptr, now);
      s.ed.cells.for_each([&](db::CellDef& def) {
        if (needs_write(def)) {
          cells.push_back(&def);
        }
      });
    } else {
      for (db::CellDef* def : cells) {
        update_timestamps(s.ed.cells, def, now);
      }
    }
    for (db::CellDef* def : cells) {
      write_one(s, *def);
    }
    return CommandResult::success();
  });
}

// --- path, addpath --------------------------------------------------------------

void set_or_append(std::vector<std::string>& path, std::string_view text) {
  if (text.starts_with('+')) {
    text.remove_prefix(1);
    for (std::string& entry : split_search_path(text)) {
      path.push_back(std::move(entry));
    }
    return;
  }
  path = split_search_path(text);
}

CommandResult path_report(const SearchPaths& p) {
  return CommandResult::success(Value{Value::List{
      Value{"Search path for cells is \"" + join_search_path(p.path) + "\""},
      Value{"Cell library search path is \"" + join_search_path(p.library) + "\""},
      Value{"System search path is \"" + join_search_path(p.system) + "\""},
  }});
}

CommandResult path_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  SearchPaths& paths = editor.value()->search_paths;
  if (inv.args.size() > 2) {
    return usage("Usage: path [search|cell|sys] [[+]path]");
  }
  if (inv.args.empty()) {
    return path_report(paths);
  }
  static constexpr std::array<std::string_view, 4> kOptions = {"search", "cell", "sys", "help"};
  const int option = lookup(inv.args[0], kOptions);
  if (option == 3 || (option < 0 && inv.args.size() == 2)) {
    return usage("Usage: path [search|cell|sys] [[+]path]");
  }
  if (option < 0) {
    set_or_append(paths.path, inv.args[0]);
    return CommandResult::success();
  }
  std::vector<std::string>* target = option == 0 ? &paths.path : (option == 1 ? &paths.library : &paths.system);
  if (inv.args.size() == 1) {
    return CommandResult::success(Value{join_search_path(*target)});
  }
  set_or_append(*target, inv.args[1]);
  return CommandResult::success();
}

CommandResult addpath_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.size() != 1) {
    return usage("Usage: addpath appended_search_path");
  }
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  for (std::string& entry : split_search_path(inv.args[0])) {
    editor.value()->search_paths.path.push_back(std::move(entry));
  }
  return CommandResult::success();
}

// --- locking, crashbackups -----------------------------------------------------

CommandResult locking_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  CellSession& session = cell_session(*editor.value());
  if (inv.args.empty()) {
    return CommandResult::success(Value{session.locking ? "enabled" : "disabled"});
  }
  static constexpr std::array<std::string_view, 10> kYesNo = {"disable", "no",  "false", "off", "0",
                                                              "enable",  "yes", "true",  "on",  "1"};
  const int option = lookup(inv.args[0], kYesNo);
  if (option < 0) {
    return usage("Unknown locking option \"" + inv.args[0] + "\"");
  }
  session.locking = option > 4;
  return CommandResult::success();
}

CommandResult crashbackups_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  CellSession& s = cell_session(*editor.value());
  const std::string option = inv.args.empty() ? "start" : inv.args[0];
  using Timer = CellSession::BackupTimer;
  if (option == "start") {
    if (!s.backup_interval_ms) {
      s.backup_interval_ms = 600000;
    }
    s.backup_timer = *s.backup_interval_ms > 0 ? Timer::running : Timer::stopped;
  } else if (option == "resume") {
    if (s.backup_interval_ms && *s.backup_interval_ms > 0) {
      s.backup_timer = Timer::running;
    }
  } else if (option == "stop" || option == "cancel") {
    s.backup_timer = Timer::stopped;
  } else if (option == "disable") {
    s.backup_timer = Timer::disabled;
    s.backup_interval_ms.reset();
  } else {
    return usage("Usage: crashbackups [start|resume|stop|cancel|disable]");
  }
  return CommandResult::success();
}

}  // namespace

bool register_cell_commands(cmd::CommandRegistry& registry) {
  const std::array<cmd::CommandSpec, 7> specs = {{
      {"load", "load [name] [-force] [-dereference] [-silent] [-nowindow] [-fail]",
       "Loads a cell (reading it and its descendants through the search paths) and makes it the edit cell.",
       load_cmd},
      {"save", "save [file]", "Saves the edit cell, updating timestamps first; a .tcl name writes a command script.",
       save_cmd},
      {"writeall", "writeall [force] [cell ...]", "Saves every modified cell, or the named cells.", writeall_cmd},
      {"path", "path [search|cell|sys] [[+]path]", "Shows, sets or appends to a cell search path.", path_cmd},
      {"addpath", "addpath path", "Appends to the cell search path.", addpath_cmd},
      {"locking", "locking [enable|disable]", "Shows or sets advisory file locking.", locking_cmd},
      {"crashbackups", "crashbackups [start|resume|stop|cancel|disable]", "Controls periodic crash backups.",
       crashbackups_cmd},
  }};
  bool ok = true;
  for (const cmd::CommandSpec& spec : specs) {
    ok = !registry.add(spec).has_value() && ok;
  }
  return ok;
}

}  // namespace hefesto::cell
