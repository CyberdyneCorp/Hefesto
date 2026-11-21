// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Vulkan OffscreenTarget of the 3D view (view-3d "Platform mechanism of
// the 3D view"; openspec/changes/m10-3d-view/design.md D8, tasks.md 5.2).
//
// Two buffers, each with three images of the same pixel size:
//   - color: VK_FORMAT_R8G8B8A8_SRGB created with MUTABLE_FORMAT, drawn
//     through an _SRGB view (the shader writes linear color, the view encodes
//     it) and shown through an _UNORM view, which is what Raven's
//     Rvulkan_texture_create_from_image_view expects ("a UNORM view of an
//     sRGB-encoded image"). It is in SHADER_READ_ONLY_OPTIMAL after a frame.
//   - depth: VK_FORMAT_D32_SFLOAT, cleared to 1.0.
//   - pick id: VK_FORMAT_R32_UINT, cleared to 0 (view3d::kNoPick).
// A frame draws into the back buffer and then swaps, so the color image the
// viewport shows is never the one being drawn.
//
// Protocol with the viewport (texture mode): after each frame, re-point the
// viewport's texture at shown().view (Rvulkan_texture_set_image_view, which
// returns once Raven no longer samples the previous view) before drawing the
// next frame, because the next frame draws into the previously shown buffer.
// resize() only records the new size; each buffer is recreated at that size
// the next time it becomes the back buffer, so the shown buffer stays valid
// until it is replaced.
//
// The public interface names no Vulkan type (handles cross as opaque
// pointers and 64-bit integers, as in Raven's C API).
//
// Owner: RENDER3D (design D3).

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "hefesto/render/mesh3d_renderer.hpp"
#include "hefesto/render/renderer.hpp"

namespace hefesto::render {

/// The color image the viewport shows.
struct ShownImage {
  std::uint64_t view = 0;  // raw VkImageView (UNORM view); 0 before the first frame
  int width = 0;
  int height = 0;
};

/// The three images of the shown buffer, read back (tests; slow).
struct TargetReadback {
  int width = 0;
  int height = 0;
  std::vector<std::uint32_t> color;  // RGBA8, sRGB-encoded, R in the low byte, row-major
  std::vector<float> depth;
  std::vector<std::uint32_t> pick;
};

class VulkanOffscreenTarget final : public OffscreenTarget {
 public:
  static constexpr int kBufferCount = 2;

  /// Creates the render pass, the command buffer and the first back buffer
  /// on `device` (Raven's device in the shell, HeadlessCanvas's in tests).
  /// `lock` is taken around every queue submission. Throws VulkanError when
  /// the device lacks a needed format or a Vulkan call fails, and
  /// std::invalid_argument when a size is not positive.
  VulkanOffscreenTarget(const VulkanDevice& device, int width, int height, QueueLock lock = {});
  ~VulkanOffscreenTarget() override;

  /// The size the next frame draws at.
  [[nodiscard]] int width() const noexcept override;
  [[nodiscard]] int height() const noexcept override;
  /// The viewport's resize callback. Sizes below 1 are clamped to 1.
  void resize(int width, int height) override;

  [[nodiscard]] ShownImage shown() const noexcept;
  /// Frames drawn so far (each one swapped the buffers).
  [[nodiscard]] std::uint64_t frame_count() const noexcept;
  /// Reads the color, depth and pick images of the shown buffer; empty
  /// before the first frame.
  [[nodiscard]] TargetReadback read_back();

  /// The Vulkan side, for the renderer in this library (mesh3d_target_vk.hpp).
  struct Impl;
  [[nodiscard]] Impl& impl() noexcept { return *impl_; }

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::render
