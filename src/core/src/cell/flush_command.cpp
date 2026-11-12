// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `flush` (cmd-hierarchy "Flush command"): discards a cell's in-memory
// changes by clearing it and reading it again from its file, after asking
// through the front end's cmd::PromptHost when the cell has changes.
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdFI.c (CmdFlush) and
// commands/CmdSubrs.c (cmdFlushCell); Magic is (c) the Regents of the
// University of California (THIRD_PARTY_NOTICES.md).

#include <array>
#include <string>
#include <vector>

#include "hefesto/cell/commands.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cmd/prompt.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "io_session.hpp"

namespace hefesto::cell::detail {

namespace {

using cmd::CommandResult;

/// DBCellClearDef: paint, labels, instances, properties and elements go.
void clear_definition(db::CellDef& def) {
  const std::size_t planes = def.plane_count();
  def.set_plane_count(0);
  def.set_plane_count(planes);
  def.labels().clear();
  def.elements().clear();
  db::clear_properties(def);
  while (!def.uses().empty()) {
    (void)db::remove_use(*def.uses().back());
  }
  def.clear_use_ids();  // "Reduce clutter by reinitializing the id hash table"
}

/// cmdFlushCell: every parent rechecks the use's area (TT_CHECKSUBCELL).
void mark_parents(editor::EditorState& editor, const db::CellDef& def) {
  const std::vector<db::CellUse*> uses = def.instances();
  for (const db::CellUse* u : uses) {
    if (u->parent != nullptr) {
      drc::check_this(editor, *u->parent, u->bbox);
    }
  }
}

bool has_changes(const db::CellDef& def) {
  const db::CellFlags& f = def.flags();
  return f.modified || f.child_timestamps_changed || f.child_bboxes_changed;
}

}  // namespace

bool flush_cell(IoSession& s, db::CellDef& def, bool dereference) {
  if (s.ed.edit_cell != nullptr && s.ed.edit_cell != s.ed.edit_root && s.ed.edit_cell != &def) {
    for (const db::CellUse* use : s.ed.edit_cell->instances()) {
      if (use->parent == &def) {
        s.emit(codes::kBadUsage, "Cannot flush cell whose subcell is being edited.\n" + def.name() + " not flushed");
        return false;
      }
    }
  }
  s.ed.undo.clear();  // UndoFlush
  if (dereference) {
    def.flags().dereference = true;
    def.set_file_path({});
  }
  s.ed.windows.area_changed(def, def.bbox(), std::nullopt);
  mark_parents(s.ed, def);
  clear_definition(def);
  def.flags().available = false;
  ReadOptions read;
  read.ignore_tech = true;
  (void)read_cell(s.io, def, read);
  def.flags().available = true;
  db::recompute_bbox(def, s.io.magic_compat);
  def.flags().modified = false;
  def.flags().needs_new_timestamp = false;
  s.ed.windows.area_changed(def, def.bbox(), std::nullopt);
  mark_parents(s.ed, def);
  return true;
}

namespace {

struct FlushArgs {
  std::vector<std::string> words;
  bool dereference = false;
  bool noprompt = false;
};

FlushArgs parse_flush(const std::vector<std::string>& args) {
  FlushArgs a{args, false, false};
  if (!a.words.empty() && a.words.back().starts_with("-deref")) {
    a.dereference = true;
    a.words.pop_back();
  }
  if (!a.words.empty() && a.words.back() == "-noprompt") {
    a.noprompt = true;
    a.words.pop_back();
  }
  return a;
}

db::CellDef* flush_target(const IoSession& s, const FlushArgs& a) {
  if (!a.words.empty()) {
    return s.ed.cells.find(a.words[0]);
  }
  if (s.ed.edit_cell != nullptr) {
    return s.ed.edit_cell;
  }
  const view::LayoutWindow* w = s.ed.windows.current();
  return w != nullptr ? w->root() : nullptr;
}

CommandResult flush_cmd(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const FlushArgs a = parse_flush(inv.args);
  if (a.words.size() > 1) {
    return fail(codes::kBadUsage, "Usage: flush [cellname] [-noprompt] [-dereference]");
  }
  return with_session(ctx, [&](IoSession& s) {
    db::CellDef* def = flush_target(s, a);
    if (def == nullptr) {
      return CommandResult::success();  // an unknown cell does nothing
    }
    const bool changed = has_changes(*def);
    if (!a.noprompt && changed) {
      // Magic stops only on `no`; an unaskable question goes ahead with
      // magic-compat ON (Q-SCRIPT-48).
      const std::string question = "Really throw away all changes made to cell " + def->name() + "? ";
      if (!cmd::confirm(s.ed.prompt, question, s.io.magic_compat)) {
        return CommandResult::success();
      }
    }
    if (flush_cell(s, *def, a.dereference)) {
      s.ed.selection.clear();
      s.emit(codes::kReadNote, std::string{"[Flushed"} + (changed ? " Modifications were Discarded" : "") + "]");
    }
    return CommandResult::success();
  });
}

}  // namespace

bool register_flush_command(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"flush", "flush [cellname] [-noprompt] [-dereference]",
                                    "Discards the changes made to a cell (default: the edit cell) by reading it "
                                    "again from its file.",
                                    flush_cmd})
              .has_value();
}

}  // namespace hefesto::cell::detail

namespace hefesto::cell {

cmd::CommandResult dereference_cell(cmd::CommandContext& ctx, db::CellDef& def) {
  return detail::with_session(ctx, [&](detail::IoSession& s) {
    if (detail::flush_cell(s, def, true)) {
      s.ed.selection.clear();  // SelectClear
    }
    return cmd::CommandResult::success();
  });
}

}  // namespace hefesto::cell
