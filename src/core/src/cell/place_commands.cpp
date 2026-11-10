// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `getcell`/`get` and `dump` (cmd-hierarchy "Placement reference syntax for
// getcell and dump", "Getcell command", "Dump command"). The cell placed or
// copied is read from its file when needed (`load` reads only the named
// cell), then placed through the edit frame.
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdCD.c
// (cmdDumpParseArgs, cmdDumpFunc, CmdDump), commands/CmdFI.c (CmdGetcell)
// and select/selOps.c (SelectDump); Magic is (c) the Regents of the
// University of California (THIRD_PARTY_NOTICES.md).

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "db/command_util.hpp"
#include "db/edit_util.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/select/selection.hpp"
#include "io_session.hpp"
#include "placement.hpp"

namespace hefesto::cell::detail {

namespace {

using cmd::CommandResult;
namespace cu = db::cmdutil;

struct CellName {
  std::string cell;  // the definition name (last path component, no .mag)
  std::string path;  // the name as given when it has a directory part
};

std::string strip_mag(std::string name) {
  if (name.size() > 4 && name.ends_with(".mag")) {
    name.resize(name.size() - 4);
  }
  return name;
}

CellName split_cell_name(const std::string& arg) {
  const std::size_t slash = arg.rfind('/');
  if (slash == std::string::npos) {
    return {strip_mag(arg), {}};
  }
  return {strip_mag(arg.substr(slash + 1)), strip_mag(arg)};
}

db::CellDef* create_def(IoSession& s, const std::string& name) {
  auto made = s.ed.cells.create(name, s.ed.technology->plane_count());
  return made ? made.value() : nullptr;
}

// The path-mismatch rule of cmdDumpParseArgs: a definition that already
// points elsewhere is not reused; a new one named <cell>_a, <cell>_b, ... is.
db::CellDef* definition_for(IoSession& s, const CellName& n, std::string_view command) {
  db::CellDef* def = s.ed.cells.find(n.cell);
  if (def == nullptr) {
    def = create_def(s, n.cell);
  }
  if (def == nullptr || n.path.empty()) {
    return def;
  }
  const std::string existing = def->file_path().generic_string();
  if (existing.empty()) {
    def->set_file_path(n.path);
    return def;
  }
  const auto same = s.io.host.same_file(existing + ".mag", n.path + ".mag");
  if (existing == n.path || (same && *same)) {
    return def;
  }
  s.emit(codes::kConflictingLocation, "Warning:  Cell file path mismatch.  Existing cell has path \"" + existing +
                                          "\", while " + std::string{command} + " path is \"" + n.path + "\".");
  std::string renamed;
  for (char c = 'a'; c <= 'z'; ++c) {
    renamed = n.cell + "_" + c;
    if (s.ed.cells.find(renamed) == nullptr) {
      break;
    }
  }
  s.emit(codes::kConflictingLocation, "Renaming cell to \"" + renamed + "\" to avoid conflict.");
  db::CellDef* made = create_def(s, renamed);
  if (made != nullptr) {
    made->set_file_path(n.path);
  }
  return made;
}

// DBIsAncestor(def, edit): `edit` is `def` or appears below it.
bool is_ancestor(const db::CellDef& def, const db::CellDef& edit) {
  if (&def == &edit) {
    return true;
  }
  bool found = false;
  db::walk_hierarchy(def, [&](const db::CellUse& use, const db::Transform&, int) {
    found = found || use.def == &edit;
    return !found;
  });
  return found;
}

/// The child cell of `getcell`/`dump`, read and checked; nullptr after an
/// error was reported in `error`.
db::CellDef* placed_cell(IoSession& s, const cmd::Invocation& inv, CommandResult& error) {
  if (s.ed.edit_cell == nullptr) {
    error = fail(db::codes::kNoEditCell, "No cell def being edited; cannot place cell use!");
    return nullptr;
  }
  const CellName name = split_cell_name(inv.args[0]);
  if (auto bad = illegal_chars(name.cell, "", "Cell name")) {
    error = fail(codes::kInvalidName, *bad);
    return nullptr;
  }
  db::CellDef* def = definition_for(s, name, inv.name == "dump" ? "dump" : "getcell");
  if (def == nullptr) {
    error = fail(codes::kNoCell, "Could not create cell \"" + inv.args[0] + "\"");
    return nullptr;
  }
  def->flags().not_found = false;
  ReadOptions read;
  read.ignore_tech = true;
  if (!read_cell(s.io, *def, read).ok() && !def->flags().available) {
    error = CommandResult::success();  // dbReadOpen printed why; Magic places nothing
    return nullptr;
  }
  db::recompute_bbox(*def, s.io.magic_compat);
  if (is_ancestor(*def, *s.ed.edit_cell)) {
    error = fail(codes::kBadUsage, "The edit cell is already a descendant of \"" + inv.args[0] +
                                       "\",\n    which means that you're trying to create a circular\n"
                                       "    structure.  This isn't legal.");
    return nullptr;
  }
  return def;
}

// Every type but space (the paint a dump copies).
tech::TypeMask all_but_space() {
  tech::TypeMask m = tech::TypeMask::all();
  m.set(tech::kSpace, false);
  return m;
}

// --- getcell ----------------------------------------------------------------------------

bool exact_copy_exists(const db::CellDef& parent, const db::CellDef& child, const db::Transform& t) {
  for (const auto& u : parent.uses()) {
    if (u->def == &child && u->transform == t && !u->array.is_array()) {
      return true;
    }
  }
  return false;
}

CommandResult getcell_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.empty()) {
    return fail(codes::kBadUsage, std::string{"Missing cell name in \""} + inv.name + "\" command.\n" +
                                      placement_usage(inv.name));
  }
  return with_session(ctx, [&](IoSession& s) {
    CommandResult error = CommandResult::success();
    db::CellDef* child = placed_cell(s, inv, error);
    if (child == nullptr) {
      return error;
    }
    auto cs = cu::session(ctx);
    if (!cs.ok()) {
      return CommandResult::failure(cs.error());
    }
    auto placement = parse_placement(cs.value(), *child, inv);
    if (!placement.ok()) {
      return CommandResult::failure(placement.error());
    }
    db::CellDef& edit = *s.ed.edit_cell;
    const db::Transform to_edit = db::compose(db::invert(s.ed.edit_transform), placement.value());
    if (exact_copy_exists(edit, *child, to_edit)) {
      return fail(codes::kBadUsage, "Can't place a cell on an exact copy of itself.");
    }
    db::PlaceOptions po;
    po.transform = to_edit;
    auto use = db::place(edit, *child, po);
    if (!use.ok()) {
      return CommandResult::failure(use.error());
    }
    db::recompute_bbox(edit, s.io.magic_compat);
    drc::check_this(s.ed, edit, use.value()->bbox);  // CmdGetcell, TT_CHECKSUBCELL
    cu::set_current_use(cs.value(), use.value());
    db::edit::set_box(s.ed, s.ed.edit_root, editor::to_root(s.ed, use.value()->bbox));  // DBWSetBox(EditRootDef)
    select::EditCopies selection(s.ed);  // SelectClear + SelectAndCopy2
    selection.add_use(*use.value());
    selection.commit();
    return CommandResult::success(cmd::Value{use.value()->id});
  });
}

// --- dump -------------------------------------------------------------------------------

// SelectDump + SelectAndCopy2: the child's own paint, labels and instances
// copied into the edit cell; the copies become the selection.
void dump_contents(IoSession& s, const cu::Session& cs, const db::CellDef& child, const db::Transform& to_edit) {
  db::CellDef& edit = *s.ed.edit_cell;
  select::EditCopies selection(s.ed);
  const db::Rect all = child.bbox();
  (void)db::copy_paint(child, all, to_edit, edit, *s.ed.technology, all_but_space());
  selection.add_paint(child, all, to_edit);
  for (const db::Label& l : child.labels()) {
    db::Label copy = l;
    copy.rect = db::apply(to_edit, l.rect);
    copy.justification = db::transform_position(to_edit, l.justification);
    copy.rotation = db::transform_angle(to_edit, l.rotation, s.io.magic_compat);
    auto put = db::put_label(edit, std::move(copy), false, s.ed.fonts.fonts.size());
    if (put.ok()) {
      cu::update_font_label(cs, *put.value());
      selection.add_label(*put.value());
    }
  }
  for (const auto& u : child.uses()) {
    if (u->def == nullptr) {
      continue;
    }
    db::PlaceOptions po;
    if (db::find_use(edit, u->id) == nullptr) {
      po.id = u->id;  // kept when free; otherwise a unique one is generated
    }
    po.transform = db::compose(to_edit, u->transform);
    po.array = u->array;
    po.locked = u->locked;
    if (auto copy = db::place(edit, *u->def, po); copy.ok()) {
      selection.add_use(*copy.value());
    }
  }
  db::mark_modified(edit);
  db::recompute_bbox(edit, s.io.magic_compat);
  drc::check_this(s.ed, edit, db::apply(to_edit, all));  // SelectDump -> SelectAndCopy2
  selection.commit();
}

CommandResult dump_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.empty()) {
    return fail(codes::kBadUsage, "Missing cell name in \"dump\" command.\n" + placement_usage("dump"));
  }
  return with_session(ctx, [&](IoSession& s) {
    CommandResult error = CommandResult::success();
    db::CellDef* child = placed_cell(s, inv, error);
    if (child == nullptr) {
      return error;
    }
    auto cs = cu::session(ctx);
    if (!cs.ok()) {
      return CommandResult::failure(cs.error());
    }
    auto placement = parse_placement(cs.value(), *child, inv);
    if (!placement.ok()) {
      return CommandResult::failure(placement.error());
    }
    dump_contents(s, cs.value(), *child, db::compose(db::invert(s.ed.edit_transform), placement.value()));
    return CommandResult::success();
  });
}

}  // namespace

bool register_place_commands(cmd::CommandRegistry& registry) {
  constexpr std::string_view kSynopsis =
      " name [child refpoint] [parent refpoint] [orientation [refpoint]]";
  bool ok = !registry
                 .add(cmd::CommandSpec{"getcell", "getcell" + std::string{kSynopsis},
                                       "Places an instance of a cell (read from its file when needed) at the box; "
                                       "the box and the selection then cover the new instance.",
                                       getcell_cmd})
                 .has_value();
  ok = !registry.add(cmd::CommandSpec{"get", "get" + std::string{kSynopsis}, "Synonym of getcell.", getcell_cmd})
            .has_value() &&
       ok;
  ok = !registry
            .add(cmd::CommandSpec{"dump", "dump" + std::string{kSynopsis},
                                  "Copies the contents of a cell into the edit cell; the copies become the selection.",
                                  dump_cmd})
            .has_value() &&
       ok;
  ok = register_flatten_command(registry) && ok;
  return ok;
}

}  // namespace hefesto::cell::detail
