// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Instances, arrays, bounding boxes and flattening (capability
// layout-hierarchy; label search across the hierarchy from
// layout-labels-ports "Hierarchical label search").
// (Magic ref: database/DBcell.c, database/DBcellbox.c, database/DBcellname.c,
// database/DBcellcopy.c, commands/CmdFI.c CmdFlatten)
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::db {

struct PlaceOptions {
  std::optional<std::string> id;  // explicit identifier; otherwise "Instance identifier assignment"
  Transform transform;
  ArrayInfo array;
  bool locked = false;
};

/// Places `child` in `parent` (DBCellNewUse + DBLinkCell + DBPlaceCell).
/// Rejects recursion (DB-USE-001) and a taken explicit id (DB-USE-002).
/// Computes the use's boxes from the child's current boxes and marks the
/// parent modified and needing a new timestamp. The parent's own boxes are
/// not recomputed (call update_bounding_boxes()).
[[nodiscard]] diag::Outcome<CellUse*> place(CellDef& parent, CellDef& child, const PlaceOptions& options);

/// Removes `use` from its parent (marks the parent modified). `use` is
/// destroyed.
[[nodiscard]] diag::Status remove_use(CellUse& use);

/// DBFindUse: the use of `parent` whose id is `id`, ignoring a trailing
/// array subscript (then retrying the full string); nullptr if none.
[[nodiscard]] CellUse* find_use(const CellDef& parent, std::string_view id);

/// DBReLinkCell: renames a use (no-op for its own name, DB-USE-002 when
/// taken); marks the parent modified (but not needing a new timestamp).
[[nodiscard]] diag::Status rename_use(CellUse& use, std::string_view new_id);

/// Locks or unlocks a use; marks the parent modified (Q-MODEL-13, fix).
void set_use_locked(CellUse& use, bool locked);

/// The M1-M4 subcell enumeration order: the reverse of the cell's use
/// list (newest first), in both magic-compat modes. It is Magic's
/// DBCellEnum order until a partial-area search over 10 or more uses bins
/// the cell's BPlane, and the magic-compat OFF canonical order. Callers
/// move to db::uses_in_search_order(def, compat) (db/use_index.hpp), which
/// follows the bin walk with magic-compat ON, as task 5.2 migrates them.
/// (Magic ref: bplane/bpMain.c, bplane/bpEnum.c.)
[[nodiscard]] std::vector<CellUse*> uses_in_search_order(const CellDef& def);

/// DBGenerateUniqueIds: after reading a cell, the first use with a given id
/// keeps it, later duplicates lose theirs, and every use without an id gets
/// `<child name>_<k>` (Q-MODEL-8: the part after the last `/` when
/// `magic_compat` is false). Returns the number of ids assigned.
std::size_t make_use_ids_unique(CellDef& parent, bool magic_compat = true);

/// "Array element naming": the name of element (x, y), e.g. "X1[2,0]".
[[nodiscard]] std::string array_element_name(const CellUse& use, std::int32_t x, std::int32_t y);

/// "Arrays": the transform from the child of element (x, y) to the parent.
[[nodiscard]] Transform element_transform(const CellUse& use, std::int32_t x, std::int32_t y);

/// DBMakeArray: sets the array of `use` from root-coordinate parameters
/// through the root-to-child rotation `root_to_child`, then recomputes the
/// use's boxes. Marks the parent modified.
void make_array(CellUse& use, const Transform& root_to_child, const ArrayInfo& root_array);

/// DBComputeUseBbox: the use's bbox and extended bbox from its child.
void compute_use_bbox(CellUse& use);

/// Recomputes `def`'s bbox and extended bbox from paint, labels and uses
/// ("Bounding box and extended bounding box"), then repositions its uses in
/// every parent and propagates upward (parents are flagged
/// child_bboxes_changed, not modified). `magic_compat` false counts only
/// the paint planes (Q-MODEL-19). Outline-font label text boxes come from
/// Label::text_bbox.
void recompute_bbox(CellDef& def, bool magic_compat = true);

/// Same as recompute_bbox(def) (kept for callers holding a technology).
void update_bounding_boxes(CellDef& def, const tech::Technology& tech);

/// "Descend rules" used by flattening.
enum class Descend : std::uint8_t { all, no_subcircuit, no_vendor, property_flatten };

/// Whether hierarchical operations in `mode` descend into `use`.
[[nodiscard]] bool descends(const CellUse& use, Descend mode);

/// Whether hierarchical operations in `mode` descend into a use of `def`.
[[nodiscard]] bool descends(const CellDef& def, Descend mode);

/// Which labels flattening copies: `all` (flatCopyAllLabels: every
/// descended label, prefixed with its path, rotation and offset copied
/// untransformed in magic-compat, Q-MODEL-20), `top_only` (DBCellCopyAllLabels
/// of `-dotoplabels`: only the source's own labels, rotation and offset
/// transformed, duplicates replaced) or `none`.
enum class FlattenLabels : std::uint8_t { all, top_only, none };

/// DBFontLabelSetBBox for a label being written: fills `text_bbox` of an
/// outline-font label (nullopt for the default font) and normalises its
/// rotation into [0, 360) as Magic stores it; flattening runs it on every
/// label it writes.
using FontLabelUpdate = std::function<void(Label&)>;

struct FlattenOptions {
  Descend descend = Descend::all;
  FlattenLabels labels = FlattenLabels::all;
  bool magic_compat = true;  // Q-MODEL-7, Q-MODEL-20
  FontLabelUpdate font_update;  // empty: copied labels get no text box
};

/// "Flattening into a new cell": a copy of `source` with every descended
/// level of hierarchy flattened, created in `library` under `new_name`
/// (DB-CELL-001 if it exists). Non-descended uses are kept as instances.
/// With `magic_compat`, a `source` that fails the descend test itself gives
/// an empty cell, except for the labels of FlattenLabels::top_only
/// (Q-MODEL-33).
[[nodiscard]] diag::Outcome<CellDef*> flatten_to_new(CellLibrary& library, const CellDef& source,
                                                     std::string_view new_name, const tech::Technology& tech,
                                                     const FlattenOptions& options = {});

/// "Flattening an instance in place" (DBFlattenInPlace with doclear). `use`
/// is destroyed on success.
[[nodiscard]] diag::Status flatten_in_place(CellUse& use, const tech::Technology& tech,
                                            const FlattenOptions& options = {});

/// Depth-first walk of the instance tree under `root`: every array element
/// of every use, in use order and element order (y then x ascending), with
/// the element's accumulated transform to root coordinates. Return false to
/// skip the subtree.
using UseVisitor = std::function<bool(const CellUse& use, const Transform& to_root, int depth)>;
void walk_hierarchy(const CellDef& root, const UseVisitor& visit);

/// One label found by search_labels().
struct LabelHit {
  const Label* label = nullptr;
  std::string path;  // element names followed by '/', empty at the root
  Transform to_root;
};

/// What a label search compares with the area: the attachment rectangle
/// (connectivity, attachment), or for display also the rendered text box of
/// an outline-font label (font-labels "Font labels in bounding boxes and
/// searches"; Magic's TF_LABEL_DISPLAY).
enum class LabelMatch : std::uint8_t { attach, display };

/// "Hierarchical label search": labels of `root` touching `area`, then the
/// labels of descended subcells overlapping `area` expanded by one unit,
/// each subcell recursively. With LabelMatch::display a font label is also
/// found when its `text_bbox` touches (root) or overlaps (subcells) the
/// area. Return false from the visitor to stop.
void search_labels(const CellDef& root, const Rect& area, const std::function<bool(const LabelHit&)>& visit,
                   Descend descend = Descend::all, LabelMatch match = LabelMatch::attach);

}  // namespace hefesto::db
