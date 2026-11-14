// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask generation error reporting (capability mask-output-styles, "Mask
// generation error reporting"; CIFError): a reportable problem met while
// generating a mask layer (no room for contact cuts, a tile inverted by a
// shrink) becomes a feedback area on the cell being generated, with the
// text "CIF error in cell <cell>, layer <layer>: <message>", unless the
// style's `no-errors` option is set or no cell is designated for errors.
// The operator kernels (OPS) and the generators (GEN) report through one
// MaskErrorReporter per generation.
//
// (Magic ref: cif/CIFmain.c:418-437 CIFError, dbwind/DBWfdback.c
// DBWFeedbackAdd)
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/view/feedback.hpp"

namespace hefesto::mask {

/// Where mask generation errors go: Magic's CIFErrorDef (the cell, or none)
/// and the feedback list with the windows that find its displayed root.
struct MaskErrorTarget {
  const db::CellDef* cell = nullptr;  // CIFErrorDef; nullptr = errors are ignored
  view::FeedbackList* feedback = nullptr;
  const view::WindowManager* windows = nullptr;
  view::HighlightLayer* highlights = nullptr;
};

class MaskErrorReporter {
 public:
  MaskErrorReporter(const OutputStyle& style, MaskErrorTarget target) : style_(style), target_(target) {}

  /// The layer being generated (CIFErrorLayer).
  void set_layer(std::size_t layer) noexcept { layer_ = layer; }

  /// CIFError: `area` is in mask units of the style (scaled coordinates
  /// with factor scale_factor). Returns true when a feedback area was
  /// added.
  bool report(const db::Rect& area, std::string_view message);

  /// The text CIFError gives `message` on the current cell and layer.
  [[nodiscard]] std::string text(std::string_view message) const;

  /// Errors added as feedback, and errors dropped (`no-errors`, no cell, or
  /// no displayed root), since construction.
  [[nodiscard]] std::size_t reported() const noexcept { return reported_; }
  [[nodiscard]] std::size_t suppressed() const noexcept { return suppressed_; }

 private:
  const OutputStyle& style_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  MaskErrorTarget target_;
  std::size_t layer_ = 0;
  std::size_t reported_ = 0;
  std::size_t suppressed_ = 0;
};

}  // namespace hefesto::mask
