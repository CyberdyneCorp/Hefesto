// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// select::Selection storage and its undo event (cmd-selection "Selection
// model"). Owner: SELECT.
// (Magic ref: select/selCreate.c:110-300 SelectInit/SelectClear,
// select/selUndo.c:175-300 SelRememberForUndo)

#include "hefesto/select/selection.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::select {

namespace {

// Magic's selection cell name (SelectDef "__SELECT__").
constexpr std::string_view kSelectionCellName = "__SELECT__";

/// Takes `use` out of its definition's instance list (DBUnLinkCell's
/// cd_parents part); the selection's uses are linked like any other.
void unlink(const db::CellUse& use) {
  if (use.def != nullptr) {
    auto& inst = use.def->instances();
    inst.erase(std::remove(inst.begin(), inst.end(), &use), inst.end());
  }
}

bool same_placement(const db::CellUse& u, const db::CellDef& child, const db::Transform& t, const db::ArrayInfo& a) {
  return u.def == &child && u.transform == t && u.array == a;
}

/// selRedisplay: erases the highlights of the selection `from` (its root
/// and bounding box) and draws those of the editor's current selection.
void request_redraw(editor::EditorState& editor, db::CellDef* old_root, const db::Rect& old_area) {
  if (old_root != nullptr && !old_area.is_null()) {
    editor.highlights.request(old_root, old_area, true);
  }
  const db::Rect now = editor.selection.bbox();
  if (editor.selection.root() != nullptr && !now.is_null()) {
    editor.highlights.request(editor.selection.root(), now, false);
  }
}

/// One selection change: the contents before and after.
class SelectionEvent final : public undo::Event {
 public:
  SelectionEvent(Snapshot before, Snapshot after) : before_(std::move(before)), after_(std::move(after)) {}
  void undo(editor::EditorState& editor) override { replay(editor, before_); }
  void redo(editor::EditorState& editor) override { replay(editor, after_); }
  [[nodiscard]] std::string describe() const override { return "selection change"; }

 private:
  static void replay(editor::EditorState& editor, const Snapshot& to) {
    db::CellDef* old_root = editor.selection.root();
    const db::Rect old_area = editor.selection.bbox();
    editor.selection.restore(to);
    request_redraw(editor, old_root, old_area);
  }

  Snapshot before_;
  Snapshot after_;
};

}  // namespace

Selection::Selection() = default;

Selection::~Selection() {
  if (def_ != nullptr) {
    for (const auto& u : def_->uses()) {
      unlink(*u);
    }
  }
}

void Selection::make_def(std::size_t plane_count) {
  if (def_ != nullptr) {
    for (const auto& u : def_->uses()) {
      unlink(*u);
    }
  }
  def_ = std::make_unique<db::CellDef>(std::string{kSelectionCellName}, plane_count);
  def_->flags().internal = true;
  def_->flags().available = true;
  cycle.last_selected_use = nullptr;
}

void Selection::set_root(db::CellDef* root, std::size_t plane_count) {
  if (root != root_ || def_ == nullptr || def_->plane_count() != plane_count) {
    make_def(plane_count);
    provenance_.clear();
    offset_.reset();
  }
  root_ = root;
}

bool Selection::empty() const {
  if (def_ == nullptr) {
    return true;
  }
  if (!def_->labels().empty() || !def_->uses().empty()) {
    return false;
  }
  for (std::size_t p = 0; p < def_->plane_count(); ++p) {
    if (!def_->plane(static_cast<tech::PlaneId>(p)).empty()) {
      return false;
    }
  }
  return true;
}

void Selection::clear() {
  if (def_ != nullptr) {
    make_def(def_->plane_count());
  }
  provenance_.clear();
  offset_.reset();
  area_types = tech::TypeMask{};
}

void Selection::prune(const db::CellLibrary& library) {
  std::set<const db::CellDef*> live;
  library.for_each([&](db::CellDef& d) { live.insert(&d); });
  if (root_ != nullptr && live.count(root_) == 0) {
    root_ = nullptr;
    clear();
    return;
  }
  if (def_ == nullptr) {
    return;
  }
  auto& uses = def_->uses();
  uses.erase(std::remove_if(uses.begin(), uses.end(), [&](const auto& u) { return live.count(u->def) == 0; }),
             uses.end());
  // The last cell selected may have gone with its definition.
  const bool kept = std::any_of(uses.begin(), uses.end(),
                                [&](const auto& u) { return u.get() == cycle.last_selected_use; });
  if (!kept) {
    cycle.last_selected_use = nullptr;
  }
}

db::CellUse* Selection::add_use(db::CellDef& child, const std::string& id, const db::Transform& transform,
                                const db::ArrayInfo& array, bool locked) {
  if (def_ == nullptr) {
    return nullptr;
  }
  for (const auto& u : def_->uses()) {
    if (same_placement(*u, child, transform, array)) {
      return nullptr;
    }
  }
  auto use = std::make_unique<db::CellUse>();
  use->serial = db::next_use_serial();  // DBCellNewUse order (db::recompute_bbox)
  use->id = id.empty() || db::find_use(*def_, id) != nullptr ? std::string{} : id;
  if (use->id.empty()) {
    std::size_t k = 0;
    const std::string stem = child.name() + "_";
    while (db::find_use(*def_, stem + std::to_string(k)) != nullptr) {
      ++k;
    }
    use->id = stem + std::to_string(k);
  }
  use->def = &child;
  use->parent = def_.get();
  use->transform = transform;
  use->array = array;
  use->locked = locked;
  db::compute_use_bbox(*use);
  db::CellUse* raw = use.get();
  def_->uses().push_back(std::move(use));
  child.instances().insert(child.instances().begin(), raw);  // DBLinkCell
  return raw;
}

void Selection::remove_use(const db::CellUse& use) {
  if (def_ == nullptr) {
    return;
  }
  if (cycle.last_selected_use == &use) {
    cycle.last_selected_use = nullptr;
  }
  unlink(use);
  auto& uses = def_->uses();
  uses.erase(std::remove_if(uses.begin(), uses.end(), [&](const auto& u) { return u.get() == &use; }), uses.end());
}

db::Rect Selection::bbox() const {
  db::Rect box{0, 0, -1, -1};
  if (def_ == nullptr) {
    return box;
  }
  for (std::size_t p = tech::kFirstUserPlane; p < def_->plane_count(); ++p) {
    box = db::include(box, def_->plane(static_cast<tech::PlaneId>(p)).bounding_box());
  }
  for (const db::Label& l : def_->labels()) {
    box = db::include(box, l.rect.is_null() ? db::Rect{l.rect.x0, l.rect.y0, l.rect.x0 + 1, l.rect.y0 + 1} : l.rect);
  }
  for (const auto& u : def_->uses()) {
    box = db::include(box, u->bbox);
  }
  if (offset_ && !box.is_null()) {
    box = db::apply(db::translation(offset_->x, offset_->y), box);
  }
  return box;
}

Snapshot Selection::snapshot() const {
  Snapshot s;
  s.root = root_;
  s.offset = offset_;
  s.bbox = bbox();
  s.provenance = provenance_;
  if (def_ == nullptr) {
    return s;
  }
  for (std::size_t p = 0; p < def_->plane_count(); ++p) {
    s.planes.push_back(def_->plane(static_cast<tech::PlaneId>(p)).clone());
  }
  s.labels = def_->labels();
  for (const auto& u : def_->uses()) {
    s.uses.push_back(Snapshot::Use{u->def, u->id, u->transform, u->array, u->locked});
  }
  return s;
}

void Selection::restore(const Snapshot& snapshot) {
  root_ = snapshot.root;
  offset_ = snapshot.offset;
  provenance_ = snapshot.provenance;
  if (snapshot.planes.empty()) {
    if (def_ != nullptr) {
      for (const auto& u : def_->uses()) {
        unlink(*u);  // the definitions must not keep pointers to uses about to be destroyed
      }
    }
    def_.reset();
    return;
  }
  make_def(snapshot.planes.size());
  for (std::size_t p = 0; p < snapshot.planes.size(); ++p) {
    def_->plane(static_cast<tech::PlaneId>(p)) = snapshot.planes[p].clone();
  }
  def_->labels() = snapshot.labels;
  for (const Snapshot::Use& u : snapshot.uses) {
    (void)add_use(*u.def, u.id, u.transform, u.array, u.locked);
  }
  db::recompute_bbox(*def_);
}

EditCopies::EditCopies(editor::EditorState& editor) : editor_(editor), before_(editor.selection.snapshot()) {
  db::CellDef* root = editor.edit_root != nullptr ? editor.edit_root : editor.edit_cell;
  if (const view::LayoutWindow* w = editor.windows.current(); w != nullptr && w->root() != nullptr) {
    root = w->root();
  }
  editor.selection.set_root(root, editor.technology->plane_count());
  editor.selection.clear();
}

void EditCopies::add_use(const db::CellUse& use) {
  db::CellDef* sel = editor_.selection.def();
  if (sel == nullptr || use.def == nullptr) {
    return;
  }
  db::PlaceOptions po;
  po.id = use.id;
  po.transform = db::compose(editor_.edit_transform, use.transform);
  po.array = use.array;
  po.locked = use.locked;
  if (auto copy = db::place(*sel, *use.def, po); copy.ok()) {
    editor_.selection.provenance().push_back(
        Provenance{Provenance::Kind::use, editor_.edit_cell, use.id, copy.value()->bbox});
  }
}

void EditCopies::add_label(const db::Label& label) {
  db::CellDef* sel = editor_.selection.def();
  if (sel == nullptr) {
    return;
  }
  db::Label copy = label;
  copy.rect = db::apply(editor_.edit_transform, label.rect);
  copy.justification = db::transform_position(editor_.edit_transform, label.justification);
  editor_.selection.provenance().push_back(Provenance{Provenance::Kind::label, editor_.edit_cell, {}, copy.rect});
  sel->labels().push_back(std::move(copy));
}

void EditCopies::add_paint(const db::CellDef& source, const db::Rect& area, const db::Transform& to_edit) {
  db::CellDef* sel = editor_.selection.def();
  if (sel == nullptr) {
    return;
  }
  tech::TypeMask types = tech::TypeMask::all();
  types.set(tech::kSpace, false);
  const db::Transform to_root = db::compose(editor_.edit_transform, to_edit);
  (void)db::copy_paint(source, area, to_root, *sel, *editor_.technology, types);
  editor_.selection.provenance().push_back(
      Provenance{Provenance::Kind::paint, editor_.edit_cell, {}, db::apply(to_root, area)});
}

void EditCopies::commit() {
  if (committed_) {
    return;
  }
  committed_ = true;
  if (db::CellDef* sel = editor_.selection.def()) {
    db::recompute_bbox(*sel);
  }
  record_change(editor_, std::move(before_));
}

void record_change(editor::EditorState& editor, Snapshot before) {
  request_redraw(editor, before.root, before.bbox);
  editor.undo.record(std::make_unique<SelectionEvent>(std::move(before), editor.selection.snapshot()));
}

}  // namespace hefesto::select
