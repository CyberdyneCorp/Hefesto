// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SessionMacroHost and the undo step of a LEF read (session_host.hpp;
// lefRead.c:2112-2175 lefFindCell/DBCellClearDef/DBCellSetAvail,
// 2443-2447 DBWAreaChanged, 2420 DRCCheckThis; Q-LEFDEF-22: Magic disables
// undo for the read instead).
//
// Owner: LEFR (design D2).

#include "lef/read/session_host.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::lef::detail {

namespace {

/// Everything of a cell a LEF read changes: paint, labels, properties,
/// flags, bounding boxes and timestamp.
struct CellState {
  std::vector<db::Plane> planes;
  std::vector<db::Label> labels;
  db::PropertyStore properties;
  db::CellFlags flags;
  db::Rect bbox;
  db::Rect extended;
  std::int64_t timestamp = 0;
};

CellState capture(const db::CellDef& cell) {
  CellState state;
  for (std::size_t p = 0; p < cell.plane_count(); ++p) {
    state.planes.push_back(cell.plane(static_cast<tech::PlaneId>(p)).clone());
  }
  state.labels = cell.labels();
  state.properties = cell.properties();
  state.flags = cell.flags();
  state.bbox = cell.bbox();
  state.extended = cell.extended_bbox();
  state.timestamp = cell.timestamp();
  return state;
}

void restore(editor::EditorState& editor, db::CellDef& cell, const CellState& state) {
  const db::Rect before = cell.bbox();
  if (state.planes.size() == cell.plane_count()) {
    for (std::size_t p = 0; p < state.planes.size(); ++p) {
      cell.plane(static_cast<tech::PlaneId>(p)) = state.planes[p].clone();
    }
  }
  cell.labels() = state.labels;
  cell.properties() = state.properties;
  cell.flags() = state.flags;
  cell.set_bboxes(state.bbox, state.extended);
  cell.set_timestamp(state.timestamp);
  editor.undo.note_changed(cell, db::include(before, state.bbox));
}

/// A cell with no paint, labels or properties (the state of a created cell
/// that cannot be removed).
CellState empty_state(const db::CellDef& cell) {
  CellState state;
  for (std::size_t p = 0; p < cell.plane_count(); ++p) {
    state.planes.emplace_back();
  }
  state.flags = cell.flags();
  state.flags.fixed_bbox = false;
  state.bbox = db::Rect{0, 0, 1, 1};
  state.extended = state.bbox;
  return state;
}

/// Whether the session points at `cell` outside the cell library: a
/// window, the edit cell or frame, the box or the selection.
bool referenced(editor::EditorState& editor, const db::CellDef& cell) {
  bool shown = false;
  editor.windows.for_each([&](view::LayoutWindow& w) { shown = shown || w.root() == &cell; });
  return shown || editor.edit_cell == &cell || editor.edit_root == &cell || editor.box_root == &cell ||
         editor.selection.root() == &cell;
}

/// The read's undo step.
class LefReadEvent final : public undo::Event {
 public:
  struct Created {
    std::string name;
    CellState after;
  };
  struct Annotated {
    std::string name;
    CellState before;
    CellState after;
  };

  LefReadEvent(std::string description, std::vector<Created> created, std::vector<Annotated> annotated)
      : description_(std::move(description)), created_(std::move(created)), annotated_(std::move(annotated)) {}

  void undo(editor::EditorState& editor) override {
    for (auto it = annotated_.rbegin(); it != annotated_.rend(); ++it) {
      if (db::CellDef* cell = editor.cells.find(it->name)) {
        restore(editor, *cell, it->before);
      }
    }
    for (auto it = created_.rbegin(); it != created_.rend(); ++it) {
      if (db::CellDef* cell = editor.cells.find(it->name)) {
        remove(editor, *cell);
      }
    }
  }

  void redo(editor::EditorState& editor) override {
    for (const Created& made : created_) {
      db::CellDef* cell = editor.cells.find(made.name);
      if (cell == nullptr) {
        auto fresh = editor.cells.create(made.name, made.after.planes.size());
        cell = fresh.ok() ? fresh.value() : nullptr;
      }
      if (cell != nullptr) {
        restore(editor, *cell, made.after);
      }
    }
    for (const Annotated& changed : annotated_) {
      if (db::CellDef* cell = editor.cells.find(changed.name)) {
        restore(editor, *cell, changed.after);
      }
    }
  }

  [[nodiscard]] std::string describe() const override { return description_; }

 private:
  // A created cell leaves the library unless the session still shows or
  // uses it; then it is only emptied.
  static void remove(editor::EditorState& editor, db::CellDef& cell) {
    if (referenced(editor, cell) || !editor.cells.check_removable(cell).ok()) {
      restore(editor, cell, empty_state(cell));
      return;
    }
    editor.windows.area_changed(cell, cell.bbox(), std::nullopt);
    editor.drc.pending.remove(cell);
    (void)editor.cells.remove(cell);
  }

  std::string description_;
  std::vector<Created> created_;
  std::vector<Annotated> annotated_;
};

}  // namespace

struct SessionMacroHost::Kept {
  std::string name;
  CellState before;
};

SessionMacroHost::SessionMacroHost(editor::EditorState& editor) : editor_(editor) {}

SessionMacroHost::~SessionMacroHost() = default;

db::CellDef* SessionMacroHost::find(std::string_view name) {
  return editor_.cells.find(name);
}

db::CellDef* SessionMacroHost::create(std::string_view name) {
  auto made = editor_.cells.create(name, editor_.technology->plane_count());
  if (!made.ok()) {
    return nullptr;
  }
  db::CellDef* cell = made.value();
  cell->flags().available = true;
  created_.emplace_back(name);
  return cell;
}

void SessionMacroHost::annotating(db::CellDef& cell) {
  const bool kept = std::any_of(annotated_.begin(), annotated_.end(),
                                [&](const auto& k) { return k->name == cell.name(); });
  if (!kept) {
    annotated_.push_back(std::make_unique<Kept>(Kept{cell.name(), capture(cell)}));
  }
}

void SessionMacroHost::finished(db::CellDef& cell, std::optional<db::Rect> drc_area) {
  editor_.windows.area_changed(cell, cell.bbox(), std::nullopt);
  if (drc_area) {
    drc::check_this(editor_, cell, *drc_area);
  }
}

std::unique_ptr<undo::Event> SessionMacroHost::take_event(std::string description) {
  std::vector<LefReadEvent::Created> created;
  for (const std::string& name : created_) {
    if (const db::CellDef* cell = editor_.cells.find(name)) {
      created.push_back(LefReadEvent::Created{name, capture(*cell)});
    }
  }
  std::vector<LefReadEvent::Annotated> annotated;
  for (auto& kept : annotated_) {
    if (const db::CellDef* cell = editor_.cells.find(kept->name)) {
      annotated.push_back(LefReadEvent::Annotated{kept->name, std::move(kept->before), capture(*cell)});
    }
  }
  created_.clear();
  annotated_.clear();
  if (created.empty() && annotated.empty()) {
    return nullptr;
  }
  return std::make_unique<LefReadEvent>(std::move(description), std::move(created), std::move(annotated));
}

}  // namespace hefesto::lef::detail
