// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// undo::DbRecorder and the database undo events (layout-undo "Recorded event
// kinds", "Undo and redo semantics", "Undoable property changes and flatten
// in place with magic-compat off"). Owner: UNDO.
// Behavior from Magic VLSI 8.3.694 (fc5daa9e) database/DBundo.c (paint,
// split, label and cell-use clients, dbUndoOpenCell/CloseCell), Copyright
// (C) 1985, 1990 Regents of the University of California (permissive
// license, THIRD_PARTY_NOTICES.md).

#include "hefesto/undo/db_recorder.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::undo {

namespace {

using editor::EditorState;

db::Rect unite(const db::Rect& a, const db::Rect& b) noexcept {
  if (a.is_null()) {
    return b;
  }
  if (b.is_null()) {
    return a;
  }
  return db::Rect{std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}

// The area to redisplay for a label or point: zero-width rectangles grow by
// one unit so the window notices them.
db::Rect visible_area(const db::Rect& r) noexcept {
  return db::Rect{r.x0, r.y0, std::max(r.x1, r.x0 + 1), std::max(r.y1, r.y0 + 1)};
}

db::CellUse* use_with_id(const db::CellDef& parent, std::string_view id) {
  for (const auto& use : parent.uses()) {
    if (use->id == id) {
      return use.get();
    }
  }
  return nullptr;
}

// ---- paint ------------------------------------------------------------------

// Consecutive tile changes of one cell. Undo paints each change's old type
// back over its rectangle or triangle, newest first; redo the new type,
// oldest first. The rectangle holds exactly one type before and after the
// change, so setting the type is Magic's erase-new-then-paint-old.
class PaintEvent final : public Event {
 public:
  explicit PaintEvent(db::CellDef& def) : def_(&def) {}

  [[nodiscard]] const db::CellDef* cell() const noexcept { return def_; }
  void add(const db::TileChange& change) { changes_.push_back(change); }

  void undo(EditorState& editor) override { apply(editor, true); }
  void redo(EditorState& editor) override { apply(editor, false); }
  [[nodiscard]] std::string describe() const override {
    return "paint " + def_->name() + ": " + std::to_string(changes_.size()) + " tile change(s)";
  }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == def_; }

 private:
  void apply(EditorState& editor, bool backward) {
    if (!editor.undo.is_live(def_)) {
      return;
    }
    db::Rect area;
    // dbUndoPaintForw/Back: DRCCheckThis over each replayed rectangle.
    if (backward) {
      for (auto it = changes_.rbegin(); it != changes_.rend(); ++it) {
        area = unite(area, set_type(*it, it->old_type));
        drc::check_this(editor, *def_, it->area);
      }
    } else {
      for (const db::TileChange& change : changes_) {
        area = unite(area, set_type(change, change.new_type));
        drc::check_this(editor, *def_, change.area);
      }
    }
    db::mark_modified(*def_);
    editor.undo.note_changed(*def_, area);
  }

  db::Rect set_type(const db::TileChange& change, tech::TileType type) {
    if (row_type_ != type || !row_ready_) {
      row_.fill(type);
      row_type_ = type;
      row_ready_ = true;
    }
    const db::PaintRow& row = row_;
    db::Plane& plane = def_->plane(change.plane);
    if (change.triangle) {
      (void)plane.paint_triangle(change.area, change.triangle->diagonal, change.triangle->right_side, row);
    } else {
      (void)plane.paint(change.area, row);
    }
    return change.area;
  }

  db::CellDef* def_;
  std::vector<db::TileChange> changes_;
  // The "set every type to row_type_" row of the last replayed change.
  db::PaintRow row_{};
  tech::TileType row_type_ = tech::kSpace;
  bool row_ready_ = false;
};

// Split-tile structure only: Hefesto's planes re-merge split tiles on every
// paint, so replaying a fracture or join never changes the geometry.
class SplitEvent final : public Event {
 public:
  SplitEvent(db::CellDef& def, const db::SplitChange& change) : def_(&def), change_(change) {}

  void undo(EditorState&) override {}
  void redo(EditorState&) override {}
  [[nodiscard]] std::string describe() const override {
    return std::string{change_.kind == db::SplitChange::Kind::fracture ? "split" : "join"} + " " + def_->name();
  }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == def_; }

 private:
  db::CellDef* def_;
  db::SplitChange change_;
};

// ---- labels -----------------------------------------------------------------

class LabelEvent final : public Event {
 public:
  LabelEvent(db::CellDef& def, db::Label label, bool put) : def_(&def), label_(std::move(label)), put_(put) {}

  void undo(EditorState& editor) override { apply(editor, !put_); }
  void redo(EditorState& editor) override { apply(editor, put_); }
  [[nodiscard]] std::string describe() const override {
    return std::string{put_ ? "put label " : "erase label "} + "\"" + label_.text + "\" in " + def_->name();
  }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == def_; }

 private:
  void apply(EditorState& editor, bool put) {
    if (!editor.undo.is_live(def_)) {
      return;
    }
    auto& labels = def_->labels();
    if (put) {
      labels.push_back(label_);  // DBPutFontLabel appends
    } else {
      const auto it = std::find(labels.rbegin(), labels.rend(), label_);
      if (it == labels.rend()) {
        return;
      }
      labels.erase(std::next(it).base());
    }
    db::mark_modified(*def_);
    editor.undo.note_changed(*def_, visible_area(label_.text_bbox ? unite(*label_.text_bbox, label_.rect)
                                                                  : label_.rect));
  }

  db::CellDef* def_;
  db::Label label_;
  bool put_;
};

// ---- cell uses --------------------------------------------------------------

// The full state of a use (Magic's cellUE): enough to re-create it.
struct UseState {
  db::CellDef* child = nullptr;
  std::string id;
  db::Transform transform;
  db::ArrayInfo array;
  bool locked = false;
};

UseState use_state(const db::CellUse& use) {
  return UseState{use.def, use.id, use.transform, use.array, use.locked};
}

class UseEvent final : public Event {
 public:
  UseEvent(db::CellDef& parent, UseState state, bool placed)
      : parent_(&parent), state_(std::move(state)), placed_(placed) {}

  void undo(EditorState& editor) override { apply(editor, !placed_); }
  void redo(EditorState& editor) override { apply(editor, placed_); }
  [[nodiscard]] std::string describe() const override {
    return std::string{placed_ ? "place use " : "delete use "} + state_.id + " in " + parent_->name();
  }
  [[nodiscard]] bool references(const db::CellDef& def) const override {
    return &def == parent_ || &def == state_.child;
  }

 private:
  void apply(EditorState& editor, bool place) {
    if (!editor.undo.is_live(parent_)) {
      return;
    }
    if (place) {
      if (!editor.undo.is_live(state_.child) || use_with_id(*parent_, state_.id) != nullptr) {
        return;
      }
      db::PlaceOptions options;
      options.id = state_.id;
      options.transform = state_.transform;
      options.array = state_.array;
      options.locked = state_.locked;
      const auto placed = db::place(*parent_, *state_.child, options);
      if (placed.ok()) {
        editor.undo.note_changed(*parent_, placed.value()->bbox);
        drc::check_this(editor, *parent_, placed.value()->bbox);  // dbUndoCellForw/Back, TT_CHECKSUBCELL
      }
      return;
    }
    db::CellUse* use = use_with_id(*parent_, state_.id);
    if (use == nullptr) {
      return;
    }
    const db::Rect area = use->bbox;
    if (db::remove_use(*use).ok()) {
      editor.undo.note_changed(*parent_, area);
      drc::check_this(editor, *parent_, area);  // dbUndoCellForw/Back, TT_CHECKSUBCELL
    }
  }

  db::CellDef* parent_;
  UseState state_;
  bool placed_;
};

class UseIdEvent final : public Event {
 public:
  UseIdEvent(db::CellDef& parent, std::string old_id, std::string new_id)
      : parent_(&parent), old_id_(std::move(old_id)), new_id_(std::move(new_id)) {}

  void undo(EditorState& editor) override { apply(editor, new_id_, old_id_); }
  void redo(EditorState& editor) override { apply(editor, old_id_, new_id_); }
  [[nodiscard]] std::string describe() const override {
    return "use id " + old_id_ + " -> " + new_id_ + " in " + parent_->name();
  }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == parent_; }

 private:
  void apply(EditorState& editor, const std::string& from, const std::string& to) {
    if (!editor.undo.is_live(parent_)) {
      return;
    }
    if (db::CellUse* use = use_with_id(*parent_, from); use != nullptr && db::rename_use(*use, to).ok()) {
      editor.undo.note_changed(*parent_, use->bbox);
    }
  }

  db::CellDef* parent_;
  std::string old_id_;
  std::string new_id_;
};

class UseLockEvent final : public Event {
 public:
  UseLockEvent(db::CellDef& parent, std::string id, bool locked)
      : parent_(&parent), id_(std::move(id)), locked_(locked) {}

  void undo(EditorState& editor) override { apply(editor, !locked_); }
  void redo(EditorState& editor) override { apply(editor, locked_); }
  [[nodiscard]] std::string describe() const override {
    return std::string{locked_ ? "lock use " : "unlock use "} + id_ + " in " + parent_->name();
  }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == parent_; }

 private:
  void apply(EditorState& editor, bool locked) {
    if (!editor.undo.is_live(parent_)) {
      return;
    }
    if (db::CellUse* use = use_with_id(*parent_, id_); use != nullptr) {
      db::set_use_locked(*use, locked);
      editor.undo.note_changed(*parent_, db::Rect{});
    }
  }

  db::CellDef* parent_;
  std::string id_;
  bool locked_;
};

// ---- properties (magic-compat OFF, D-10) -------------------------------------

class PropertyEvent final : public Event {
 public:
  PropertyEvent(db::CellDef& def, std::string name, std::optional<db::PropertyValue> old_value,
                std::optional<db::PropertyValue> new_value)
      : def_(&def), name_(std::move(name)), old_(std::move(old_value)), new_(std::move(new_value)) {}

  void undo(EditorState& editor) override { apply(editor, old_); }
  void redo(EditorState& editor) override { apply(editor, new_); }
  [[nodiscard]] std::string describe() const override { return "property " + name_ + " of " + def_->name(); }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == def_; }

 private:
  void apply(EditorState& editor, const std::optional<db::PropertyValue>& value) {
    if (!editor.undo.is_live(def_)) {
      return;
    }
    db::set_property(*def_, name_, value);
    db::mark_modified(*def_);  // as the property command does (layout-properties)
    editor.undo.note_changed(*def_, db::Rect{});
  }

  db::CellDef* def_;
  std::string name_;
  std::optional<db::PropertyValue> old_;
  std::optional<db::PropertyValue> new_;
};

}  // namespace

void DbRecorder::tile_changed(db::CellDef& def, const db::TileChange& change) {
  if (!log_.recording()) {
    return;
  }
  auto* event = dynamic_cast<PaintEvent*>(log_.open_back());
  if (event == nullptr || event->cell() != &def) {
    auto fresh = std::make_unique<PaintEvent>(def);
    event = fresh.get();
    log_.record(std::move(fresh));
  }
  event->add(change);
}

void DbRecorder::split_changed(db::CellDef& def, const db::SplitChange& change) {
  if (log_.recording()) {
    log_.record(std::make_unique<SplitEvent>(def, change));
  }
}

void DbRecorder::label_put(db::CellDef& def, const db::Label& label) {
  if (log_.recording()) {
    log_.record(std::make_unique<LabelEvent>(def, label, true));
  }
}

void DbRecorder::label_erased(db::CellDef& def, const db::Label& label) {
  if (log_.recording()) {
    log_.record(std::make_unique<LabelEvent>(def, label, false));
  }
}

void DbRecorder::use_placed(db::CellDef& parent, const db::CellUse& use) {
  if (log_.recording()) {
    log_.record(std::make_unique<UseEvent>(parent, use_state(use), true));
  }
}

void DbRecorder::use_deleted(db::CellDef& parent, const db::CellUse& use) {
  if (log_.recording()) {
    log_.record(std::make_unique<UseEvent>(parent, use_state(use), false));
  }
}

void DbRecorder::use_id_cleared(db::CellDef& parent, const db::CellUse&, std::string_view old_id) {
  pending_parent_ = &parent;
  pending_old_id_ = std::string{old_id};
}

void DbRecorder::use_id_set(db::CellDef& parent, const db::CellUse&, std::string_view new_id) {
  const bool paired = pending_parent_ == &parent;
  pending_parent_ = nullptr;
  if (paired && log_.recording() && pending_old_id_ != new_id) {
    log_.record(std::make_unique<UseIdEvent>(parent, std::move(pending_old_id_), std::string{new_id}));
  }
  pending_old_id_.clear();
}

void DbRecorder::use_locked(db::CellDef& parent, const db::CellUse& use, bool locked) {
  if (log_.recording()) {
    log_.record(std::make_unique<UseLockEvent>(parent, use.id, locked));
  }
}

void DbRecorder::modified_changed(db::CellDef&, bool) {
  // Not recorded: replays mark the cells they change modified (Q-MODEL-12).
}

void DbRecorder::property_changed(db::CellDef& def, std::string_view name,
                                  const std::optional<db::PropertyValue>& old_value,
                                  const std::optional<db::PropertyValue>& new_value) {
  if (magic_compat() || !log_.recording() || old_value == new_value) {
    return;
  }
  log_.record(std::make_unique<PropertyEvent>(def, std::string{name}, old_value, new_value));
}

void DbRecorder::cell_removed(db::CellDef& def) {
  log_.cell_deleted(def);
}

}  // namespace hefesto::undo
