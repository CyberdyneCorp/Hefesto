// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The workloads of hefesto-bench-view3d (design D17, task 13.1): a layer
// stack and a rendered cell for the layout-layer mesh builder.
//
//   synthetic  a unit cell of 10 x 10 slots, each a rectangle or an L (two
//              tiles) on one of ndiff, poly, metal1 and metal2, arrayed with
//              gaps until the region holds at least --rects rectangles;
//              heights and thicknesses are sky130's (diff, poly, metal1,
//              metal2), set by hand, so it needs no PDK.
//   mask       the synthetic region with an output style (one mask layer
//              per type with sky130's `render` values, and a grown NWELL),
//              built in mask-layer mode: mask::generate_flat over the
//              bounding box, then the mask-layer mesh (task 13.3).
//   array      the 100 x 100 array of sky130_fd_sc_hd__dfxtp_1 (the M2 and
//              DRC benchmark array), under sky130A.
//   block      every sky130_fd_sc_hd cell placed once, in rows of 40,000
//              internal units, abutting at their bounding boxes (as the DRC
//              bench's block), under sky130A.
//
// The sky130 workloads load the pinned sky130A data set (--pdk-root) and the
// display styles of its styletype (`mos`) from the --style-dir directories
// (Magic's scmos/ and graphics/), so the stack has
// one layer per (type, display style), as the 3D view builds it.

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::bench_view3d {

struct WorkloadOptions {
  std::string name = "synthetic";       // synthetic | mask | array | block
  std::uint64_t rects = 1'000'000;      // synthetic, mask: minimum layout rectangles
  std::filesystem::path pdk_root;       // the sky130A variant directory
  std::vector<std::filesystem::path> style_dirs;  // where mos.OpenGL.dstyle, its cmap and color.glyphs are
};

/// A loaded workload: the stack, the rendered cell, and whatever owns them.
class Workload {
 public:
  virtual ~Workload() = default;
  [[nodiscard]] virtual const view3d::LayerStack& stack() const = 0;
  [[nodiscard]] virtual const db::CellDef& cell() const = 0;
  /// A line describing the workload for the report.
  [[nodiscard]] virtual std::string description() const = 0;
  /// The mask-layer source (build_from_mask), or null for the layout layers.
  [[nodiscard]] virtual const view3d::MaskSource* mask_source() const { return nullptr; }
};

/// Loads the named workload, or returns nullptr with `error` set.
[[nodiscard]] std::unique_ptr<Workload> load_workload(const WorkloadOptions& options, std::string& error);

}  // namespace hefesto::bench_view3d
