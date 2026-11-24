// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Vulkan objects of VulkanMesh3dRenderer that do not depend on the mesh
// (private to hefesto_render_vk): the GPU instance and layer layouts the
// shaders read (shaders/mesh3d_common.glsl), the pipelines of every kind and
// pass, the multisampled color pass and its images. Owner: RENDER3D.

#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace hefesto::render::mesh3d {

/// The three instanced pipelines.
enum class Kind : std::uint8_t { rect, side, triangle };
inline constexpr std::size_t kKindCount = 3;

/// The passes a pipeline is built for.
enum class Variant : std::uint8_t {
  opaque,            // target pass: color and pick, depth writes
  transparent,       // target pass: blended color, no pick, no depth writes
  pick,              // target pass at level >= 1: pick and depth only
  msaa_opaque,       // multisampled color pass: color, depth writes
  msaa_transparent,  // multisampled color pass: blended color, no depth writes
};
inline constexpr std::size_t kVariantCount = 5;

inline constexpr VkSampleCountFlagBits kMsaaSamples = VK_SAMPLE_COUNT_4_BIT;

/// Instance layouts (std430-free vertex input; 4-byte fields only).
struct GpuRect {
  float x0, y0, x1, y1;
  std::uint32_t layer;
  std::uint32_t pick;  // chunk-local pick index
};
struct GpuSide {
  float x0, y0, x1, y1;
  std::uint32_t layer_flags;  // layer | side_flag bits << 16
  float height, thickness;
  std::uint32_t pick;
};
struct GpuTriangle {
  float x0, y0, x1, y1, x2, y2;
  std::uint32_t layer;
  float height, thickness;
  std::uint32_t pick;
};

/// One entry of the layer storage buffer (std430).
struct GpuLayer {
  std::array<float, 4> color;  // display RGB, alpha
  std::array<float, 4> zinfo;  // h, t, 0, 0
};

/// The push constants of every draw (vertex stage).
struct FramePush {
  std::array<float, 16> mvp;
  std::array<float, 4> light;          // the board light (unit, displayed scene); ambient
  std::array<float, 4> zscale;         // s_z; 0; 0; 0
  std::array<std::uint32_t, 4> params;  // pick base; lighting; highlight active; 0
};
static_assert(sizeof(FramePush) == 112);  // within the 128 bytes every device offers

/// Vertices per instance of each kind, drawn as one triangle strip: the
/// face of a rectangle or a triangle toward the viewer (the shader picks
/// the top or the bottom face), a side face's quad.
[[nodiscard]] constexpr std::uint32_t vertices_per_instance(Kind kind) noexcept {
  return kind == Kind::triangle ? 3U : 4U;
}

/// Shader modules, layouts, the multisampled pass and every pipeline.
class Pipelines {
 public:
  /// `target_pass`: VulkanOffscreenTarget's render pass (color, pick, depth).
  Pipelines(VkPhysicalDevice physical, VkDevice device, VkRenderPass target_pass);
  ~Pipelines();
  Pipelines(const Pipelines&) = delete;
  Pipelines& operator=(const Pipelines&) = delete;

  /// VK_NULL_HANDLE for the msaa variants when the device lacks 4x MSAA.
  [[nodiscard]] VkPipeline get(Kind kind, Variant variant) const noexcept;
  [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }
  [[nodiscard]] VkDescriptorSetLayout set_layout() const noexcept { return set_layout_; }
  /// The multisampled color pass: 4x color and depth, resolved into the
  /// target's color image; VK_NULL_HANDLE without 4x MSAA.
  [[nodiscard]] VkRenderPass msaa_pass() const noexcept { return msaa_pass_; }
  [[nodiscard]] bool msaa() const noexcept { return msaa_pass_ != VK_NULL_HANDLE; }

 private:
  void create(VkRenderPass target_pass);
  void destroy() noexcept;

  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  std::array<VkShaderModule, kKindCount> vert_{};
  VkShaderModule frag_ = VK_NULL_HANDLE;
  VkShaderModule msaa_frag_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkRenderPass msaa_pass_ = VK_NULL_HANDLE;
  std::array<std::array<VkPipeline, kVariantCount>, kKindCount> pipelines_{};
};

/// The multisampled color and depth images of one size (transient).
class MsaaImages {
 public:
  MsaaImages(VkPhysicalDevice physical, VkDevice device) : physical_(physical), device_(device) {}
  ~MsaaImages() { destroy(); }
  MsaaImages(const MsaaImages&) = delete;
  MsaaImages& operator=(const MsaaImages&) = delete;

  /// Recreates the images when the size differs.
  void ensure(int width, int height);
  [[nodiscard]] VkImageView color_view() const noexcept { return color_.view; }
  [[nodiscard]] VkImageView depth_view() const noexcept { return depth_.view; }

 private:
  struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
  };
  void create_image(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, Image& out);
  void destroy_image(Image& image) noexcept;
  void destroy() noexcept;

  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  Image color_;
  Image depth_;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace hefesto::render::mesh3d
