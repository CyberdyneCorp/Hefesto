// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "vk_util.hpp"

#include <string>

#include "hefesto/render/renderer.hpp"

namespace hefesto::render::vk {

void check(VkResult result, std::string_view what) {
  if (result != VK_SUCCESS) {
    throw VulkanError(std::string(what) + " failed (VkResult " + std::to_string(static_cast<int>(result)) +
                      ")");
  }
}

std::uint32_t find_memory_type(VkPhysicalDevice physical, std::uint32_t type_bits,
                               VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties props{};
  vkGetPhysicalDeviceMemoryProperties(physical, &props);
  for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i) {
    const bool allowed = (type_bits & (1U << i)) != 0;
    if (allowed && (props.memoryTypes[i].propertyFlags & flags) == flags) {
      return i;
    }
  }
  throw VulkanError("no Vulkan memory type with the requested properties");
}

Buffer create_buffer(VkPhysicalDevice physical, VkDevice device, VkDeviceSize size,
                     VkBufferUsageFlags usage, VkMemoryPropertyFlags flags) {
  Buffer out;
  out.size = size;
  VkBufferCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.size = size;
  info.usage = usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  check(vkCreateBuffer(device, &info, nullptr, &out.buffer), "vkCreateBuffer");

  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(device, out.buffer, &req);
  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex = find_memory_type(physical, req.memoryTypeBits, flags);
  const VkResult result = vkAllocateMemory(device, &alloc, nullptr, &out.memory);
  if (result != VK_SUCCESS) {
    vkDestroyBuffer(device, out.buffer, nullptr);
    check(result, "vkAllocateMemory");
  }
  check(vkBindBufferMemory(device, out.buffer, out.memory, 0), "vkBindBufferMemory");
  return out;
}

void destroy_buffer(VkDevice device, Buffer& buffer) noexcept {
  if (buffer.buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, buffer.buffer, nullptr);
  }
  if (buffer.memory != VK_NULL_HANDLE) {
    vkFreeMemory(device, buffer.memory, nullptr);
  }
  buffer = Buffer{};
}

}  // namespace hefesto::render::vk
