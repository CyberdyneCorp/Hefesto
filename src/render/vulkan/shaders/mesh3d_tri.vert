// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450
#extension GL_GOOGLE_include_directive : require

//
// Mesh3dRenderer: one split-tile Triangle per instance, drawn as 3
// vertices: the top face at z_top when it faces the viewer, the bottom
// face at z_bot otherwise, each counter-clockwise seen from outside
// (Magic's w3dFillPolygon draws both; culling keeps at most one), with the
// triangle's own h and t (its source tile's left type, MESH).

#include "mesh3d_common.glsl"

layout(location = 0) in vec4 in_p01;  // x0, y0, x1, y1 (chunk-local)
layout(location = 1) in vec2 in_p2;
layout(location = 2) in uint in_layer;
layout(location = 3) in vec2 in_ht;
layout(location = 4) in uint in_pick;

void main() {
  vec2 p0 = in_p01.xy;
  vec2 p1 = in_p01.zw;
  vec2 p2 = in_p2;
  vec2 e1 = p1 - p0;
  vec2 e2 = p2 - p0;
  if (e1.x * e2.y - e1.y * e2.x < 0.0) {  // make p0, p1, p2 counter-clockwise
    vec2 t = p1;
    p1 = p2;
    p2 = t;
  }
  vec2 z = z_range(in_ht);
  bool top = front_facing(vec3(p0, z.x), vec3(p1, z.x), vec3(p2, z.x));
  int v = gl_VertexIndex;
  vec2 xy = v == 0 ? p0 : ((v == 1) == top ? p1 : p2);
  emit(in_layer, vec3(xy, top ? z.x : z.y), vec3(0.0, 0.0, top ? 1.0 : -1.0), in_pick);
}
