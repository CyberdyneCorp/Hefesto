// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// TEXT output of a GDS structure (capability gds-write "Label layers",
// "Label TEXT records"; openspec/changes/m5-mask-gds-cif/design.md D12).
// The structure body writer calls:
// - emit_label_layer for each written `labellayer` mask layer, in style
//   order among the other mask layers, whatever `gds labels` says: one TEXT
//   per solid non-split tile at the tile's centre (calmaPaintLabelFunc);
// - emit_labels after the last mask layer, only when `gds labels` is yes:
//   the plain labels in label-list order, then the port labels by port
//   index (Q-MASK-205, our own stable merge sort over the reversed list),
//   with the port-layer rule of Q-MASK-206, the MAG rule of Q-MASK-221 and
//   the font-size formulas of Q-MASK-122 (calmaWriteLabelFunc).
// Both write through the structure's ElementWriter (its string_record never
// changes the label text, Q-MASK-208) and return its first error.
//
// Owner: GDSW (design D2): src/core/src/gds/write/labels.cpp (task 10.5),
// which until then writes nothing.

#pragma once

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/gds/write/emit.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::gds {

/// "Label layers": the TEXT elements of `plane`, the generated plane of
/// the label layer `layer` (its name is the STRING, its `calma` numbers
/// the LAYER and TEXTTYPE).
[[nodiscard]] diag::Status emit_label_layer(const mask::MaskPlane& plane, const mask::OutputLayer& layer,
                                            ElementWriter& out);

/// "Label TEXT records": the labels of `cell` (out.cell()).
[[nodiscard]] diag::Status emit_labels(const db::CellDef& cell, ElementWriter& out);

}  // namespace hefesto::gds
