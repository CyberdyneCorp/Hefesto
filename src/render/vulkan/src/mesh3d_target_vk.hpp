// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Vulkan side of VulkanOffscreenTarget (private to hefesto_render_vk):
// the render pass the 3D pipelines are built for and the frame protocol
// (begin_frame records into the back buffer, end_frame submits, waits and
// swaps). Owner: RENDER3D.

#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "hefesto/render/mesh3d_target.hpp"

namespace hefesto::render {

/// Render pass attachments, in order.
inline constexpr std::uint32_t kColorAttachment = 0;  // R8G8B8A8_SRGB (sRGB view)
inline constexpr std::uint32_t kPickAttachment = 1;   // R32_UINT
inline constexpr std::uint32_t kDepthAttachment = 2;  // D32_SFLOAT
inline constexpr VkFormat kColorFormat3d = VK_FORMAT_R8G8B8A8_SRGB;
inline constexpr VkFormat kShownFormat3d = VK_FORMAT_R8G8B8A8_UNORM;
inline constexpr VkFormat kPickFormat3d = VK_FORMAT_R32_UINT;
inline constexpr VkFormat kDepthFormat3d = VK_FORMAT_D32_SFLOAT;

/// One image with its memory and a view in the image's own format.
struct TargetImage {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
};

/// One of the two buffers.
struct TargetBuffer {
  TargetImage color;
  VkImageView shown_view = VK_NULL_HANDLE;  // UNORM view of `color`
  TargetImage pick;
  TargetImage depth;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  int width = 0;
  int height = 0;
  bool drawn = false;  // holds a finished frame (color in SHADER_READ_ONLY_OPTIMAL)
};

/// What a frame records into.
struct FrameRecording {
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkRenderPass pass = VK_NULL_HANDLE;
  int width = 0;
  int height = 0;
};

struct VulkanOffscreenTarget::Impl {
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  std::uint32_t family = 0;
  QueueLock lock;

  VkRenderPass pass = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  std::array<TargetBuffer, kBufferCount> buffers{};
  int back = 0;  // index of the buffer the next frame draws into
  int width = 0;
  int height = 0;
  std::uint64_t frames = 0;
  bool recording = false;

  Impl(const VulkanDevice& device, int width, int height, QueueLock lock);
  ~Impl();
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  /// Recreates the back buffer when its size differs from the target's,
  /// begins the command buffer and the render pass, and clears color to
  /// `clear` (linear RGBA), depth to 1 and pick ids to 0.
  FrameRecording begin_frame(const std::array<float, 4>& clear);
  /// Ends the render pass, submits under the queue lock, waits for the GPU
  /// and makes the drawn buffer the shown one.
  void end_frame();

  [[nodiscard]] const TargetBuffer& shown_buffer() const noexcept {
    return buffers[static_cast<std::size_t>(1 - back)];
  }
  /// Records `record(cmd)` into a one-time command buffer, submits it under
  /// the queue lock and waits.
  template <typename Fn>
  void run_once(Fn record);
  void submit_and_wait();
  void begin_commands();

  void destroy_buffer(TargetBuffer& buffer) noexcept;
  void create_buffer(TargetBuffer& buffer, int buffer_width, int buffer_height);
  void destroy() noexcept;
};

template <typename Fn>
void VulkanOffscreenTarget::Impl::run_once(Fn record) {
  begin_commands();
  record(cmd);
  submit_and_wait();
}

}  // namespace hefesto::render
