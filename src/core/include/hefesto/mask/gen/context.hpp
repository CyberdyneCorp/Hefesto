// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GenerationContext (capability mask-hierarchical-generation;
// openspec/changes/m5-mask-gds-cif/design.md D4 "Generation", D8, D14): the
// scratch planes, progress callback, cancel token and statistics of one
// caller. Generation has no globals, so one context per caller (the command
// thread, each DRC worker) makes it reentrant.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GEN fills in
// the implementation in src/core/src/mask/gen/ (design D2).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/ops/evaluate.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/platform/clock.hpp"

namespace hefesto::mask {

/// One generation error as CIFError records it: a feedback entry over
/// `area` (mask units, `scale` mask units per layout unit) in `cell`.
struct GenerationProblem {
  const db::CellDef* cell = nullptr;
  db::Rect area;
  std::int32_t scale = 1;
  std::string text;
};

class GenerationContext {
 public:
  GenerationContext() = default;
  GenerationContext(const GenerationContext&) = delete;
  GenerationContext& operator=(const GenerationContext&) = delete;

  PlanePool pool;                          // reused across generations
  const Progress* progress = nullptr;      // optional
  GenerationStatistics statistics;
  diag::DiagnosticSink* sink = nullptr;    // generation errors; optional
  /// The feedback entries of the generation errors reported with a sink,
  /// in order; the command thread adds them to the editor's feedback
  /// (add_problem_feedback) once the generation is over.
  std::vector<GenerationProblem> problems;
  platform::Clock* clock = nullptr;        // times the progress reports; nullptr = the steady clock

  /// An OperatorContext over this context's pool, progress, statistics and
  /// sink, for generating `cell` under `compat`.
  [[nodiscard]] OperatorContext operator_context(const db::CellDef* cell, const compat::CompatSettings& compat);

  /// True when the host asked to stop.
  [[nodiscard]] bool cancelled() const noexcept { return progress != nullptr && progress->cancelled(); }
};

}  // namespace hefesto::mask
