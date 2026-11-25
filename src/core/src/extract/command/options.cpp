// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The option parsers of `extract` (options.hpp; capability extract-commands
// "Command syntax and option matching", "Extraction options", "Option
// defaults", "Warnings", "Style, halo and step size", "Output path and file
// location", "Unique command and temporary unique option",
// "Extract all, cell, parents and showparents"; tasks.md 9.1-9.4).
//
// Q-EXTRACT-25 (replicate): `extract halo` and `extract stepsize` change
// the loaded style in place (a new ExtractSession::current with the value),
// so a grid rescale scales them as every style distance and a style
// selection or technology load restores the tech-file values.
//
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdE.c (CmdExtract) and
// extract/ExtTech.c (ExtPrintPath, ExtSetPath, ExtPrintStyle, ExtSetStyle),
// extract/ExtLength.c (ExtSetDriver, ExtSetReceiver, ExtLengthClear),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTCMD.

#include "extract/command/options.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <strings.h>
#include <vector>

#include "hefesto/cell/timestamps.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/command/commands.hpp"
#include "hefesto/extract/command/run.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::extract::detail {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

// cmdExtOption indices.
enum DoOption : std::size_t {
  kDoAdjust = 0,
  kDoAll = 1,
  kDoCapacitance = 2,
  kDoCorners = 3,
  kDoCoupling = 4,
  kDoExtresist = 5,
  kDoLength = 6,
  kDoLocal = 7,
  kDoLumped = 8,
  kDoLabelcheck = 9,
  kDoAliases = 10,
  kDoUnique = 11,
  kDoResistance = 12,
  kDoFringehalo = 13,
};

CommandResult ok() {
  return CommandResult::success();
}

// "where option is one of:" and the table, as CmdExtract prints its usages.
void print_table(const cu::Session& s, std::span<const std::string_view> table, std::string_view trailer = {}) {
  std::string text = "where option is one of:\n";
  for (const std::string_view entry : table) {
    text += "  " + std::string{entry} + "\n";
  }
  text += trailer;
  cu::print_block(s, std::move(text));
}

std::string_view on_off(bool on) {
  return on ? "do" : "no";
}

// `extract do` / `extract no` with no option.
void print_options(const cu::Session& s, const ExtractOptions& o) {
  char corners[64];
  std::snprintf(corners, sizeof corners, "%g", o.corner_factor);
  std::string t = "The following are the extractor option settings:\n";
  t += std::string{on_off(o.adjust)} + " adjust\n";
  t += std::string{on_off(o.capacitance)} + " capacitance\n";
  t += std::string{on_off(o.corners)} + " corners (scale factor " + corners + ")\n";
  t += std::string{on_off(o.coupling)} + " coupling\n";
  t += std::string{on_off(o.fringehalo)} + " fringehalo\n";
  t += std::string{on_off(o.length)} + " length\n";
  t += std::string{on_off(o.lumped)} + " lumped R\n";
  t += std::string{on_off(o.labelcheck)} + " label check\n";
  t += std::string{on_off(o.aliases)} + " aliases\n";
  t += std::string{on_off(o.unique)} + " unique\n";
  t += std::string{on_off(o.unique_notopports)} + " unique notopports\n";
  t += std::string{on_off(o.extresist)} + " resistance (extresist)\n";
  cu::print_block(s, std::move(t));
}

// The ExtOptions bits of one option (EXT_DO*), as setters.
void set_flags(ExtractOptions& o, std::size_t option, bool on) {
  switch (option) {
    case kDoAdjust: o.adjust = on; break;
    case kDoCapacitance: o.capacitance = on; break;
    case kDoCorners: o.corners = on; break;
    case kDoCoupling: o.coupling = on; break;
    case kDoLength: o.length = on; break;
    case kDoLumped: o.lumped = on; break;
    case kDoLabelcheck: o.labelcheck = on; break;
    case kDoAliases: o.aliases = on; break;
    case kDoExtresist:
    case kDoResistance: o.extresist = on; break;
    case kDoFringehalo: o.fringehalo = on; break;
    default: break;
  }
}

// EXT_DOALL: adjust, capacitance, coupling, lumped R, length, unique.
void set_all(ExtractOptions& o, bool on) {
  o.adjust = o.capacitance = o.coupling = o.lumped = o.length = o.unique = on;
}

// `extract do unique X` with X not `notop...`: Magic prints the usage and
// then applies the table index 11 (0xb) as a bit mask, which sets or clears
// adjust, capacitance and lumped R (CmdE.c:1451-1461). magic-compat ON
// replicates it; OFF changes nothing (proposed Q-EXTRACT-38).
void unique_option(const cu::Session& s, ExtractOptions& o, const std::vector<std::string>& args, bool on) {
  if (args.size() != 3) {
    o.unique = on;
    return;
  }
  if (args[2].starts_with("notop")) {
    o.unique = o.unique_notopports = on;
    return;
  }
  cu::warn(s, "Usage:  extract do unique [notopports]");
  if (s.magic_compat) {
    o.adjust = o.capacitance = o.lumped = on;
  }
}

CommandResult corners_option(const cu::Session& s, ExtractOptions& o, const std::vector<std::string>& args, bool on) {
  if (args.size() == 3) {
    double k = 0.0;
    if (std::sscanf(args[2].c_str(), "%lf", &k) != 1) {
      cu::warn(s, "Usage:  extract do corners [value]");
      return ok();
    }
    o.corner_factor = k;
  }
  o.corners = on;
  return ok();
}

// EXTDO / EXTNO.
CommandResult do_no(const cu::Session& s, const OptionArgs& a, bool on) {
  ExtractOptions& o = s.editor->extract.options;
  if (a.args.size() < 2) {
    print_options(s, o);
    return ok();
  }
  const cmd::LookupResult found = cmd::lookup(a.args[1], extract_do_options());
  if (found.match != cmd::Match::found) {
    cu::warn(s, "Usage: extract do option");
    cu::warn(s, "   or  extract no option");
    print_table(s, extract_do_options());
    return ok();
  }
  switch (found.index) {
    case kDoAll: set_all(o, on); break;
    case kDoCorners: return corners_option(s, o, a.args, on);
    case kDoUnique: unique_option(s, o, a.args, on); break;
    case kDoLocal: o.path = on ? std::optional<std::string>{"."} : std::nullopt; break;
    default: set_flags(o, found.index, on); break;
  }
  return ok();
}

// EXTWARN.
CommandResult warn(const cu::Session& s, const OptionArgs& a) {
  ExtractOptions& o = s.editor->extract.options;
  if (a.args.size() < 2) {
    cu::print_block(s, "The following extractor warnings are enabled:\n    " + std::string{on_off(o.warn_dup)} +
                           " dup\n    " + std::string{on_off(o.warn_fets)} + " fets\n    " +
                           std::string{on_off(o.warn_labels)} + " labels\n");
    return ok();
  }
  const bool no = a.args.size() > 2 && a.args[1] == "no";
  const cmd::LookupResult found = cmd::lookup(no ? a.args[2] : a.args[1], extract_warn_options());
  if (found.match != cmd::Match::found) {
    cu::warn(s, "Usage: extract warnings [no] option");
    print_table(s, extract_warn_options());
    return ok();
  }
  const bool on = !no;
  const std::size_t w = found.index;  // all, dup, fets, labels
  if (w == 0 || w == 1) {
    o.warn_dup = on;
  }
  if (w == 0 || w == 2) {
    o.warn_fets = on;
  }
  if (w == 0 || w == 3) {
    o.warn_labels = on;
  }
  return ok();
}

void add_unique(std::vector<std::string>& list, const std::vector<std::string>& args) {
  for (std::size_t i = 2; i < args.size(); ++i) {
    if (std::find(list.begin(), list.end(), args[i]) == list.end()) {
      list.push_back(args[i]);  // ExtSetDriver / ExtSetReceiver: one hash entry per name
    }
  }
}

// EXTLENGTH.
CommandResult length(const cu::Session& s, const OptionArgs& a) {
  LengthLists& lists = s.editor->extract.length;
  const cmd::LookupResult found =
      a.args.size() < 2 ? cmd::LookupResult{} : cmd::lookup(a.args[1], extract_length_options());
  if (found.match != cmd::Match::found) {
    cu::warn(s, "Usage: extract length option [args]");
    print_table(s, extract_length_options());
    return ok();
  }
  if (found.index == 0) {
    lists = LengthLists{};
    return ok();
  }
  if (a.args.size() < 3) {
    cu::warn(s, "Must specify one or more terminal names");
    return ok();
  }
  add_unique(found.index == 1 ? lists.drivers : lists.receivers, a.args);
  return ok();
}

// EXTPATH: ExtPrintPath / ExtSetPath.
CommandResult path(const cu::Session& s, const OptionArgs& a) {
  std::optional<std::string>& p = s.editor->extract.options.path;
  if (a.args.size() < 2) {
    if (a.dolist) {
      return CommandResult::success(p ? *p : std::string{"(none)"});
    }
    cu::print(s, p ? "The extraction path is: " + *p : std::string{"(none)"});
    return ok();
  }
  const std::string& v = a.args[1];
  const bool none = strcasecmp(v.c_str(), "none") == 0 || strcasecmp(v.c_str(), "(none)") == 0 ||
                    strcasecmp(v.c_str(), "null") == 0;
  p = none ? std::nullopt : std::optional<std::string>{v};
  return ok();
}

std::string style_names(const ExtractSession& session, std::string_view sep) {
  std::string out;
  for (const std::string& n : session.styles.names()) {
    out += (out.empty() ? "" : std::string{sep}) + n;
  }
  return out;
}

// ExtPrintStyle(dolist, doforall, docurrent).
CommandResult print_style(const cu::Session& s, bool dolist, bool doforall, bool docurrent) {
  const ExtractSession& session = s.editor->extract;
  cmd::Value result;
  if (docurrent && session.current == nullptr) {
    cu::warn(s, "Error: No style is set");
  } else if (docurrent && dolist) {
    result = session.current->name;
  } else if (docurrent) {
    cu::print(s, "The current style is \"" + session.current->name + "\".");
  }
  if (doforall && dolist) {
    cmd::Value::List names;
    for (const std::string& n : session.styles.names()) {
      names.emplace_back(n);
    }
    result = std::move(names);
  } else if (doforall) {
    cu::print(s, "The extraction styles are: " + style_names(session, ", ") + ".");
  }
  return CommandResult::success(std::move(result));
}

// ExtLoadStyle: the named style, micron-converted and scaled to the grid
// (as the tech-load hook installs the first one); the overrides are gone.
void load_style(cmd::CommandContext& ctx, const cu::Session& s, const std::string& name) {
  editor::EditorState& editor = *s.editor;
  ExtractSession& session = editor.extract;
  const std::shared_ptr<const ExtStyle> parsed = session.styles.select(name, ctx.sink());
  if (parsed == nullptr) {
    return;
  }
  ExtStyle style = parsed->micron_convert(editor.mask.output_styles.current_style(), ctx.sink());
  if (editor.grid.internal != editor.grid.lambda) {
    style = style.rescale(static_cast<std::int32_t>(editor.grid.lambda), static_cast<std::int32_t>(editor.grid.internal),
                          editor.drc.compat_settings());
  }
  session.current = std::make_shared<const ExtStyle>(std::move(style));
  session.overrides = Overrides{};
}

// ExtSetStyle: the unique style NAME is a prefix of (case-sensitive).
CommandResult set_style(cmd::CommandContext& ctx, const cu::Session& s, const std::string& name) {
  std::optional<std::string> match;
  for (const std::string& n : s.editor->extract.styles.names()) {
    if (n.compare(0, name.size(), name) != 0) {
      continue;
    }
    if (match) {
      cu::warn(s, "Extraction style \"" + name + "\" is ambiguous.");
      return print_style(s, false, true, true);
    }
    match = n;
  }
  if (!match) {
    cu::warn(s, "\"" + name + "\" is not one of the extraction styles Magic knows.");
    return print_style(s, false, true, true);
  }
  load_style(ctx, s, *match);
  cu::print(s, "Extraction style is now \"" + name + "\"");
  return ok();
}

// EXTSTYLE.
CommandResult style(cmd::CommandContext& ctx, const cu::Session& s, const OptionArgs& a) {
  if (a.args.size() < 2) {
    return print_style(s, a.dolist, a.doforall, !a.doforall);
  }
  return set_style(ctx, s, a.args[1]);
}

// EXTHALO / EXTSTEPSIZE: print, or set in place (Q-EXTRACT-25).
CommandResult halo_or_step(const cu::Session& s, const OptionArgs& a, bool halo) {
  ExtractSession& session = s.editor->extract;
  if (session.current == nullptr) {
    cu::warn(s, "No extraction style set.");
    return ok();
  }
  if (a.args.size() == 1) {
    const std::int32_t v = halo ? session.current->side_halo : session.current->step_size;
    return CommandResult::success(cu::report_value(s, v, cmd::Axis::x));
  }
  if (a.args.size() != 2) {
    return wrong_args(s);
  }
  auto dist = cu::parse_distance(s, a.args[1], cmd::Axis::x, true);
  if (!dist.ok()) {
    return cu::fail(dist.error());
  }
  const db::Coord d = dist.value();
  if (halo ? d < 0 : d <= 0) {
    cu::warn(s, halo ? "Bad halo distance.  Halo must be non-negative."
                     : "Bad step size.  Step size must be strictly positive.");
    return ok();
  }
  ExtStyle changed = *session.current;
  (halo ? changed.side_halo : changed.step_size) = static_cast<std::int32_t>(d);
  (halo ? changed.side_halo_exact : changed.step_exact) = static_cast<double>(d);
  session.current = std::make_shared<const ExtStyle>(std::move(changed));
  (halo ? session.overrides.halo : session.overrides.step_size) = static_cast<std::int32_t>(d);
  return ok();
}

// The run's root cell (windCheckOnlyWindow, else the box tool's cell).
db::CellDef* root_cell(const cu::Session& s) {
  if (const view::LayoutWindow* w = s.editor->windows.current(); w != nullptr) {
    return w->root();
  }
  if (s.editor->box && s.editor->box_root != nullptr) {
    return s.editor->box_root;
  }
  return s.editor->edit_root != nullptr ? s.editor->edit_root : s.editor->edit_cell;
}

// CmdGetSelectedCell: the definition of the first selected instance.
db::CellDef* selected_cell(const cu::Session& s) {
  const db::CellDef* sel = s.editor->selection.def();
  if (sel == nullptr) {
    return nullptr;
  }
  for (db::CellUse* use : db::uses_in_search_order(*sel, compat::CompatSettings{s.magic_compat, true})) {
    if (use->def != nullptr) {
      return use->def;
    }
  }
  return nullptr;
}

RunRequest request_for(cmd::CommandContext& ctx, const cu::Session& s, RunMode mode, const db::CellDef* root) {
  RunRequest r;
  r.mode = mode;
  r.root = root != nullptr ? root->name() : std::string{};
  r.sink = &ctx.sink();
  r.files = &ctx.services().files;
  r.loader = gds::command_subtree_loader(ctx, *s.editor);
  r.now = cell::stamp_from_time(ctx.services().clock.wall_now());
  editor::EditorState* editor = s.editor;
  r.variable = [editor](std::string_view name) -> std::optional<std::string> {
    if (editor->script.interp == nullptr) {
      return std::nullopt;
    }
    const std::optional<cmd::Value> v = editor->script.interp->get_var(name);
    return v ? std::optional<std::string>{v->to_string()} : std::nullopt;
  };
  return r;
}

// EXTALL and bare `extract` (ExtAll, ExtIncremental).
CommandResult run_tree(cmd::CommandContext& ctx, const cu::Session& s, RunMode mode) {
  return run(*s.editor, request_for(ctx, s, mode, root_cell(s)), nullptr);
}

// EXTCELL, EXTPARENTS, EXTSHOWPARENTS: the selected cell.
CommandResult run_selected(cmd::CommandContext& ctx, const cu::Session& s, const OptionArgs& a, RunMode mode) {
  if (mode == RunMode::cell && a.args.size() != 2) {
    return wrong_args(s);
  }
  db::CellDef* selected = selected_cell(s);
  if (selected == nullptr) {
    cu::warn(s, "No cell selected");
    return ok();
  }
  RunRequest r = request_for(ctx, s, mode, selected);
  if (mode == RunMode::cell) {
    r.output_name = a.args[1];
  }
  return run(*s.editor, r, nullptr);
}

// EXTUNIQUE.
CommandResult unique(cmd::CommandContext& ctx, const cu::Session& s, const OptionArgs& a) {
  std::size_t mode = 0;  // UNIQALL
  if (a.args.size() >= 2) {
    const cmd::LookupResult found = cmd::lookup(a.args[1], extract_unique_options());
    if (found.match != cmd::Match::found) {
      cu::warn(s, "Usage: extract unique [option]");
      print_table(s, extract_unique_options(), "No option is equivalent to option \"all\"\n");
      return ok();
    }
    mode = found.index;
  }
  return run_unique(*s.editor, request_for(ctx, s, RunMode::all, root_cell(s)), static_cast<UniqueMode>(mode));
}

}  // namespace

CommandResult wrong_args(const cu::Session& s) {
  cu::warn(s, "Wrong number of arguments in \"extract\" command.  Try \"extract help\" for help.");
  return CommandResult::success();
}

CommandResult run_option(cmd::CommandContext& ctx, const cu::Session& s, std::size_t option, const OptionArgs& a) {
  switch (option) {
    case kIncremental: return run_tree(ctx, s, RunMode::incremental);
    case kAll: return run_tree(ctx, s, RunMode::all);
    case kCell: return run_selected(ctx, s, a, RunMode::cell);
    case kParents: return run_selected(ctx, s, a, RunMode::parents);
    case kShowParents: return run_selected(ctx, s, a, RunMode::showparents);
    case kDo: return do_no(s, a, true);
    case kNo: return do_no(s, a, false);
    case kWarn: return warn(s, a);
    case kLength: return length(s, a);
    case kPath: return path(s, a);
    case kStyle: return style(ctx, s, a);
    case kHalo: return halo_or_step(s, a, true);
    case kStepSize: return halo_or_step(s, a, false);
    case kUnique: return unique(ctx, s, a);
    default: return ok();
  }
}

}  // namespace hefesto::extract::detail
