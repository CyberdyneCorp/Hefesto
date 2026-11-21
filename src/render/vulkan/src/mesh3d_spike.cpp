// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M10 renderer spike's test mesh and pipeline (tasks.md 5.1). Owner: RENDER3D.

#include "hefesto/render/mesh3d_spike.hpp"

#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "generated/mesh3d_spv.hpp"
#include "mesh3d_target_vk.hpp"
#include "vk_util.hpp"

namespace hefesto::render {

namespace {

struct Box {
  float x0;
  float y0;
  float x1;
  float y1;
  float z;
};

struct Rgb {
  float r;
  float g;
  float b;
};

/// Two triangles; counter-clockwise on screen (Vulkan framebuffer, y down)
/// unless `back_facing`.
void add_quad(std::vector<SpikeVertex>& out, const Box& q, const Rgb& c, SpikeQuad id, bool back_facing = false) {
  const auto v = [&](float x, float y) {
    return SpikeVertex{x, y, q.z, c.r, c.g, c.b, 1.0F, static_cast<std::uint32_t>(id)};
  };
  const SpikeVertex tl = v(q.x0, q.y0);
  const SpikeVertex bl = v(q.x0, q.y1);
  const SpikeVertex br = v(q.x1, q.y1);
  const SpikeVertex tr = v(q.x1, q.y0);
  if (back_facing) {
    out.insert(out.end(), {tl, br, bl, tl, tr, br});
  } else {
    out.insert(out.end(), {tl, bl, br, tl, br, tr});
  }
}

VkShaderModule make_shader(VkDevice device, const std::uint32_t* code, std::size_t bytes) {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = bytes;
  info.pCode = code;
  VkShaderModule module = VK_NULL_HANDLE;
  vk::check(vkCreateShaderModule(device, &info, nullptr, &module), "vkCreateShaderModule (3D spike)");
  return module;
}

VkPipelineLayout make_layout(VkDevice device) {
  VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SpikeTransform)};
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pushConstantRangeCount = 1;
  info.pPushConstantRanges = &range;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  vk::check(vkCreatePipelineLayout(device, &info, nullptr, &layout), "vkCreatePipelineLayout (3D spike)");
  return layout;
}

/// The fixed-function state of the spike pipeline: depth test LESS_OR_EQUAL
/// with writes, back faces culled, no blending, color writes to both the
/// color and the pick-id attachments.
struct FixedState {
  VkVertexInputBindingDescription binding{0, sizeof(SpikeVertex), VK_VERTEX_INPUT_RATE_VERTEX};
  std::array<VkVertexInputAttributeDescription, 3> attributes{
      VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SpikeVertex, x)},
      VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SpikeVertex, r)},
      VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32_UINT, offsetof(SpikeVertex, pick)}};
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  VkPipelineViewportStateCreateInfo viewport{};
  VkPipelineRasterizationStateCreateInfo raster{};
  VkPipelineMultisampleStateCreateInfo multisample{};
  VkPipelineDepthStencilStateCreateInfo depth{};
  std::array<VkPipelineColorBlendAttachmentState, 2> blend_attachments{};
  VkPipelineColorBlendStateCreateInfo blend{};
  std::array<VkDynamicState, 2> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};

  FixedState() {
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.data();
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0F;
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;  // Q-MISC-77: the last coplanar face wins
    blend_attachments[kColorAttachment].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachments[kPickAttachment].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;  // integer: never blended
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = static_cast<std::uint32_t>(blend_attachments.size());
    blend.pAttachments = blend_attachments.data();
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
    dynamic.pDynamicStates = dynamic_states.data();
  }
  FixedState(const FixedState&) = delete;
  FixedState& operator=(const FixedState&) = delete;
};

VkPipeline make_pipeline(VkDevice device, VkRenderPass pass, VkPipelineLayout layout, VkShaderModule vert,
                         VkShaderModule frag) {
  const FixedState s;
  std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag;
  stages[1].pName = "main";
  VkGraphicsPipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.stageCount = static_cast<std::uint32_t>(stages.size());
  info.pStages = stages.data();
  info.pVertexInputState = &s.vertex_input;
  info.pInputAssemblyState = &s.assembly;
  info.pViewportState = &s.viewport;
  info.pRasterizationState = &s.raster;
  info.pMultisampleState = &s.multisample;
  info.pDepthStencilState = &s.depth;
  info.pColorBlendState = &s.blend;
  info.pDynamicState = &s.dynamic;
  info.layout = layout;
  info.renderPass = pass;
  VkPipeline pipeline = VK_NULL_HANDLE;
  vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline),
            "vkCreateGraphicsPipelines (3D spike)");
  return pipeline;
}

}  // namespace

std::vector<SpikeVertex> spike_test_mesh() {
  std::vector<SpikeVertex> out;
  add_quad(out, Box{-1.0F, -1.0F, 0.5F, 0.5F, 0.25F}, Rgb{1.0F, 0.0F, 0.0F}, SpikeQuad::near_red);
  add_quad(out, Box{-0.5F, -1.0F, 1.0F, 0.5F, 0.75F}, Rgb{0.0F, 1.0F, 0.0F}, SpikeQuad::far_green);
  add_quad(out, Box{-1.0F, -1.0F, 0.0F, -0.5F, 0.25F}, Rgb{0.0F, 0.0F, 1.0F}, SpikeQuad::coplanar_blue);
  add_quad(out, Box{-1.0F, -1.0F, 1.0F, 1.0F, 0.1F}, Rgb{1.0F, 1.0F, 1.0F}, SpikeQuad::back_facing, true);
  return out;
}

struct Mesh3dSpike::Impl {
  VulkanOffscreenTarget::Impl* target = nullptr;
  VkDevice device = VK_NULL_HANDLE;
  VkShaderModule vert = VK_NULL_HANDLE;
  VkShaderModule frag = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  vk::Buffer vertices;
  std::uint32_t vertex_count = 0;

  explicit Impl(VulkanOffscreenTarget::Impl& t) : target(&t), device(t.device) {
    try {
      vert = make_shader(device, spv::kMesh3dSpikeVertSpv, sizeof(spv::kMesh3dSpikeVertSpv));
      frag = make_shader(device, spv::kMesh3dSpikeFragSpv, sizeof(spv::kMesh3dSpikeFragSpv));
      layout = make_layout(device);
      pipeline = make_pipeline(device, t.pass, layout, vert, frag);
      upload(t.physical);
    } catch (...) {
      destroy();
      throw;
    }
  }
  ~Impl() { destroy(); }
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  // A host-visible buffer is enough for the spike's 24 vertices.
  void upload(VkPhysicalDevice physical) {
    const std::vector<SpikeVertex> mesh = spike_test_mesh();
    const VkDeviceSize bytes = mesh.size() * sizeof(SpikeVertex);
    vertices = vk::create_buffer(physical, device, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mapped = nullptr;
    vk::check(vkMapMemory(device, vertices.memory, 0, bytes, 0, &mapped), "vkMapMemory (3D spike)");
    std::memcpy(mapped, mesh.data(), static_cast<std::size_t>(bytes));
    vkUnmapMemory(device, vertices.memory);
    vertex_count = static_cast<std::uint32_t>(mesh.size());
  }

  void destroy() noexcept {
    vk::destroy_buffer(device, vertices);
    if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline, nullptr);
    if (layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout, nullptr);
    if (frag != VK_NULL_HANDLE) vkDestroyShaderModule(device, frag, nullptr);
    if (vert != VK_NULL_HANDLE) vkDestroyShaderModule(device, vert, nullptr);
    pipeline = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
    frag = VK_NULL_HANDLE;
    vert = VK_NULL_HANDLE;
  }

  void record(const FrameRecording& f, const SpikeTransform& transform) const {
    const VkViewport viewport{0.0F, 0.0F, static_cast<float>(f.width), static_cast<float>(f.height), 0.0F, 1.0F};
    const VkRect2D scissor{{0, 0}, {static_cast<std::uint32_t>(f.width), static_cast<std::uint32_t>(f.height)}};
    vkCmdSetViewport(f.cmd, 0, 1, &viewport);
    vkCmdSetScissor(f.cmd, 0, 1, &scissor);
    vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdPushConstants(f.cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SpikeTransform), transform.data());
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(f.cmd, 0, 1, &vertices.buffer, &offset);
    vkCmdDraw(f.cmd, vertex_count, 1, 0, 0);
  }
};

Mesh3dSpike::Mesh3dSpike(VulkanOffscreenTarget& target) : impl_(std::make_unique<Impl>(target.impl())) {}

Mesh3dSpike::~Mesh3dSpike() = default;

void Mesh3dSpike::draw(VulkanOffscreenTarget& target, const SpikeTransform& transform) {
  if (&target.impl() != impl_->target) {
    throw std::invalid_argument("Mesh3dSpike::draw: not the target the spike was built for");
  }
  const FrameRecording frame = impl_->target->begin_frame(kSpikeBackground);
  impl_->record(frame, transform);
  impl_->target->end_frame();
}

}  // namespace hefesto::render
