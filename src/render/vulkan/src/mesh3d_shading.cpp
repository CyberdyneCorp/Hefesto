// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pick ids and the colors and lighting of the 3D view (mesh3d_pick.hpp,
// mesh3d_shading.hpp; design.md D8, D10). Owner: RENDER3D.

#include <algorithm>
#include <cmath>
#include <limits>

#include "hefesto/display/scene.hpp"
#include "hefesto/render/mesh3d_pick.hpp"
#include "hefesto/render/mesh3d_shading.hpp"

namespace hefesto::render {

// --- pick ids ----------------------------------------------------------------

namespace {

constexpr std::uint64_t kMaxPickId = std::numeric_limits<view3d::PickId>::max();

std::uint64_t kind_size(const view3d::Chunk& chunk, PickKind kind) noexcept {
  switch (kind) {
    case PickKind::rect:
      return chunk.rects.size();
    case PickKind::side:
      return chunk.sides.size();
    case PickKind::triangle:
      return chunk.triangles.size();
  }
  return 0;
}

}  // namespace

std::uint64_t pick_instance_count(const view3d::Chunk& chunk) noexcept {
  return chunk.rects.size() + chunk.sides.size() + chunk.triangles.size();
}

std::uint64_t pick_local_index(const view3d::Chunk& chunk, PickKind kind, std::size_t index) noexcept {
  switch (kind) {
    case PickKind::rect:
      return index;
    case PickKind::side:
      return chunk.rects.size() + index;
    case PickKind::triangle:
      return chunk.rects.size() + chunk.sides.size() + index;
  }
  return index;
}

view3d::PickId encode_pick(const view3d::MeshModel& mesh, const PickRef& ref) noexcept {
  if (ref.chunk >= mesh.chunks.size() || ref.index >= kind_size(mesh.chunks[ref.chunk], ref.kind)) {
    return view3d::kNoPick;
  }
  std::uint64_t id = 1;
  for (std::size_t c = 0; c < ref.chunk; ++c) {
    id += pick_instance_count(mesh.chunks[c]);
  }
  id += pick_local_index(mesh.chunks[ref.chunk], ref.kind, ref.index);
  return id > kMaxPickId ? view3d::kNoPick : static_cast<view3d::PickId>(id);
}

std::optional<PickRef> decode_pick(const view3d::MeshModel& mesh, view3d::PickId id) noexcept {
  if (id == view3d::kNoPick) {
    return std::nullopt;
  }
  std::uint64_t rest = id - 1U;
  for (std::size_t c = 0; c < mesh.chunks.size(); ++c) {
    const view3d::Chunk& chunk = mesh.chunks[c];
    if (rest >= pick_instance_count(chunk)) {
      rest -= pick_instance_count(chunk);
      continue;
    }
    for (const PickKind kind : {PickKind::rect, PickKind::side, PickKind::triangle}) {
      if (rest < kind_size(chunk, kind)) {
        return PickRef{c, kind, static_cast<std::size_t>(rest)};
      }
      rest -= kind_size(chunk, kind);
    }
  }
  return std::nullopt;
}

// --- colors and lighting -------------------------------------------------------

std::vector<LayerPaint> layer_paints(const view3d::LayerStack& stack, const display::StyleSet* styles) {
  std::vector<LayerPaint> out(stack.size());
  if (styles == nullptr) {
    return out;
  }
  for (std::size_t i = 0; i < out.size(); ++i) {
    const display::ResolvedStyle s = display::resolve_style(*styles, stack.layers()[i].style);
    out[i].rgb = {s.r, s.g, s.b};
    out[i].drawable = s.fill == display::FillKind::solid || s.fill == display::FillKind::stipple;
  }
  return out;
}

std::array<float, 3> background_color(const display::StyleSet* styles) {
  if (styles == nullptr) {
    return {0.0F, 0.0F, 0.0F};
  }
  const display::ResolvedStyle s = display::resolve_style(*styles, display::kStyleTransparent);
  return {s.r, s.g, s.b};
}

float light_intensity(Lighting lighting, const std::array<double, 3>& model, const std::array<float, 3>& normal) {
  if (lighting == Lighting::magic) {
    const double lx = 50.0 - model[0];
    const double ly = 50.0 - model[1];
    const double lz = 50.0 - model[2];
    const double len = std::sqrt((lx * lx) + (ly * ly) + (lz * lz));
    return 0.6F + (len > 0.0 ? static_cast<float>(std::max(lz / len, 0.0)) : 1.0F);
  }
  constexpr float kInvSqrt3 = 0.57735027F;
  return 0.4F + std::max((normal[0] + normal[1] + normal[2]) * kInvSqrt3, 0.0F);
}

std::array<float, 3> lit_color(const std::array<float, 3>& rgb, float intensity) {
  return {std::clamp(rgb[0] * intensity, 0.0F, 1.0F), std::clamp(rgb[1] * intensity, 0.0F, 1.0F),
          std::clamp(rgb[2] * intensity, 0.0F, 1.0F)};
}

float srgb_to_linear(float c) noexcept {
  return c <= 0.04045F ? c / 12.92F : std::pow((c + 0.055F) / 1.055F, 2.4F);
}

float linear_to_srgb(float c) noexcept {
  return c <= 0.0031308F ? c * 12.92F : (1.055F * std::pow(c, 1.0F / 2.4F)) - 0.055F;
}

}  // namespace hefesto::render
