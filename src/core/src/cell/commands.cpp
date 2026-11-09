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

#include <algorithm>
#include <array>
#include <optional>

#include "crash_commands.hpp"
#include "hefesto/cell/crash.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "io_session.hpp"

namespace hefesto::cell {

namespace {

using cmd::CommandResult;
using cmd::Value;
using detail::fail;
using detail::IoSession;
using detail::lookup;
using detail::show_in_window;
using detail::with_session;

CommandResult usage(std::string text) {
  return fail(codes::kBadUsage, std::move(text));
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

// Magic's "(UNNAMED)" cell: the root of a window that shows no cell. It is
// created when first needed and, as in Magic's batch runs, stays in the
// library after another cell is loaded over it (DBWloadWindow deletes it
// only while the display runs). (Magic ref: dbwind/DBWprocs.c:321-345)
diag::Outcome<db::CellDef*> unnamed_def(IoSession& s) {
  if (db::CellDef* def = s.ed.cells.find("(UNNAMED)")) {
    return def;
  }
  auto made = s.ed.cells.create("(UNNAMED)", s.ed.technology->plane_count());
  if (!made) {
    return made.error();
  }
  made.value()->flags().available = true;
  return made.value();
}

// A window that shows no cell shows "(UNNAMED)" (Magic's startup window).
diag::Status adopt_startup_root(IoSession& s) {
  const view::LayoutWindow* w = s.ed.windows.current();
  if (w == nullptr || w->root() != nullptr) {
    return diag::Done{};
  }
  auto def = unnamed_def(s);
  if (!def) {
    return def.error();
  }
  return diag::Done{};
}

CommandResult load_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const auto args = parse_load(inv.args);
  if (!args) {
    return usage("Usage: load [name [-force] [-dereference] [-silent] [-nowindow] [-fail]]");
  }
  return with_session(ctx, [&](IoSession& s) {
    if (auto adopted = adopt_startup_root(s); !adopted) {
      return CommandResult::failure(adopted.error());
    }
    auto loaded = args->name ? load_cell(s.io, *args->name, args->options) : unnamed_def(s);
    if (!loaded && loaded.error().code == codes::kConflictingLocation) {
      return CommandResult::success();  // DBWloadWindow printed the conflict and loads nothing
    }
    if (!loaded) {
      return CommandResult::failure(loaded.error());
    }
    show_in_window(s.ed, *loaded.value());
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
    if (auto renamed = s.ed.cells.rename_definition(def, base); !renamed) {
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
  CellSession& session = editor.value()->cell_session;
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

/// Applies the timer state to the host clock, if any.
void arm_timer(CellSession& s) {
  if (!s.timer) {
    return;
  }
  if (s.backup_timer == CellSession::BackupTimer::running) {
    s.timer->schedule(*s.backup_interval_ms);
  } else {
    s.timer->cancel();
  }
}

CommandResult crashbackups_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  CellSession& s = editor.value()->cell_session;
  static constexpr std::array<std::string_view, 5> kOptions = {"start", "resume", "stop", "cancel", "disable"};
  // tools.tcl matches the option exactly (a Tcl switch), not by prefix.
  const auto* match = inv.args.empty() ? kOptions.begin() : std::find(kOptions.begin(), kOptions.end(), inv.args[0]);
  const int option = match == kOptions.end() ? -1 : static_cast<int>(match - kOptions.begin());
  using Timer = CellSession::BackupTimer;
  const bool interval_set = s.backup_interval_ms && *s.backup_interval_ms > 0;
  switch (option) {
    case 0:
      s.backup_interval_ms = s.backup_interval_ms.value_or(600000);
      s.backup_timer = *s.backup_interval_ms > 0 ? Timer::running : Timer::stopped;
      break;
    case 1:
      s.backup_timer = interval_set ? Timer::running : s.backup_timer;
      break;
    case 2:
    case 3:
      s.backup_timer = Timer::stopped;
      break;
    case 4:
      s.backup_timer = Timer::disabled;
      s.backup_interval_ms.reset();
      break;
    default:
      return usage("Usage: crashbackups [start|resume|stop|cancel|disable]");
  }
  arm_timer(s);
  return CommandResult::success();
}

}  // namespace

bool register_cell_commands(cmd::CommandRegistry& registry) {
  const std::array<cmd::CommandSpec, 7> specs = {{
      {"load", "load [name] [-force] [-dereference] [-silent] [-nowindow] [-fail]",
       "Loads a cell (reading it through the search paths; subcells are read when needed) and makes it the edit cell.",
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
  bool ok = detail::register_crash_commands(registry);
  ok = detail::register_place_commands(registry) && ok;
  for (const cmd::CommandSpec& spec : specs) {
    ok = !registry.add(spec).has_value() && ok;
  }
  return ok;
}

}  // namespace hefesto::cell
