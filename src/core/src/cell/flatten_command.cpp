// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `flatten` (cmd-hierarchy "Flatten command"): reads the source cell's
// unavailable descendants, then flattens into a new cell, the box area of
// a cell (`-dobox`) or in place (`-doinplace`, the named instance or the
// selected ones). Flattening into a cell is never recorded for undo;
// flattening in place is recorded only with magic-compat OFF (D-10,
// Q-CMDEDIT-31).
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdFI.c (CmdFlatten);
// Magic is (c) the Regents of the University of California
// (THIRD_PARTY_NOTICES.md).

#include <optional>
#include <string>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/hints/hints.hpp"
#include "io_session.hpp"
#include "placement.hpp"

namespace hefesto::cell::detail {

namespace {

using cmd::CommandResult;
namespace cu = db::cmdutil;

struct FlattenRequest {
  db::FlattenOptions options;
  bool in_place = false;
  bool dobox = false;
  bool dolabels = true;    // -nolabels / -dolabels
  bool toplabels = false;  // -dotoplabels / -notoplabels
  std::optional<std::string> name;
};

constexpr std::string_view kOptionList =
    "options are: -nolabels, -nosubcircuits, -noports, -novendor, -dotoplabels, -doproperty, -dobox, -doinplace";

// CmdFlatten's switch on the character after "-no"/"-do"; an unknown one
// prints the option list and is ignored.
void apply_option(FlattenRequest& r, std::string_view arg, bool invert, IoSession& s) {
  switch (arg[3]) {
    case 'b':
      r.dobox = !invert;
      break;
    case 'i':
      r.in_place = !invert;
      break;
    case 'l':
      r.dolabels = !invert;
      break;
    case 't':
      r.toplabels = !invert;
      break;
    case 'p':
      if (arg.substr(3, 4) == "prop") {
        r.options.descend = invert ? db::Descend::all : db::Descend::property_flatten;
      }  // otherwise -noports/-doports: parsed, without effect, as in Magic
      break;
    case 's':
      r.options.descend = invert ? db::Descend::no_subcircuit : db::Descend::all;
      break;
    case 'v':
      r.options.descend = invert ? db::Descend::no_vendor : db::Descend::all;
      break;
    default:
      s.emit(codes::kBadUsage, std::string{kOptionList});
      break;
  }
}

diag::Outcome<FlattenRequest> parse_flatten(const std::vector<std::string>& args, IoSession& s) {
  FlattenRequest r;
  r.options.magic_compat = s.io.magic_compat;
  for (const std::string& a : args) {
    if (a.size() <= 3 || a[0] != '-') {
      r.name = a;
      continue;
    }
    const std::string_view prefix = std::string_view{a}.substr(1, 2);
    if (prefix != "no" && prefix != "do") {
      return diag::make(codes::kBadUsage, "Bad flatten option \"" + a + "\";  must start with \"-no\" or \"-do\"");
    }
    apply_option(r, a, prefix == "no", s);
  }
  // CmdFlatten prefers the top labels over all labels; DBFlattenInPlace the
  // reverse (Magic ref: commands/CmdFI.c:2205-2217, database/DBcellcopy.c:624-631).
  const db::FlattenLabels top = r.toplabels ? db::FlattenLabels::top_only : db::FlattenLabels::none;
  const db::FlattenLabels all = r.dolabels ? db::FlattenLabels::all : db::FlattenLabels::none;
  r.options.labels = r.in_place ? (r.dolabels ? db::FlattenLabels::all : top)
                                : (r.toplabels ? db::FlattenLabels::top_only : all);
  return r;
}

/// Suspends undo recording for one scope (UndoDisable / UndoEnable).
class UndoSuspension {
 public:
  UndoSuspension(undo::UndoLog& log, bool active) : log_(active ? &log : nullptr) {
    if (log_ != nullptr) {
      log_->suspend();
    }
  }
  UndoSuspension(const UndoSuspension&) = delete;
  UndoSuspension& operator=(const UndoSuspension&) = delete;
  ~UndoSuspension() {
    if (log_ != nullptr) {
      log_->resume();
    }
  }

 private:
  undo::UndoLog* log_;
};

// --- in place ------------------------------------------------------------------

CommandResult flatten_one_in_place(IoSession& s, const FlattenRequest& r) {
  db::CellUse* use = db::find_use(*s.ed.edit_cell, *r.name);
  if (use == nullptr || use->id != *r.name) {
    return fail(db::codes::kUseNotFound, "No cell use " + *r.name + " found in edit cell.");
  }
  auto st = db::flatten_in_place(*use, *s.ed.technology, r.options);
  return st.ok() ? CommandResult::success() : CommandResult::failure(st.error());
}

// DBCellFlattenAllCells over the selection, then SelectDeleteUses: every
// selected instance of the edit cell is flattened in place.
CommandResult flatten_selected_in_place(IoSession& s, const FlattenRequest& r) {
  for (const InstanceRef& ref : selected_instances(s.ed)) {
    if (ref.use->parent != s.ed.edit_cell) {
      continue;
    }
    if (auto st = db::flatten_in_place(*ref.use, *s.ed.technology, r.options); !st.ok()) {
      return CommandResult::failure(st.error());
    }
  }
  s.ed.selection.clear();
  return CommandResult::success();
}

CommandResult flatten_in_place(IoSession& s, const FlattenRequest& r) {
  if (s.ed.edit_cell == nullptr) {
    return fail(db::codes::kNoEditCell, "The current cell is not editable.");
  }
  expand_all(s.io, *s.ed.edit_cell);
  const UndoSuspension quiet(s.ed.undo, s.io.magic_compat);
  CommandResult result = r.name ? flatten_one_in_place(s, r) : flatten_selected_in_place(s, r);
  db::recompute_bbox(*s.ed.edit_cell, s.io.magic_compat);
  return result;
}

// --- into a cell ------------------------------------------------------------------

db::CellDef* flatten_source(const IoSession& s) {
  if (s.ed.edit_cell != nullptr) {
    return s.ed.edit_cell;
  }
  const view::LayoutWindow* w = s.ed.windows.current();
  return w != nullptr ? w->root() : nullptr;
}

std::string scratch_name(const IoSession& s) {
  std::string name = "__flatten_box__";
  while (s.ed.cells.find(name) != nullptr) {
    name += "_";
  }
  return name;
}

// The box part of a flattened copy (`-dobox`): paint clipped to the box,
// the labels in it and the kept instances overlapping it, added to `dest`.
void copy_box_area(const db::CellDef& flat, const db::Rect& box, db::CellDef& dest, const tech::Technology& tech) {
  tech::TypeMask paint = tech::TypeMask::all();
  paint.set(tech::kSpace, false);
  (void)db::copy_paint(flat, box, db::kIdentity, dest, tech, paint);
  for (const db::Label& l : flat.labels()) {
    if (db::label_in_area(l.rect, box)) {
      (void)db::put_label(dest, l, false, 0);
    }
  }
  for (const auto& u : flat.uses()) {
    if (u->def != nullptr && db::overlap(u->bbox, box)) {
      db::PlaceOptions po;
      po.transform = u->transform;
      po.array = u->array;
      (void)db::place(dest, *u->def, po);
    }
  }
}

CommandResult flatten_box(IoSession& s, const db::CellDef& source, const FlattenRequest& r) {
  if (!s.ed.box) {
    return fail(db::codes::kNoBox, "Put the box in a window first.");
  }
  if (s.ed.box_root != nullptr && s.ed.box_root != &source) {
    return fail(db::codes::kNoBox, "The box is not in the edit cell!");
  }
  db::CellDef* dest = s.ed.cells.find(*r.name);
  if (dest == nullptr) {
    auto made = s.ed.cells.create(*r.name, s.ed.technology->plane_count());
    if (!made) {
      return CommandResult::failure(made.error());
    }
    dest = made.value();
    dest->flags().available = true;
  }
  auto flat = db::flatten_to_new(s.ed.cells, source, scratch_name(s), *s.ed.technology, r.options);
  if (!flat.ok()) {
    return CommandResult::failure(flat.error());
  }
  copy_box_area(*flat.value(), *s.ed.box, *dest, *s.ed.technology);
  // DBFlatCopyMaskHints over the box: the flattened copy holds the
  // descendants' hints (never the source's own); keep their part in the box.
  hints::copy_hints(*flat.value(), *dest, db::kIdentity, *s.ed.box);
  s.ed.drc.pending.remove(*flat.value());  // DBCellDeleteDef: DRCRemovePending
  (void)s.ed.cells.remove(*flat.value());
  db::recompute_bbox(*dest, s.io.magic_compat);
  return CommandResult::success();
}

CommandResult flatten_into_cell(IoSession& s, const FlattenRequest& r) {
  if (!r.name) {
    return fail(codes::kBadUsage, "usage: flatten [-<option>...] destcell");
  }
  db::CellDef* source = flatten_source(s);
  if (source == nullptr) {
    return fail(db::codes::kNoEditCell, "There is no cell to flatten.");
  }
  if (!r.dobox && s.ed.cells.find(*r.name) != nullptr) {
    return fail(db::codes::kCellExists, *r.name + " already exists");
  }
  expand_all(s.io, *source);  // DBTreeSrLabels/DBCellCopyAllPaint read as they descend
  const UndoSuspension quiet(s.ed.undo, true);
  if (r.dobox) {
    return flatten_box(s, *source, r);
  }
  auto made = db::flatten_to_new(s.ed.cells, *source, *r.name, *s.ed.technology, r.options);
  return made.ok() ? CommandResult::success() : CommandResult::failure(made.error());  // no result
}

CommandResult flatten_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return with_session(ctx, [&](IoSession& s) {
    auto req = parse_flatten(inv.args, s);
    if (!req.ok()) {
      return CommandResult::failure(req.error());
    }
    auto cs = cu::session(ctx);
    if (cs.ok()) {
      req.value().options.font_update = [sess = cs.value()](db::Label& l) { cu::update_font_label(sess, l); };
    }
    return req.value().in_place ? flatten_in_place(s, req.value()) : flatten_into_cell(s, req.value());
  });
}

}  // namespace

bool register_flatten_command(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"flatten", "flatten [-<option> ...] destcell | flatten -doinplace [instance]",
                                    "Flattens the edit cell into a new cell (-dobox: the box area, added to the cell; "
                                    "-doinplace: an instance, or the selected ones, in place).",
                                    flatten_cmd})
              .has_value();
}

}  // namespace hefesto::cell::detail
