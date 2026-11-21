// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared by the Mesh3dRenderer vertex shaders (openspec/changes/m10-3d-view
// design D8, tasks.md 5.3): the frame's push constants, the per-layer and
// highlight storage buffers, z from h, t and s_z, and Magic's lighting.
// Keep in sync with src/mesh3d_renderer.cpp (FramePush, GpuLayer) and
// include/hefesto/render/mesh3d_shading.hpp (the CPU copy of the lighting).

layout(push_constant) uniform Frame {
  mat4 mvp;        // chunk-local (x, y, z) -> Vulkan clip space (y down, depth (w - z) / 2: nearer is smaller)
  vec4 origin_sz;  // x, y: the chunk origin in root coordinates (lighting); z: s_z
  uvec4 params;    // x: pick base; y: lighting (0 Magic, 1 directional); z: highlight active
} frame;

struct Layer {
  vec4 color;  // display RGB (sRGB-encoded values, as Magic hands them to GL), alpha
  vec4 zinfo;  // h, t (as drawn: LayerStack::draw_height), unused, unused
};

layout(std430, set = 0, binding = 0) readonly buffer Layers {
  Layer layers[];
};

// Bit (id - 1) is set for each pick id of the highlight set.
layout(std430, set = 0, binding = 1) readonly buffer Highlight {
  uint bits[];
};

layout(location = 0) out vec4 out_color;
layout(location = 1) flat out uint out_pick;

// z_top = -h * s_z and z_bot = z_top - t * s_z (view-3d "Rendered geometry").
vec2 z_range(vec2 ht) {
  float top = -ht.x * frame.origin_sz.z;
  return vec2(top, top - ht.y * frame.origin_sz.z);
}

vec3 srgb_to_linear(vec3 c) {
  vec3 low = c / 12.92;
  vec3 high = pow((c + 0.055) / 1.055, vec3(2.4));
  return mix(high, low, lessThanEqual(c, vec3(0.04045)));
}

bool highlighted(uint pick) {
  uint bit = pick - 1u;
  return (bits[bit >> 5] & (1u << (bit & 31u))) != 0u;
}

// Model z is -h * s_z and Magic's faces are counter-clockwise seen from
// outside in right-handed coordinates, so the viewer of the default view is
// on the +z side and the face at z_top (the layer's h end) has the normal
// (0, 0, 1) (CAMERA: z' grows toward the viewer).
// Magic (magic-compat ON): GL fixed-function lighting with every transform
// in the projection matrix and no glNormal call, so the normal is (0, 0, 1)
// for every face and the positional light #1 at (50, 50, 50) is in model
// coordinates; ambient = GL's global 0.2 + light #0's 0.4.
// Directional (OFF): the light Magic's comment calls directional, from
// (1, 1, 1), on the face's own normal, with the spec's ambient 0.4.
float light(vec3 model, vec3 normal) {
  if (frame.params.y == 0u) {
    vec3 to_light = vec3(50.0) - model;
    float len = length(to_light);
    return 0.6 + (len > 0.0 ? max(to_light.z / len, 0.0) : 1.0);
  }
  return 0.4 + max(dot(normal, vec3(0.57735027)), 0.0);
}

// Places a chunk-local vertex and writes the color and pick outputs.
void emit(uint layer, vec3 local, vec3 normal, uint pick_local) {
  Layer l = layers[layer];
  gl_Position = frame.mvp * vec4(local, 1.0);
  vec3 model = vec3(frame.origin_sz.xy + local.xy, local.z);
  vec3 c = clamp(l.color.rgb * light(model, normal), 0.0, 1.0);
  uint pick = frame.params.x + pick_local;
  if (frame.params.z != 0u && !highlighted(pick)) {
    c *= 0.35;  // not in the highlight set: dimmed
  }
  out_color = vec4(srgb_to_linear(c), l.color.a);
  out_pick = pick;
}
