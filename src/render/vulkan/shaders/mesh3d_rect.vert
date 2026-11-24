// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450
#extension GL_GOOGLE_include_directive : require

//
// Mesh3dRenderer: one RectInstance per instance, drawn as a 4-vertex
// triangle strip: the top face at z_top (counter-clockwise seen from +z)
// when it faces the viewer, the bottom face at z_bot (clockwise from +z)
// otherwise. Magic's w3dFillTile draws both and culling keeps at most one
// (opposite windings over the same footprint); when neither faces the
// viewer (seen edge on) the bottom face is emitted and culled.

#include "mesh3d_common.glsl"

layout(location = 0) in vec4 in_rect;  // x0, y0, x1, y1 (chunk-local)
layout(location = 1) in uint in_layer;
layout(location = 2) in uint in_pick;  // chunk-local pick index

// Corner selectors (0: min, 1: max) in strip order; each strip is the two
// triangles of its face with the face's winding.
const ivec2 kTop[4] = ivec2[](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));
const ivec2 kBottom[4] = ivec2[](ivec2(0, 0), ivec2(0, 1), ivec2(1, 0), ivec2(1, 1));

void main() {
  vec2 lo = min(in_rect.xy, in_rect.zw);
  vec2 hi = max(in_rect.xy, in_rect.zw);
  vec2 z = z_range(layers[in_layer].zinfo.xy);
  bool top = front_facing(vec3(lo, z.x), vec3(hi.x, lo.y, z.x), vec3(lo.x, hi.y, z.x));
  ivec2 corner = top ? kTop[gl_VertexIndex] : kBottom[gl_VertexIndex];
  vec3 local = vec3(corner.x == 0 ? lo.x : hi.x, corner.y == 0 ? lo.y : hi.y, top ? z.x : z.y);
  emit(in_layer, local, vec3(0.0, 0.0, top ? 1.0 : -1.0), in_pick);
}
