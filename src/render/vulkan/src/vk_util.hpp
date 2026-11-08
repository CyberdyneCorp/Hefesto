// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private Vulkan helpers of hefesto_render_vk.

#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string_view>

namespace hefesto::render::vk {

/// Throws VulkanError naming `what` when `result` is not VK_SUCCESS.
void check(VkResult result, std::string_view what);

template <typename Handle>
Handle from_opaque(void* p) noexcept {
  return reinterpret_cast<Handle>(p);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename Handle>
void* to_opaque(Handle h) noexcept {
  return reinterpret_cast<void*>(h);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

/// Index of a memory type allowed by `type_bits` with all `flags`; throws if none.
std::uint32_t find_memory_type(VkPhysicalDevice physical, std::uint32_t type_bits,
                               VkMemoryPropertyFlags flags);

/// A buffer and its dedicated memory.
struct Buffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
};

Buffer create_buffer(VkPhysicalDevice physical, VkDevice device, VkDeviceSize size,
                     VkBufferUsageFlags usage, VkMemoryPropertyFlags flags);
void destroy_buffer(VkDevice device, Buffer& buffer) noexcept;

}  // namespace hefesto::render::vk
