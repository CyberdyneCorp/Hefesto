// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450
#extension GL_GOOGLE_include_directive : require

//
// Mesh3dRenderer: one net-overlay prism per instance (DrawParams::net_overlay,
// task 10.4), drawn as a 30-vertex triangle list: the face at h (z_bottom)
// or the one at h + t (z_top), whichever faces the viewer (as
// mesh3d_rect.vert), then the four side faces counter-clockwise seen from outside (as
// mesh3d_side.vert, the outside on the right of a->b); culling keeps the
// side faces toward the viewer. The overlay's layer entry holds its colour
// and opacity; pick id 0 (never dimmed; the overlay pipelines write no pick).

#include "mesh3d_common.glsl"

layout(location = 0) in vec4 in_rect;  // x0, y0, x1, y1 (relative to the overlay's origin)
layout(location = 1) in vec2 in_ht;    // h (z_bottom), t (z_top - z_bottom)
layout(location = 2) in uint in_layer;

const ivec2 kTop[4] = ivec2[](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));
const ivec2 kBottom[4] = ivec2[](ivec2(0, 0), ivec2(0, 1), ivec2(1, 0), ivec2(1, 1));
// The strip (0, 1, 2, 3) of a face as a triangle list.
const int kStrip[6] = int[](0, 1, 2, 2, 1, 3);
// Each side's a and b corners, counter-clockwise around the rectangle.
const ivec2 kRing[4] = ivec2[](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1), ivec2(0, 1));
// A side quad's corners in strip order (0: a_bot, 1: b_bot, 2: b_top, 3: a_top).
const int kQuad[4] = int[](0, 1, 3, 2);

vec2 corner_of(ivec2 c, vec2 lo, vec2 hi) {
  return vec2(c.x == 0 ? lo.x : hi.x, c.y == 0 ? lo.y : hi.y);
}

void main() {
  vec2 lo = min(in_rect.xy, in_rect.zw);
  vec2 hi = max(in_rect.xy, in_rect.zw);
  vec2 z = z_range(in_ht);
  int face = gl_VertexIndex / 6;
  int k = kStrip[gl_VertexIndex % 6];
  if (face == 0) {
    bool top = front_facing(vec3(lo, z.x), vec3(hi.x, lo.y, z.x), vec3(lo.x, hi.y, z.x));
    vec2 xy = corner_of(top ? kTop[k] : kBottom[k], lo, hi);
    emit(in_layer, vec3(xy, top ? z.x : z.y), vec3(0.0, 0.0, top ? 1.0 : -1.0), 0u);
    return;
  }
  vec2 a = corner_of(kRing[face - 1], lo, hi);
  vec2 b = corner_of(kRing[face % 4], lo, hi);
  int c = kQuad[k];
  vec2 xy = (c == 0 || c == 3) ? a : b;
  vec2 d = b - a;
  float len = length(d);
  vec3 normal = len > 0.0 ? vec3(d.y / len, -d.x / len, 0.0) : vec3(0.0);
  emit(in_layer, vec3(xy, c < 2 ? z.y : z.x), normal, 0u);
}
