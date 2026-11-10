// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the EDIT commands. The box undo event follows Magic's
// DBWundo box events (dbwind/DBWundo.c:261-301); the edit box follows
// ToolGetEditBox (dbwind/DBWtools.c:320-352). Owner: EDIT.

#include "db/edit_util.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::db::edit {

namespace {

std::string describe_box(const CellDef* root, const std::optional<Rect>& area) {
  if (!area || root == nullptr) {
    return "no box";
  }
  return root->name() + " (" + std::to_string(area->x0) + "," + std::to_string(area->y0) + ")-(" +
         std::to_string(area->x1) + "," + std::to_string(area->y1) + ")";
}

// dbwRecordBoxArea: asks the highlight layer to erase (the old box) or draw
// (the new box) in the windows showing the box root (DBWHLRedraw).
void request_box_redraw(editor::EditorState& editor, bool erase) {
  if (CellDef* root = box_root(editor); root != nullptr && editor.box) {
    editor.highlights.request(root, *editor.box, erase);
  }
}

// One box change: the old and new (root cell, rectangle) (cmd-box "Box state").
class BoxEvent : public undo::Event {
 public:
  BoxEvent(CellDef* old_root, std::optional<Rect> old_area, CellDef* new_root, std::optional<Rect> new_area)
      : old_root_(old_root), old_area_(old_area), new_root_(new_root), new_area_(new_area) {}

  void undo(editor::EditorState& editor) override { apply(editor, old_root_, old_area_); }
  void redo(editor::EditorState& editor) override { apply(editor, new_root_, new_area_); }
  [[nodiscard]] std::string describe() const override {
    return "box " + describe_box(old_root_, old_area_) + " -> " + describe_box(new_root_, new_area_);
  }

 private:
  static void apply(editor::EditorState& editor, CellDef* root, const std::optional<Rect>& area) {
    request_box_redraw(editor, true);
    editor.box = area;
    editor.box_root = area ? root : nullptr;
    request_box_redraw(editor, false);
  }

  CellDef* old_root_;
  std::optional<Rect> old_area_;
  CellDef* new_root_;
  std::optional<Rect> new_area_;
};

}  // namespace

CellDef* edit_root(const editor::EditorState& editor) noexcept {
  return editor.edit_root != nullptr ? editor.edit_root : editor.edit_cell;
}

CellDef* box_root(const editor::EditorState& editor) noexcept {
  if (!editor.box) {
    return nullptr;
  }
  return editor.box_root != nullptr ? editor.box_root : edit_root(editor);
}

CellDef* default_root(const editor::EditorState& editor) noexcept {
  if (const view::LayoutWindow* w = editor.windows.current(); w != nullptr && w->root() != nullptr) {
    return w->root();
  }
  return edit_root(editor);
}

void set_box(editor::EditorState& editor, CellDef* root, std::optional<Rect> area) {
  CellDef* old_root = box_root(editor);
  const std::optional<Rect> old_area = editor.box;
  if (old_area == area && (!area || old_root == root)) {
    return;
  }
  request_box_redraw(editor, true);
  editor.box = area;
  editor.box_root = area ? root : nullptr;
  request_box_redraw(editor, false);
  editor.undo.record(std::make_unique<BoxEvent>(old_root, old_area, editor.box_root, area));
}

diag::Outcome<Rect> edit_box(const cmdutil::Session& s) {
  const editor::EditorState& ed = *s.editor;
  if (!ed.box) {
    return diag::make(codes::kNoBox, "Box tool must be present");
  }
  if (ed.edit_cell == nullptr || box_root(ed) != edit_root(ed)) {
    return diag::make(codes::kBoxNotInEditWindow, "The box isn't in a window on the edit cell.");
  }
  return editor::to_edit(ed, *ed.box);
}

diag::Outcome<Target> paint_target(cmd::CommandContext& ctx, std::string_view no_edit_cell) {
  auto s = cmdutil::session(ctx);
  if (!s.ok()) {
    return s.error();
  }
  editor::EditorState& ed = *s.value().editor;
  if (ed.edit_cell == nullptr || ed.edit_cell->flags().no_edit) {
    return diag::make(codes::kNoEditCell, std::string{no_edit_cell});
  }
  auto area = edit_box(s.value());
  if (!area.ok()) {
    return area.error();
  }
  return Target{s.value(), ed.edit_cell, *ed.box, area.value()};
}

PaintOptions paint_options(const cmdutil::Session& s) {
  PaintOptions o;
  o.locked = s.editor->locked_types;
  o.magic_compat = s.magic_compat;
  return o;
}

// DBAdjustLabels with its "Moving label" / "Adjusting size of label" notes
// (Magic ref: database/DBlabel.c:690-725), then DBWAreaChanged over the
// edited area (ui-windows-and-views "Change-driven redisplay"): the changed
// layers, or every layer and the label extent when labels changed. DRC areas
// are queued by the M3 checker from the same changes (cmd-paint-erase
// "Common effects", item 7).
void finish(const Target& t, const Rect& area, const tech::TypeMask& layers, const Effects& effects) {
  bool labels = effects.labels_changed;
  if (effects.adjust_labels) {
    std::vector<std::string> notes;
    labels = adjust_labels(*t.def, *t.s.tech, area, &notes) || labels;
    for (std::string& n : notes) {
      cmdutil::print(t.s, std::move(n));
    }
  }
  if (effects.clear_selection) {
    t.s.editor->selection.clear();
  }
  recompute_bbox(*t.def, t.s.magic_compat);
  t.s.editor->windows.area_changed(*t.def, area, labels ? std::nullopt : std::optional<tech::TypeMask>(layers));
}

const view::LayoutWindow* window_on(const editor::EditorState& editor, const CellDef* root) {
  const view::LayoutWindow* current = editor.windows.current();
  if (current != nullptr && current->root() == root) {
    return current;
  }
  const view::LayoutWindow* found = nullptr;
  editor.windows.for_each([&](const view::LayoutWindow& w) {
    if (found == nullptr && w.root() == root) {
      found = &w;
    }
  });
  return found;
}

namespace {

void search_def(const CellDef& def, const Transform& to_root, const Rect& area, const tech::TypeMask& types,
                const std::function<void(const FoundTile&)>& visit) {
  const Rect local = apply(invert(to_root), area);
  for (std::size_t p = 1; p < def.plane_count(); ++p) {
    def.plane(static_cast<tech::PlaneId>(p)).enumerate(local, types, [&](const TileView& t) {
      FoundTile f{apply(to_root, t.area), t.type, std::nullopt, &def, to_root};
      if (t.split) {
        f.side = transform_triangle(to_root, t.diagonal, t.right_side);
      }
      visit(f);
      return true;
    });
  }
}

}  // namespace

void search_tree(const editor::EditorState& editor, const CellDef& root, const Rect& area,
                 const tech::TypeMask& types, Depth depth, const std::function<void(const FoundTile&)>& visit) {
  search_def(root, kIdentity, area, types, visit);
  const view::LayoutWindow* window = depth == Depth::all ? nullptr : window_on(editor, &root);
  if (depth == Depth::expanded && window == nullptr) {
    return;
  }
  walk_hierarchy(root, [&](const CellUse& use, const Transform& to_root, int) {
    if (use.def == nullptr || (window != nullptr && !window->is_expanded(use))) {
      return false;
    }
    search_def(*use.def, to_root, area, types, visit);
    return true;
  });
}

cmd::CommandResult usage(std::string text) {
  return cmdutil::fail(cmdutil::bad_argument(std::move(text)));
}

}  // namespace hefesto::db::edit
