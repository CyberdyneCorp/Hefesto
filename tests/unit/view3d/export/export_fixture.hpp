// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Helpers of the view3d/export tests: hand-built stacks and meshes (boxes
// with their four side faces, split-tile triangles), a parsed glTF (cgltf_parse
// and cgltf_validate) and binary STL reading. Owner: EXPORT (M10 tasks 11.3,
// 11.4).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cgltf.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/display/styles.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/view3d/export/export.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d::test {

inline StackLayer layout_layer(std::string name, float h, float t, std::size_t style = 0) {
  StackLayer layer;
  layer.name = std::move(name);
  layer.type = static_cast<tech::TileType>(10);
  layer.style = style;
  layer.height = h;
  layer.thickness = t;
  layer.source = ValueSource::extract;
  return layer;
}

inline StackLayer mask_layer(std::string name, std::size_t index, std::optional<std::array<float, 2>> render) {
  StackLayer layer;
  layer.name = std::move(name);
  layer.mask_layer = index;
  layer.style = kPaleHighlightStyle;
  if (render) {
    layer.height = (*render)[0];
    layer.thickness = (*render)[1];
    layer.source = ValueSource::render;
  }
  return layer;
}

/// A rectangle with its four side faces (counter-clockwise boundary, so
/// every outward normal is to the right).
inline void add_box(Chunk& chunk, LayerId layer, float x0, float y0, float x1, float y1) {
  chunk.rects.push_back(RectInstance{x0, y0, x1, y1, layer});
  chunk.sides.push_back(SideInstance{x0, y0, x1, y0, layer, 0, 0.0F, 0.0F});
  chunk.sides.push_back(SideInstance{x1, y0, x1, y1, layer, 0, 0.0F, 0.0F});
  chunk.sides.push_back(SideInstance{x1, y1, x0, y1, layer, 0, 0.0F, 0.0F});
  chunk.sides.push_back(SideInstance{x0, y1, x0, y0, layer, 0, 0.0F, 0.0F});
}

/// One chunk at the region's lower-left corner.
inline MeshModel one_chunk_mesh(Mode mode, const db::Rect& region, const LayerStack& stack) {
  MeshModel mesh;
  mesh.mode = mode;
  mesh.region = region;
  Chunk chunk;
  chunk.area = region;
  chunk.origin_x = region.x0;
  chunk.origin_y = region.y0;
  mesh.chunks.push_back(std::move(chunk));
  for (std::size_t i = 0; i < stack.size(); ++i) {
    mesh.layer_heights.push_back(stack.draw_height(static_cast<LayerId>(i)));
  }
  return mesh;
}

inline std::string text_of(const std::vector<std::byte>& bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

/// A glTF file parsed and validated by cgltf.
class ParsedGltf {
 public:
  explicit ParsedGltf(std::string bytes) : bytes_(std::move(bytes)) {
    const cgltf_options options{};
    result = cgltf_parse(&options, bytes_.data(), bytes_.size(), &data_);
    if (result == cgltf_result_success) {
      validation = cgltf_validate(data_);
    }
  }
  ~ParsedGltf() { cgltf_free(data_); }
  ParsedGltf(const ParsedGltf&) = delete;
  ParsedGltf& operator=(const ParsedGltf&) = delete;

  [[nodiscard]] const cgltf_data& data() const { return *data_; }
  [[nodiscard]] const cgltf_node* node(std::string_view name) const {
    for (cgltf_size i = 0; i < data_->nodes_count; ++i) {
      if (data_->nodes[i].name != nullptr && name == data_->nodes[i].name) {
        return &data_->nodes[i];
      }
    }
    return nullptr;
  }
  /// The POSITION accessor of a layer node's mesh.
  [[nodiscard]] const cgltf_accessor* positions(std::string_view name) const {
    const cgltf_node* n = node(name);
    if (n == nullptr || n->mesh == nullptr) {
      return nullptr;
    }
    const cgltf_primitive& p = n->mesh->primitives[0];
    for (cgltf_size i = 0; i < p.attributes_count; ++i) {
      if (p.attributes[i].type == cgltf_attribute_type_position) {
        return p.attributes[i].data;
      }
    }
    return nullptr;
  }
  /// glTF's +Y extent of a layer: its slab thickness in the file.
  [[nodiscard]] double thickness(std::string_view name) const {
    const cgltf_accessor* a = positions(name);
    REQUIRE(a != nullptr);
    return static_cast<double>(a->max[1]) - static_cast<double>(a->min[1]);
  }
  [[nodiscard]] std::string asset_extras() const {
    return data_->asset.extras.data != nullptr ? std::string{data_->asset.extras.data} : std::string{};
  }

  cgltf_result result = cgltf_result_success;
  cgltf_result validation = cgltf_result_success;

 private:
  std::string bytes_;
  cgltf_data* data_ = nullptr;
};

/// Binary STL: header text and triangle count.
struct StlFile {
  std::string header;
  std::uint32_t triangles = 0;
  std::size_t size = 0;
};

inline StlFile read_stl(std::string_view bytes) {
  REQUIRE(bytes.size() >= 84);
  StlFile f;
  f.header = std::string{bytes.substr(0, 80)};
  for (std::size_t i = 0; i < 4; ++i) {
    f.triangles |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[80 + i])) << (8U * i);
  }
  f.size = bytes.size();
  return f;
}

/// Little-endian float at `offset`.
inline float f32_at(std::string_view bytes, std::size_t offset) {
  std::uint32_t word = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    word |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + i])) << (8U * i);
  }
  float value = 0.0F;
  std::memcpy(&value, &word, sizeof value);
  return value;
}

/// A style set with two layout styles: 52 "metal1 wiring" (blue, solid) and
/// 53 "outline only" (outline: not drawn).
inline display::StyleSet two_styles() {
  display::StyleSet set;
  set.colors.entries.resize(3);
  set.colors.entries[1] = display::ColorEntry{0, 0, 255, "blue"};
  set.colors.entries[2] = display::ColorEntry{255, 0, 0, "red"};
  set.table.styles.resize(display::kFixedStyleCount + 2);
  set.table.layout_count = 2;
  display::DisplayStyle& metal = set.table.styles[display::kFixedStyleCount];
  metal.color = 1;
  metal.mask = 127;
  metal.fill = display::FillKind::solid;
  metal.long_name = "metal1 wiring";
  display::DisplayStyle& outline = set.table.styles[display::kFixedStyleCount + 1];
  outline.color = 2;
  outline.mask = 127;
  outline.fill = display::FillKind::outline;
  outline.long_name = "outline only";
  return set;
}

}  // namespace hefesto::view3d::test
