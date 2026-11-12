// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The selection (capability cmd-selection, requirement "Selection model";
// design.md D6): a flattened, root-coordinate copy of chosen paint, labels
// and uses, kept in an internal db::CellDef (Magic's SelectDef) that is never
// in the cell namespace nor saved, plus the provenance of each item so that
// `what` can say where it came from. It is tied to the selection root (the
// top cell of the window it was made in) and may float by an offset not yet
// applied to the layout (`select move`). Every change records an undo event
// (select::record_change below).
//
// Uses in the selection keep their array parameters and identifiers and are
// linked into their definition's instance list (CellDef::instances) as in
// Magic, so deleting a cell definition removes them (CellLibrary::remove
// clears internal parents first); clearing the selection unlinks them.
// prune() forgets a deleted root.
// (Magic ref: select/selCreate.c:110-300, select/selUndo.c:175-300)
//
// Owner: SELECT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/connect/search.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::select {

/// Where a selected item came from: the source cell and the instance path
/// from the selection root to it (element names joined by '/', empty for
/// the root itself).
struct Provenance {
  enum class Kind : std::uint8_t { paint, label, use };
  Kind kind = Kind::paint;
  const db::CellDef* source = nullptr;
  std::string use_path;
  /// The item's area in root coordinates (paint rectangle, label rectangle
  /// or use bounding box).
  db::Rect area;
};

/// The point-selection cycling state Magic keeps in CmdSelect's statics
/// ("Point selection and cycling", "Cell selection").
struct CycleState {
  enum class Level : std::uint8_t { none, chunk, region, net };
  /// The type chosen last (TT_SELECTBASE - 1: none yet).
  int type = -1;
  /// The neighbourhood of the last reference point, and the command number
  /// of the last point selection.
  db::Rect last_area{-100, -100, -200, -200};
  std::uint64_t last_command = 0;
  Level level = Level::none;
  db::Rect chunk;
  bool less_cycle = false;
  bool less_cell_cycle = false;
  /// Cell cycling: the use selected last (identity only, never dereferenced)
  /// and the array element of it.
  const void* last_use = nullptr;
  db::Point last_indices;
  /// The use SelectCell added last (replaced by the next cell click).
  const db::CellUse* last_selected_use = nullptr;
};

/// A copy of the selection's contents (undo/redo of selection changes).
struct Snapshot {
  db::CellDef* root = nullptr;
  std::vector<db::Plane> planes;
  std::vector<db::Label> labels;
  struct Use {
    db::CellDef* def = nullptr;
    std::string id;
    db::Transform transform;
    db::ArrayInfo array;
    bool locked = false;
  };
  std::vector<Use> uses;
  std::vector<Provenance> provenance;
  std::optional<db::Point> offset;
  /// Selection::bbox() when taken (the highlight area to erase).
  db::Rect bbox;
};

class Selection {
 public:
  Selection();
  ~Selection();
  Selection(const Selection&) = delete;
  Selection& operator=(const Selection&) = delete;

  /// The selection root (nullptr: nothing has been selected yet).
  [[nodiscard]] db::CellDef* root() const noexcept { return root_; }
  /// Makes `root` the selection root; a different root clears the selection
  /// first ("Selection model").
  void set_root(db::CellDef* root, std::size_t plane_count);

  /// Creates the (empty) selection cell when there is none yet, leaving the
  /// root unset: Magic's SelectDef exists from start-up, so the selection
  /// commands see an empty selection before anything was ever selected.
  void ensure_cell(std::size_t plane_count);

  /// The internal cell holding the selected items in root coordinates;
  /// nullptr until set_root() or ensure_cell(). Never in a CellLibrary (no change recorder).
  [[nodiscard]] db::CellDef* def() noexcept { return def_.get(); }
  [[nodiscard]] const db::CellDef* def() const noexcept { return def_.get(); }

  [[nodiscard]] std::vector<Provenance>& provenance() noexcept { return provenance_; }
  [[nodiscard]] const std::vector<Provenance>& provenance() const noexcept { return provenance_; }

  /// The floating offset (`select move`), not yet applied to the layout.
  [[nodiscard]] const std::optional<db::Point>& floating_offset() const noexcept { return offset_; }
  void set_floating_offset(std::optional<db::Point> offset) noexcept { offset_ = offset; }

  /// True when nothing (no paint, label or use) is selected.
  [[nodiscard]] bool empty() const;

  /// Removes every item and the offset; the root is kept.
  void clear();

  /// Forgets the root when it is no longer in `library` (a deleted cell),
  /// clearing everything, and drops uses whose definition is gone.
  void prune(const db::CellLibrary& library);

  /// Adds a use of `child` with the root transform `transform` and `array`
  /// (SelectCell's copy): the identifier `id` when free in the selection,
  /// else a generated one. Returns nullptr, adding nothing, when an
  /// identical use (same child, transform and array) is already selected.
  db::CellUse* add_use(db::CellDef& child, const std::string& id, const db::Transform& transform,
                       const db::ArrayInfo& array, bool locked = false);
  /// Removes (and unlinks) a use of the selection (owned by def()).
  void remove_use(const db::CellUse& use);

  /// The bounding box of the selection in root coordinates, the floating
  /// offset applied (Magic's SelectUse bbox); a null rectangle when empty.
  [[nodiscard]] db::Rect bbox() const;

  [[nodiscard]] Snapshot snapshot() const;
  void restore(const Snapshot& snapshot);

  /// `select do|no|simple labels` (Magic's SelectDoLabels).
  connect::LabelPolicy label_policy = connect::LabelPolicy::full;
  /// The types the last area selection asked for (Magic's
  /// SelectDef->cd_types; empty after clear()): editing the selection then
  /// skips edit-cell paint of other types without a non-edit warning.
  tech::TypeMask area_types;
  /// Point-selection cycling (`select` with no option).
  CycleState cycle;

 private:
  void make_def(std::size_t plane_count);

  db::CellDef* root_ = nullptr;
  std::unique_ptr<db::CellDef> def_;
  std::vector<Provenance> provenance_;
  std::optional<db::Point> offset_;
};

/// Records `before` and the editor's current selection as one undo event
/// (selection changes restore their before/after contents on undo/redo),
/// and asks the highlight layer to erase the old selection and draw the new
/// one (selRedisplay; also on undo/redo).
void record_change(editor::EditorState& editor, Snapshot before);

/// The selection after an edit that copies items into the edit cell
/// (`getcell`, `dump`: SelectClear, then SelectAndCopy2 of each new item):
/// the constructor makes the current window's root (the edit root without a
/// window) the selection root and clears the selection; the add_* calls copy
/// edit-cell items into it in root coordinates with their provenance;
/// commit() records the change for undo and requests the highlight redraw.
class EditCopies {
 public:
  explicit EditCopies(editor::EditorState& editor);
  EditCopies(const EditCopies&) = delete;
  EditCopies& operator=(const EditCopies&) = delete;
  ~EditCopies() = default;

  /// A use of the edit cell.
  void add_use(const db::CellUse& use);
  /// A label of the edit cell.
  void add_label(const db::Label& label);
  /// The paint of `source` under `area`, placed in the edit cell through
  /// `to_edit` (every type but space).
  void add_paint(const db::CellDef& source, const db::Rect& area, const db::Transform& to_edit);
  /// Records the change since construction (once).
  void commit();

 private:
  editor::EditorState& editor_;
  Snapshot before_;
  bool committed_ = false;
};

}  // namespace hefesto::select
