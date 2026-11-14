// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell database: definitions, instances and the global cell namespace
// (capability layout-hierarchy, requirements "Cell definitions and the cell
// namespace", "Cell instances", "Instance identifier assignment", "Arrays",
// "Bounding box and extended bounding box", "Internal cells").
// (Magic ref: database/database.h.in:338-537, database/DBcellname.c)
//
// Ownership: CellLibrary owns every CellDef; a CellDef owns its CellUses.
// Pointers stay valid until the def/use is deleted (stable addresses).
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::db {

class CellDef;
class ChangeRecorder;  // db/change_recorder.hpp
class UseIndex;        // db/use_index.hpp

/// Observable definition flags ("Cell definitions and the cell namespace").
struct CellFlags {
  bool available = false;   // contents loaded
  bool modified = false;
  bool not_found = false;   // a load failed once
  bool internal = false;
  bool needs_new_timestamp = false;
  bool child_timestamps_changed = false;
  bool child_bboxes_changed = false;
  bool fixed_bbox = false;
  bool no_edit = false;     // read-only or locked by another session
  bool vendor_gds = false;
  bool dereference = false;
  bool fixed_timestamp = false;
  bool bbox_processed = false;  // CDPROCESSED: handled by the last timestamp-mismatch pass

  friend bool operator==(const CellFlags&, const CellFlags&) = default;
};

/// Array parameters of a use ("Arrays").
struct ArrayInfo {
  std::int32_t xlo = 0;
  std::int32_t xhi = 0;
  std::int32_t ylo = 0;
  std::int32_t yhi = 0;
  Coord xsep = 0;
  Coord ysep = 0;

  [[nodiscard]] bool is_array() const noexcept { return xlo != xhi || ylo != yhi; }
  friend bool operator==(const ArrayInfo&, const ArrayInfo&) = default;
};

/// One instance of a child definition inside a parent.
struct CellUse {
  std::string id;              // unique within the parent ("Instance identifier assignment")
  CellDef* def = nullptr;      // the child definition
  CellDef* parent = nullptr;
  Transform transform;         // child -> parent coordinates
  ArrayInfo array;
  bool locked = false;
  Rect bbox;                   // in parent coordinates, whole array
  Rect extended;               // extended bbox in parent coordinates, whole array
  /// Creation order (db::next_use_serial), so the age of a use can be
  /// compared with a window load (db::recompute_bbox).
  std::uint64_t serial = 0;
};

/// A persistent display element kept with a cell (cell-file-format
/// "Element records", "Writer elements"). Stored and written back by CELL;
/// drawing them is a display concern. Coordinates are internal units; a
/// text element's anchor is (area.x0, area.y0).
struct Element {
  enum class Kind : std::uint8_t { rectangle, line, text };
  Kind kind = Kind::rectangle;
  std::string name;
  std::vector<std::string> styles;  // display-style long names, in order
  Rect area;
  std::string text;                 // text elements only
  bool persistent = true;           // temporary elements are never written
  bool half_x = false;              // line flags
  bool half_y = false;
  bool arrow_left = false;
  bool arrow_right = false;
  std::uint8_t text_position = 0;   // GEO position (0 = center, 1 = north, ...)
  std::uint8_t text_size = 1;       // small, medium, large, xlarge, default

  friend bool operator==(const Element&, const Element&) = default;
};

class CellDef {
 public:
  CellDef(std::string name, std::size_t plane_count);
  ~CellDef();
  CellDef(const CellDef&) = delete;
  CellDef& operator=(const CellDef&) = delete;

  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] CellFlags& flags() noexcept { return flags_; }
  [[nodiscard]] const CellFlags& flags() const noexcept { return flags_; }

  /// File the definition was read from or will be written to (cell-library-resolution).
  [[nodiscard]] const std::filesystem::path& file_path() const noexcept { return file_path_; }
  void set_file_path(std::filesystem::path path) { file_path_ = std::move(path); }

  /// Timestamp (cell-timestamps); 0 = none.
  [[nodiscard]] std::int64_t timestamp() const noexcept { return timestamp_; }
  void set_timestamp(std::int64_t stamp) noexcept { timestamp_ = stamp; }

  /// Paint planes, indexed by tech::PlaneId (built-in planes included).
  [[nodiscard]] std::size_t plane_count() const noexcept { return planes_.size(); }
  [[nodiscard]] Plane& plane(tech::PlaneId id) { return planes_.at(id); }
  [[nodiscard]] const Plane& plane(tech::PlaneId id) const { return planes_.at(id); }
  /// A technology reload with a different plane count (TechLoad's
  /// saveNumPlanes handling): planes beyond `count` are dropped with their
  /// paint, new planes start empty; planes below both counts are kept.
  void set_plane_count(std::size_t count) { planes_.resize(count); }

  /// Labels in creation order (the `.mag` output order).
  [[nodiscard]] std::vector<Label>& labels() noexcept { return labels_; }
  [[nodiscard]] const std::vector<Label>& labels() const noexcept { return labels_; }

  [[nodiscard]] PropertyStore& properties() noexcept { return properties_; }
  [[nodiscard]] const PropertyStore& properties() const noexcept { return properties_; }

  /// Persistent display elements in read/creation order (names unique).
  [[nodiscard]] std::vector<Element>& elements() noexcept { return elements_; }
  [[nodiscard]] const std::vector<Element>& elements() const noexcept { return elements_; }

  /// Child instances in creation order (the `.mag` writer sorts them).
  /// Taking the list for writing tells the use index to check it against
  /// the list on its next read (db/use_index.hpp).
  [[nodiscard]] const std::vector<std::unique_ptr<CellUse>>& uses() const noexcept { return uses_; }
  [[nodiscard]] std::vector<std::unique_ptr<CellUse>>& uses() noexcept {
    uses_touched_.store(true, std::memory_order_relaxed);
    return uses_;
  }

  /// The cell-use index of this definition (Magic's cd_cellPlane): the
  /// enumeration and area-search order of its uses (db/use_index.hpp).
  /// Searches may rebuild its bins, so it is reachable from a const
  /// definition; it guards itself with its own mutex.
  [[nodiscard]] UseIndex& use_index() const noexcept { return *use_index_; }
  /// Whether uses() was taken for writing since the last call (used by the
  /// use index; clears the flag).
  [[nodiscard]] bool take_uses_touched() const noexcept {
    return uses_touched_.exchange(false, std::memory_order_relaxed);
  }

  /// Every use of this definition, in any parent (Magic's cd_parents list:
  /// the most recently placed first). Maintained by place()/remove_use().
  [[nodiscard]] const std::vector<CellUse*>& instances() const noexcept { return instances_; }
  [[nodiscard]] std::vector<CellUse*>& instances() noexcept { return instances_; }

  /// Bounding box and extended bounding box (labels included); a new
  /// definition has (0,0)-(1,1) for both.
  [[nodiscard]] const Rect& bbox() const noexcept { return bbox_; }
  [[nodiscard]] const Rect& extended_bbox() const noexcept { return extended_bbox_; }
  void set_bboxes(const Rect& bbox, const Rect& extended) noexcept {
    bbox_ = bbox;
    extended_bbox_ = extended;
  }

  /// The change recorder of the library that owns this definition
  /// (CellLibrary::set_recorder); nullptr when none is installed or the
  /// definition belongs to no library (e.g. the selection's internal cell).
  /// The db primitives report each change to it (db/change_recorder.hpp).
  [[nodiscard]] ChangeRecorder* recorder() const noexcept {
    return recorder_slot_ != nullptr ? *recorder_slot_ : nullptr;
  }

  /// When the definition was first shown as a window's root
  /// (db::next_use_serial; 0 = never). Magic creates a root use for every
  /// window load and never frees it, so that use stays at the end of
  /// cd_parents (db::recompute_bbox).
  [[nodiscard]] std::uint64_t first_window_serial() const noexcept { return first_window_serial_; }
  void note_window_root() noexcept;

  /// Magic's cd_idHash: the instance identifiers linked in this definition,
  /// in Magic's string-hash order (16 buckets at creation). A linked id
  /// (DBSetUseIdHash) keeps its entry when it is unlinked (DBUnLinkCell
  /// clears the value), so the order depends on the whole link history.
  /// `cellname list childinst` reads it with magic-compat ON (Q-CELL-46).
  /// Maintained by db::place, db::remove_use, db::rename_use and
  /// db::make_use_ids_unique; reset by clear_use_ids (DBCellClearDef).
  void link_use_id(const std::string& id) { use_ids_.insert(id, true); }
  void unlink_use_id(const std::string& id) { (void)use_ids_.clear_value(id); }
  void clear_use_ids() { use_ids_ = util::MagicHashTable<bool>{kUseIdBuckets}; }
  /// The linked identifiers in hash-table order.
  [[nodiscard]] std::vector<std::string> linked_use_ids() const;

 private:
  friend class CellLibrary;  // renames definitions, links the recorder

  std::string name_;
  CellFlags flags_;
  std::filesystem::path file_path_;
  std::int64_t timestamp_ = 0;
  std::vector<Plane> planes_;
  std::vector<Label> labels_;
  PropertyStore properties_;
  std::vector<Element> elements_;
  std::vector<std::unique_ptr<CellUse>> uses_;
  mutable std::atomic<bool> uses_touched_{false};
  std::unique_ptr<UseIndex> use_index_;  // created by the constructor, never null
  std::vector<CellUse*> instances_;
  Rect bbox_{0, 0, 1, 1};
  Rect extended_bbox_{0, 0, 1, 1};
  ChangeRecorder* const* recorder_slot_ = nullptr;  // CellLibrary::Impl::recorder
  std::uint64_t first_window_serial_ = 0;
  static constexpr std::size_t kUseIdBuckets = 16;  // DBCellNewDef: HashInit(&cd_idHash, 16, ...)
  util::MagicHashTable<bool> use_ids_{kUseIdBuckets};
};

/// The next value of the process-wide creation counter shared by cell uses
/// and window root uses (thread-safe, starts at 1).
[[nodiscard]] std::uint64_t next_use_serial() noexcept;

/// Marks the definition modified and needing a new timestamp (CDMODIFIED |
/// CDGETNEWSTAMP), as every paint, label and instance change does. A change
/// of the modified flag is reported to the change recorder.
void mark_modified(CellDef& def);

/// Sets only the modified flag (CDMODIFIED, no new timestamp), reporting
/// the change to the change recorder.
void set_modified_flag(CellDef& def);

/// DBPropPut: puts (or with nullopt removes) a property, honoring the
/// no-edit flag (silently ignored) and keeping the fixed-bbox and
/// vendor-GDS flags in step with FIXED_BBOX and GDS_FILE
/// (layout-properties "Typed property store", "Reserved property names").
/// The value is stored as given; see coerce_reserved_property().
void set_property(CellDef& def, std::string_view name, std::optional<PropertyValue> value);

/// Removes every property and clears the fixed-bbox and vendor-GDS flags.
void clear_properties(CellDef& def);

/// "Subcircuit determination": true iff some label is a port.
[[nodiscard]] bool is_subcircuit(const CellDef& def);

/// The global cell namespace ("Cell definitions and the cell namespace").
class CellLibrary {
 public:
  CellLibrary();
  ~CellLibrary();
  CellLibrary(const CellLibrary&) = delete;
  CellLibrary& operator=(const CellLibrary&) = delete;
  CellLibrary(CellLibrary&&) noexcept;
  CellLibrary& operator=(CellLibrary&&) noexcept;

  /// Creates an empty definition; a trailing ".mag" is stripped, an empty
  /// name becomes "(UNNAMED)". Fails (DB- code) when the name exists.
  [[nodiscard]] diag::Outcome<CellDef*> create(std::string_view name, std::size_t plane_count);
  [[nodiscard]] CellDef* find(std::string_view name) const;
  /// "Renaming a cell definition": fails for internal cells (DB-CELL-003),
  /// an existing `new_name` (DB-CELL-001) and vendor-GDS cells unless
  /// `force` (DB-CELL-004; forcing removes GDS_FILE/GDS_START/GDS_END).
  /// Marks every parent and (Q-MODEL-15) the definition itself modified.
  [[nodiscard]] diag::Status rename(CellDef& def, std::string_view new_name, bool force = false);
  /// DBCellRenameDef: only the name changes (no checks besides an existing
  /// name, nothing marked modified); `save` uses it after writing a cell
  /// under a new name.
  [[nodiscard]] diag::Status rename_definition(CellDef& def, std::string_view new_name);
  /// "Deleting a cell definition": fails for internal cells (DB-CELL-003)
  /// and while a non-internal parent instantiates it (DB-CELL-002, naming
  /// the instance); uses in internal parents are removed first. `def` is
  /// destroyed on success.
  [[nodiscard]] diag::Status remove(CellDef& def);
  /// The checks of remove() without removing anything: fails when the
  /// definition is internal or instantiated in a non-internal cell.
  [[nodiscard]] diag::Status check_removable(const CellDef& def) const;
  /// Every definition in Magic's cell hash-table order (observable by
  /// `cellname list`, `writeall` and the save order of golden cases). The
  /// visitor must not create, rename or remove definitions.
  void for_each(const std::function<void(CellDef&)>& visit) const;
  [[nodiscard]] std::size_t size() const noexcept;

  /// Installs the observer every definition of this library reports its
  /// primitive changes to (openspec/changes/m2-interactive-editing/design.md
  /// D4; undo::DbRecorder in a session). Not owned; nullptr (the default)
  /// records nothing at no cost. Definitions created before and after the
  /// call both see the current recorder.
  void set_recorder(ChangeRecorder* recorder) noexcept;
  [[nodiscard]] ChangeRecorder* recorder() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::db
