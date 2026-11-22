// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450
#extension GL_GOOGLE_include_directive : require

//
// Mesh3dRenderer: one SideInstance per instance, drawn as a quad (6
// vertices) standing on its segment from z_bot to z_top, counter-clockwise
// seen from outside (Magic's w3dFillDiagonal order: a_bot, b_bot, b_top,
// a_top with the outside on the right of a->b). A face flagged own_height
// (a split tile's, MESH) uses its own h and t, every other its layer's.

#include "mesh3d_common.glsl"

layout(location = 0) in vec4 in_seg;          // x0, y0, x1, y1 (chunk-local)
layout(location = 1) in uint in_layer_flags;  // layer | side_flag bits << 16
layout(location = 2) in vec2 in_ht;           // h, t with side_flag::own_height
layout(location = 3) in uint in_pick;

const uint kOutwardLeft = 0x01u;
const uint kOwnHeight = 0x08u;
const int kQuad[6] = int[](0, 1, 2, 0, 2, 3);

void main() {
  uint layer = in_layer_flags & 0xFFFFu;
  uint flags = in_layer_flags >> 16;
  vec2 a = in_seg.xy;
  vec2 b = in_seg.zw;
  if ((flags & kOutwardLeft) != 0u) {
    a = in_seg.zw;
    b = in_seg.xy;
  }
  vec2 z = z_range((flags & kOwnHeight) != 0u ? in_ht : layers[layer].zinfo.xy);
  int corner = kQuad[gl_VertexIndex];
  vec2 xy = (corner == 0 || corner == 3) ? a : b;
  vec2 d = b - a;
  float len = length(d);
  vec3 normal = len > 0.0 ? vec3(d.y / len, -d.x / len, 0.0) : vec3(0.0);
  emit(layer, vec3(xy, corner < 2 ? z.y : z.x), normal, in_pick);
}
