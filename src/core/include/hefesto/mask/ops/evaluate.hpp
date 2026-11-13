// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layer evaluation (capability mask-output-operators "Layer evaluation
// pipeline for an area"; openspec/changes/m5-mask-gds-cif/design.md D4,
// D6): evaluate_layer is the CIFGenLayer equivalent. It runs the operators
// of one output (or input) layer in order over an area and returns the
// resulting mask plane. OperatorContext carries the compat settings, the
// cell being generated (bbox, tagged, net and mask-hints read it), the
// scratch pool, the progress (and its cancel token) and the statistics; it
// holds no globals, so one context per caller makes evaluation reentrant.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); OPS fills in
// the implementation in src/core/src/mask/ops/ (design D2).

#pragma once

#include <cstddef>
#include <span>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::mask {

struct OperatorContext {
  compat::CompatSettings compat;
  const db::CellDef* cell = nullptr;          // the cell being generated (bbox, tagged, net, mask-hints)
  PlanePool* pool = nullptr;                  // scratch planes; required
  const Progress* progress = nullptr;         // cancel token and reporting; optional
  GenerationStatistics* statistics = nullptr; // optional
  diag::DiagnosticSink* sink = nullptr;       // generation errors; optional
};

/// What the operators of a layer read: the layout planes of the (flattened)
/// cell, and the planes of the style's layers evaluated so far, indexed by
/// layer (nullptr for a layer not evaluated).
struct LayerInputs {
  const db::CellDef* layout = nullptr;
  std::span<MaskPlane* const> layers;
};

/// CIFGenLayer: evaluates layer `layer` of `style` over `area` (internal
/// units) into a plane from ctx.pool, which the caller releases.
[[nodiscard]] MaskPlane& evaluate_layer(const OutputStyle& style, std::size_t layer, const LayerInputs& inputs,
                                        const db::Rect& area, OperatorContext& ctx);

}  // namespace hefesto::mask
