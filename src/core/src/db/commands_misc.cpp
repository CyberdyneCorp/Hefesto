// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The miscellaneous layout commands of capability cmd-misc-edit: `xor`,
// `random`, `shell`, the `straighten` surface and the informative wizard
// and build-specific commands. Ported from Magic VLSI 8.3.694 (fc5daa9e)
// commands/CmdTZ.c (CmdXor), commands/CmdRS.c (CmdRandom, CmdShell) and
// plow/PlowCmd.c (CmdStraighten), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md). `see`,
// `crosshair`, `findbox` and `feedback` are summarized by cmd-misc-edit and
// implemented by their owners (view). Owner: EDIT.

#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "db/command_util.hpp"
#include "db/edit_util.hpp"
#include "db/tech_masks.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/process_runner.hpp"

namespace hefesto::db {

namespace cu = cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;

namespace {

// --- xor ------------------------------------------------------------------------

struct XorRequest {
  Descend descend = Descend::all;
  bool labels = true;
  std::string dest;
};

// The option a word selects: CmdXor reads the fourth character of the
// first option for every option with magic-compat ON (Q-CMDEDIT-95).
void apply_xor_option(XorRequest& r, char which, const cu::Session& s) {
  switch (which) {
    case 'l':
      r.labels = false;
      break;
    case 's':
      r.descend = Descend::no_subcircuit;
      break;
    case 'v':
      r.descend = Descend::no_vendor;
      break;
    default:
      cu::warn(s, "options are: -nolabels, -nosubcircuits -novendor");
      break;
  }
}

diag::Outcome<XorRequest> parse_xor(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.empty()) {
    return cu::bad_argument("usage: xor [-<option>...] destcell");
  }
  XorRequest r;
  r.dest = args.back();
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (!args[i].starts_with("-no")) {
      return cu::bad_argument("usage: xor [-<option>...] destcell");
    }
    if (args[i].size() > 3) {
      const std::string& chooser = s.magic_compat ? args[0] : args[i];
      apply_xor_option(r, chooser.size() > 3 ? chooser[3] : '\0', s);
    }
  }
  return r;
}

// The XOR result row: painting `t` over `h` gives space when they are the
// same type, else `t`; painting space always gives space.
PaintRow xor_row(tech::TileType t) {
  PaintRow row{};
  for (std::size_t h = 0; h < row.size(); ++h) {
    row[h] = (t == tech::kSpace || h == t) ? tech::kSpace : t;
  }
  return row;
}

diag::Status xor_paint(const CellDef& flat, CellDef& dest) {
  for (std::size_t p = 1; p < flat.plane_count() && p < dest.plane_count(); ++p) {
    std::vector<TileView> tiles;
    flat.plane(static_cast<tech::PlaneId>(p)).enumerate(whole_plane(), all_but_space_and_drc(), [&](const TileView& t) {
      tiles.push_back(t);
      return true;
    });
    Plane& plane = dest.plane(static_cast<tech::PlaneId>(p));
    for (const TileView& t : tiles) {
      const auto st = t.split ? plane.paint_triangle(t.area, t.diagonal, t.right_side, xor_row(t.type))
                              : plane.paint(t.area, xor_row(t.type));
      if (!st.ok()) {
        return st;
      }
    }
  }
  return diag::Done{};
}

// The flattened copy's labels and kept instances, moved into `dest`.
void xor_copy_rest(CellDef& flat, CellDef& dest) {
  for (const Label& l : flat.labels()) {
    (void)put_label(dest, l);
  }
  std::vector<CellUse*> kept;
  for (const auto& u : flat.uses()) {
    kept.push_back(u.get());
  }
  for (CellUse* u : kept) {
    if (u->def != nullptr) {
      PlaceOptions po;
      po.transform = u->transform;
      po.array = u->array;
      (void)place(dest, *u->def, po);
    }
    (void)remove_use(*u);  // unlink from the child before the scratch library goes
  }
}

CommandResult cmd_xor(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  const cu::Session& sess = s.value();
  auto req = parse_xor(sess, inv.args);
  if (!req.ok()) {
    return cu::fail(req.error());
  }
  CellDef* dest = sess.editor->cells.find(req.value().dest);
  if (dest == nullptr) {
    return cu::fail(diag::make(codes::kCellNotFound, req.value().dest + " does not exist"));
  }
  const CellDef* source = sess.editor->edit_cell != nullptr ? sess.editor->edit_cell : edit::default_root(*sess.editor);
  if (source == nullptr) {
    return cu::fail(diag::make(codes::kNoEditCell, "No cell def being edited!"));
  }
  FlattenOptions fo;
  fo.descend = req.value().descend;
  fo.labels = req.value().labels ? FlattenLabels::all : FlattenLabels::none;
  fo.magic_compat = sess.magic_compat;
  fo.font_update = [&sess](Label& l) { cu::update_font_label(sess, l); };
  CellLibrary scratch;  // never recorded: xor is not undoable
  auto flat = flatten_to_new(scratch, *source, "(xor)", *sess.tech, fo);
  if (!flat.ok()) {
    return cu::fail(flat.error());
  }
  sess.editor->undo.suspend();
  const auto st = xor_paint(*flat.value(), *dest);
  xor_copy_rest(*flat.value(), *dest);
  sess.editor->undo.resume();
  if (!st.ok()) {
    return cu::fail(st.error());
  }
  mark_modified(*dest);
  recompute_bbox(*dest, sess.magic_compat);
  sess.editor->windows.area_changed(*dest, dest->bbox(), std::nullopt);
  return CommandResult::success();
}

// --- random -----------------------------------------------------------------------

// The generator (Magic uses the C library's random()); one per process, so
// a seed gives the same sequence within Hefesto.
std::mt19937& generator() {
  static std::mt19937 gen{1};
  return gen;
}

CommandResult cmd_random(CommandContext& ctx, const Invocation& inv) {
  const auto& a = inv.args;
  if (a.empty()) {
    return CommandResult::success(Value{static_cast<std::int64_t>(generator()() >> 1U)});
  }
  if (a.size() > 2 || a[0] != "seed" || (a.size() == 2 && !cu::str_is_int(a[1]))) {
    auto s = cu::session(ctx);
    if (s.ok()) {
      cu::print(s.value(), "usage: random [seed [<value>]]");
    }
    return CommandResult::success();
  }
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
  const std::int64_t seed = a.size() == 2 ? cu::c_atoi(a[1]) : now.count();
  generator().seed(static_cast<std::mt19937::result_type>(static_cast<std::uint64_t>(seed)));
  return CommandResult::success();
}

// --- shell --------------------------------------------------------------------------

CommandResult cmd_shell(CommandContext& ctx, const Invocation& inv) {
  platform::ProcessRunner& runner = ctx.services().processes;
  if (!runner.supported()) {
    return cu::fail(diag::make(codes::kNotAvailable, "shell: unavailable on this platform"));  // Q-CMDEDIT-96
  }
  if (inv.args.empty()) {
    return CommandResult::success();
  }
  platform::ProcessRequest req;
  req.program = "/bin/sh";
  req.args = {"-c", cu::join_args(inv.args, 0)};
  auto out = runner.run(req);
  if (!out.ok()) {
    return cu::fail(out.error());
  }
  std::string text = out.value().stdout_data + out.value().stderr_data;
  while (!text.empty() && text.back() == '\n') {
    text.pop_back();
  }
  if (!text.empty()) {
    ctx.emit(diag::make(codes::kReport, std::move(text)));
  }
  return CommandResult::success();
}

// --- straighten -----------------------------------------------------------------------

// The surface of CmdStraighten: argument and context checks; the plowing
// engine arrives with capability `plow` (M12).
CommandResult cmd_straighten(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return edit::usage("Usage: straighten manhattan-direction");
  }
  const auto dir = cmd::parse_direction(inv.args[0], true);
  if (!dir.ok()) {
    return cu::fail(dir.error());
  }
  auto t = edit::paint_target(ctx, "No cell def being edited!");
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  return cu::fail(diag::make(codes::kNotAvailable, "straighten: plowing is not available in this version"));
}

// --- wizard and build-specific commands ------------------------------------------------

// cmd-misc-edit "Wizard and debug commands": informative; Hefesto reports
// them unavailable without touching the layout.
CommandResult cmd_unavailable(CommandContext&, const Invocation& inv) {
  return cu::fail(diag::make(codes::kNotAvailable, inv.name + ": not available in Hefesto"));
}

struct Unavailable {
  std::string_view name;
  std::string_view synopsis;
  std::string_view help;
};

constexpr std::array<Unavailable, 9> kUnavailable = {{
    {"*extract", "*extract ?args?", "Extractor debugging (informative; not available)."},
    {"*plow", "*plow command ?args?", "Plow debugging (informative; not available)."},
    {"*psearch", "*psearch plane count", "Times point searches (informative; not available)."},
    {"*showmem", "*showmem ?file?", "Prints internal memory use (informative; not available)."},
    {"*showtech", "*showtech ?file?", "Dumps the internal technology tables (informative; not available)."},
    {"*tilestats", "*tilestats ?file?", "Prints tile-use statistics (informative; not available)."},
    {"*tsearch", "*tsearch plane count", "Times area searches (informative; not available)."},
    {"makesw", "makesw ?args?", "LLNL switch-level tool (not part of the Magic Tcl build; not available)."},
    {"sgraph", "sgraph ?args?", "LLNL graph tool (not part of the Magic Tcl build; not available)."},
}};

}  // namespace

namespace cmdutil {

bool register_misc_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "xor", "xor ?-nolabels? ?-nosubcircuits? ?-novendor? cell",
                "Paints the exclusive-or of the edit cell into another cell.", cmd_xor);
  ok = add(registry, "random", "random ?seed ?value??", "Returns a pseudo-random number or seeds the generator.",
           cmd_random) && ok;
  ok = add(registry, "shell", "shell command ?args?", "Runs a shell command.", cmd_shell) && ok;
  ok = add(registry, "straighten", "straighten direction", "Straightens jogs in the box (needs plowing).",
           cmd_straighten) && ok;
  for (const Unavailable& u : kUnavailable) {
    ok = add(registry, std::string{u.name}, std::string{u.synopsis}, std::string{u.help}, cmd_unavailable) && ok;
  }
  return ok;
}

}  // namespace cmdutil

}  // namespace hefesto::db
