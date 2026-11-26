// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// VulkanMesh3dRenderer: chunk upload by (id, version), the opaque, transparency
// and multisampled passes, and the pick read (design.md D6, D8, D10;
// tasks.md 5.3, 5.4). Owner: RENDER3D.

#include "hefesto/render/mesh3d_vulkan_renderer.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "hefesto/render/mesh3d_pick.hpp"
#include "hefesto/render/mesh3d_shading.hpp"
#include "mesh3d_pipelines_vk.hpp"
#include "mesh3d_target_vk.hpp"
#include "vk_util.hpp"

namespace hefesto::render {

namespace {

using mesh3d::FramePush;
using mesh3d::GpuLayer;
using mesh3d::GpuRect;
using mesh3d::GpuSide;
using mesh3d::GpuTriangle;
using mesh3d::Kind;
using mesh3d::kKindCount;
using mesh3d::Variant;

constexpr VkMemoryPropertyFlags kHostMemory =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
constexpr std::array<Kind, kKindCount> kKinds{Kind::rect, Kind::triangle, Kind::side};

std::size_t index_of(Kind kind) {
  return static_cast<std::size_t>(kind);
}

// --- chunk packing -----------------------------------------------------------

/// The instances of one layer in a packed kind array.
struct Range {
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

GpuRect to_gpu(const view3d::RectInstance& r, std::uint32_t pick) {
  return GpuRect{r.x0, r.y0, r.x1, r.y1, r.layer, pick};
}

GpuSide to_gpu(const view3d::SideInstance& s, std::uint32_t pick) {
  return GpuSide{s.x0, s.y0, s.x1, s.y1, std::uint32_t{s.layer} | (std::uint32_t{s.flags} << 16U), s.height,
                 s.thickness, pick};
}

GpuTriangle to_gpu(const view3d::Triangle& t, std::uint32_t pick) {
  return GpuTriangle{t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2], t.layer, t.height, t.thickness, pick};
}

/// Packs one kind's instances grouped by layer (stable), with their
/// chunk-local pick index `first_pick + i`; `runs[layer]` is the layer's range.
template <typename Src>
auto pack(const std::vector<Src>& src, std::uint32_t first_pick, std::vector<Range>& runs) {
  std::vector<std::uint32_t> order(src.size());
  std::iota(order.begin(), order.end(), 0U);
  std::stable_sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) {
    return src[a].layer < src[b].layer;
  });
  std::vector<decltype(to_gpu(src.front(), 0U))> out;
  out.reserve(src.size());
  for (const std::uint32_t i : order) {
    const view3d::LayerId layer = src[i].layer;
    if (layer >= runs.size()) {
      runs.resize(static_cast<std::size_t>(layer) + 1U, Range{static_cast<std::uint32_t>(out.size()), 0});
    }
    if (runs[layer].count == 0) {
      runs[layer].first = static_cast<std::uint32_t>(out.size());
    }
    ++runs[layer].count;
    out.push_back(to_gpu(src[i], first_pick + i));
  }
  return out;
}

/// One uploaded chunk.
struct GpuChunk {
  std::uint32_t id = 0;
  std::uint64_t version = 0;
  db::Rect area;
  std::array<std::size_t, kKindCount> counts{};
  double origin_x = 0.0;
  double origin_y = 0.0;
  vk::Buffer buffer;
  std::array<VkDeviceSize, kKindCount> offset{};
  std::array<std::vector<Range>, kKindCount> runs;

  [[nodiscard]] std::uint64_t instances() const noexcept { return counts[0] + counts[1] + counts[2]; }
  [[nodiscard]] Range range(Kind kind, std::size_t layer) const noexcept {
    const auto& r = runs[index_of(kind)];
    return layer < r.size() ? r[layer] : Range{};
  }
};

std::array<std::size_t, kKindCount> counts_of(const view3d::Chunk& c) {
  std::array<std::size_t, kKindCount> n{};
  n[index_of(Kind::rect)] = c.rects.size();
  n[index_of(Kind::side)] = c.sides.size();
  n[index_of(Kind::triangle)] = c.triangles.size();
  return n;
}

/// MESH gives versions from one process-wide counter, so (id, version)
/// names a chunk's content across builds and rebuilds. Version 0 is a model
/// made by hand, never cached.
bool same_chunk(const GpuChunk& g, const view3d::Chunk& c) {
  return c.version != 0 && g.id == c.id && g.version == c.version;
}

VkDeviceSize align16(VkDeviceSize v) {
  return (v + 15U) & ~VkDeviceSize{15U};
}

template <typename T>
void write_at(VkDevice device, const vk::Buffer& b, VkDeviceSize offset, const std::vector<T>& data) {
  if (data.empty()) {
    return;
  }
  void* mapped = nullptr;
  const VkDeviceSize bytes = data.size() * sizeof(T);
  vk::check(vkMapMemory(device, b.memory, offset, bytes, 0, &mapped), "vkMapMemory (3D)");
  std::memcpy(mapped, data.data(), static_cast<std::size_t>(bytes));
  vkUnmapMemory(device, b.memory);
}

// --- frame preparation -------------------------------------------------------

enum class LayerDraw : std::uint8_t { skip, opaque, transparent };

using Mat = std::array<double, 16>;  // column-major

/// clip = m * S(s, s, 1) * T(ox, oy, 0) * diag(1, 1, -1) (s:
/// DrawParams::xy_scale), then GL clip -> Vulkan clip: y down and depth
/// (w - z') / 2 (z' grows toward the viewer, CAMERA). Magic's model has
/// z = -h * s_z, so Magic's viewer sees the physical underside; CAMERA's
/// matrix is applied to the model mirrored in z (the displayed scene, z
/// growing with the height), so projection and depth agree that the viewer
/// is on the side of the higher layers (R1, Q-MISC-81; Q-MISC-84, both
/// modes): the top view shows higher layers in front with the picture
/// unchanged, and a tilted view such as the isometric preset looks from
/// above with the upper layers higher on the screen. Round 3 mirrored only
/// the depth, which in tilted views drew the scene upside down. The mirror
/// reverses every winding, so the pipelines' front faces are the clockwise
/// ones (mesh3d_pipelines.cpp).
Mat chunk_matrix(const view3d::Matrix4& m, double ox, double oy, double s) {
  Mat c{};
  for (std::size_t i = 0; i < 16; ++i) c[i] = static_cast<double>(m[i]);
  for (std::size_t r = 0; r < 4; ++r) {
    c[r] *= s;
    c[4 + r] *= s;
    c[8 + r] = -c[8 + r];
    c[12 + r] = (c[r] * ox) + (c[4 + r] * oy) + c[12 + r];
  }
  for (std::size_t col = 0; col < 4; ++col) {
    const double y = c[(col * 4) + 1];
    const double z = c[(col * 4) + 2];
    const double w = c[(col * 4) + 3];
    c[(col * 4) + 1] = -y;
    c[(col * 4) + 2] = 0.5 * (w - z);
  }
  return c;
}

/// DrawParams::xy_scale, or 1 when it is not a positive finite number.
double xy_scale_of(const DrawParams& p) {
  return std::isfinite(p.xy_scale) && p.xy_scale > 0.0F ? static_cast<double>(p.xy_scale) : 1.0;
}

/// The Vulkan depth of chunk-local (x, y, 0) under `c`.
double depth_at(const Mat& c, double x, double y) {
  const double z = (c[2] * x) + (c[6] * y) + c[14];
  const double w = (c[3] * x) + (c[7] * y) + c[15];
  return w != 0.0 ? z / w : z;
}

/// The substrate slab's draw (DrawParams::substrate): one rectangle and its
/// four side faces in a small buffer, on a layer entry past the stack's.
struct SlabDraw {
  bool active = false;
  std::uint32_t layer = 0;  // its GpuLayer entry
  FramePush push{};
  GpuRect rect{};
  std::array<GpuSide, 4> sides{};
};

/// The net overlay's draw (DrawParams::net_overlay): overlay_faces() as
/// rectangles and side faces relative to the first top's corner, one layer
/// entry per z range after the stack's (and the slab's).
struct OverlayDraw {
  FramePush push{};
  std::vector<GpuRect> rects;
  std::vector<GpuSide> sides;

  [[nodiscard]] bool empty() const noexcept { return rects.empty(); }
};

struct Frame {
  std::vector<LayerDraw> layers;
  std::vector<std::size_t> ordinal;  // per layer: its coplanar_bias ordinal
  std::vector<GpuLayer> gpu_layers;
  std::vector<FramePush> push;         // per chunk
  std::vector<std::size_t> back_to_front;  // chunk indices, farthest first
  std::array<float, 4> clear{};
  SlabDraw slab;
  OverlayDraw overlay;  // empty: no overlay
  bool clipped = false;  // a section plane is set: the clipped pipelines
  bool any_transparent = false;
  int width = 0;
  int height = 0;
};

float layer_alpha(const DrawParams& p, std::size_t layer) {
  return layer < p.alpha.size() ? p.alpha[layer] : 1.0F;
}

bool layer_hidden(const DrawParams& p, std::size_t layer) {
  return layer < p.hidden.size() && p.hidden[layer];
}

LayerDraw classify(const DrawParams& p, std::size_t layer, const LayerPaint& paint) {
  const float alpha = layer_alpha(p, layer);
  if (layer_hidden(p, layer) || !paint.drawable || !(alpha > 0.0F)) {
    return LayerDraw::skip;
  }
  return alpha < 1.0F ? LayerDraw::transparent : LayerDraw::opaque;
}

// The h and t a layer's rectangles and plain side faces are drawn with: the
// model's MeshModel::layer_heights (the stack's, or a contact spanned by the
// J1 option), else the stack's draw_height.
view3d::DrawHeight layer_height(const DrawParams& p, const std::vector<view3d::DrawHeight>& model, std::size_t i) {
  if (i < model.size()) {
    return model[i];
  }
  return p.stack != nullptr ? p.stack->draw_height(static_cast<view3d::LayerId>(i)) : view3d::DrawHeight{};
}

/// Every layer's draw, colour, heights and coplanar ordinal: the n-th drawn
/// layer in stack order (n from 1) is drawn with coplanar_bias(n), so on
/// faces at the same height the later layer wins at every pixel (task 17.5,
/// Q-MISC-77); the slab is drawn with no bias.
void prepare_layers(const DrawParams& p, const std::vector<view3d::DrawHeight>& model_heights, std::size_t count,
                    Frame& f) {
  std::vector<LayerPaint> paints =
      p.stack != nullptr ? layer_paints(*p.stack, p.styles) : std::vector<LayerPaint>{};
  paints.resize(count);
  f.layers.assign(count, LayerDraw::skip);
  f.ordinal.assign(count, 0U);
  f.gpu_layers.assign(std::max<std::size_t>(count, 1U), GpuLayer{});
  std::size_t drawn = 0;
  for (std::size_t i = 0; i < count; ++i) {
    f.layers[i] = classify(p, i, paints[i]);
    f.any_transparent = f.any_transparent || f.layers[i] == LayerDraw::transparent;
    drawn += f.layers[i] != LayerDraw::skip ? 1U : 0U;
    f.ordinal[i] = drawn;
    const view3d::DrawHeight z = layer_height(p, model_heights, i);
    const float alpha = std::clamp(layer_alpha(p, i), 0.0F, 1.0F);
    f.gpu_layers[i] = GpuLayer{{paints[i].rgb[0], paints[i].rgb[1], paints[i].rgb[2], alpha},
                               {z.height, z.thickness, 0.0F, 0.0F}};
  }
}

/// The section plane, when set and finite.
std::optional<SectionPlane> plane_of(const DrawParams& p) {
  if (!p.section_plane.has_value() || !std::isfinite(p.section_plane->position)) {
    return std::nullopt;
  }
  return p.section_plane;
}

/// FramePush::clip for a chunk at (ox, oy): keep where s * (coordinate -
/// position) >= 0, s = +1 to keep the greater side; (0, 0, 1) keeps all.
std::array<float, 4> clip_of(const std::optional<SectionPlane>& plane, double ox, double oy) {
  if (!plane.has_value()) {
    return {0.0F, 0.0F, 1.0F, 0.0F};
  }
  const double s = plane->keep_greater ? 1.0 : -1.0;
  const bool x = plane->axis == SectionPlane::Axis::x;
  const double origin = x ? ox : oy;
  return {x ? static_cast<float>(s) : 0.0F, x ? 0.0F : static_cast<float>(s),
          static_cast<float>(s * (origin - plane->position)), 0.0F};
}

/// The slab's rectangle and side faces relative to its area's lower-left
/// corner, counter-clockwise, so each side's outward normal is on its
/// right (no side_flag::outward_left).
void slab_geometry(const view3d::SubstrateSlab& slab, std::uint32_t layer, SlabDraw& out) {
  const auto w = static_cast<float>(slab.area.width());
  const auto h = static_cast<float>(slab.area.height());
  out.rect = GpuRect{0.0F, 0.0F, w, h, layer, 0U};
  const std::array<std::array<float, 4>, 4> segs{{{0, 0, w, 0}, {w, 0, w, h}, {w, h, 0, h}, {0, h, 0, 0}}};
  for (std::size_t i = 0; i < 4; ++i) {
    out.sides[i] = GpuSide{segs[i][0], segs[i][1], segs[i][2], segs[i][3], layer, 0.0F, 0.0F, 0U};
  }
}

bool usable_slab(const DrawParams& p, std::size_t layer_count) {
  if (!p.substrate.has_value()) {
    return false;
  }
  const view3d::SubstrateSlab& s = *p.substrate;
  return s.area.width() > 0 && s.area.height() > 0 && std::isfinite(s.height) && std::isfinite(s.thickness) &&
         layer_count < 0xFFFFU;  // the side faces pack the layer in 16 bits
}

/// The overlay's draw heights of a z range: GpuLayer::zinfo's h and t.
std::array<float, 2> overlay_ht(float z_bottom, float z_top) {
  return {z_bottom, z_top - z_bottom};
}

std::array<float, 3> overlay_rgb(const DrawParams& p) {
  std::array<float, 3> c = p.overlay_color;
  for (float& v : c) v = std::isfinite(v) ? std::clamp(v, 0.0F, 1.0F) : 0.0F;
  return c;
}

std::array<float, 4> clear_color(const DrawParams& p) {
  std::array<float, 3> bg = p.background.value_or(background_color(p.styles));
  for (float& c : bg) c = std::isfinite(c) ? std::clamp(c, 0.0F, 1.0F) : 0.0F;
  return {srgb_to_linear(bg[0]), srgb_to_linear(bg[1]), srgb_to_linear(bg[2]), 1.0F};
}

/// One z range's prisms as y intervals per column of x (Column::ys sorted,
/// merged, abutting ones joined); consecutive slabs with the same intervals
/// share a column.
struct Column {
  float x0 = 0.0F;
  float x1 = 0.0F;
  std::vector<std::pair<float, float>> ys;
};
using Intervals = std::vector<std::pair<float, float>>;

Intervals merged(Intervals v) {
  std::sort(v.begin(), v.end());
  Intervals out;
  for (const auto& [lo, hi] : v) {
    if (!out.empty() && lo <= out.back().second) {
      out.back().second = std::max(out.back().second, hi);
    } else {
      out.emplace_back(lo, hi);
    }
  }
  return out;
}

/// The pieces of [lo, hi) not covered by `b` (sorted, merged).
void subtract(float lo, float hi, const Intervals& b, Intervals& out) {
  float cur = lo;
  for (const auto& [blo, bhi] : b) {
    if (bhi <= cur || blo >= hi) {
      continue;
    }
    if (blo > cur) {
      out.emplace_back(cur, blo);
    }
    cur = std::max(cur, bhi);
  }
  if (cur < hi) {
    out.emplace_back(cur, hi);
  }
}

Intervals minus(const Intervals& a, const Intervals& b) {
  Intervals out;
  for (const auto& [lo, hi] : a) subtract(lo, hi, b, out);
  return out;
}

std::vector<Column> columns_of(const std::vector<OverlayPrism>& group) {
  std::vector<float> xs;
  for (const OverlayPrism& p : group) {
    xs.push_back(p.x0);
    xs.push_back(p.x1);
  }
  std::sort(xs.begin(), xs.end());
  xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
  std::vector<Column> out;
  for (std::size_t k = 0; k + 1 < xs.size(); ++k) {
    Intervals ys;
    for (const OverlayPrism& p : group) {
      if (p.x0 <= xs[k] && p.x1 >= xs[k + 1]) ys.emplace_back(p.y0, p.y1);
    }
    ys = merged(std::move(ys));
    if (!out.empty() && out.back().ys == ys) {
      out.back().x1 = xs[k + 1];
    } else {
      out.push_back(Column{xs[k], xs[k + 1], std::move(ys)});
    }
  }
  return out;
}

/// The tops and boundary side faces of one z range's columns, each side
/// with the outside on the right of (x0, y0) -> (x1, y1).
void emit_columns(const std::vector<Column>& cols, float zb, float zt, OverlayFaces& out) {
  static const Intervals kNone;
  for (std::size_t c = 0; c < cols.size(); ++c) {
    const Column& col = cols[c];
    for (const auto& [lo, hi] : col.ys) {
      out.tops.push_back(OverlayPrism{col.x0, lo, col.x1, hi, zb, zt});
      out.sides.push_back(OverlayFaces::Side{col.x0, lo, col.x1, lo, zb, zt});  // -y
      out.sides.push_back(OverlayFaces::Side{col.x1, hi, col.x0, hi, zb, zt});  // +y
    }
    for (const auto& [lo, hi] : minus(col.ys, c > 0 ? cols[c - 1].ys : kNone)) {
      out.sides.push_back(OverlayFaces::Side{col.x0, hi, col.x0, lo, zb, zt});  // -x
    }
    for (const auto& [lo, hi] : minus(col.ys, c + 1 < cols.size() ? cols[c + 1].ys : kNone)) {
      out.sides.push_back(OverlayFaces::Side{col.x1, lo, col.x1, hi, zb, zt});  // +x
    }
  }
}

/// A prism with its corners and z range ordered; nullopt when a field is
/// not finite or its rectangle is empty.
std::optional<OverlayPrism> normalized(const OverlayPrism& o) {
  const bool finite = std::isfinite(o.x0) && std::isfinite(o.y0) && std::isfinite(o.x1) && std::isfinite(o.y1) &&
                      std::isfinite(o.z_bottom) && std::isfinite(o.z_top);
  if (!finite || o.x0 == o.x1 || o.y0 == o.y1) {
    return std::nullopt;
  }
  return OverlayPrism{std::min(o.x0, o.x1), std::min(o.y0, o.y1), std::max(o.x0, o.x1),
                      std::max(o.y0, o.y1), std::min(o.z_bottom, o.z_top), std::max(o.z_bottom, o.z_top)};
}

}  // namespace

OverlayFaces overlay_faces(std::span<const OverlayPrism> prisms) {
  std::map<std::pair<float, float>, std::vector<OverlayPrism>> groups;  // by z range
  for (const OverlayPrism& o : prisms) {
    if (const auto n = normalized(o)) groups[{n->z_bottom, n->z_top}].push_back(*n);
  }
  OverlayFaces out;
  for (const auto& [z, group] : groups) emit_columns(columns_of(group), z.first, z.second, out);
  return out;
}

// --- Impl ------------------------------------------------------------------------

struct VulkanMesh3dRenderer::Impl {
  VulkanOffscreenTarget* owner = nullptr;
  VulkanOffscreenTarget::Impl* target = nullptr;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  mesh3d::Pipelines pipelines;
  mesh3d::MsaaImages msaa_images;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  VkDescriptorSet set = VK_NULL_HANDLE;
  vk::Buffer layer_buffer;
  vk::Buffer highlight_buffer;
  vk::Buffer slab_buffer;  // the substrate slab's rectangle, then its four sides
  vk::Buffer overlay_buffer;  // the net overlay's rectangles, then its side faces (grown, kept)

  std::vector<GpuChunk> chunks;  // model order
  std::vector<std::uint32_t> bases;  // pick base of each chunk
  std::size_t layer_count = 0;   // 1 + the largest layer id of the model
  std::vector<view3d::DrawHeight> heights;  // MeshModel::layer_heights of the uploaded model
  std::uint64_t uploads = 0;
  bool drawn = false;

  explicit Impl(VulkanOffscreenTarget& t)
      : owner(&t), target(&t.impl()), physical(t.impl().physical), device(t.impl().device),
        pipelines(physical, device, t.impl().pass), msaa_images(physical, device) {
    try {
      create_descriptors();
    } catch (...) {
      destroy();
      throw;
    }
  }
  ~Impl() { destroy(); }
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void create_descriptors() {
    const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = 1;
    info.poolSizeCount = 1;
    info.pPoolSizes = &size;
    vk::check(vkCreateDescriptorPool(device, &info, nullptr, &pool), "vkCreateDescriptorPool (3D)");
    const VkDescriptorSetLayout set_layout = pipelines.set_layout();
    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = pool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &set_layout;
    vk::check(vkAllocateDescriptorSets(device, &alloc, &set), "vkAllocateDescriptorSets (3D)");
    ensure_buffer(layer_buffer, sizeof(GpuLayer));
    ensure_buffer(highlight_buffer, sizeof(std::uint32_t));
    update_descriptors();
  }

  void destroy() noexcept {
    for (GpuChunk& c : chunks) vk::destroy_buffer(device, c.buffer);
    chunks.clear();
    vk::destroy_buffer(device, layer_buffer);
    vk::destroy_buffer(device, highlight_buffer);
    vk::destroy_buffer(device, slab_buffer);
    vk::destroy_buffer(device, overlay_buffer);
    if (pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, pool, nullptr);
    pool = VK_NULL_HANDLE;
  }

  /// Grows `b` (a host-visible storage buffer) to hold `bytes`; true when
  /// it was recreated.
  bool ensure_buffer(vk::Buffer& b, VkDeviceSize bytes) {
    if (b.buffer != VK_NULL_HANDLE && b.size >= bytes) {
      return false;
    }
    vk::destroy_buffer(device, b);
    b = vk::create_buffer(physical, device, std::bit_ceil(std::max<VkDeviceSize>(bytes, 16U)),
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, kHostMemory);
    return true;
  }

  void update_descriptors() {
    const std::array<VkDescriptorBufferInfo, 2> infos{VkDescriptorBufferInfo{layer_buffer.buffer, 0, VK_WHOLE_SIZE},
                                                      VkDescriptorBufferInfo{highlight_buffer.buffer, 0,
                                                                             VK_WHOLE_SIZE}};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::uint32_t i = 0; i < writes.size(); ++i) {
      writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[i].dstSet = set;
      writes[i].dstBinding = i;
      writes[i].descriptorCount = 1;
      writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
  }

  // --- upload ---

  GpuChunk upload_chunk(const view3d::Chunk& c) {
    GpuChunk g;
    g.id = c.id;
    g.version = c.version;
    g.area = c.area;
    g.counts = counts_of(c);
    g.origin_x = c.origin_x;
    g.origin_y = c.origin_y;
    const auto pick_of = [&](PickKind kind) { return static_cast<std::uint32_t>(pick_local_index(c, kind, 0)); };
    const auto rects = pack(c.rects, pick_of(PickKind::rect), g.runs[index_of(Kind::rect)]);
    const auto sides = pack(c.sides, pick_of(PickKind::side), g.runs[index_of(Kind::side)]);
    const auto tris = pack(c.triangles, pick_of(PickKind::triangle), g.runs[index_of(Kind::triangle)]);
    g.offset[index_of(Kind::rect)] = 0;
    g.offset[index_of(Kind::side)] = align16(rects.size() * sizeof(GpuRect));
    g.offset[index_of(Kind::triangle)] = g.offset[index_of(Kind::side)] + align16(sides.size() * sizeof(GpuSide));
    const VkDeviceSize bytes = g.offset[index_of(Kind::triangle)] + align16(tris.size() * sizeof(GpuTriangle));
    g.buffer = vk::create_buffer(physical, device, std::max<VkDeviceSize>(bytes, 16U),
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, kHostMemory);
    try {
      write_at(device, g.buffer, g.offset[index_of(Kind::rect)], rects);
      write_at(device, g.buffer, g.offset[index_of(Kind::side)], sides);
      write_at(device, g.buffer, g.offset[index_of(Kind::triangle)], tris);
    } catch (...) {
      vk::destroy_buffer(device, g.buffer);
      throw;
    }
    ++uploads;
    return g;
  }

  void upload(const view3d::MeshModel& mesh) {
    std::unordered_map<std::uint32_t, std::size_t> previous;
    for (std::size_t i = 0; i < chunks.size(); ++i) previous.emplace(chunks[i].id, i);
    std::vector<GpuChunk> next;
    next.reserve(mesh.chunks.size());
    std::uint64_t total = 0;
    for (const view3d::Chunk& c : mesh.chunks) {
      total += pick_instance_count(c);
      if (total >= std::numeric_limits<view3d::PickId>::max()) {
        throw std::length_error("Mesh3dRenderer: the mesh has more instances than 32-bit pick ids");
      }
    }
    for (const view3d::Chunk& c : mesh.chunks) {
      const auto it = previous.find(c.id);
      if (it != previous.end() && chunks[it->second].buffer.buffer != VK_NULL_HANDLE &&
          same_chunk(chunks[it->second], c)) {
        next.push_back(std::move(chunks[it->second]));
        chunks[it->second].buffer = vk::Buffer{};
      } else {
        next.push_back(upload_chunk(c));
      }
    }
    for (GpuChunk& old : chunks) vk::destroy_buffer(device, old.buffer);
    chunks = std::move(next);
    heights = mesh.layer_heights;
    index_chunks();
  }

  void index_chunks() {
    bases.assign(chunks.size(), 0U);
    std::uint64_t base = 1;
    layer_count = 0;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
      bases[i] = static_cast<std::uint32_t>(base);
      base += chunks[i].instances();
      for (const auto& runs : chunks[i].runs) layer_count = std::max(layer_count, runs.size());
    }
  }

  [[nodiscard]] std::uint64_t instance_total() const noexcept {
    return chunks.empty() ? 0U : bases.back() - 1U + chunks.back().instances();
  }

  // --- frame ---

  /// The push constants of a draw whose model is offset by (ox, oy).
  static FramePush push_for(const view3d::ViewState& view, const DrawParams& p, const view3d::Matrix4& m, double ox,
                            double oy, const std::array<std::uint32_t, 4>& params) {
    const Mat cm = chunk_matrix(m, ox, oy, xy_scale_of(p));
    FramePush push{};
    for (std::size_t k = 0; k < 16; ++k) push.mvp[k] = static_cast<float>(cm[k]);
    const std::array<float, 3> light = board_light(view.view_z);
    push.light = {light[0], light[1], light[2], kBoardAmbient};
    push.zscale = {view.scale_z, 0.0F, 0.0F, 0.0F};
    push.params = params;
    push.clip = clip_of(plane_of(p), ox, oy);
    return push;
  }

  Frame prepare(const view3d::ViewState& view, const DrawParams& p) const {
    Frame f;
    f.width = owner->width();
    f.height = owner->height();
    const std::size_t count = std::max(layer_count, p.stack != nullptr ? p.stack->size() : 0U);
    prepare_layers(p, heights, count, f);
    f.clear = clear_color(p);
    const view3d::Matrix4 m = view3d::matrix(view, f.width, f.height, p.projection);
    const std::uint32_t lighting = static_cast<std::uint32_t>(p.lighting);
    const std::uint32_t highlight = p.highlight != nullptr && !p.highlight->empty() ? 1U : 0U;
    const double scale = xy_scale_of(p);
    f.clipped = plane_of(p).has_value();
    std::vector<double> depth(chunks.size());
    f.push.resize(chunks.size());
    for (std::size_t i = 0; i < chunks.size(); ++i) {
      const GpuChunk& c = chunks[i];
      const Mat cm = chunk_matrix(m, c.origin_x, c.origin_y, scale);
      f.push[i] = push_for(view, p, m, c.origin_x, c.origin_y, {bases[i], lighting, highlight, 0U});
      const double cx = (static_cast<double>(c.area.x0) + static_cast<double>(c.area.x1)) / 2.0 - c.origin_x;
      const double cy = (static_cast<double>(c.area.y0) + static_cast<double>(c.area.y1)) / 2.0 - c.origin_y;
      depth[i] = depth_at(cm, cx, cy);
    }
    f.back_to_front.resize(chunks.size());
    std::iota(f.back_to_front.begin(), f.back_to_front.end(), std::size_t{0});
    std::stable_sort(f.back_to_front.begin(), f.back_to_front.end(),
                     [&](std::size_t a, std::size_t b) { return depth[a] > depth[b]; });
    prepare_slab(view, p, m, f);
    prepare_overlay(view, p, m, f);
    return f;
  }

  /// The substrate slab (DrawParams::substrate): a layer entry after the
  /// stack's, in its neutral colour, drawn with pick id 0 (kNoPick).
  static void prepare_slab(const view3d::ViewState& view, const DrawParams& p, const view3d::Matrix4& m, Frame& f) {
    if (!usable_slab(p, f.layers.size())) {
      return;
    }
    const view3d::SubstrateSlab& slab = *p.substrate;
    SlabDraw& d = f.slab;
    d.active = true;
    d.layer = static_cast<std::uint32_t>(f.layers.size());
    f.gpu_layers.resize(static_cast<std::size_t>(d.layer) + 1U);
    f.gpu_layers[d.layer] = GpuLayer{{slab.color[0], slab.color[1], slab.color[2], 1.0F},
                                     {slab.height, slab.thickness, 0.0F, 0.0F}};
    d.push = push_for(view, p, m, slab.area.x0, slab.area.y0, {0U, static_cast<std::uint32_t>(p.lighting), 0U, 0U});
    slab_geometry(slab, d.layer, d);
  }

  /// The net overlay (DrawParams::net_overlay): overlay_faces() of the
  /// prisms, one layer entry per z range after the stack's and the slab's,
  /// in the overlay colour at kOverlayAlpha, pick base 0 (never dimmed; its
  /// pipelines write no pick id).
  static void prepare_overlay(const view3d::ViewState& view, const DrawParams& p, const view3d::Matrix4& m, Frame& f) {
    if (p.net_overlay.empty()) {
      return;
    }
    const OverlayFaces faces = overlay_faces(p.net_overlay);
    if (faces.tops.empty() || f.gpu_layers.size() + faces.tops.size() >= 0xFFFFU) {
      return;  // nothing to draw (or more layer entries than side faces can name)
    }
    const auto ox = static_cast<double>(faces.tops.front().x0);
    const auto oy = static_cast<double>(faces.tops.front().y0);
    const auto rel = [](float v, double origin) { return static_cast<float>(static_cast<double>(v) - origin); };
    const std::array<float, 3> rgb = overlay_rgb(p);
    std::map<std::array<float, 2>, std::uint32_t> layers;  // z range -> GpuLayer entry
    const auto layer_of = [&](float z_bottom, float z_top) {
      const std::array<float, 2> ht = overlay_ht(z_bottom, z_top);
      const auto [it, added] = layers.emplace(ht, static_cast<std::uint32_t>(f.gpu_layers.size()));
      if (added) {
        f.gpu_layers.push_back(GpuLayer{{rgb[0], rgb[1], rgb[2], kOverlayAlpha}, {ht[0], ht[1], 0.0F, 0.0F}});
      }
      return it->second;
    };
    for (const OverlayPrism& t : faces.tops) {
      f.overlay.rects.push_back(
          GpuRect{rel(t.x0, ox), rel(t.y0, oy), rel(t.x1, ox), rel(t.y1, oy), layer_of(t.z_bottom, t.z_top), 0U});
    }
    for (const OverlayFaces::Side& e : faces.sides) {
      f.overlay.sides.push_back(GpuSide{rel(e.x0, ox), rel(e.y0, oy), rel(e.x1, ox), rel(e.y1, oy),
                                        layer_of(e.z_bottom, e.z_top), 0.0F, 0.0F, 0U});
    }
    f.overlay.push = push_for(view, p, m, ox, oy, {0U, static_cast<std::uint32_t>(p.lighting), 0U, 0U});
  }

  void write_frame_buffers(const Frame& f, const DrawParams& p) {
    bool recreated = ensure_buffer(layer_buffer, f.gpu_layers.size() * sizeof(GpuLayer));
    std::vector<std::uint32_t> bits(std::max<std::uint64_t>((instance_total() + 31U) / 32U, 1U), 0U);
    if (p.highlight != nullptr) {
      for (const view3d::PickId id : p.highlight->ids) {
        if (id != view3d::kNoPick && id <= instance_total()) {
          bits[(id - 1U) >> 5U] |= 1U << ((id - 1U) & 31U);
        }
      }
    }
    recreated = ensure_buffer(highlight_buffer, bits.size() * sizeof(std::uint32_t)) || recreated;
    if (recreated) {
      update_descriptors();
    }
    write_at(device, layer_buffer, 0, f.gpu_layers);
    write_at(device, highlight_buffer, 0, bits);
    write_slab(f.slab);
    write_overlay(f.overlay);
  }

  void write_overlay(const OverlayDraw& overlay) {
    if (overlay.empty()) {
      return;
    }
    const VkDeviceSize sides_at = align16(overlay.rects.size() * sizeof(GpuRect));
    const VkDeviceSize bytes = sides_at + (overlay.sides.size() * sizeof(GpuSide));
    if (overlay_buffer.buffer == VK_NULL_HANDLE || overlay_buffer.size < bytes) {
      vk::destroy_buffer(device, overlay_buffer);
      overlay_buffer = vk::create_buffer(physical, device, std::bit_ceil(bytes), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         kHostMemory);
    }
    write_at(device, overlay_buffer, 0, overlay.rects);
    write_at(device, overlay_buffer, sides_at, overlay.sides);
  }

  static constexpr VkDeviceSize kSlabSidesOffset = (sizeof(GpuRect) + 15U) & ~VkDeviceSize{15U};

  void write_slab(const SlabDraw& slab) {
    if (!slab.active) {
      return;
    }
    if (slab_buffer.buffer == VK_NULL_HANDLE) {
      slab_buffer = vk::create_buffer(physical, device, kSlabSidesOffset + sizeof(slab.sides),
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, kHostMemory);
    }
    write_at(device, slab_buffer, 0, std::vector<GpuRect>{slab.rect});
    write_at(device, slab_buffer, kSlabSidesOffset, std::vector<GpuSide>(slab.sides.begin(), slab.sides.end()));
  }

  /// Records draws, binding each pipeline once per run of draws.
  struct Recorder {
    const Impl& r;
    const Frame& f;
    VkCommandBuffer cmd;
    VkPipeline bound = VK_NULL_HANDLE;
    std::size_t bias = 0;  // the coplanar_bias ordinal last set

    void begin(int width, int height) const {
      const VkViewport viewport{0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height), 0.0F, 1.0F};
      const VkRect2D scissor{{0, 0}, {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)}};
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.pipelines.layout(), 0, 1, &r.set, 0, nullptr);
      vkCmdSetDepthBias(cmd, 0.0F, 0.0F, 0.0F);
    }

    /// The depth bias of the layer with coplanar ordinal `ordinal` (0: none).
    void set_bias(std::size_t ordinal) {
      if (ordinal == bias) {
        return;
      }
      const std::array<float, 2> b = coplanar_bias(ordinal);
      vkCmdSetDepthBias(cmd, b[0], 0.0F, b[1]);
      bias = ordinal;
    }

    void bind(Kind kind, Variant variant) {
      const VkPipeline pipeline = r.pipelines.get(kind, variant, f.clipped);
      if (pipeline != bound) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        bound = pipeline;
      }
    }

    /// The substrate slab, first in the opaque pass: depth written, pick
    /// id 0, so every layer drawn after it wins on equal depth.
    void slab(Variant variant) {
      if (!f.slab.active) {
        return;
      }
      set_bias(0);
      vkCmdPushConstants(cmd, r.pipelines.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(FramePush), &f.slab.push);
      for (const auto& [kind, offset, count] :
           {std::tuple{Kind::rect, VkDeviceSize{0}, 1U}, std::tuple{Kind::side, kSlabSidesOffset, 4U}}) {
        bind(kind, variant);
        vkCmdBindVertexBuffers(cmd, 0, 1, &r.slab_buffer.buffer, &offset);
        vkCmdDraw(cmd, mesh3d::vertices_per_instance(kind), count, 0, 0);
      }
    }

    void draw(std::size_t chunk, std::size_t layer, Kind kind, Variant variant) {
      const GpuChunk& c = r.chunks[chunk];
      const Range range = c.range(kind, layer);
      if (range.count == 0) {
        return;
      }
      bind(kind, variant);
      const VkDeviceSize offset = c.offset[index_of(kind)];
      vkCmdBindVertexBuffers(cmd, 0, 1, &c.buffer.buffer, &offset);
      vkCmdPushConstants(cmd, r.pipelines.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(FramePush),
                         &f.push[chunk]);
      vkCmdDraw(cmd, mesh3d::vertices_per_instance(kind), range.count, 0, range.first);
    }

    /// The opaque layers in stack order (the last style wins on coplanar
    /// faces, Q-MISC-77), every chunk of each.
    void opaque(Variant variant) {
      slab(variant);
      for (std::size_t layer = 0; layer < f.layers.size(); ++layer) {
        if (f.layers[layer] != LayerDraw::opaque) {
          continue;
        }
        set_bias(f.ordinal[layer]);
        for (const Kind kind : kKinds) {
          for (std::size_t chunk = 0; chunk < r.chunks.size(); ++chunk) draw(chunk, layer, kind, variant);
        }
      }
    }

    /// The net overlay, last: over everything drawn (no depth test), no
    /// pick id. `msaa`: the multisampled pass's pipeline.
    void overlay(bool msaa) {
      if (f.overlay.empty()) {
        return;
      }
      vkCmdPushConstants(cmd, r.pipelines.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(FramePush),
                         &f.overlay.push);
      const VkDeviceSize sides_at = align16(f.overlay.rects.size() * sizeof(GpuRect));
      for (const auto& [kind, offset, count] :
           {std::tuple{Kind::rect, VkDeviceSize{0}, f.overlay.rects.size()},
            std::tuple{Kind::side, sides_at, f.overlay.sides.size()}}) {
        if (count == 0) {
          continue;
        }
        const VkPipeline pipeline = r.pipelines.overlay(kind, msaa, f.clipped);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        bound = pipeline;
        vkCmdBindVertexBuffers(cmd, 0, 1, &r.overlay_buffer.buffer, &offset);
        vkCmdDraw(cmd, mesh3d::vertices_per_instance(kind), static_cast<std::uint32_t>(count), 0, 0);
      }
    }

    /// The transparent layers, chunks back to front.
    void transparent(Variant variant) {
      if (!f.any_transparent) {
        return;
      }
      for (const std::size_t chunk : f.back_to_front) {
        for (std::size_t layer = 0; layer < f.layers.size(); ++layer) {
          if (f.layers[layer] != LayerDraw::transparent) {
            continue;
          }
          set_bias(f.ordinal[layer]);
          for (const Kind kind : kKinds) draw(chunk, layer, kind, variant);
        }
      }
    }
  };

  /// The multisampled color pass (level >= 1), resolved into the back
  /// buffer's color image. Returns the frame's framebuffer, to destroy
  /// after the submission.
  VkFramebuffer record_msaa(VkCommandBuffer cmd, const Frame& f) {
    const TargetBuffer& back = target->back_buffer();
    msaa_images.ensure(back.width, back.height);
    const std::array<VkImageView, 3> views{msaa_images.color_view(), msaa_images.depth_view(), back.color.view};
    VkFramebufferCreateInfo fb_info{};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = pipelines.msaa_pass();
    fb_info.attachmentCount = static_cast<std::uint32_t>(views.size());
    fb_info.pAttachments = views.data();
    fb_info.width = static_cast<std::uint32_t>(back.width);
    fb_info.height = static_cast<std::uint32_t>(back.height);
    fb_info.layers = 1;
    VkFramebuffer fb = VK_NULL_HANDLE;
    vk::check(vkCreateFramebuffer(device, &fb_info, nullptr, &fb), "vkCreateFramebuffer (3D msaa)");
    std::array<VkClearValue, 3> clears{};
    clears[0].color = {{f.clear[0], f.clear[1], f.clear[2], f.clear[3]}};
    clears[1].depthStencil = {1.0F, 0};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = pipelines.msaa_pass();
    begin.framebuffer = fb;
    begin.renderArea.extent = {fb_info.width, fb_info.height};
    begin.clearValueCount = static_cast<std::uint32_t>(clears.size());
    begin.pClearValues = clears.data();
    vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
    Recorder rec{*this, f, cmd};
    rec.begin(back.width, back.height);
    rec.opaque(Variant::msaa_opaque);
    rec.transparent(Variant::msaa_transparent);
    rec.overlay(true);
    vkCmdEndRenderPass(cmd);
    return fb;
  }

  void draw(const view3d::ViewState& view, const DrawParams& p) {
    const Frame f = prepare(view, p);
    if (f.clipped) {
      pipelines.ensure_clipped();
    }
    if (!f.overlay.empty()) {
      pipelines.ensure_overlay(f.clipped);
    }
    write_frame_buffers(f, p);
    const bool msaa = p.level >= 1 && pipelines.msaa();
    const FrameRecording frame = target->begin_frame(f.clear);
    Recorder rec{*this, f, frame.cmd};
    rec.begin(frame.width, frame.height);
    if (msaa) {
      rec.opaque(Variant::pick);  // pick ids and depth; the color comes from the msaa pass
    } else {
      rec.opaque(Variant::opaque);
      rec.transparent(Variant::transparent);
      rec.overlay(false);
    }
    VkFramebuffer fb = VK_NULL_HANDLE;
    const auto after = [&](VkCommandBuffer cmd) { fb = record_msaa(cmd, f); };
    try {
      target->end_frame(msaa ? std::function<void(VkCommandBuffer)>(after) : std::function<void(VkCommandBuffer)>());
    } catch (...) {
      if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device, fb, nullptr);
      throw;
    }
    if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device, fb, nullptr);
    drawn = true;
  }
};

// --- VulkanMesh3dRenderer ----------------------------------------------------------

VulkanMesh3dRenderer::VulkanMesh3dRenderer(VulkanOffscreenTarget& target)
    : impl_(std::make_unique<Impl>(target)) {}

VulkanMesh3dRenderer::~VulkanMesh3dRenderer() = default;

void VulkanMesh3dRenderer::upload(const view3d::MeshModel& mesh) {
  impl_->upload(mesh);
}

void VulkanMesh3dRenderer::draw(const view3d::ViewState& view, const DrawParams& params, OffscreenTarget& target) {
  if (&target != impl_->owner) {
    throw std::invalid_argument("VulkanMesh3dRenderer::draw: not the target the renderer was built for");
  }
  impl_->draw(view, params);
}

view3d::PickId VulkanMesh3dRenderer::read_pick(int x, int y) {
  return impl_->drawn ? impl_->target->read_pick(x, y) : view3d::kNoPick;
}

std::uint64_t VulkanMesh3dRenderer::chunk_uploads() const noexcept {
  return impl_->uploads;
}

bool VulkanMesh3dRenderer::multisampling() const noexcept {
  return impl_->pipelines.msaa();
}

bool VulkanMesh3dRenderer::overlay_ready() const noexcept {
  return impl_->pipelines.overlay(mesh3d::Kind::rect, false, false) != VK_NULL_HANDLE ||
         impl_->pipelines.overlay(mesh3d::Kind::rect, false, true) != VK_NULL_HANDLE;
}

}  // namespace hefesto::render
