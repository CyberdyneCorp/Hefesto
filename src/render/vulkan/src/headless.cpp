// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/headless.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <string_view>
#include <vector>

#include "vk_util.hpp"

namespace hefesto::render {

namespace {

// Spelled out: older Vulkan headers do not define these names.
constexpr const char* kPortabilityEnumeration = "VK_KHR_portability_enumeration";
constexpr const char* kPortabilitySubset = "VK_KHR_portability_subset";
constexpr const char* kMemoryBudget = "VK_EXT_memory_budget";
constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
constexpr VkInstanceCreateFlags kEnumeratePortabilityBit = 0x00000001;  // VK_..._PORTABILITY_BIT_KHR
constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;

bool has_instance_extension(const char* name) {
  std::uint32_t count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> props(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
  return std::ranges::any_of(props, [name](const auto& p) { return std::string_view(p.extensionName) == name; });
}

bool has_layer(const char* name) {
  std::uint32_t count = 0;
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> props(count);
  vkEnumerateInstanceLayerProperties(&count, props.data());
  return std::ranges::any_of(props, [name](const auto& p) { return std::string_view(p.layerName) == name; });
}

bool has_device_extension(VkPhysicalDevice physical, const char* name) {
  std::uint32_t count = 0;
  vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> props(count);
  vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, props.data());
  return std::ranges::any_of(props, [name](const auto& p) { return std::string_view(p.extensionName) == name; });
}

VkInstance create_instance(bool validation) {
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "hefesto-headless";
  app.pEngineName = "hefesto";
  app.apiVersion = VK_API_VERSION_1_1;

  std::vector<const char*> extensions;
  VkInstanceCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  info.pApplicationInfo = &app;
  if (has_instance_extension(kPortabilityEnumeration)) {
    extensions.push_back(kPortabilityEnumeration);
    info.flags |= kEnumeratePortabilityBit;
  }
  const char* layers[] = {kValidationLayer};
  if (validation && has_layer(kValidationLayer)) {
    info.enabledLayerCount = 1;
    info.ppEnabledLayerNames = layers;
  }
  info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
  info.ppEnabledExtensionNames = extensions.data();
  VkInstance instance = VK_NULL_HANDLE;
  vk::check(vkCreateInstance(&info, nullptr, &instance), "vkCreateInstance");
  return instance;
}

std::optional<std::uint32_t> graphics_family(VkPhysicalDevice physical) {
  std::uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
  for (std::uint32_t i = 0; i < count; ++i) {
    if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
      return i;
    }
  }
  return std::nullopt;
}

int device_rank(VkPhysicalDevice physical) {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(physical, &props);
  switch (props.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
    default: return 0;
  }
}

VkPhysicalDevice pick_physical_device(VkInstance instance) {
  std::uint32_t count = 0;
  vkEnumeratePhysicalDevices(instance, &count, nullptr);
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(instance, &count, devices.data());
  VkPhysicalDevice best = VK_NULL_HANDLE;
  int best_rank = -1;
  for (VkPhysicalDevice d : devices) {
    if (graphics_family(d) && device_rank(d) > best_rank) {
      best = d;
      best_rank = device_rank(d);
    }
  }
  if (best == VK_NULL_HANDLE) {
    throw VulkanError("no Vulkan device with a graphics queue");
  }
  return best;
}

}  // namespace

struct HeadlessCanvas::Impl {
  int width = 0;
  int height = 0;
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  std::uint32_t family = 0;
  bool memory_budget_ext = false;

  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory image_memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkRenderPass pass = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkQueryPool queries = VK_NULL_HANDLE;
  double timestamp_period_ns = 0.0;

  std::unique_ptr<CanvasRenderer> renderer;

  explicit Impl(const HeadlessOptions& o) : width(o.width), height(o.height) {
    try {
      instance = create_instance(o.enable_validation);
      physical = pick_physical_device(instance);
      family = *graphics_family(physical);
      create_device();
      create_target();
      create_commands();
      renderer = std::make_unique<CanvasRenderer>(
          VulkanDevice{vk::to_opaque(instance), vk::to_opaque(physical), vk::to_opaque(device),
                       vk::to_opaque(queue), family});
    } catch (...) {
      destroy();
      throw;
    }
  }
  ~Impl() { destroy(); }
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void create_device() {
    const float priority = 1.0F;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    std::vector<const char*> extensions;
    // The spec requires enabling portability_subset when the device has it (MoltenVK).
    if (has_device_extension(physical, kPortabilitySubset)) {
      extensions.push_back(kPortabilitySubset);
    }
    memory_budget_ext = has_device_extension(physical, kMemoryBudget);
    if (memory_budget_ext) {
      extensions.push_back(kMemoryBudget);
    }
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    vk::check(vkCreateDevice(physical, &info, nullptr, &device), "vkCreateDevice");
    vkGetDeviceQueue(device, family, 0, &queue);
  }

  void create_image() {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = kColorFormat;
    info.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    vk::check(vkCreateImage(device, &info, nullptr, &image), "vkCreateImage");
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, image, &req);
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = vk::find_memory_type(physical, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vk::check(vkAllocateMemory(device, &alloc, nullptr, &image_memory), "vkAllocateMemory (target)");
    vk::check(vkBindImageMemory(device, image, image_memory, 0), "vkBindImageMemory");

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = kColorFormat;
    view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vk::check(vkCreateImageView(device, &view_info, nullptr, &view), "vkCreateImageView");
  }

  void create_pass() {
    VkAttachmentDescription color{};
    color.format = kColorFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;
    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    vk::check(vkCreateRenderPass(device, &info, nullptr, &pass), "vkCreateRenderPass");

    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = pass;
    fb.attachmentCount = 1;
    fb.pAttachments = &view;
    fb.width = static_cast<std::uint32_t>(width);
    fb.height = static_cast<std::uint32_t>(height);
    fb.layers = 1;
    vk::check(vkCreateFramebuffer(device, &fb, nullptr, &framebuffer), "vkCreateFramebuffer");
  }

  void create_target() {
    create_image();
    create_pass();
  }

  void create_commands() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = family;
    vk::check(vkCreateCommandPool(device, &pool_info, nullptr, &pool), "vkCreateCommandPool");
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    vk::check(vkAllocateCommandBuffers(device, &alloc, &cmd), "vkAllocateCommandBuffers");
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vk::check(vkCreateFence(device, &fence_info, nullptr, &fence), "vkCreateFence");
    create_queries();
  }

  void create_queries() {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical, &props);
    if (families[family].timestampValidBits == 0 || props.limits.timestampPeriod <= 0.0F) {
      return;
    }
    timestamp_period_ns = static_cast<double>(props.limits.timestampPeriod);
    VkQueryPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = 2;
    vk::check(vkCreateQueryPool(device, &info, nullptr, &queries), "vkCreateQueryPool");
  }

  /// Destroys `handle` with `destroy_fn` when it was created.
  template <typename Handle, typename Fn>
  void release(Handle& handle, Fn destroy_fn) noexcept {
    if (handle != VK_NULL_HANDLE) {
      destroy_fn(device, handle, nullptr);
      handle = VK_NULL_HANDLE;
    }
  }

  void destroy() noexcept {
    if (device != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device);
      renderer.reset();
      release(queries, vkDestroyQueryPool);
      release(fence, vkDestroyFence);
      release(pool, vkDestroyCommandPool);
      release(framebuffer, vkDestroyFramebuffer);
      release(pass, vkDestroyRenderPass);
      release(view, vkDestroyImageView);
      release(image, vkDestroyImage);
      release(image_memory, vkFreeMemory);
      vkDestroyDevice(device, nullptr);
      device = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
      vkDestroyInstance(instance, nullptr);
      instance = VK_NULL_HANDLE;
    }
  }

  void begin_commands() {
    vk::check(vkResetCommandBuffer(cmd, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk::check(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer");
  }

  void submit_and_wait() {
    vk::check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vk::check(vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit");
    vk::check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    vk::check(vkResetFences(device, 1, &fence), "vkResetFences");
  }

  void record_frame(const ViewTransform& view_transform, std::span<const LayerBatch> draws,
                    std::span<const LayerBatch> polygon_draws) {
    if (queries != VK_NULL_HANDLE) {
      vkCmdResetQueryPool(cmd, queries, 0, 2);
      vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
    }
    VkClearValue clear{};
    clear.color = {{0.043F, 0.047F, 0.055F, 1.0F}};  // design-system "canvas" #0B0C0E
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = pass;
    begin.framebuffer = framebuffer;
    begin.renderArea.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
    begin.clearValueCount = 1;
    begin.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
    renderer->record(DrawTarget{vk::to_opaque(cmd), vk::to_opaque(pass), 0, 0, width, height, true, false, {}},
                     view_transform, draws, polygon_draws);
    vkCmdEndRenderPass(cmd);
    if (queries != VK_NULL_HANDLE) {
      vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
    }
  }

  std::optional<double> gpu_time_ms() {
    if (queries == VK_NULL_HANDLE) {
      return std::nullopt;
    }
    std::array<std::uint64_t, 2> ticks{};
    const VkResult r = vkGetQueryPoolResults(device, queries, 0, 2, sizeof(ticks), ticks.data(),
                                             sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT);
    if (r != VK_SUCCESS || ticks[1] < ticks[0]) {
      return std::nullopt;
    }
    return static_cast<double>(ticks[1] - ticks[0]) * timestamp_period_ns * 1e-6;
  }
};

HeadlessCanvas::HeadlessCanvas(const HeadlessOptions& options) : impl_(std::make_unique<Impl>(options)) {}
HeadlessCanvas::~HeadlessCanvas() = default;

CanvasRenderer& HeadlessCanvas::renderer() noexcept {
  return *impl_->renderer;
}
int HeadlessCanvas::width() const noexcept {
  return impl_->width;
}
int HeadlessCanvas::height() const noexcept {
  return impl_->height;
}

FrameTiming HeadlessCanvas::render(const ViewTransform& view, std::span<const LayerBatch> draws,
                                   std::span<const LayerBatch> polygon_draws) {
  const auto start = std::chrono::steady_clock::now();
  impl_->begin_commands();
  impl_->record_frame(view, draws, polygon_draws);
  impl_->submit_and_wait();
  const std::chrono::duration<double, std::milli> cpu = std::chrono::steady_clock::now() - start;
  return FrameTiming{cpu.count(), impl_->gpu_time_ms()};
}

std::uint32_t HeadlessCanvas::read_pixel(int x, int y) {
  vk::Buffer host = vk::create_buffer(impl_->physical, impl_->device, 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  impl_->begin_commands();
  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageOffset = {x, y, 0};
  region.imageExtent = {1, 1, 1};
  vkCmdCopyImageToBuffer(impl_->cmd, impl_->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, host.buffer, 1, &region);
  impl_->submit_and_wait();
  void* mapped = nullptr;
  std::uint32_t pixel = 0;
  if (vkMapMemory(impl_->device, host.memory, 0, 4, 0, &mapped) == VK_SUCCESS) {
    std::memcpy(&pixel, mapped, sizeof(pixel));
    vkUnmapMemory(impl_->device, host.memory);
  }
  vk::destroy_buffer(impl_->device, host);
  return pixel;
}

std::string HeadlessCanvas::device_name() const {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(impl_->physical, &props);
  return props.deviceName;
}

std::string HeadlessCanvas::driver_info() const {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(impl_->physical, &props);
  return "vendor " + std::to_string(props.vendorID) + ", driver " + std::to_string(props.driverVersion);
}

std::string HeadlessCanvas::api_version() const {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(impl_->physical, &props);
  return std::to_string(VK_API_VERSION_MAJOR(props.apiVersion)) + "." +
         std::to_string(VK_API_VERSION_MINOR(props.apiVersion)) + "." +
         std::to_string(VK_API_VERSION_PATCH(props.apiVersion));
}

MemoryBudget HeadlessCanvas::memory_budget() const {
  MemoryBudget out;
  if (!impl_->memory_budget_ext) {
    return out;
  }
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  VkPhysicalDeviceMemoryProperties2 props{};
  props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  props.pNext = &budget;
  vkGetPhysicalDeviceMemoryProperties2(impl_->physical, &props);
  for (std::uint32_t i = 0; i < props.memoryProperties.memoryHeapCount; ++i) {
    if ((props.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
      out.device_local_usage += budget.heapUsage[i];
      out.device_local_budget += budget.heapBudget[i];
    }
  }
  out.available = true;
  return out;
}

}  // namespace hefesto::render
