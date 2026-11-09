// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/renderer.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

#include "generated/rect_spv.hpp"
#include "vk_util.hpp"

static_assert(sizeof(void*) == 8, "the renderer passes non-dispatchable Vulkan handles as pointers");

namespace hefesto::render {

namespace {

/// Push constants, matching `View` in shaders/rect.vert.
struct PushConstants {
  std::int32_t origin[2];
  float scale[2];
  float offset[2];
  float min_size[2];
  float color[4];
  std::uint32_t stipple[4];  // rows 0-3, rows 4-7, enabled, line pattern
  std::int32_t anchor[4];    // left pixel column, bottom pixel row, width, height
  std::int32_t base[4];      // polygon base x, y, mode (0 quads, 1 diagonals), unused
};
static_assert(sizeof(PushConstants) == 96);

/// The three pipelines: instanced quads, instanced diagonals (line list) and
/// plain triangles (polygons).
enum class PipelineKind : std::uint8_t { quads, diagonals, polygons };
constexpr std::size_t kPipelineCount = 3;

constexpr VkShaderStageFlags kPushStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

std::uint32_t pack_rows(const std::array<std::uint8_t, 8>& rows, std::size_t first) {
  std::uint32_t word = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    word |= std::uint32_t{rows[first + i]} << (8U * i);
  }
  return word;
}

/// Staging chunk: bounds the host memory an upload needs, whatever the scene size.
constexpr VkDeviceSize kStagingChunk = VkDeviceSize{32} << 20U;

/// An instance buffer replaced by an upload is freed only after this many
/// further record() calls, so frames still in flight never read freed memory.
constexpr std::uint64_t kRetireAfterRecords = 4;

class ScopedQueueLock {
 public:
  explicit ScopedQueueLock(const QueueLock& lock) : lock_(lock) {
    if (lock_.lock) {
      lock_.lock();
    }
  }
  ~ScopedQueueLock() {
    if (lock_.unlock) {
      lock_.unlock();
    }
  }
  ScopedQueueLock(const ScopedQueueLock&) = delete;
  ScopedQueueLock& operator=(const ScopedQueueLock&) = delete;

 private:
  const QueueLock& lock_;
};

VkShaderModule make_shader(VkDevice device, const std::uint32_t* code, std::size_t bytes) {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = bytes;
  info.pCode = code;
  VkShaderModule module = VK_NULL_HANDLE;
  vk::check(vkCreateShaderModule(device, &info, nullptr, &module), "vkCreateShaderModule");
  return module;
}

/// Fixed-function state of the rectangle pipeline, kept together so that the
/// create-info pointers stay valid until vkCreateGraphicsPipelines returns.
struct PipelineState {
  VkPipelineShaderStageCreateInfo stages[2]{};
  VkVertexInputBindingDescription binding{};
  VkVertexInputAttributeDescription attribute{};
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  VkPipelineViewportStateCreateInfo viewport{};
  VkPipelineRasterizationStateCreateInfo raster{};
  VkPipelineMultisampleStateCreateInfo multisample{};
  VkPipelineColorBlendAttachmentState blend_attachment{};
  VkPipelineColorBlendStateCreateInfo blend{};
  VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};

  PipelineState(VkShaderModule vert, VkShaderModule frag, PipelineKind kind) {
    for (auto& stage : stages) {
      stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stage.pName = "main";
    }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    init_vertex_input(kind);
    init_raster_and_blend();
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;
  }
  PipelineState(const PipelineState&) = delete;
  PipelineState& operator=(const PipelineState&) = delete;

  void init_vertex_input(PipelineKind kind) {
    const bool polygons = kind == PipelineKind::polygons;
    binding.binding = 0;
    binding.stride = polygons ? sizeof(PolygonVertex) : sizeof(RectInstance);
    binding.inputRate = polygons ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
    attribute.location = 0;
    attribute.binding = 0;
    attribute.format = polygons ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32A32_SINT;
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &attribute;
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology =
        kind == PipelineKind::diagonals ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }

  void init_raster_and_blend() {
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1.0F;
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    // Straight alpha "over" blending: layers are composited in layer order.
    blend_attachment.blendEnable = VK_TRUE;
    blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attachment;
  }
};

PushConstants make_push_constants(const ViewTransform& view, const LayerStyle& style, const DrawTarget& target,
                                  const std::int32_t (&base)[2]) {
  return PushConstants{{view.origin_x, view.origin_y},
                       {view.scale_x, view.scale_y},
                       {view.offset_x, view.offset_y},
                       {view.min_size_x, view.min_size_y},
                       {style.r, style.g, style.b, style.a},
                       {pack_rows(style.stipple, 0), pack_rows(style.stipple, 4), style.stippled ? 1U : 0U,
                        std::uint32_t{style.dash}},
                       {target.x, target.y + target.height - 1, target.width, target.height},
                       {base[0], base[1], style.diagonals ? 1 : 0, 0}};
}

/// One draw of the merged rectangle and polygon draw lists.
struct DrawStep {
  const LayerBatch* batch = nullptr;
  bool polygon = false;
};

/// Rectangle and polygon draws merged by layer (both lists are in layer
/// order), rectangles first within a layer.
std::vector<DrawStep> merge_draws(std::span<const LayerBatch> rects, std::span<const LayerBatch> polygons) {
  std::vector<DrawStep> steps;
  steps.reserve(rects.size() + polygons.size());
  std::size_t i = 0;
  std::size_t j = 0;
  while (i < rects.size() || j < polygons.size()) {
    const bool take_rect = j >= polygons.size() || (i < rects.size() && rects[i].layer <= polygons[j].layer);
    steps.push_back(take_rect ? DrawStep{&rects[i++], false} : DrawStep{&polygons[j++], true});
  }
  return steps;
}

}  // namespace

std::uint32_t vulkan_header_version() noexcept {
  return VK_HEADER_VERSION_COMPLETE;
}

struct CanvasRenderer::Impl {
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  std::uint32_t queue_family = 0;
  QueueLock queue_lock;

  VkShaderModule vert = VK_NULL_HANDLE;
  VkShaderModule poly_vert = VK_NULL_HANDLE;
  VkShaderModule frag = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkRenderPass pipeline_pass = VK_NULL_HANDLE;
  std::array<VkPipeline, kPipelineCount> pipelines{};

  std::mutex scene_mutex;  // guards instances, polygons, styles, retired, records
  vk::Buffer instances;
  vk::Buffer polygons;
  std::int32_t polygon_base[2] = {0, 0};
  std::vector<LayerStyle> styles = spike_layer_styles();
  struct Retired {
    vk::Buffer buffer;
    std::uint64_t retired_at = 0;
  };
  std::vector<Retired> retired;
  std::uint64_t records = 0;

  Impl(const VulkanDevice& d, QueueLock lock)
      : physical(vk::from_opaque<VkPhysicalDevice>(d.physical_device)),
        device(vk::from_opaque<VkDevice>(d.device)),
        queue(vk::from_opaque<VkQueue>(d.queue)),
        queue_family(d.queue_family),
        queue_lock(std::move(lock)) {
    if (device == VK_NULL_HANDLE || physical == VK_NULL_HANDLE || queue == VK_NULL_HANDLE) {
      throw VulkanError("CanvasRenderer: incomplete VulkanDevice");
    }
    vert = make_shader(device, spv::kRectVertSpv, sizeof(spv::kRectVertSpv));
    poly_vert = make_shader(device, spv::kPolyVertSpv, sizeof(spv::kPolyVertSpv));
    frag = make_shader(device, spv::kRectFragSpv, sizeof(spv::kRectFragSpv));
    VkPushConstantRange range{kPushStages, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &range;
    vk::check(vkCreatePipelineLayout(device, &info, nullptr, &layout), "vkCreatePipelineLayout");
  }

  ~Impl() {
    {
      const ScopedQueueLock guard(queue_lock);
      vkDeviceWaitIdle(device);
    }
    for (auto& r : retired) {
      vk::destroy_buffer(device, r.buffer);
    }
    vk::destroy_buffer(device, instances);
    vk::destroy_buffer(device, polygons);
    destroy_pipelines();
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
    vkDestroyShaderModule(device, poly_vert, nullptr);
    vkDestroyShaderModule(device, vert, nullptr);
  }
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void destroy_pipelines() {
    for (VkPipeline& p : pipelines) {
      if (p != VK_NULL_HANDLE) vkDestroyPipeline(device, p, nullptr);
      p = VK_NULL_HANDLE;
    }
  }

  void ensure_pipelines(VkRenderPass pass) {
    if (pass == pipeline_pass && pipelines[0] != VK_NULL_HANDLE) {
      return;
    }
    destroy_pipelines();
    for (std::size_t i = 0; i < kPipelineCount; ++i) {
      const auto kind = static_cast<PipelineKind>(i);
      pipelines[i] = create_pipeline(pass, kind);
    }
    pipeline_pass = pass;
  }

  [[nodiscard]] VkPipeline pipeline(PipelineKind kind) const { return pipelines[static_cast<std::size_t>(kind)]; }

  VkPipeline create_pipeline(VkRenderPass pass, PipelineKind kind) {
    PipelineState state(kind == PipelineKind::polygons ? poly_vert : vert, frag, kind);
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = state.stages;
    info.pVertexInputState = &state.vertex_input;
    info.pInputAssemblyState = &state.assembly;
    info.pViewportState = &state.viewport;
    info.pRasterizationState = &state.raster;
    info.pMultisampleState = &state.multisample;
    info.pColorBlendState = &state.blend;
    info.pDynamicState = &state.dynamic;
    info.layout = layout;
    info.renderPass = pass;
    VkPipeline created = VK_NULL_HANDLE;
    vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &created),
              "vkCreateGraphicsPipelines");
    return created;
  }

  /// Copies `bytes` of `src` to a new device-local vertex buffer (empty when bytes is 0).
  vk::Buffer make_vertex_buffer(const void* src, VkDeviceSize bytes) {
    vk::Buffer fresh;
    if (bytes == 0) return fresh;
    fresh = vk::create_buffer(physical, device, bytes,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    copy_through_staging(static_cast<const std::uint8_t*>(src), bytes, fresh.buffer);
    return fresh;
  }

  void clear_area(VkCommandBuffer cmd, const DrawTarget& target) const {
    VkClearAttachment attachment{};
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.colorAttachment = 0;
    attachment.clearValue.color = {{target.clear_color.r, target.clear_color.g, target.clear_color.b,
                                    target.clear_color.a}};
    const VkClearRect rect{{{target.x, target.y},
                            {static_cast<std::uint32_t>(target.width), static_cast<std::uint32_t>(target.height)}},
                           0,
                           1};
    vkCmdClearAttachments(cmd, 1, &attachment, 1, &rect);
  }

  /// Records one draw; binds its pipeline and buffer when they change.
  void record_step(VkCommandBuffer cmd, const DrawStep& step, const ViewTransform& view, const DrawTarget& target,
                   std::uint32_t& pushed_layer, VkPipeline& bound) {
    const LayerBatch& batch = *step.batch;
    const vk::Buffer& buffer = step.polygon ? polygons : instances;
    const std::uint64_t capacity = buffer.size / (step.polygon ? sizeof(PolygonVertex) : sizeof(RectInstance));
    if (batch.count == 0 || std::uint64_t{batch.first} + batch.count > capacity) {
      return;  // never read past the buffer, even with stale ranges
    }
    static const LayerStyle fallback{};
    const LayerStyle& style = batch.layer < styles.size() ? styles[batch.layer] : fallback;
    const PipelineKind kind =
        step.polygon ? PipelineKind::polygons : (style.diagonals ? PipelineKind::diagonals : PipelineKind::quads);
    if (pipeline(kind) != bound) {
      bound = pipeline(kind);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bound);
      const VkDeviceSize zero = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &buffer.buffer, &zero);
      pushed_layer = std::numeric_limits<std::uint32_t>::max();
    }
    if (batch.layer != pushed_layer) {
      const PushConstants pc = make_push_constants(view, style, target, polygon_base);
      vkCmdPushConstants(cmd, layout, kPushStages, 0, sizeof(pc), &pc);
      pushed_layer = batch.layer;
    }
    switch (kind) {
      case PipelineKind::quads: vkCmdDraw(cmd, 6, batch.count, 0, batch.first); break;
      case PipelineKind::diagonals: vkCmdDraw(cmd, 4, batch.count, 0, batch.first); break;
      case PipelineKind::polygons: vkCmdDraw(cmd, batch.count, 1, batch.first, 0); break;
    }
  }

  /// Runs `record_fn` in a one-shot command buffer on the queue and waits for it.
  template <typename Fn>
  void submit_and_wait(VkCommandPool pool, Fn&& record_fn) {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vk::check(vkAllocateCommandBuffers(device, &alloc, &cmd), "vkAllocateCommandBuffers");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk::check(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer");
    std::forward<Fn>(record_fn)(cmd);
    vk::check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vk::check(vkCreateFence(device, &fence_info, nullptr, &fence), "vkCreateFence");
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VkResult result = VK_SUCCESS;
    {
      const ScopedQueueLock guard(queue_lock);
      result = vkQueueSubmit(queue, 1, &submit, fence);
    }
    if (result == VK_SUCCESS) {
      result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
    vk::check(result, "vkQueueSubmit/vkWaitForFences (upload)");
  }

  /// Copies `bytes` into `dst` chunk by chunk through one reusable staging buffer.
  void copy_through_staging(const std::uint8_t* src, VkDeviceSize bytes, VkBuffer dst) {
    const VkDeviceSize chunk = std::min(bytes, kStagingChunk);
    vk::Buffer staging = vk::create_buffer(
        physical, device, chunk, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = queue_family;
    VkCommandPool pool = VK_NULL_HANDLE;
    vk::check(vkCreateCommandPool(device, &pool_info, nullptr, &pool), "vkCreateCommandPool");
    void* mapped = nullptr;
    vk::check(vkMapMemory(device, staging.memory, 0, chunk, 0, &mapped), "vkMapMemory");
    for (VkDeviceSize done = 0; done < bytes; done += chunk) {
      const VkDeviceSize n = std::min(chunk, bytes - done);
      std::memcpy(mapped, src + done, static_cast<std::size_t>(n));
      submit_and_wait(pool, [&](VkCommandBuffer cmd) {
        const VkBufferCopy region{0, done, n};
        vkCmdCopyBuffer(cmd, staging.buffer, dst, 1, &region);
      });
    }
    vkUnmapMemory(device, staging.memory);
    vkDestroyCommandPool(device, pool, nullptr);
    vk::destroy_buffer(device, staging);
  }

  void free_retired_locked() {
    auto expired = [this](Retired& r) {
      if (records - r.retired_at < kRetireAfterRecords) {
        return false;
      }
      vk::destroy_buffer(device, r.buffer);
      return true;
    };
    std::erase_if(retired, expired);
  }
};

CanvasRenderer::CanvasRenderer(const VulkanDevice& device, QueueLock lock)
    : impl_(std::make_unique<Impl>(device, std::move(lock))) {}

CanvasRenderer::~CanvasRenderer() = default;
CanvasRenderer::CanvasRenderer(CanvasRenderer&&) noexcept = default;
CanvasRenderer& CanvasRenderer::operator=(CanvasRenderer&&) noexcept = default;

UploadStats CanvasRenderer::upload(std::span<const RectInstance> instances) {
  const auto start = std::chrono::steady_clock::now();
  const VkDeviceSize bytes = instances.size_bytes();
  vk::Buffer fresh = impl_->make_vertex_buffer(instances.data(), bytes);
  {
    const std::lock_guard guard(impl_->scene_mutex);
    impl_->retired.push_back({impl_->instances, impl_->records});
    impl_->instances = fresh;
  }
  const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
  return UploadStats{elapsed.count(), bytes};
}

UploadStats CanvasRenderer::upload_polygons(std::span<const PolygonVertex> vertices, std::int32_t base_x,
                                            std::int32_t base_y) {
  const auto start = std::chrono::steady_clock::now();
  const VkDeviceSize bytes = vertices.size_bytes();
  vk::Buffer fresh = impl_->make_vertex_buffer(vertices.data(), bytes);
  {
    const std::lock_guard guard(impl_->scene_mutex);
    impl_->retired.push_back({impl_->polygons, impl_->records});
    impl_->polygons = fresh;
    impl_->polygon_base[0] = base_x;
    impl_->polygon_base[1] = base_y;
  }
  const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
  return UploadStats{elapsed.count(), bytes};
}

void CanvasRenderer::set_layer_styles(std::span<const LayerStyle> styles) {
  const std::lock_guard guard(impl_->scene_mutex);
  impl_->styles.assign(styles.begin(), styles.end());
}

void CanvasRenderer::record(const DrawTarget& target, const ViewTransform& view, std::span<const LayerBatch> draws,
                            std::span<const LayerBatch> polygon_draws) {
  auto* cmd = vk::from_opaque<VkCommandBuffer>(target.command_buffer);
  const std::lock_guard guard(impl_->scene_mutex);
  ++impl_->records;
  impl_->free_retired_locked();
  if (target.clear) {
    impl_->clear_area(cmd, target);
  }
  if (impl_->instances.buffer == VK_NULL_HANDLE && impl_->polygons.buffer == VK_NULL_HANDLE) {
    return;
  }
  impl_->ensure_pipelines(vk::from_opaque<VkRenderPass>(target.render_pass));
  if (target.set_viewport) {
    const VkViewport vp{static_cast<float>(target.x), static_cast<float>(target.y),
                        static_cast<float>(target.width), static_cast<float>(target.height), 0.0F, 1.0F};
    const VkRect2D scissor{{target.x, target.y},
                           {static_cast<std::uint32_t>(target.width), static_cast<std::uint32_t>(target.height)}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
  }
  std::uint32_t pushed_layer = std::numeric_limits<std::uint32_t>::max();
  VkPipeline bound = VK_NULL_HANDLE;
  for (const DrawStep& step : merge_draws(draws, polygon_draws)) {
    impl_->record_step(cmd, step, view, target, pushed_layer, bound);
  }
}

std::uint64_t CanvasRenderer::device_bytes() const noexcept {
  const std::lock_guard guard(impl_->scene_mutex);
  return impl_->instances.size;
}

std::size_t CanvasRenderer::instance_count() const noexcept {
  const std::size_t count = device_bytes() / sizeof(RectInstance);  // 64-bit targets only
  return count;
}

}  // namespace hefesto::render
