// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ChangeRecorder: the optional observer the layout database reports each
// primitive change to (openspec/changes/m2-interactive-editing/design.md D4
// and D5; capability layout-undo "Recorded event kinds"). It is Magic's
// DBundo hook: undo::DbRecorder implements it and turns each call into an
// undo event; tests install their own.
//
// The recorder is installed per library (db::CellLibrary::set_recorder) and
// reached from a definition through CellDef::recorder(). A null recorder
// (the default) records nothing; primitives test the pointer first, so
// recording costs nothing when it is off. Every call happens after the
// argument checks of the primitive and describes one change that has been,
// or is about to be, applied; the recorder must not modify the database.
// Suspension (file reads, scratch cells, replay) is the recorder's concern
// (undo::UndoLog), not the caller's.
//
// Callers (who reports what):
//   tile_changed       db paint primitives (paint.cpp; EDIT), per tile;
//                      Manhattan tiles since the scaffold, split tiles and
//                      triangle paint by EDIT (task 3.7)
//   split_changed      db split-tile fracture/join (EDIT task 3.7)
//   label_put/erased   db label primitives (HIER task 6.6)
//   use_*              db/hierarchy.cpp use primitives (EDIT task 3.7)
//   modified_changed   CellDef modified flag changes (EDIT task 3.7)
//   property_changed   db::change_property (HIER task 6.6); db::set_property
//                      (file readers) stays unrecorded
//   cell_removed       db::CellLibrary::remove
// Editor-level events (box, selection, edit cell, wiring, color map) do not
// pass through here: their areas record them with undo::UndoLog::record.
//
// Owner: INTEGRATION (shared interface); UNDO implements it.

#pragma once

#include <optional>
#include <string_view>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {

/// One tile (or one side of a split tile) whose type changes over `area`.
/// Undo paints `old_type` back over `area` (or over that triangle) without
/// image resolution (Magic's paintUE).
struct TileChange {
  tech::PlaneId plane = 0;
  Rect area;
  tech::TileType old_type = tech::kSpace;
  tech::TileType new_type = tech::kSpace;
  /// The changed side when the tile is split; nullopt for a Manhattan tile.
  std::optional<TriangleSide> triangle;

  friend bool operator==(const TileChange&, const TileChange&) = default;
};

/// A split tile created from a rectangle (fracture) or merged back into
/// one (join) over `area` (Magic's splitUE).
struct SplitChange {
  enum class Kind : std::uint8_t { fracture, join };
  Kind kind = Kind::fracture;
  tech::PlaneId plane = 0;
  Rect area;
  Diagonal diagonal = Diagonal::nw_se;
  tech::TileType left_type = tech::kSpace;
  tech::TileType right_type = tech::kSpace;

  friend bool operator==(const SplitChange&, const SplitChange&) = default;
};

class ChangeRecorder {
 public:
  ChangeRecorder() = default;
  ChangeRecorder(const ChangeRecorder&) = delete;
  ChangeRecorder& operator=(const ChangeRecorder&) = delete;
  virtual ~ChangeRecorder() = default;

  // Paint. `def` is the cell whose plane changes.
  virtual void tile_changed(CellDef& def, const TileChange& change) = 0;
  virtual void split_changed(CellDef& def, const SplitChange& change) = 0;

  // Labels: the full contents, port data included. An attribute change is
  // reported as label_erased(old) then label_put(new).
  virtual void label_put(CellDef& def, const Label& label) = 0;
  virtual void label_erased(CellDef& def, const Label& label) = 0;

  // Uses, with their full state (child, identifier, transform, array, lock,
  // bounding boxes). A move, rotation or re-array is use_deleted(old) then
  // use_placed(new). use_deleted is called before the use is destroyed.
  virtual void use_placed(CellDef& parent, const CellUse& use) = 0;
  virtual void use_deleted(CellDef& parent, const CellUse& use) = 0;
  // An identifier change: use_id_cleared(old id) then use_id_set(new id).
  virtual void use_id_cleared(CellDef& parent, const CellUse& use, std::string_view old_id) = 0;
  virtual void use_id_set(CellDef& parent, const CellUse& use, std::string_view new_id) = 0;
  virtual void use_locked(CellDef& parent, const CellUse& use, bool locked) = 0;

  /// The modified flag of `def` changes from `was_modified`.
  virtual void modified_changed(CellDef& def, bool was_modified) = 0;

  /// A property is set (`new_value`) or removed (nullopt); `old_value` is
  /// what it held before (nullopt when absent). Recorded as an undo event
  /// only with magic-compat OFF (decision D-10; the recorder decides).
  virtual void property_changed(CellDef& def, std::string_view name, const std::optional<PropertyValue>& old_value,
                                const std::optional<PropertyValue>& new_value) = 0;

  /// `def` is about to be destroyed (db::CellLibrary::remove). The library
  /// reports nothing else while it renames or removes a cell (layout-undo
  /// "Non-recorded operations"); undo::DbRecorder clears the history when it
  /// holds events of `def`.
  virtual void cell_removed(CellDef& /*def*/) {}
};

/// Detaches a library's recorder for its lifetime, for operations the
/// database never records (layout-undo "Non-recorded operations": cell
/// rename and delete in CellLibrary, `.mag` reads in the cell area).
class ScopedRecorderDetach {
 public:
  explicit ScopedRecorderDetach(CellLibrary& library) noexcept : library_(library), saved_(library.recorder()) {
    library_.set_recorder(nullptr);
  }
  ScopedRecorderDetach(const ScopedRecorderDetach&) = delete;
  ScopedRecorderDetach& operator=(const ScopedRecorderDetach&) = delete;
  ~ScopedRecorderDetach() { library_.set_recorder(saved_); }

 private:
  CellLibrary& library_;
  ChangeRecorder* saved_;
};

/// A recorder that ignores every call: the base for recorders interested
/// in a few kinds (tests, statistics).
class NullChangeRecorder : public ChangeRecorder {
 public:
  void tile_changed(CellDef&, const TileChange&) override {}
  void split_changed(CellDef&, const SplitChange&) override {}
  void label_put(CellDef&, const Label&) override {}
  void label_erased(CellDef&, const Label&) override {}
  void use_placed(CellDef&, const CellUse&) override {}
  void use_deleted(CellDef&, const CellUse&) override {}
  void use_id_cleared(CellDef&, const CellUse&, std::string_view) override {}
  void use_id_set(CellDef&, const CellUse&, std::string_view) override {}
  void use_locked(CellDef&, const CellUse&, bool) override {}
  void modified_changed(CellDef&, bool) override {}
  void property_changed(CellDef&, std::string_view, const std::optional<PropertyValue>&,
                        const std::optional<PropertyValue>&) override {}
};

}  // namespace hefesto::db
