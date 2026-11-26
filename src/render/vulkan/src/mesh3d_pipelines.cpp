// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// VulkanMesh3dRenderer's pipelines, multisampled pass and images (design.md
// D8, tasks.md 5.3). Owner: RENDER3D.

#include <cstddef>
#include <vector>

#include "generated/mesh3d_renderer_spv.hpp"
#include "mesh3d_pipelines_vk.hpp"
#include "mesh3d_target_vk.hpp"
#include "vk_util.hpp"

namespace hefesto::render::mesh3d {

namespace {

constexpr VkColorComponentFlags kRgba =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

VkShaderModule make_shader(VkDevice device, const std::uint32_t* code, std::size_t bytes) {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = bytes;
  info.pCode = code;
  VkShaderModule module = VK_NULL_HANDLE;
  vk::check(vkCreateShaderModule(device, &info, nullptr, &module), "vkCreateShaderModule (3D)");
  return module;
}

/// Whether the device can draw 4x multisampled color and depth of the
/// target's formats.
bool supports_msaa(VkPhysicalDevice physical) {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(physical, &props);
  const VkSampleCountFlags limits = props.limits.framebufferColorSampleCounts &
                                    props.limits.framebufferDepthSampleCounts;
  if ((limits & kMsaaSamples) == 0) {
    return false;
  }
  const auto format_ok = [&](VkFormat format, VkImageUsageFlags usage) {
    VkImageFormatProperties fp{};
    const VkResult r = vkGetPhysicalDeviceImageFormatProperties(physical, format, VK_IMAGE_TYPE_2D,
                                                                VK_IMAGE_TILING_OPTIMAL, usage, 0, &fp);
    return r == VK_SUCCESS && (fp.sampleCounts & kMsaaSamples) != 0;
  };
  return format_ok(kColorFormat3d, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) &&
         format_ok(kDepthFormat3d,
                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
}

VkAttachmentDescription msaa_attachment(VkFormat format, VkSampleCountFlagBits samples) {
  VkAttachmentDescription a{};
  a.format = format;
  a.samples = samples;
  a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  a.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  a.finalLayout = format == kDepthFormat3d ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                           : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  return a;
}

// Attachments: 0 the 4x color, 1 the 4x depth, 2 the target's color image
// (its sRGB view), which the resolve overwrites whole and leaves in
// SHADER_READ_ONLY_OPTIMAL as the target pass does.
VkRenderPass create_msaa_pass(VkDevice device) {
  VkAttachmentDescription resolve = msaa_attachment(kColorFormat3d, VK_SAMPLE_COUNT_1_BIT);
  resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  resolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  const std::array<VkAttachmentDescription, 3> attachments{msaa_attachment(kColorFormat3d, kMsaaSamples),
                                                           msaa_attachment(kDepthFormat3d, kMsaaSamples), resolve};
  const VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  const VkAttachmentReference depth{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  const VkAttachmentReference resolved{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color;
  subpass.pResolveAttachments = &resolved;
  subpass.pDepthStencilAttachment = &depth;

  constexpr VkPipelineStageFlags kStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  constexpr VkAccessFlags kWrites = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  std::array<VkSubpassDependency, 2> deps{};
  // In: the target pass of this frame (and Raven's earlier sampling, which
  // that pass already waited for) finish writing before the resolve.
  deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  deps[0].dstSubpass = 0;
  deps[0].srcStageMask = kStages | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
  deps[0].srcAccessMask = kWrites;
  deps[0].dstStageMask = kStages;
  deps[0].dstAccessMask = kWrites;
  // Out: the resolved color is visible to Raven's fragment shader and the copies.
  deps[1].srcSubpass = 0;
  deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  deps[1].srcStageMask = kStages;
  deps[1].srcAccessMask = kWrites;
  deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
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
  vk::check(vkCreateRenderPass(device, &info, nullptr, &pass), "vkCreateRenderPass (3D msaa)");
  return pass;
}

VkDescriptorSetLayout create_set_layout(VkDevice device) {
  std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
  for (std::uint32_t i = 0; i < bindings.size(); ++i) {
    bindings[i].binding = i;  // 0: layers, 1: highlight bits
    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  }
  VkDescriptorSetLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = static_cast<std::uint32_t>(bindings.size());
  info.pBindings = bindings.data();
  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  vk::check(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout), "vkCreateDescriptorSetLayout (3D)");
  return layout;
}

VkPipelineLayout create_layout(VkDevice device, VkDescriptorSetLayout set_layout) {
  const VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(FramePush)};
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = 1;
  info.pSetLayouts = &set_layout;
  info.pushConstantRangeCount = 1;
  info.pPushConstantRanges = &range;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  vk::check(vkCreatePipelineLayout(device, &info, nullptr, &layout), "vkCreatePipelineLayout (3D)");
  return layout;
}

/// The instance-rate vertex input of each kind.
struct VertexInput {
  VkVertexInputBindingDescription binding{};
  std::vector<VkVertexInputAttributeDescription> attributes;
};

VertexInput input_for(Kind kind) {
  constexpr VkFormat kVec4 = VK_FORMAT_R32G32B32A32_SFLOAT;
  constexpr VkFormat kVec2 = VK_FORMAT_R32G32_SFLOAT;
  constexpr VkFormat kUint = VK_FORMAT_R32_UINT;
  VertexInput in;
  in.binding.binding = 0;
  in.binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
  switch (kind) {
    case Kind::rect:
      in.binding.stride = sizeof(GpuRect);
      in.attributes = {{0, 0, kVec4, offsetof(GpuRect, x0)},
                       {1, 0, kUint, offsetof(GpuRect, layer)},
                       {2, 0, kUint, offsetof(GpuRect, pick)}};
      break;
    case Kind::side:
      in.binding.stride = sizeof(GpuSide);
      in.attributes = {{0, 0, kVec4, offsetof(GpuSide, x0)},
                       {1, 0, kUint, offsetof(GpuSide, layer_flags)},
                       {2, 0, kVec2, offsetof(GpuSide, height)},
                       {3, 0, kUint, offsetof(GpuSide, pick)}};
      break;
    case Kind::triangle:
      in.binding.stride = sizeof(GpuTriangle);
      in.attributes = {{0, 0, kVec4, offsetof(GpuTriangle, x0)},
                       {1, 0, kVec2, offsetof(GpuTriangle, x2)},
                       {2, 0, kUint, offsetof(GpuTriangle, layer)},
                       {3, 0, kVec2, offsetof(GpuTriangle, height)},
                       {4, 0, kUint, offsetof(GpuTriangle, pick)}};
      break;
    case Kind::overlay:
      in.binding.stride = sizeof(GpuPrism);
      in.attributes = {{0, 0, kVec4, offsetof(GpuPrism, x0)},
                       {1, 0, kVec2, offsetof(GpuPrism, height)},
                       {2, 0, kUint, offsetof(GpuPrism, layer)}};
      break;
  }
  return in;
}

/// What a variant writes and how.
struct VariantState {
  bool msaa = false;
  bool color = true;
  bool pick = true;
  bool blend = false;
  bool depth_write = true;
  bool depth_test = true;
};

VariantState variant_state(Variant v) {
  switch (v) {
    case Variant::opaque:
      return {false, true, true, false, true};
    case Variant::transparent:
      return {false, true, false, true, false};
    case Variant::pick:
      return {false, false, true, false, true};
    case Variant::msaa_opaque:
      return {true, true, false, false, true};
    case Variant::msaa_transparent:
      return {true, true, false, true, false};
    case Variant::overlay:
      return {false, true, false, true, false, false};
    case Variant::msaa_overlay:
      return {true, true, false, true, false, false};
  }
  return {};
}

VkPipelineColorBlendAttachmentState color_blend(const VariantState& s) {
  VkPipelineColorBlendAttachmentState a{};
  a.colorWriteMask = s.color ? kRgba : 0;
  if (s.blend) {
    a.blendEnable = VK_TRUE;
    a.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    a.colorBlendOp = VK_BLEND_OP_ADD;
    a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    a.alphaBlendOp = VK_BLEND_OP_ADD;
  }
  return a;
}

/// The fixed-function state of one pipeline: depth test LESS_OR_EQUAL
/// (Q-MISC-77; the overlay variants test nothing) with a dynamic depth bias
/// that the renderer sets per layer (coplanar_bias, task 17.5; not on the
/// overlay), back faces culled. The depth is mirrored so the viewer sits
/// on the side of the higher layers (R1, Q-MISC-81), which reverses the
/// screen winding of the faces it sees: they are the clockwise ones.
struct FixedState {
  VertexInput input;
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  VkPipelineViewportStateCreateInfo viewport{};
  VkPipelineRasterizationStateCreateInfo raster{};
  VkPipelineMultisampleStateCreateInfo multisample{};
  VkPipelineDepthStencilStateCreateInfo depth{};
  std::array<VkPipelineColorBlendAttachmentState, 2> blend_attachments{};
  VkPipelineColorBlendStateCreateInfo blend{};
  std::array<VkDynamicState, 3> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                               VK_DYNAMIC_STATE_DEPTH_BIAS};
  VkPipelineDynamicStateCreateInfo dynamic{};

  FixedState(Kind kind, const VariantState& s) : input(input_for(kind)) {
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &input.binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(input.attributes.size());
    vertex_input.pVertexAttributeDescriptions = input.attributes.data();
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // One strip per instance; an overlay prism is a list of five faces.
    assembly.topology = kind == Kind::overlay ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.depthBiasEnable = kind == Kind::overlay ? VK_FALSE : VK_TRUE;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;  // the z-mirrored scene (R1, Q-MISC-84): faces toward the viewer above
    raster.lineWidth = 1.0F;
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = s.msaa ? kMsaaSamples : VK_SAMPLE_COUNT_1_BIT;
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = s.depth_test ? VK_TRUE : VK_FALSE;
    depth.depthWriteEnable = s.depth_write ? VK_TRUE : VK_FALSE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;  // Q-MISC-77: the last coplanar style wins
    blend_attachments[kColorAttachment] = color_blend(s);
    blend_attachments[kPickAttachment].colorWriteMask = s.pick ? VK_COLOR_COMPONENT_R_BIT : 0;  // never blended
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = s.msaa ? 1U : 2U;
    blend.pAttachments = blend_attachments.data();
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = kind == Kind::overlay ? 2U : static_cast<std::uint32_t>(dynamic_states.size());
    dynamic.pDynamicStates = dynamic_states.data();
  }
  FixedState(const FixedState&) = delete;
  FixedState& operator=(const FixedState&) = delete;
};

struct PipelineSpec {
  Kind kind;
  Variant variant;
  VkShaderModule vert;
  VkShaderModule frag;
  VkRenderPass pass;
  bool clipped;  // the fragment shader's specialization constant 0 (kClip)
};

VkPipeline make_pipeline(VkDevice device, VkPipelineLayout layout, const PipelineSpec& spec) {
  const FixedState s(spec.kind, variant_state(spec.variant));
  std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = spec.vert;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = spec.frag;
  stages[1].pName = "main";
  const VkBool32 clip = spec.clipped ? VK_TRUE : VK_FALSE;
  const VkSpecializationMapEntry clip_entry{0, 0, sizeof(VkBool32)};
  const VkSpecializationInfo specialization{1, &clip_entry, sizeof(clip), &clip};
  stages[1].pSpecializationInfo = &specialization;
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
  info.renderPass = spec.pass;
  VkPipeline pipeline = VK_NULL_HANDLE;
  vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline),
            "vkCreateGraphicsPipelines (3D)");
  return pipeline;
}

bool is_msaa(Variant v) {
  return v == Variant::msaa_opaque || v == Variant::msaa_transparent || v == Variant::msaa_overlay;
}

}  // namespace

// --- Pipelines -------------------------------------------------------------

Pipelines::Pipelines(VkPhysicalDevice physical, VkDevice device, VkRenderPass target_pass)
    : physical_(physical), device_(device) {
  try {
    create(target_pass);
  } catch (...) {
    destroy();
    throw;
  }
}

Pipelines::~Pipelines() {
  destroy();
}

void Pipelines::create(VkRenderPass target_pass) {
  vert_[static_cast<std::size_t>(Kind::rect)] =
      make_shader(device_, spv::kMesh3dRectVertSpv, sizeof(spv::kMesh3dRectVertSpv));
  vert_[static_cast<std::size_t>(Kind::side)] =
      make_shader(device_, spv::kMesh3dSideVertSpv, sizeof(spv::kMesh3dSideVertSpv));
  vert_[static_cast<std::size_t>(Kind::triangle)] =
      make_shader(device_, spv::kMesh3dTriVertSpv, sizeof(spv::kMesh3dTriVertSpv));
  frag_ = make_shader(device_, spv::kMesh3dFragSpv, sizeof(spv::kMesh3dFragSpv));
  msaa_frag_ = make_shader(device_, spv::kMesh3dMsaaFragSpv, sizeof(spv::kMesh3dMsaaFragSpv));
  set_layout_ = create_set_layout(device_);
  layout_ = create_layout(device_, set_layout_);
  if (supports_msaa(physical_)) {
    msaa_pass_ = create_msaa_pass(device_);
  }
  target_pass_ = target_pass;
  create_table(false, pipelines_);
}

void Pipelines::create_table(bool clipped, Table& out) {
  for (std::size_t k = 0; k < kKindCount; ++k) {
    for (std::size_t v = 0; v < kVariantCount; ++v) {
      const auto variant = static_cast<Variant>(v);
      if (is_msaa(variant) && msaa_pass_ == VK_NULL_HANDLE) {
        continue;
      }
      const PipelineSpec spec{static_cast<Kind>(k), variant, vert_[k], is_msaa(variant) ? msaa_frag_ : frag_,
                              is_msaa(variant) ? msaa_pass_ : target_pass_, clipped};
      out[k][v] = make_pipeline(device_, layout_, spec);
    }
  }
}

void Pipelines::ensure_clipped() {
  if (have_clipped_) {
    return;
  }
  try {
    create_table(true, clipped_);
  } catch (...) {
    destroy_table(clipped_);
    throw;
  }
  have_clipped_ = true;
}

void Pipelines::ensure_overlay(bool clipped) {
  auto& row = overlay_[clipped ? 1U : 0U];
  if (row[0] != VK_NULL_HANDLE) {
    return;
  }
  if (overlay_vert_ == VK_NULL_HANDLE) {
    overlay_vert_ = make_shader(device_, spv::kMesh3dOverlayVertSpv, sizeof(spv::kMesh3dOverlayVertSpv));
  }
  try {
    row[0] = make_pipeline(device_, layout_,
                           PipelineSpec{Kind::overlay, Variant::overlay, overlay_vert_, frag_, target_pass_, clipped});
    if (msaa_pass_ != VK_NULL_HANDLE) {
      row[1] = make_pipeline(device_, layout_, PipelineSpec{Kind::overlay, Variant::msaa_overlay, overlay_vert_,
                                                            msaa_frag_, msaa_pass_, clipped});
    }
  } catch (...) {
    for (VkPipeline& p : row) {
      if (p != VK_NULL_HANDLE) vkDestroyPipeline(device_, p, nullptr);
      p = VK_NULL_HANDLE;
    }
    throw;
  }
}

VkPipeline Pipelines::overlay(bool msaa, bool clipped) const noexcept {
  return overlay_[clipped ? 1U : 0U][msaa ? 1U : 0U];
}

void Pipelines::destroy_overlay() noexcept {
  for (auto& row : overlay_) {
    for (VkPipeline& p : row) {
      if (p != VK_NULL_HANDLE) vkDestroyPipeline(device_, p, nullptr);
      p = VK_NULL_HANDLE;
    }
  }
  if (overlay_vert_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, overlay_vert_, nullptr);
  overlay_vert_ = VK_NULL_HANDLE;
}

void Pipelines::destroy_table(Table& table) noexcept {
  for (auto& row : table) {
    for (VkPipeline& p : row) {
      if (p != VK_NULL_HANDLE) vkDestroyPipeline(device_, p, nullptr);
      p = VK_NULL_HANDLE;
    }
  }
}

void Pipelines::destroy() noexcept {
  destroy_table(pipelines_);
  destroy_table(clipped_);
  have_clipped_ = false;
  destroy_overlay();
  if (msaa_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, msaa_pass_, nullptr);
  if (layout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, layout_, nullptr);
  if (set_layout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, set_layout_, nullptr);
  for (VkShaderModule m : {frag_, msaa_frag_, vert_[0], vert_[1], vert_[2]}) {
    if (m != VK_NULL_HANDLE) vkDestroyShaderModule(device_, m, nullptr);
  }
  msaa_pass_ = VK_NULL_HANDLE;
  layout_ = VK_NULL_HANDLE;
  set_layout_ = VK_NULL_HANDLE;
  frag_ = VK_NULL_HANDLE;
  msaa_frag_ = VK_NULL_HANDLE;
  vert_ = {};
}

VkPipeline Pipelines::get(Kind kind, Variant variant, bool clipped) const noexcept {
  const Table& table = clipped ? clipped_ : pipelines_;
  return table[static_cast<std::size_t>(kind)][static_cast<std::size_t>(variant)];
}

// --- MsaaImages ------------------------------------------------------------

void MsaaImages::ensure(int width, int height) {
  if (width == width_ && height == height_ && color_.view != VK_NULL_HANDLE && depth_.view != VK_NULL_HANDLE) {
    return;
  }
  destroy();
  width_ = width;
  height_ = height;
  create_image(kColorFormat3d, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
               VK_IMAGE_ASPECT_COLOR_BIT, color_);
  create_image(kDepthFormat3d, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
               VK_IMAGE_ASPECT_DEPTH_BIT, depth_);
}

void MsaaImages::create_image(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, Image& out) {
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = format;
  info.extent = {static_cast<std::uint32_t>(width_), static_cast<std::uint32_t>(height_), 1};
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = kMsaaSamples;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vk::check(vkCreateImage(device_, &info, nullptr, &out.image), "vkCreateImage (3D msaa)");
  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(device_, out.image, &req);
  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex = vk::find_memory_type(physical_, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vk::check(vkAllocateMemory(device_, &alloc, nullptr, &out.memory), "vkAllocateMemory (3D msaa)");
  vk::check(vkBindImageMemory(device_, out.image, out.memory, 0), "vkBindImageMemory (3D msaa)");
  VkImageViewCreateInfo view{};
  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.image = out.image;
  view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view.format = format;
  view.subresourceRange = {aspect, 0, 1, 0, 1};
  vk::check(vkCreateImageView(device_, &view, nullptr, &out.view), "vkCreateImageView (3D msaa)");
}

void MsaaImages::destroy_image(Image& image) noexcept {
  if (image.view != VK_NULL_HANDLE) vkDestroyImageView(device_, image.view, nullptr);
  if (image.image != VK_NULL_HANDLE) vkDestroyImage(device_, image.image, nullptr);
  if (image.memory != VK_NULL_HANDLE) vkFreeMemory(device_, image.memory, nullptr);
  image = Image{};
}

void MsaaImages::destroy() noexcept {
  destroy_image(color_);
  destroy_image(depth_);
  width_ = 0;
  height_ = 0;
}

}  // namespace hefesto::render::mesh3d
