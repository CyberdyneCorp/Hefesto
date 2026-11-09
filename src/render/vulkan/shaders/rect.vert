// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Instanced rectangles: one instance per rectangle (ivec4 x0, y0, x1, y1 in
// internal units). The world-to-NDC transform subtracts an integer origin
// first, so precision does not depend on the absolute coordinate (see
// hefesto/render/camera.hpp).
//
// Mode 0 (triangle list, six vertices per instance): the filled rectangle.
// Mode 1 (line list, four vertices per instance): its two diagonals, the
// `cross` style of contacts, skipped when the rectangle is under 4 pixels in
// both directions (Magic's GR_THRESH in graphics/grClip.c).
#version 450

layout(location = 0) in ivec4 in_rect;

layout(push_constant) uniform View {
  ivec2 origin;
  vec2 scale;
  vec2 offset;
  vec2 min_size;
  vec4 color;
  uvec4 stipple;  // x: rows 0-3, y: rows 4-7 (row 0 in the low byte), z: enabled, w: line pattern
  ivec4 anchor;   // x: left pixel column, y: bottom pixel row, z: width, w: height of the drawing area
  ivec4 base;     // xy: polygon base point, z: mode
} view;

layout(location = 0) out vec4 out_color;
// Pixels along the line from its left (or bottom) end, for line patterns.
layout(location = 1) noperspective out float out_along;

const vec2 kCorners[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
                                vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));

void diagonals(vec2 a, vec2 b, vec2 px) {
  // a = (x0, y0), b = (x1, y1): lines (x0,y0)-(x1,y1) and (x0,y1)-(x1,y0).
  int v = gl_VertexIndex;
  vec2 p = v == 0 ? a : (v == 1 ? b : (v == 2 ? vec2(a.x, b.y) : vec2(b.x, a.y)));
  vec2 size = abs(b - a) * px;
  if (size.x < 4.0 && size.y < 4.0) {
    p = a;  // degenerate: nothing is drawn
  }
  gl_Position = vec4(p, 0.0, 1.0);
  out_along = (v & 1) == 0 ? 0.0 : length((b - a) * px);
}

void main() {
  vec2 a = vec2(in_rect.xy - view.origin) * view.scale + view.offset;
  vec2 b = vec2(in_rect.zw - view.origin) * view.scale + view.offset;
  vec2 px = 0.5 * vec2(view.anchor.zw);  // NDC units to pixels
  out_color = view.color;
  if (view.base.z == 1) {
    diagonals(a, b, px);
    return;
  }
  vec2 lo = min(a, b);
  vec2 hi = max(a, b);
  // Rectangles smaller than a pixel are drawn one pixel wide, as Magic does
  // at low zoom, instead of vanishing.
  vec2 center = 0.5 * (lo + hi);
  vec2 half_size = max(0.5 * (hi - lo), 0.5 * view.min_size);
  vec2 corner = kCorners[gl_VertexIndex];
  vec2 p = mix(center - half_size, center + half_size, corner);
  gl_Position = vec4(p, 0.0, 1.0);
  // NDC y grows downward: count vertical lines from the bottom end.
  vec2 extent = (hi - lo) * px;
  out_along = extent.x >= extent.y ? (p.x - (center.x - half_size.x)) * px.x
                                   : ((center.y + half_size.y) - p.y) * px.y;
}
