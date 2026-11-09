// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hefesto's Vulkan canvas renderer (desktop; MoltenVK on macOS).
//
// It draws a RectScene as instanced quads (or diagonals) and a PolygonScene
// as triangles, one draw per layer, into a render
// pass owned by someone else: the desktop shell's viewport draw callback, or
// the offscreen target of the benchmark (hefesto/render/headless.hpp).
//
// This header deliberately does not include <vulkan/vulkan.h>: Vulkan handles
// cross it as opaque pointers, as in Raven's own C API, so the shell and the
// benchmark can include it without the Vulkan headers on their include path
// mattering. Only the .cpp files of this library include Vulkan.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

#include "hefesto/render/camera.hpp"
#include "hefesto/render/scene.hpp"

namespace hefesto::render {

/// Vulkan API version the renderer was compiled against (VK_HEADER_VERSION_COMPLETE).
[[nodiscard]] std::uint32_t vulkan_header_version() noexcept;

/// Thrown when a Vulkan call fails. what() names the call and the VkResult.
class VulkanError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// A Vulkan device the renderer borrows (it never destroys these objects).
/// Every handle is the raw Vulkan handle cast to a pointer.
struct VulkanDevice {
  void* instance = nullptr;         // VkInstance
  void* physical_device = nullptr;  // VkPhysicalDevice
  void* device = nullptr;           // VkDevice
  void* queue = nullptr;            // VkQueue (graphics)
  std::uint32_t queue_family = 0;
};

/// Taken around every use of the queue when the queue is shared with another
/// thread (Raven renders on its own thread). Empty functions mean no locking.
struct QueueLock {
  std::function<void()> lock;
  std::function<void()> unlock;
};

/// What an upload cost.
struct UploadStats {
  double seconds = 0.0;
  std::uint64_t bytes = 0;
};

/// The area to draw into, in framebuffer pixels. When `set_viewport` is
/// false the caller has already set the viewport and scissor (Raven does).
struct DrawTarget {
  void* command_buffer = nullptr;  // VkCommandBuffer, inside a render pass
  void* render_pass = nullptr;     // VkRenderPass, one color attachment
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  bool set_viewport = false;
  /// Clears the area to `clear_color` first (vkCmdClearAttachments), for a
  /// viewport inside someone else's render pass (the canvas background).
  bool clear = false;
  LayerStyle clear_color{};
};

class CanvasRenderer {
 public:
  explicit CanvasRenderer(const VulkanDevice& device, QueueLock lock = {});
  ~CanvasRenderer();
  CanvasRenderer(const CanvasRenderer&) = delete;
  CanvasRenderer& operator=(const CanvasRenderer&) = delete;
  CanvasRenderer(CanvasRenderer&&) noexcept;
  CanvasRenderer& operator=(CanvasRenderer&&) noexcept;

  /// Copies the instances to a device-local buffer (through a staging
  /// buffer) and waits for the copy. Replaces the previous instances; the old
  /// buffer is freed a few record() calls later, once no frame can use it.
  /// May run on another thread than record() (the shell uploads on the UI
  /// thread while Raven records on its render thread).
  UploadStats upload(std::span<const RectInstance> instances);

  /// Copies the filled triangles (PolygonScene::vertices, positions relative
  /// to (base_x, base_y)) to the device, like upload(). Replaces the previous
  /// polygons.
  UploadStats upload_polygons(std::span<const PolygonVertex> vertices, std::int32_t base_x, std::int32_t base_y);

  /// Per-layer colors, indexed by layer; layers past the end use white.
  void set_layer_styles(std::span<const LayerStyle> styles);

  /// Records the draw commands: one vkCmdDraw per range. `draws` index the
  /// uploaded instances (RectScene::batches() or CanvasIndex::visible_ranges()),
  /// `polygon_draws` the uploaded polygon vertices (PolygonScene::batches or
  /// visible_polygon_batches()); both are in layer order and are drawn
  /// merged by layer, so a layer's style decides how its range is drawn
  /// (filled, stippled, dashed, diagonals). Builds the pipelines for
  /// `target.render_pass` on first use.
  void record(const DrawTarget& target, const ViewTransform& view, std::span<const LayerBatch> draws,
              std::span<const LayerBatch> polygon_draws = {});

  /// Bytes of device memory the renderer holds (instance buffer).
  [[nodiscard]] std::uint64_t device_bytes() const noexcept;
  [[nodiscard]] std::size_t instance_count() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::render
