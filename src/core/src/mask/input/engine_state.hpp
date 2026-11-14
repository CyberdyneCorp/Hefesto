// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// INPUT-private: the state of a ReadEngine, shared by engine.cpp (the
// ReadSink operations, cells, calls, labels), scale.cpp (CIFScaleCoord, the
// grid subdivision, CIFInputRescale) and current.cpp (CIFPaintCurrent).
//
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/input/engine.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::mask {

struct ReadEngine::State {
  InputStyle style;
  Options options;
  PlanePool pool;
  std::vector<MaskPlane*> top;    // cifEditCellPlanes
  std::vector<MaskPlane*> cell;   // cifSubcellPlanes
  db::CellDef* reading = nullptr; // cifReadCellDef while a cell is read (nullptr: top level)
  /// The saved copyup planes of each cell (CDFLATGDS / cd_client), indexed
  /// by CIF layer.
  std::unordered_map<const db::CellDef*, std::vector<MaskPlane*>> saved;
  bool snap_warned = false;       // cifSeenSnapWarning
  bool euclidean_noted = false;   // Q-MASK-306 note given
  bool tagged_noted = false;      // Q-MASK-310 note given
  /// The rules as an OutputStyle that evaluate_layer runs: one placeholder
  /// layer per CIF layer (index = CIF layer), then one layer per rule.
  OutputStyle evaluation;
  bool evaluation_valid = false;

  void emit(diag::Diagnostic d) const {
    if (options.sink != nullptr) {
      options.sink->emit(std::move(d));
    }
  }
};

namespace input_detail {

/// The evaluation style of `style` for the session's output style
/// `output` (nullptr when none) and `compat` (Q-MASK-306, 307, 310).
[[nodiscard]] OutputStyle evaluation_style(const InputStyle& style, const OutputStyle* output,
                                           const tech::Technology& technology, const compat::CompatSettings& compat);

/// dbScalePlane: every tile of `plane` scaled by n/d (DBScaleValue).
void scale_plane(MaskPlane& plane, std::int64_t n, std::int64_t d, bool magic_compat);

/// Paints one tile view (rectangle or reported triangle) of a mask plane at
/// `area` into `into`.
void paint_tile(MaskPlane& into, const db::TileView& tile, const db::Rect& area, bool magic_compat);

}  // namespace input_detail

}  // namespace hefesto::mask
