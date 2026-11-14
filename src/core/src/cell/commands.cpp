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
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "crash_commands.hpp"
#include "db/command_util.hpp"
#include "hefesto/cell/crash.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/cmd/prompt.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/script/binding/output.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "io_session.hpp"

namespace hefesto::cell {

namespace detail {

namespace {

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

// A window that shows no cell shows "(UNNAMED)": Magic's DBWcreate loads it
// into every new layout window (DBWloadWindow with no name), so the window a
// front end opened before the first `load` (the desktop canvas, before the
// command-line cell) has it as its root and the load below deletes it while
// the display runs. Creating it without showing it left "(UNNAMED)" in the
// Cells manager next to the command-line cell. Not recorded.
// (Magic ref: dbwind/DBWprocs.c:131-154,321-345)
diag::Status adopt_startup_root(IoSession& s) {
  const view::LayoutWindow* w = s.ed.windows.current();
  if (w == nullptr || w->root() != nullptr) {
    return diag::Done{};
  }
  auto def = unnamed_def(s);
  if (!def) {
    return def.error();
  }
  const undo::ScopedSuspend quiet(s.ed.undo);
  show_in_window(s.ed, *def.value());
  return diag::Done{};
}

// DBWloadWindow's deleteDef: the current window's "(UNNAMED)" root, when
// the display runs (not NULL graphics, not `updatedisplay suspend`) and the
// cell was never touched; loading another cell deletes it. With NULL
// graphics Magic keeps it, so batch runs list it after a load.
// (Magic ref: dbwind/DBWprocs.c:321-331,608-617)
db::CellDef* disposable_unnamed(const IoSession& s) {
  if (!s.ed.display_running || s.ed.windows.display_suspended()) {
    return nullptr;
  }
  const view::LayoutWindow* w = s.ed.windows.current();
  db::CellDef* root = w != nullptr ? w->root() : nullptr;
  if (root == nullptr || root->name() != "(UNNAMED)") {
    return nullptr;
  }
  const db::CellFlags& f = root->flags();
  const bool touched = f.modified || f.child_bboxes_changed || f.child_timestamps_changed;
  return touched ? nullptr : root;
}

// DBCellDelete(UNNAMED, TRUE) after the load: renamed first so that another
// window showing it gets a fresh "(UNNAMED)" (WindUnload); not recorded.
// (Magic ref: database/DBcellname.c:228-341)
void delete_unnamed(IoSession& s, db::CellDef& doomed) {
  if (!s.ed.cells.check_removable(doomed).ok()) {
    return;
  }
  const undo::ScopedSuspend quiet(s.ed.undo);
  (void)s.ed.cells.rename(doomed, "__UNNAMED__");
  s.ed.windows.for_each([&](view::LayoutWindow& w) {
    if (w.root() == &doomed) {
      if (auto fresh = unnamed_def(s)) {
        w.set_root(fresh.value());
      }
    }
  });
  s.ed.drc.pending.remove(doomed);
  if (s.ed.box_root == &doomed) {
    s.ed.box_root = nullptr;
    s.ed.box.reset();
  }
  (void)s.ed.cells.remove(doomed);
}

// `load NAME -fail`: a cell neither in memory nor in a file is not created
// (DBW_LOAD_FAIL). A cell read here is then loaded as one in memory.
std::optional<cmd::CommandResult> refuse_missing(IoSession& s, const std::string& name, const ReadOptions& options) {
  const std::string root = load_definition_name(name);
  if (s.ed.cells.find(root) != nullptr) {
    return std::nullopt;
  }
  auto made = s.ed.cells.create(root, s.ed.technology->plane_count());
  if (!made) {
    return cmd::CommandResult::failure(made.error());
  }
  db::CellDef& def = *made.value();
  if (const std::string recorded = load_name(name); recorded != root) {
    def.set_file_path(recorded);  // as load_cell records it
  }
  // DBTestOpen: a missing file is only reported as "No file ... found or
  // readable.", without the reader's "couldn't be read" (CMDEDIT-HIER-04).
  diag::NullSink quiet;
  LookupContext lookup{s.io.fs, s.io.paths, s.io.vars, quiet};
  const bool findable =
      find_cell_file(lookup, def.name(), def.file_path().generic_string(), options.dereference).has_value();
  if (findable && (read_cell(s.io, def, options).ok() || def.flags().available)) {
    db::recompute_bbox(def, s.io.magic_compat);
    return std::nullopt;
  }
  s.ed.drc.pending.remove(def);  // a format error may have enqueued it (DRCRemovePending)
  (void)s.ed.cells.remove(def);
  if (!options.prints(ReadOptions::Verbosity::warnings)) {
    cmd::CommandResult unreported;  // -quiet / -silent: DBW_LOAD_QUIET prints nothing
    unreported.status = cmd::Status::failed;
    return unreported;
  }
  return fail(codes::kNotFound, "No file \"" + name + "\" found or readable.");
}

}  // namespace

cmd::CommandResult load_into_window(IoSession& s, const std::optional<std::string>& name, const ReadOptions& options,
                                    bool fail_if_missing) {
  if (auto adopted = adopt_startup_root(s); !adopted) {
    return cmd::CommandResult::failure(adopted.error());
  }
  if (name && fail_if_missing) {
    if (auto refused = refuse_missing(s, *name, options)) {
      return std::move(*refused);
    }
  }
  db::CellDef* doomed = disposable_unnamed(s);
  auto loaded = name ? load_cell(s.io, *name, options) : unnamed_def(s);
  if (!loaded && loaded.error().code == codes::kConflictingLocation) {
    return cmd::CommandResult::success();  // DBWloadWindow printed the conflict and loads nothing
  }
  if (!loaded) {
    return cmd::CommandResult::failure(loaded.error());
  }
  if (doomed == nullptr || doomed == loaded.value()) {
    show_in_window(s.ed, *loaded.value());
    return cmd::CommandResult::success();
  }
  {
    // With a cell to delete, Magic records no edit-cell change (DBWUndoOldEdit).
    const undo::ScopedSuspend quiet(s.ed.undo);
    show_in_window(s.ed, *loaded.value());
  }
  delete_unnamed(s, *doomed);
  return cmd::CommandResult::success();
}

}  // namespace detail

namespace {

namespace cu = db::cmdutil;
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

std::string base_name(std::string_view name) {
  if (const std::size_t slash = name.rfind('/'); slash != std::string_view::npos) {
    name.remove_prefix(slash + 1);
  }
  if (name.ends_with(".mag")) {
    name.remove_suffix(4);
  }
  return std::string{name};
}

struct LoadArgs {
  std::optional<std::string> name;
  ReadOptions options;
  bool fail = false;      // -fail: a missing file creates no cell
  bool nowindow = false;  // -nowindow: read into memory, no window changes
  tech::GridScale scale{1, 1};  // `scaled n [d]`: internal = n, lambda = d
  std::vector<std::string> unknown;  // options reported as "No such option"
  bool bad_scale = false;  // `scaled` with a wrong count: its own usage
};

constexpr std::string_view kLoadUsage =
    "Usage: load name [scaled n [d]] [-force] [-nowindow] [-dereference] [-quiet] [-silent] [-fail]";

void apply_load_option(LoadArgs& out, const std::string& word) {
  static constexpr std::array<std::string_view, 6> kOptions = {"-force", "-dereference", "-silent",
                                                               "-quiet",  "-nowindow",    "-fail"};
  switch (lookup(word, kOptions)) {
    case 0:
      out.options.ignore_tech = true;
      break;
    case 1:
      out.options.dereference = true;
      break;
    case 2:
      out.options.verbosity = ReadOptions::Verbosity::none;  // DB_VERBOSE_NONE
      break;
    case 3:
      out.options.verbosity = ReadOptions::Verbosity::errors;  // DB_VERBOSE_ERR
      break;
    case 4:
      out.nowindow = true;
      break;
    case 5:
      out.fail = true;
      break;
    default:
      out.unknown.push_back(word);  // CmdLoad: "No such option", then ignored
      break;
  }
}

/// CmdLoad's positional words after the name: `scale* n [d]`. Other extra
/// words are a usage error unless -force, -nowindow or -dereference is given
/// (Magic then ignores them).
bool apply_load_words(LoadArgs& out, const std::vector<std::string>& words) {
  if (words.size() <= 1) {
    return true;
  }
  if (words.size() >= 3 && words[1].starts_with("scale") && cu::str_is_int(words[2])) {
    if (words.size() > 4 || (words.size() == 4 && !cu::str_is_int(words[3]))) {
      out.bad_scale = true;
      return false;
    }
    out.scale.internal = cu::c_atoi(words[2]);
    out.scale.lambda = words.size() == 4 ? cu::c_atoi(words[3]) : 1;
    return true;
  }
  return out.options.ignore_tech || out.nowindow || out.options.dereference;
}

/// CmdLoad takes options from the end of the line only: `load -x foo`
/// names the cell `-x` and has no option.
std::optional<LoadArgs> parse_load(const std::vector<std::string>& args) {
  LoadArgs out;
  std::size_t end = args.size();
  for (; end > 0 && !args[end - 1].empty() && args[end - 1].front() == '-'; --end) {
    apply_load_option(out, args[end - 1]);
  }
  const std::vector<std::string> words(args.begin(), args.begin() + static_cast<std::ptrdiff_t>(end));
  if (!words.empty()) {
    out.name = words[0];
  }
  if (!apply_load_words(out, words)) {
    return out.bad_scale ? std::optional<LoadArgs>{out} : std::nullopt;
  }
  return out;
}

[[nodiscard]] bool scaled(const tech::GridScale& scale) noexcept {
  return scale.internal > 1 || scale.lambda > 1;
}

/// `load name scaled n [d]`: reads the cell and its whole hierarchy with
/// the read ratio multiplied by n/d, then divides it back (CmdLoad changes
/// DBLambda around DBWloadWindow and DBExpandAll).
CommandResult load_scaled(IoSession& s, const LoadArgs& args) {
  tech::GridScale& grid = s.io.grid;
  grid = tech::reduce(tech::GridScale{grid.internal * args.scale.internal, grid.lambda * args.scale.lambda});
  CommandResult result = args.nowindow ? CommandResult::success()
                                       : detail::load_into_window(s, args.name, args.options, args.fail);
  db::CellDef* top = s.ed.cells.find(base_name(*args.name));
  if (args.nowindow) {
    auto loaded = load_cell(s.io, *args.name, args.options);
    result = loaded ? CommandResult::success() : CommandResult::failure(loaded.error());
    top = loaded ? loaded.value() : nullptr;
  }
  if (top != nullptr && result.ok()) {
    s.emit(codes::kReadNote, "Recursively reading all cells at new scale.");
    expand_all(s.io, *top);
  }
  grid = tech::reduce(tech::GridScale{grid.internal * args.scale.lambda, grid.lambda * args.scale.internal});
  return result;
}

CommandResult load_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const auto args = parse_load(inv.args);
  if (!args) {
    return usage(std::string{kLoadUsage});
  }
  if (args->bad_scale) {
    return usage("Usage: load name scaled n [d] [-force] [-nowindow] [-dereference] [-quiet] [-silent] [-fail]");
  }
  if (args->name) {
    if (auto bad = illegal_cell_name(*args->name)) {
      return fail(codes::kInvalidName, *bad);
    }
  }
  return with_session(ctx, [&](IoSession& s) {
    for (const std::string& word : args->unknown) {
      s.emit(codes::kBadUsage, "No such option \"" + word + "\".");
    }
    if (args->name && scaled(args->scale)) {
      return load_scaled(s, *args);
    }
    if (args->nowindow && args->name) {
      auto loaded = load_cell(s.io, *args->name, args->options);
      return loaded ? CommandResult::success() : CommandResult::failure(loaded.error());
    }
    return detail::load_into_window(s, args->name, args->options, args->fail);
  });
}

// --- save -------------------------------------------------------------------


db::CellDef* save_target(IoSession& s) {
  if (s.ed.edit_cell != nullptr) {
    return s.ed.edit_cell;
  }
  const view::LayoutWindow* w = s.ed.windows.current();
  return w != nullptr ? w->root() : nullptr;
}

/// cmdCheckNewName's refusals of `name` for saving `def`: an existing file
/// the user does not overwrite, or a cell already named `name`.
bool name_refused(IoSession& s, const db::CellDef& def, const std::string& name) {
  // Only ".mag" is removed for the comparison; a directory stays part of
  // the name (cmdCheckNewName), so `save sub/foo` from `foo` still asks.
  const std::string base = name.size() > 4 && name.ends_with(".mag") ? name.substr(0, name.size() - 4) : name;
  if (base == def.name()) {
    return false;
  }
  const std::string file = name.ends_with(".mag") ? name : name + ".mag";
  if (s.io.fs.exists(file)) {
    // Magic stops only on `no`; an unaskable question goes ahead with
    // magic-compat ON (Q-SCRIPT-48).
    const std::string question = "File " + file + " already exists.\n  Overwrite it with " + def.name() + "? ";
    if (!cmd::confirm(s.ed.prompt, question, s.io.magic_compat)) {
      return true;
    }
  }
  if (s.ed.cells.find(base) != nullptr) {
    s.emit(codes::kBadUsage,
           "Can't rename cell '" + def.name() + "' to '" + base + "' because that cell already exists.");
    return true;
  }
  return false;
}

/// cmdCheckNewName's prompt for another file name, answered with one line
/// of input (TxGetLine); nullopt when there is none (no interpreter, end of
/// input or an empty line). Magic reads it with `sizeof returnname`, the
/// size of a pointer, so the answer keeps its first 7 characters; with
/// magic-compat OFF the whole line is the name (Q-CELL-47).
std::optional<std::string> prompt_file_name(IoSession& s, const db::CellDef& def) {
  if (s.ed.script.interp == nullptr) {
    return std::nullopt;
  }
  constexpr std::size_t kMagicAnswerBuffer = sizeof(char*);
  return script::read_line(*s.ed.script.interp, s.ed.script.output,
                           "File for cell " + def.name() + ": [hit return to abort save] ",
                           s.io.magic_compat ? kMagicAnswerBuffer : 0);
}

/// cmdCheckNewName for `save NAME`: a refused name leads to the prompt for
/// another file name, asked again until one is accepted or the answer is
/// empty (the save is then abandoned). The name to write, or nullopt.
std::optional<std::string> check_new_name(IoSession& s, const db::CellDef& def, std::string name) {
  while (name_refused(s, def, name)) {
    std::optional<std::string> answer;
    while (!answer) {
      answer = prompt_file_name(s, def);
      if (!answer) {
        s.emit(codes::kSaveAbandoned, "Cell not saved.");
        return std::nullopt;
      }
      if (auto bad = illegal_cell_name(*answer)) {
        s.emit(codes::kInvalidName, *bad);
        answer.reset();
      }
    }
    name = std::move(*answer);
  }
  return name;
}

CommandResult save_as(IoSession& s, db::CellDef& def, const std::string& requested) {
  const bool script = requested.size() > 4 && requested.ends_with(".tcl");
  const std::optional<std::string> checked = script ? std::optional{requested} : check_new_name(s, def, requested);
  if (!checked) {
    return CommandResult::success();
  }
  const std::string& name = *checked;
  if (!script && !s.session.declined.contains(def)) {
    def.flags().no_edit = false;  // "save as" writes a new file (Q-CELL-37); a declined cell stays read-only
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
  if (!inv.args.empty()) {
    if (auto bad = illegal_cell_name(inv.args[0])) {
      return fail(codes::kInvalidName, *bad);
    }
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
    update_save_stamps(s.io, def, stamp_from_time(s.io.clock.wall_now()));
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
//
// CmdWriteall and cmdWriteallFunc (Magic ref: commands/CmdTZ.c:2200-2372):
// with no argument every changed cell is offered through the prompt host
// (write, flush, skip, abort, autowrite; headless hosts answer "write");
// `force`/`modified` write without asking; named cells are written exactly.

enum class WriteAction : std::uint8_t { write, flush, skip, abort, autowrite };

bool selected_by_flags(const db::CellDef& def, int option) {
  const db::CellFlags& f = def.flags();
  if (option < 0) {
    return f.modified || f.child_bboxes_changed || f.child_timestamps_changed;
  }
  return option == 0 || f.modified;  // force: every cell; modified: content changes only
}

/// Tracks whether the user chose `autowrite` during one command.
struct WriteallRun {
  IoSession& s;
  std::vector<std::string> names;  // the named cells (only these are written)
  bool option_only = false;        // `writeall force|modified`
  bool autowrite = false;
};

WriteAction ask_writeall(WriteallRun& run, const db::CellDef& def) {
  static const std::array<std::string, 5> kAnswers = {"write", "flush", "skip", "abort", "autowrite"};
  static constexpr std::array<std::string_view, 4> kExplain = {"", "(bboxes)", "(timestamps)", "(bboxes/timestamps)"};
  std::size_t explain = 0;
  if (!def.flags().modified) {
    explain = !def.flags().child_timestamps_changed ? 1 : (!def.flags().child_bboxes_changed ? 2 : 3);
  }
  const std::string question = def.name() + " " + std::string{kExplain[explain]} +
                               ": write, autowrite, flush, skip, or abort command? ";
  return static_cast<WriteAction>(cmd::ask(run.s.ed.prompt, question, kAnswers, 0));
}

WriteAction writeall_action(WriteallRun& run, const db::CellDef& def) {
  if (run.option_only || run.autowrite) {
    if (def.flags().no_edit) {
      if (def.flags().modified) {
        run.s.emit(codes::kWriteRefused,
                   "Cell '" + def.name() + "' is not writeable;  modified cell not written.");
      }
      return WriteAction::skip;
    }
    if (!def.flags().modified && def.name() == "(UNNAMED)") {
      return WriteAction::skip;
    }
    return WriteAction::autowrite;
  }
  if (!run.names.empty()) {
    return std::find(run.names.begin(), run.names.end(), def.name()) != run.names.end() ? WriteAction::write
                                                                                        : WriteAction::skip;
  }
  return ask_writeall(run, def);
}

// One cell; false when the command is aborted.
bool writeall_one(WriteallRun& run, db::CellDef& def) {
  switch (writeall_action(run, def)) {
    case WriteAction::autowrite:
      run.autowrite = true;
      run.s.emit(codes::kSaved, "Writing '" + def.name() + "'");
      [[fallthrough]];
    case WriteAction::write:
      if (auto saved = save_cell(run.s.io, def); !saved) {
        run.s.io.sink.emit(saved.error());
      }
      return true;
    case WriteAction::flush:
      (void)detail::flush_cell(run.s, def, false);
      return true;
    case WriteAction::abort:
      return false;
    default:
      return true;
  }
}

// The named cells' timestamps are updated; false when none of them exists
// ("No such cell" for each unknown one).
bool stamp_named_cells(WriteallRun& run, std::int64_t now) {
  std::vector<db::CellDef*> found;
  for (const std::string& name : run.names) {
    db::CellDef* def = run.s.ed.cells.find(name);
    if (def == nullptr) {
      run.s.emit(codes::kNoCell, "No such cell \"" + name + "\".");
      continue;
    }
    found.push_back(def);
  }
  // One scan cache for the whole list (stamping prints nothing, so the
  // messages keep their order).
  update_save_stamps(run.s.io, found, now);
  return !found.empty();
}

CommandResult writeall_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  int option = -1;
  if (!inv.args.empty()) {
    option = lookup(inv.args[0], std::array<std::string_view, 2>{"force", "modified"});
    if (option < 0) {
      return usage("Usage: writeall [force|modified [cellname ...]]");
    }
  }
  return with_session(ctx, [&](IoSession& s) {
    s.process_mismatches();
    WriteallRun run{s, {}, inv.args.size() == 1, false};
    if (inv.args.size() > 1) {
      run.names.assign(inv.args.begin() + 1, inv.args.end());
    }
    const std::int64_t now = stamp_from_time(s.io.clock.wall_now());
    if (run.names.empty()) {
      update_save_stamps(s.io, nullptr, now);
    } else if (!stamp_named_cells(run, now)) {
      return CommandResult::success();
    }
    std::vector<db::CellDef*> cells;
    s.ed.cells.for_each([&](db::CellDef& def) {
      if (!def.flags().internal && def.flags().available && selected_by_flags(def, option)) {
        cells.push_back(&def);
      }
    });
    for (db::CellDef* def : cells) {
      if (!writeall_one(run, *def)) {
        break;
      }
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

/// CmdPath with no argument: three TxPrintf lines and no result, in every
/// build (CMDEDIT-HIER-10).
CommandResult path_report(cmd::CommandContext& ctx, const SearchPaths& p) {
  ctx.emit(diag::make(::hefesto::db::codes::kReport, "Search path for cells is \"" + join_search_path(p.path) + "\"\n" +
                                              "Cell library search path is \"" + join_search_path(p.library) +
                                              "\"\nSystem search path is \"" + join_search_path(p.system) + "\""));
  return CommandResult::success();
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
    return path_report(ctx, paths);
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

diag::Outcome<db::CellDef*> load_def(cmd::CommandContext& ctx, std::string_view name) {
  diag::Outcome<db::CellDef*> out = diag::make(codes::kNoCell, "There is no editor session.");
  (void)with_session(ctx, [&](IoSession& s) {
    out = load_cell(s.io, name, ReadOptions{});
    if (out) {
      db::recompute_bbox(*out.value(), s.io.magic_compat);
    }
    return CommandResult::success();
  });
  return out;
}

CommandResult save_def_as(cmd::CommandContext& ctx, db::CellDef& def, std::string_view file) {
  return with_session(ctx, [&](IoSession& s) {
    const auto saved = save_cell(s.io, def, file);
    return saved.ok() ? CommandResult::success() : CommandResult::failure(saved.error());
  });
}

// CmdWarnWrite counts the non-internal cells with CDMODIFIED, CDBOXESCHANGED
// or CDSTAMPSCHANGED; DBWexit goes ahead on any answer but "no", so the Tcl
// build's -1 (no `tk_dialog`) quits (commands/CmdSubrs.c:1188-1217,
// dbwind/DBWprocs.c:653-656).
bool check_cells_written(editor::EditorState& editor, bool magic_compat) {
  int count = 0;
  editor.cells.for_each([&count](db::CellDef& def) {
    const db::CellFlags& f = def.flags();
    if (!f.internal && (f.modified || f.child_bboxes_changed || f.child_timestamps_changed)) {
      ++count;
    }
  });
  if (count == 0) {
    return true;
  }
  const bool one = count == 1;
  const std::string question = std::to_string(count) + " Magic cell" + (one ? " has" : "s have") +
                               " been modified.\n  Do you want to exit magic and lose " + (one ? "it" : "them") +
                               "? ";
  return cmd::confirm(editor.prompt, question, magic_compat);
}

bool register_cell_commands(cmd::CommandRegistry& registry) {
  const std::array<cmd::CommandSpec, 7> specs = {{
      {"load", "load [name [scaled n [d]]] [-force] [-dereference] [-quiet] [-silent] [-nowindow] [-fail]",
       "Loads a cell (reading it through the search paths; subcells are read when needed) and makes it the edit cell.",
       load_cmd},
      {"save", "save [file]", "Saves the edit cell, updating timestamps first; a .tcl name writes a command script.",
       save_cmd},
      {"writeall", "writeall [force|modified [cellname ...]]",
       "Saves every changed cell (asking for each), or without asking every cell, the modified ones or the named "
       "ones.",
       writeall_cmd},
      {"path", "path [search|cell|sys] [[+]path]", "Shows, sets or appends to a cell search path.", path_cmd},
      {"addpath", "addpath path", "Appends to the cell search path.", addpath_cmd},
      {"locking", "locking [enable|disable]", "Shows or sets advisory file locking.", locking_cmd},
      {"crashbackups", "crashbackups [start|resume|stop|cancel|disable]", "Controls periodic crash backups.",
       crashbackups_cmd},
  }};
  bool ok = detail::register_crash_commands(registry);
  ok = detail::register_place_commands(registry) && ok;
  ok = detail::register_edit_commands(registry) && ok;
  ok = detail::register_flush_command(registry) && ok;
  ok = detail::register_find_commands(registry) && ok;
  ok = detail::register_element_command(registry) && ok;
  for (const cmd::CommandSpec& spec : specs) {
    ok = !registry.add(spec).has_value() && ok;
  }
  return ok;
}

}  // namespace hefesto::cell
