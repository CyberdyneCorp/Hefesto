// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract_cell: ExtCell's pipeline for one cell (capability
// extract-hierarchy; openspec/changes/m7-extraction/design.md D5, D11):
// header, basic extraction (nodes, caps, devices), subtree interactions,
// arrays and path length. ExtractContext holds everything one run needs
// (style, options, yank buffers), with no globals, so per-cell parallelism
// can be added later (Open Question 8 (a)).
//
// Scaffold state (tasks.md 3.3): extract_cell() and interactions() report
// CORE-NOTIMPL-008 until EXTHIER lands them.
//
// Owner: INTEGRATION (this header); EXTHIER (the implementation; the
// pipeline steps belong to their workstreams).

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::mask {
struct Progress;
}

namespace hefesto::extract {

class ExtWriter;

/// The scratch cells of one run's interaction yanks (ExtYank.c); owned by
/// the context, never registered in the cell table.
struct YankBuffers {
  std::unique_ptr<db::CellDef> cumulative;
  std::unique_ptr<db::CellDef> interaction;
};

/// One run's state.
struct ExtractContext {
  std::shared_ptr<const ExtStyle> style;
  ExtractOptions options;
  Overrides overrides;
  LengthLists length;
  compat::CompatSettings compat;
  const mask::Progress* progress = nullptr;
  YankBuffers yank;
};

/// What extracting one cell reports.
struct CellResult {
  std::int32_t errors = 0;
  std::int32_t warnings = 0;
  bool cancelled = false;
};

/// The interaction areas of a cell (halo and step squares; ExtSubtree.c,
/// ExtInter.c).
struct InteractionAreas {
  std::vector<db::Rect> areas;
};

[[nodiscard]] diag::Outcome<CellResult> extract_cell(ExtractContext& ctx, db::CellDef& cell, ExtWriter& writer,
                                                     bool is_top);

[[nodiscard]] diag::Outcome<InteractionAreas> interactions(const db::CellDef& cell, const ExtStyle& style,
                                                           const Overrides& overrides);

}  // namespace hefesto::extract
