// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Instanced rectangles: one instance per rectangle (ivec4 x0, y0, x1, y1 in
// internal units), six vertices per instance (two triangles, no index buffer).
// The world-to-NDC transform subtracts an integer origin first, so precision
// does not depend on the absolute coordinate (see hefesto/render/camera.hpp).
#version 450

layout(location = 0) in ivec4 in_rect;

layout(push_constant) uniform View {
  ivec2 origin;
  vec2 scale;
  vec2 offset;
  vec2 min_size;
  vec4 color;
} view;

layout(location = 0) out vec4 out_color;

const vec2 kCorners[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
                                vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));

void main() {
  vec2 a = vec2(in_rect.xy - view.origin) * view.scale + view.offset;
  vec2 b = vec2(in_rect.zw - view.origin) * view.scale + view.offset;
  vec2 lo = min(a, b);
  vec2 hi = max(a, b);
  // Rectangles smaller than a pixel are drawn one pixel wide, as Magic does
  // at low zoom, instead of vanishing.
  vec2 center = 0.5 * (lo + hi);
  vec2 half_size = max(0.5 * (hi - lo), 0.5 * view.min_size);
  vec2 corner = kCorners[gl_VertexIndex];
  gl_Position = vec4(mix(center - half_size, center + half_size, corner), 0.0, 1.0);
  out_color = view.color;
}
