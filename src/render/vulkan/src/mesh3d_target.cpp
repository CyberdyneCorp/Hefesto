// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// VulkanOffscreenTarget: the 3D view's double-buffered offscreen images
// (design.md D8, tasks.md 5.2). Owner: RENDER3D.

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "mesh3d_target_vk.hpp"
#include "vk_util.hpp"

namespace hefesto::render {

namespace {

constexpr VkImageUsageFlags kColorUsage =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constexpr VkImageUsageFlags kPickUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constexpr VkImageUsageFlags kDepthUsage =
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

/// Throws when `format` cannot be an optimal-tiling image with `features`.
void require_format(VkPhysicalDevice physical, VkFormat format, VkFormatFeatureFlags features, const char* what) {
  VkFormatProperties props{};
  vkGetPhysicalDeviceFormatProperties(physical, format, &props);
  if ((props.optimalTilingFeatures & features) != features) {
    throw VulkanError(std::string("the Vulkan device cannot use ") + what + " for the 3D view");
  }
}

void require_formats(VkPhysicalDevice physical) {
  require_format(physical, kColorFormat3d,
                 VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT,
                 "R8G8B8A8_SRGB as a color attachment");
  require_format(physical, kShownFormat3d, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "R8G8B8A8_UNORM as a sampled image");
  require_format(physical, kPickFormat3d, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT,
                 "R32_UINT as a color attachment");
  require_format(physical, kDepthFormat3d,
                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT,
                 "D32_SFLOAT as a depth attachment");
}

VkAttachmentDescription attachment(VkFormat format, VkImageLayout final_layout) {
  VkAttachmentDescription a{};
  a.format = format;
  a.samples = VK_SAMPLE_COUNT_1_BIT;
  a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // every frame clears all three
  a.finalLayout = final_layout;
  return a;
}

// Color ends in SHADER_READ_ONLY_OPTIMAL, the layout Raven samples it in;
// pick and depth end in TRANSFER_SRC_OPTIMAL for the readbacks.
VkRenderPass create_render_pass(VkDevice device) {
  const std::array<VkAttachmentDescription, 3> attachments{
      attachment(kColorFormat3d, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
      attachment(kPickFormat3d, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      attachment(kDepthFormat3d, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)};
  const std::array<VkAttachmentReference, 2> colors{
      VkAttachmentReference{kColorAttachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
      VkAttachmentReference{kPickAttachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
  const VkAttachmentReference depth{kDepthAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = static_cast<std::uint32_t>(colors.size());
  subpass.pColorAttachments = colors.data();
  subpass.pDepthStencilAttachment = &depth;

  constexpr VkPipelineStageFlags kAttachmentStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                     VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  constexpr VkAccessFlags kAttachmentWrites =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  std::array<VkSubpassDependency, 2> deps{};
  // In: Raven's earlier sampling of this color image and the readback copies
  // finish before the clears overwrite the images.
  deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  deps[0].dstSubpass = 0;
  deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
  deps[0].dstStageMask = kAttachmentStages;
  deps[0].dstAccessMask = kAttachmentWrites;
  // Out: the writes are visible to Raven's fragment shader and to the copies.
  deps[1].srcSubpass = 0;
  deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  deps[1].srcStageMask = kAttachmentStages;
  deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
  deps[1].srcAccessMask = kAttachmentWrites;
  deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
  info.pAttachments = attachments.data();
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = static_cast<std::uint32_t>(deps.size());
  info.pDependencies = deps.data();
  VkRenderPass pass = VK_NULL_HANDLE;
  vk::check(vkCreateRenderPass(device, &info, nullptr, &pass), "vkCreateRenderPass (3D)");
  return pass;
}

VkImageView create_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
  VkImageViewCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.image = image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = format;
  info.subresourceRange = {aspect, 0, 1, 0, 1};
  VkImageView view = VK_NULL_HANDLE;
  vk::check(vkCreateImageView(device, &info, nullptr, &view), "vkCreateImageView (3D)");
  return view;
}

struct ImageSpec {
  VkFormat format;
  VkImageUsageFlags usage;
  VkImageAspectFlags aspect;
  VkImageCreateFlags flags;
};

/// Creates the image, binds device-local memory, and a view in its format.
/// Fills `out` as it goes so that a throw leaves nothing unowned.
void create_image(VkPhysicalDevice physical, VkDevice device, const ImageSpec& spec, int width, int height,
                  TargetImage& out) {
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.flags = spec.flags;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = spec.format;
  info.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = spec.usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vk::check(vkCreateImage(device, &info, nullptr, &out.image), "vkCreateImage (3D)");
  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(device, out.image, &req);
  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex = vk::find_memory_type(physical, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vk::check(vkAllocateMemory(device, &alloc, nullptr, &out.memory), "vkAllocateMemory (3D target)");
  vk::check(vkBindImageMemory(device, out.image, out.memory, 0), "vkBindImageMemory (3D)");
  out.view = create_view(device, out.image, spec.format, spec.aspect);
}

void destroy_image(VkDevice device, TargetImage& image) noexcept {
  if (image.view != VK_NULL_HANDLE) vkDestroyImageView(device, image.view, nullptr);
  if (image.image != VK_NULL_HANDLE) vkDestroyImage(device, image.image, nullptr);
  if (image.memory != VK_NULL_HANDLE) vkFreeMemory(device, image.memory, nullptr);
  image = TargetImage{};
}

VkImageMemoryBarrier color_barrier(VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags src,
                                   VkAccessFlags dst) {
  VkImageMemoryBarrier b{};
  b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  b.srcAccessMask = src;
  b.dstAccessMask = dst;
  b.oldLayout = from;
  b.newLayout = to;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = image;
  b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  return b;
}

/// Copies a whole image (in TRANSFER_SRC_OPTIMAL) to `buffer` at `offset`.
void copy_image(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, int width, int height,
                VkBuffer buffer, VkDeviceSize offset) {
  VkBufferImageCopy region{};
  region.bufferOffset = offset;
  region.imageSubresource = {aspect, 0, 0, 1};
  region.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
  vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
}

/// The color image goes SHADER_READ_ONLY -> TRANSFER_SRC for the copy and back.
void copy_color(VkCommandBuffer cmd, const TargetBuffer& b, VkBuffer buffer) {
  const VkImageMemoryBarrier to_src = color_barrier(b.color.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0,
                                                    VK_ACCESS_TRANSFER_READ_BIT);
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                       0, nullptr, 1, &to_src);
  copy_image(cmd, b.color.image, VK_IMAGE_ASPECT_COLOR_BIT, b.width, b.height, buffer, 0);
  const VkImageMemoryBarrier back = color_barrier(b.color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                  VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                       0, nullptr, 1, &back);
}

template <typename T>
std::vector<T> read_mapped(VkDevice device, const vk::Buffer& host, VkDeviceSize offset, std::size_t count) {
  std::vector<T> out(count);
  void* mapped = nullptr;
  vk::check(vkMapMemory(device, host.memory, offset, count * sizeof(T), 0, &mapped), "vkMapMemory (3D readback)");
  std::memcpy(out.data(), mapped, count * sizeof(T));
  vkUnmapMemory(device, host.memory);
  return out;
}

}  // namespace

// --- Impl ------------------------------------------------------------------

VulkanOffscreenTarget::Impl::Impl(const VulkanDevice& d, int w, int h, QueueLock queue_lock)
    : physical(vk::from_opaque<VkPhysicalDevice>(d.physical_device)),
      device(vk::from_opaque<VkDevice>(d.device)),
      queue(vk::from_opaque<VkQueue>(d.queue)),
      family(d.queue_family),
      lock(std::move(queue_lock)),
      width(w),
      height(h) {
  if (w <= 0 || h <= 0) {
    throw std::invalid_argument("VulkanOffscreenTarget: the size must be positive");
  }
  require_formats(physical);
  try {
    pass = create_render_pass(device);
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = family;
    vk::check(vkCreateCommandPool(device, &pool_info, nullptr, &pool), "vkCreateCommandPool (3D)");
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    vk::check(vkAllocateCommandBuffers(device, &alloc, &cmd), "vkAllocateCommandBuffers (3D)");
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vk::check(vkCreateFence(device, &fence_info, nullptr, &fence), "vkCreateFence (3D)");
    create_buffer(buffers[static_cast<std::size_t>(back)], width, height);
  } catch (...) {
    destroy();
    throw;
  }
}

VulkanOffscreenTarget::Impl::~Impl() {
  destroy();
}

void VulkanOffscreenTarget::Impl::create_buffer(TargetBuffer& b, int w, int h) {
  b.width = w;
  b.height = h;
  b.drawn = false;
  create_image(physical, device,
               ImageSpec{kColorFormat3d, kColorUsage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT},
               w, h, b.color);
  b.shown_view = create_view(device, b.color.image, kShownFormat3d, VK_IMAGE_ASPECT_COLOR_BIT);
  create_image(physical, device, ImageSpec{kPickFormat3d, kPickUsage, VK_IMAGE_ASPECT_COLOR_BIT, 0}, w, h, b.pick);
  create_image(physical, device, ImageSpec{kDepthFormat3d, kDepthUsage, VK_IMAGE_ASPECT_DEPTH_BIT, 0}, w, h,
               b.depth);
  const std::array<VkImageView, 3> views{b.color.view, b.pick.view, b.depth.view};
  VkFramebufferCreateInfo fb{};
  fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb.renderPass = pass;
  fb.attachmentCount = static_cast<std::uint32_t>(views.size());
  fb.pAttachments = views.data();
  fb.width = static_cast<std::uint32_t>(w);
  fb.height = static_cast<std::uint32_t>(h);
  fb.layers = 1;
  vk::check(vkCreateFramebuffer(device, &fb, nullptr, &b.framebuffer), "vkCreateFramebuffer (3D)");
}

void VulkanOffscreenTarget::Impl::destroy_buffer(TargetBuffer& b) noexcept {
  if (b.framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, b.framebuffer, nullptr);
  if (b.shown_view != VK_NULL_HANDLE) vkDestroyImageView(device, b.shown_view, nullptr);
  destroy_image(device, b.color);
  destroy_image(device, b.pick);
  destroy_image(device, b.depth);
  b = TargetBuffer{};
}

void VulkanOffscreenTarget::Impl::destroy() noexcept {
  if (fence != VK_NULL_HANDLE) {
    // Our own submissions are waited for in submit_and_wait; nothing is in flight.
    vkDestroyFence(device, fence, nullptr);
    fence = VK_NULL_HANDLE;
  }
  for (TargetBuffer& b : buffers) {
    destroy_buffer(b);
  }
  if (pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, pool, nullptr);  // frees `cmd`
    pool = VK_NULL_HANDLE;
    cmd = VK_NULL_HANDLE;
  }
  if (pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, pass, nullptr);
    pass = VK_NULL_HANDLE;
  }
}

void VulkanOffscreenTarget::Impl::begin_commands() {
  vk::check(vkResetCommandBuffer(cmd, 0), "vkResetCommandBuffer (3D)");
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vk::check(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer (3D)");
}

void VulkanOffscreenTarget::Impl::submit_and_wait() {
  vk::check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (3D)");
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  if (lock.lock) lock.lock();
  const VkResult result = vkQueueSubmit(queue, 1, &submit, fence);
  if (lock.unlock) lock.unlock();
  vk::check(result, "vkQueueSubmit (3D)");
  vk::check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences (3D)");
  vk::check(vkResetFences(device, 1, &fence), "vkResetFences (3D)");
}

FrameRecording VulkanOffscreenTarget::Impl::begin_frame(const std::array<float, 4>& clear) {
  if (recording) {
    throw std::logic_error("VulkanOffscreenTarget: begin_frame while a frame is recording");
  }
  TargetBuffer& b = buffers[static_cast<std::size_t>(back)];
  if (b.width != width || b.height != height) {
    destroy_buffer(b);  // the caller re-pointed the viewport after the last frame (protocol)
    create_buffer(b, width, height);
  }
  begin_commands();
  recording = true;
  std::array<VkClearValue, 3> clears{};
  clears[kColorAttachment].color = {{clear[0], clear[1], clear[2], clear[3]}};
  clears[kPickAttachment].color.uint32[0] = 0;  // view3d::kNoPick
  clears[kDepthAttachment].depthStencil = {1.0F, 0};
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = pass;
  begin.framebuffer = b.framebuffer;
  begin.renderArea.extent = {static_cast<std::uint32_t>(b.width), static_cast<std::uint32_t>(b.height)};
  begin.clearValueCount = static_cast<std::uint32_t>(clears.size());
  begin.pClearValues = clears.data();
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
  return FrameRecording{cmd, pass, b.width, b.height};
}

void VulkanOffscreenTarget::Impl::end_frame() {
  if (!recording) {
    throw std::logic_error("VulkanOffscreenTarget: end_frame without begin_frame");
  }
  recording = false;
  vkCmdEndRenderPass(cmd);
  submit_and_wait();
  buffers[static_cast<std::size_t>(back)].drawn = true;
  back = 1 - back;
  ++frames;
}

// --- VulkanOffscreenTarget -------------------------------------------------------

VulkanOffscreenTarget::VulkanOffscreenTarget(const VulkanDevice& device, int width, int height, QueueLock lock)
    : impl_(std::make_unique<Impl>(device, width, height, std::move(lock))) {}

VulkanOffscreenTarget::~VulkanOffscreenTarget() = default;

int VulkanOffscreenTarget::width() const noexcept {
  return impl_->width;
}

int VulkanOffscreenTarget::height() const noexcept {
  return impl_->height;
}

void VulkanOffscreenTarget::resize(int width, int height) {
  impl_->width = std::max(width, 1);
  impl_->height = std::max(height, 1);
}

ShownImage VulkanOffscreenTarget::shown() const noexcept {
  const TargetBuffer& b = impl_->shown_buffer();
  if (!b.drawn) {
    return {};
  }
  return ShownImage{reinterpret_cast<std::uint64_t>(b.shown_view), b.width, b.height};  // NOLINT
}

std::uint64_t VulkanOffscreenTarget::frame_count() const noexcept {
  return impl_->frames;
}

TargetReadback VulkanOffscreenTarget::read_back() {
  const TargetBuffer& b = impl_->shown_buffer();
  if (!b.drawn) {
    return {};
  }
  const auto pixels = static_cast<std::size_t>(b.width) * static_cast<std::size_t>(b.height);
  const VkDeviceSize plane = pixels * 4U;  // every image has 4 bytes per pixel
  vk::Buffer host = vk::create_buffer(impl_->physical, impl_->device, plane * 3U, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  TargetReadback out;
  try {
    impl_->run_once([&](VkCommandBuffer cmd) {
      copy_color(cmd, b, host.buffer);
      copy_image(cmd, b.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, b.width, b.height, host.buffer, plane);
      copy_image(cmd, b.pick.image, VK_IMAGE_ASPECT_COLOR_BIT, b.width, b.height, host.buffer, plane * 2U);
    });
    out.width = b.width;
    out.height = b.height;
    out.color = read_mapped<std::uint32_t>(impl_->device, host, 0, pixels);
    out.depth = read_mapped<float>(impl_->device, host, plane, pixels);
    out.pick = read_mapped<std::uint32_t>(impl_->device, host, plane * 2U, pixels);
  } catch (...) {
    vk::destroy_buffer(impl_->device, host);
    throw;
  }
  vk::destroy_buffer(impl_->device, host);
  return out;
}

}  // namespace hefesto::render
