// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared by the Mesh3dRenderer vertex shaders (openspec/changes/m10-3d-view
// design D8, tasks.md 5.3): the frame's push constants, the per-layer and
// highlight storage buffers, z from h, t and s_z, and the board lighting.
// Keep in sync with src/mesh3d_renderer.cpp (FramePush, GpuLayer) and
// include/hefesto/render/mesh3d_shading.hpp (the CPU copy of the lighting).

layout(push_constant) uniform Frame {
  // chunk-local model (x, y, z) -> Vulkan clip space: xy scaled, z mirrored
  // (the displayed scene), y down, depth (w - z') / 2
  mat4 mvp;
  vec4 light;    // xyz: the board light (unit, displayed scene); w: the ambient term
  vec4 zscale;   // x: s_z
  uvec4 params;  // x: pick base; y: lighting (0 board); z: highlight active
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

layout(location = 0) flat out vec4 out_color;  // one color per face (flat shading)
layout(location = 1) flat out uint out_pick;

// z_top = -h * s_z and z_bot = z_top - t * s_z (view-3d "Rendered geometry").
vec2 z_range(vec2 ht) {
  float top = -ht.x * frame.zscale.x;
  return vec2(top, top - ht.y * frame.zscale.x);
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
// outside, so Magic's viewer is on the physical underside. The chunk
// matrix mirrors z (the displayed scene, z growing with the height), so
// projection and depth agree that the viewer is on the side of the higher
// layers (R1, Q-MISC-81; Q-MISC-84) and sees the faces at each layer's
// h + t end from above.
// Board lighting (Q-MISC-84, both modes; mesh3d_shading.hpp): flat per
// face, on the face's own outward normal in the displayed scene (the model
// normal mirrored in z), I = a + (1 - a) * max(n . l, 0) / l_z, at most 1.
float light(vec3 normal) {
  vec3 n = vec3(normal.xy, -normal.z);
  float a = frame.light.w;
  return min(a + (1.0 - a) * max(dot(n, frame.light.xyz), 0.0) / frame.light.z, 1.0);
}

// Whether the chunk-local triangle (p0, p1, p2) faces the viewer once
// projected: the rasterizer's own test (front faces are clockwise in
// framebuffer coordinates, R1), taken on the homogeneous (x, y, w) of the
// clip-space corners so that it holds for any w (its sign tells on which
// side of the triangle's plane the eye is). Lets an instance draw only the
// face of a top/bottom pair that can be seen.
bool front_facing(vec3 p0, vec3 p1, vec3 p2) {
  vec3 a = (frame.mvp * vec4(p0, 1.0)).xyw;
  vec3 b = (frame.mvp * vec4(p1, 1.0)).xyw;
  vec3 c = (frame.mvp * vec4(p2, 1.0)).xyw;
  return determinant(mat3(a, b, c)) > 0.0;
}

// Places a chunk-local vertex and writes the color and pick outputs.
void emit(uint layer, vec3 local, vec3 normal, uint pick_local) {
  Layer l = layers[layer];
  gl_Position = frame.mvp * vec4(local, 1.0);
  vec3 c = clamp(l.color.rgb * light(normal), 0.0, 1.0);
  uint pick = frame.params.x + pick_local;
  if (frame.params.z != 0u && !highlighted(pick)) {
    c *= 0.35;  // not in the highlight set: dimmed
  }
  out_color = vec4(srgb_to_linear(c), l.color.a);
  out_pick = pick;
}
