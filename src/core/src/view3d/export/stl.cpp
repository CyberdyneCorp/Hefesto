// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The binary STL writer (design D14): an 80-byte header of fixed text
// naming the layer (`Hefesto STL layer <name>`) or `Hefesto STL merged`,
// padded with spaces, no time; the triangle count; then per triangle its
// normal (from the integer geometry), its three vertices (µm relative to
// the model origin, z up) and a zero attribute word, all little-endian.
// Triangles in layer order, then mesh order.
//
// Owner: EXPORT (design D3).

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "view3d/export/model.hpp"

namespace hefesto::view3d {

namespace {

constexpr std::size_t kHeaderSize = 80;

std::string header_text(const ExportOptions& options, const detail::ExportModel& model) {
  std::string text = "Hefesto STL merged";
  if (options.stl_layer) {
    const auto it = std::find_if(model.layers.begin(), model.layers.end(),
                                 [&](const ExportLayer& l) { return l.layer == *options.stl_layer; });
    const std::string name = it != model.layers.end() ? it->name : std::string{};
    text = "Hefesto STL layer " + detail::safe_json_text(name);
  }
  text.resize(kHeaderSize, ' ');
  return text;
}

bool written(const ExportOptions& options, const ExportLayer& layer) {
  return !options.stl_layer || *options.stl_layer == layer.layer;
}

void put_triangles(std::vector<std::byte>& out, const detail::LayerGeometry& g) {
  for (std::size_t t = 0; t < g.triangle_count(); ++t) {
    const std::uint32_t first = g.indices[3 * t];
    for (std::size_t c = 0; c < 3; ++c) {
      detail::put_f32(out, g.normals[(3 * first) + c]);
    }
    for (std::size_t v = 0; v < 3; ++v) {
      const std::uint32_t index = g.indices[(3 * t) + v];
      for (std::size_t c = 0; c < 3; ++c) {
        detail::put_f32(out, g.positions[(3 * index) + c]);
      }
    }
    detail::put_u16(out, 0);
  }
}

}  // namespace

diag::Status write_stl(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
                       gds::ByteSink& out, diag::DiagnosticSink& sink) {
  const detail::ExportModel model = detail::build_export_model(mesh, stack, options, sink);
  std::size_t triangles = 0;
  for (std::size_t i = 0; i < model.layers.size(); ++i) {
    triangles += written(options, model.layers[i]) ? model.geometry[i].triangle_count() : 0;
  }
  std::vector<std::byte> bytes;
  bytes.reserve(kHeaderSize + 4 + (50 * triangles));
  const std::string header = header_text(options, model);
  for (const char c : header) {
    bytes.push_back(static_cast<std::byte>(c));
  }
  detail::put_u32(bytes, static_cast<std::uint32_t>(triangles));
  for (std::size_t i = 0; i < model.layers.size(); ++i) {
    if (written(options, model.layers[i])) {
      put_triangles(bytes, model.geometry[i]);
    }
  }
  return out.write(bytes);
}

}  // namespace hefesto::view3d
