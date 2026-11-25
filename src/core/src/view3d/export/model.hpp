// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The exported model shared by the glTF and STL writers (design D14): the
// exported layers and, per layer, its triangles in µm relative to the
// model origin (hefesto/view3d/export/export.hpp describes the geometry).
//
// Owner: EXPORT (design D3).

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/view3d/export/export.hpp"

namespace hefesto::view3d::detail {

/// Triangles of one layer: three floats per vertex (x, y, z with z up),
/// one normal per vertex (equal for the vertices of a face), three indices
/// per triangle, counter-clockwise seen from outside.
struct LayerGeometry {
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<std::uint32_t> indices;

  [[nodiscard]] std::size_t vertex_count() const noexcept { return positions.size() / 3; }
  [[nodiscard]] std::size_t triangle_count() const noexcept { return indices.size() / 3; }
};

struct ExportModel {
  std::vector<ExportLayer> layers;
  std::vector<LayerGeometry> geometry;  // parallel to `layers`
  double origin_x_um = 0.0;             // the region's lower-left corner in µm
  double origin_y_um = 0.0;
  double zscale = 1.0;
};

/// µm of a height or thickness as the view draws it (MeshModel and
/// own-height values): layout-layer mode and mask-layer mode with
/// magic-compat OFF are in internal units; mask-layer mode with it ON
/// holds the `render` values (µm) as stored (Q-MISC-72).
[[nodiscard]] double draw_to_um(const LayerStack& stack, float value);

/// The layers, with warnings to `sink`, and their geometry.
[[nodiscard]] ExportModel build_export_model(const MeshModel& mesh, const LayerStack& stack,
                                             const ExportOptions& options, diag::DiagnosticSink& sink);

/// The layers' triangles (export_layers already chosen).
[[nodiscard]] std::vector<LayerGeometry> build_geometry(const MeshModel& mesh, const LayerStack& stack,
                                                        const ExportOptions& options,
                                                        std::span<const ExportLayer> layers);

/// A name made safe for a JSON string or a file name: characters outside
/// printable ASCII other than UTF-8 bytes, quotes, backslashes and, for
/// files, path separators become '_'.
[[nodiscard]] std::string safe_json_text(std::string_view text);
[[nodiscard]] std::string safe_file_part(std::string_view text);

/// Little-endian appenders (the byte order of glTF buffers and STL).
void put_u16(std::vector<std::byte>& out, std::uint16_t value);
void put_u32(std::vector<std::byte>& out, std::uint32_t value);
void put_f32(std::vector<std::byte>& out, float value);

}  // namespace hefesto::view3d::detail
