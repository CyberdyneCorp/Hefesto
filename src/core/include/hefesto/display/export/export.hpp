// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// View export to SVG and PNG (capability hefesto-rendering "View export to
// SVG and PNG"; plot-output "SVG plot"; D-37, D-38, J1, Q-MISC-52;
// openspec/changes/m10-plot-route/design.md D4, D10, "Area contracts"
// EXPORT). Both writers take the LayoutScene that display::build_scene
// builds for a window and draw the whole window as shown. SVG is written
// by Hefesto itself with integer pixel coordinates; PNG is drawn by an
// integer-only CPU scan-converter, so neither needs a GPU and both are
// byte-identical across platforms.
//
// Owner: EXPORT.

#pragma once

#include <cstdint>
#include <ostream>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/display/scene.hpp"

namespace hefesto::display {

/// What the export draws besides the scene.
struct ExportOptions {
  int width = 0;   // pixel size of the window's drawing area
  int height = 0;
  std::uint32_t background = 0xffffff;  // 0xRRGGBB
  bool transparent_background = false;  // PNG only
  bool grid = false;  // the window's grid, if shown
  bool box = false;   // the box, if shown
};

/// Writes the scene as an SVG document to `out`.
[[nodiscard]] diag::Status export_svg(const LayoutScene& scene, const ExportOptions& options, std::ostream& out);

/// Writes the scene as a PNG image to `out`.
[[nodiscard]] diag::Status export_png(const LayoutScene& scene, const ExportOptions& options, std::ostream& out);

}  // namespace hefesto::display
