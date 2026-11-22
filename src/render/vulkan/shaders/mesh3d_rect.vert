// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450
#extension GL_GOOGLE_include_directive : require

//
// Mesh3dRenderer: one RectInstance per instance, drawn as 12 vertices: the
// top face at z_top (counter-clockwise on screen in the top view) and the
// bottom face at z_bot (clockwise there), as Magic's w3dFillTile.

#include "mesh3d_common.glsl"

layout(location = 0) in vec4 in_rect;  // x0, y0, x1, y1 (chunk-local)
layout(location = 1) in uint in_layer;
layout(location = 2) in uint in_pick;  // chunk-local pick index

// Corner selectors (0: min, 1: max) of the two triangles of each face.
const ivec2 kTop[6] = ivec2[](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1), ivec2(0, 0), ivec2(1, 1), ivec2(0, 1));
const ivec2 kBottom[6] = ivec2[](ivec2(0, 0), ivec2(1, 1), ivec2(1, 0), ivec2(0, 0), ivec2(0, 1), ivec2(1, 1));

void main() {
  int v = gl_VertexIndex % 6;
  bool top = gl_VertexIndex < 6;
  ivec2 corner = top ? kTop[v] : kBottom[v];
  vec2 lo = min(in_rect.xy, in_rect.zw);
  vec2 hi = max(in_rect.xy, in_rect.zw);
  vec2 z = z_range(layers[in_layer].zinfo.xy);
  vec3 local = vec3(corner.x == 0 ? lo.x : hi.x, corner.y == 0 ? lo.y : hi.y, top ? z.x : z.y);
  emit(in_layer, local, vec3(0.0, 0.0, top ? 1.0 : -1.0), in_pick);
}
