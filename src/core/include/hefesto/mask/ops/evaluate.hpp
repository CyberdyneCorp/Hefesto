// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layer evaluation (capability mask-output-operators "Layer evaluation
// pipeline for an area", "Hierarchical-mode truncation";
// openspec/changes/m5-mask-gds-cif/design.md D4, D6): evaluate_layer is the
// CIFGenLayer equivalent. It runs the operators of one output (or input)
// layer in order over an area and returns the resulting mask plane.
// evaluate_area is CIFGen's per-area part: every requested layer in style
// order over the area grown by the halo, each clipped to the area in mask
// units afterwards. OperatorContext carries the compat settings, the cell
// being generated (bbox and boundary read it), the scratch pool, the
// progress (and its cancel token) and the statistics; it holds no globals,
// so one context per caller makes evaluation reentrant.
//
// Who is who (Magic's names in parentheses):
//   LayerInputs::layout   the cell whose paint is searched (cellDef: the yank
//                         or flattened cell); its labels feed `tagged`, its
//                         MASKHINTS_* properties feed `mask-hints`
//   OperatorContext::cell the cell output is generated for (origDef and
//                         CIFErrorDef): `bbox`, `boundary`, `net` and the
//                         generation errors use it; nullptr adds nothing
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); OPS fills in
// the implementation in src/core/src/mask/ops/ (design D2).

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::mask {

/// Database tiles that stay marked after a bloat-all or tagged operator
/// under magic-compat ON (Q-MASK-230: case C triggers on a plane other than
/// the bloat plane keep Magic's client mark until the cell's generation pass
/// ends). Keyed by the tile identity of db::TileView::tile, which is stable
/// while the searched cell is not edited. Clear it (or use a new context)
/// for every generation pass of a cell.
struct StaleMarks {
  /// A marked tile: its plane and its side types (for Magic's masked
  /// resets).
  struct Entry {
    const db::Plane* plane = nullptr;
    tech::TileType left = tech::kSpace;
    tech::TileType right = tech::kSpace;
  };
  std::unordered_map<const void*, Entry> tiles;

  void clear() noexcept { tiles.clear(); }
};

struct OperatorContext {
  compat::CompatSettings compat;
  const db::CellDef* cell = nullptr;          // the cell being generated (bbox, boundary, net, errors)
  PlanePool* pool = nullptr;                  // scratch planes; required
  const Progress* progress = nullptr;         // cancel token and reporting; optional
  GenerationStatistics* statistics = nullptr; // optional
  diag::DiagnosticSink* sink = nullptr;       // generation errors; optional
  /// CIFGenLayer's `hier`: called from hierarchical processing (interaction
  /// areas and arrays), which truncates the operator list
  /// ("Hierarchical-mode truncation").
  bool hierarchical = false;
  /// Marks kept across operators of one generation pass (Q-MASK-230 ON).
  StaleMarks stale_marks{};
};

/// What the operators of a layer read: the layout planes of the (flattened)
/// cell, and the planes of the style's layers evaluated so far, indexed by
/// layer (nullptr for a layer not evaluated). `technology` maps the
/// operators' layout types to the planes searched (DBPlaneTypes); without it
/// every plane of `layout` is searched for every type.
struct LayerInputs {
  const db::CellDef* layout = nullptr;
  std::span<MaskPlane* const> layers;
  const tech::Technology* technology = nullptr;
};

/// CIFGenLayer: evaluates layer `layer` of `style` over `area` (internal
/// units, already grown by the halo: only layout tiles overlapping it are
/// read, each in full) into a plane from ctx.pool, which the caller
/// releases. The result is not clipped. In hierarchical mode
/// (ctx.hierarchical) the operator list stops where hierarchical_stop()
/// says. An operator that fails reports to ctx.sink and evaluation goes on
/// with the next one, as Magic's ignored operators do; a cancelled progress
/// stops after the current operator.
[[nodiscard]] MaskPlane& evaluate_layer(const OutputStyle& style, std::size_t layer, const LayerInputs& inputs,
                                        const db::Rect& area, OperatorContext& ctx);

/// "Hierarchical-mode truncation" (Q-MASK-26, replicate): the index of the
/// first operator that hierarchical evaluation does not run (`net`,
/// `tagged`, `boundary`, `bbox`, and `squares`, `squares-grid` or `slots`
/// not immediately followed by `grow`); nullopt when every operator runs.
[[nodiscard]] std::optional<std::size_t> hierarchical_stop(std::span<const LayerOp> ops) noexcept;

/// cifGenClip: the area to search (`expanded`, internal units: `area`
/// grown by `radius` on each finite side) and the clip area (`clip`, mask
/// units: `area` times the scale factor on each finite side). A side at or
/// beyond the plane's edge stays where it is in both.
struct GenerationClip {
  db::Rect expanded;
  db::Rect clip;
};
[[nodiscard]] GenerationClip generation_clip(const db::Rect& area, std::int32_t scale_factor,
                                             std::int64_t radius) noexcept;

/// Options of evaluate_area (CIFGen's arguments).
struct AreaOptions {
  /// Make an empty plane for each layer not requested (genAllPlanes); else
  /// that layer's entry is nullptr.
  bool all_planes = false;
  /// The halo radius in internal units; nullopt = style.halo().
  std::optional<std::int64_t> radius;
};

/// CIFGen's per-area part ("Layer evaluation pipeline for an area"):
/// evaluates the layers whose `requested` entry is true (missing entries are
/// false) in declaration order over `area` grown by the halo, each layer's
/// operators reading the unclipped results of the layers before it, then
/// clips every result to `area` times the scale factor. The planes come
/// from ctx.pool; the caller releases them. `inputs.layers` is ignored (the
/// results of this evaluation are the layers read).
[[nodiscard]] std::vector<MaskPlane*> evaluate_area(const OutputStyle& style, const std::vector<bool>& requested,
                                                    const LayerInputs& inputs, const db::Rect& area,
                                                    OperatorContext& ctx, const AreaOptions& options = {});

}  // namespace hefesto::mask
