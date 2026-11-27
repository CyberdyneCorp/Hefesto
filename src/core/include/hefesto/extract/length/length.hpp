// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Path length extraction (capability extract-commands "Path length
// extraction"; openspec/changes/m7-extraction/design.md D12): the flood of
// ExtLength.c, ported tile for tile in both magic-compat settings
// (Q-EXTRACT-12, replicate), writing `distance` records.
//
// path_lengths() runs extLength for the root cell of a run: the drivers in
// Magic's hash-table order (extDriverHash, 32 buckets), each driver's labels
// found by name (DBSrLabelLoc), the driver's net yanked flat (one
// DBTreeCopyConnect per label into a scratch cell), the receiver labels
// that touch the yanked tiles (DBTreeSrLabels with TF_LABEL_ATTACH), and
// for each driver label and receiver label the depth-first flood of
// extPathPairDistance. The flood keeps its frames on the heap, so a long
// net cannot overflow the stack, and adds the distances in 64 bits (Magic's
// 32-bit sums are undefined on overflow); both are fixes of crashes or UB.
//
// Owner: INTEGRATION (this header); EXTCMD (the implementation).

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract {

class ExtWriter;

/// A driver or receiver label of the length pass: its full hierarchical
/// name, type and rectangle in root coordinates.
struct PathLabel {
  std::string text;
  tech::TileType type = tech::kSpace;
  db::Rect rect;

  friend bool operator==(const PathLabel&, const PathLabel&) = default;
};

/// The shortest and longest first-arrival path of one driver-receiver pair
/// (Magic's INFINITY and MINFINITY when the flood never reaches it).
struct PathDistance {
  std::int64_t min = db::kInfinity;
  std::int64_t max = db::kMinusInfinity;

  friend bool operator==(const PathDistance&, const PathDistance&) = default;
};

/// The `distance` records of one root.
struct PathRecord {
  std::string driver;
  std::string receiver;
  PathDistance distance;

  friend bool operator==(const PathRecord&, const PathRecord&) = default;
};

/// The drivers of `lists` in extDriverHash's order.
[[nodiscard]] std::vector<std::string> driver_order(const LengthLists& lists);

/// extPathLabel (DBSrLabelLoc): the labels of `root` named `name`
/// (`use/.../text`), last found first, each named `name`. Empty when the
/// instance path is not found or no label has the text.
[[nodiscard]] std::vector<PathLabel> driver_labels(db::CellDef& root, const std::string& name);

/// extLengthYank's result: the scratch cell (Magic's __PATHYANK__) holding
/// the net flat, and the receiver labels touching its tiles.
struct YankedPath {
  std::unique_ptr<db::CellDef> scratch;
  std::vector<PathLabel> receivers;
};

/// extLengthYank: the net of `drivers` (space labels skipped) copied flat
/// into a new scratch cell, and the receiver labels of `lists` touching its
/// tiles, last found first. With magic-compat ON a receiver is listed once
/// per yanked tile whose area (grown by one unit at the top and right) it
/// touches, as in Magic; OFF lists each label once (Q-EXTRACT-42).
[[nodiscard]] YankedPath yank_path(const db::CellDef& root, const tech::Technology& technology,
                                   const std::vector<PathLabel>& drivers, const LengthLists& lists,
                                   bool magic_compat);

/// extPathPairDistance over the yanked net in `scratch`. The tile slots of
/// `scratch` are reset before it returns.
[[nodiscard]] PathDistance pair_distance(const db::CellDef& scratch, const tech::Technology& technology,
                                         const PathLabel& driver, const PathLabel& receiver);

/// extLength without the writer: the records in Magic's order, and the
/// names of the drivers whose labels are not found (in that order). A
/// driver label on space gets no record: Magic prints the previous pair's
/// values, or uninitialised ones for the first pair (a fix in both modes,
/// Q-EXTRACT-43).
struct PathLengthResult {
  std::vector<PathRecord> records;
  std::vector<std::string> missing;
};
[[nodiscard]] PathLengthResult compute_path_lengths(db::CellDef& root, const tech::Technology& technology,
                                                    const LengthLists& lists, bool magic_compat);

/// Writes the driver-receiver distances of `lists` for the root `cell`
/// (`Can't find terminal "<name>"` as EXTRACT-LEN-001 for a driver without
/// a label). Fails with EXTRACT-CMD-001 without a technology.
[[nodiscard]] diag::Status path_lengths(ExtractContext& ctx, db::CellDef& cell, const LengthLists& lists,
                                        ExtWriter& writer);

}  // namespace hefesto::extract
