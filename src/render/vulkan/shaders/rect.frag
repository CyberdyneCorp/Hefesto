// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450

//
// Optional 8x8 stipple in Magic's OpenGL orientation (decision X-5): row 0 is
// the bottom pixel row of each cell, anchored to the drawing area's
// bottom-left pixel, and the most significant bit is the leftmost pixel.
//
// Optional line pattern (Magic's outline pattern, glLineStipple(1, p | p << 8)):
// bit i, least significant first, paints pixel i mod 8 along the line.

layout(location = 0) in vec4 in_color;
layout(location = 1) noperspective in float in_along;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform View {
  ivec2 origin;
  vec2 scale;
  vec2 offset;
  vec2 min_size;
  vec4 color;
  uvec4 stipple;
  ivec4 anchor;
  ivec4 base;
} view;

void main() {
  if (view.stipple.z != 0u) {
    uint column = uint((int(gl_FragCoord.x) - view.anchor.x) & 7);
    uint row = uint((view.anchor.y - int(gl_FragCoord.y)) & 7);
    uint word = row < 4u ? view.stipple.x : view.stipple.y;
    uint bits = (word >> (8u * (row & 3u))) & 0xFFu;
    if (((bits >> (7u - column)) & 1u) == 0u) {
      discard;
    }
  }
  uint pattern = view.stipple.w & 0xFFu;
  if (pattern != 0u && pattern != 0xFFu) {
    uint bit = uint(max(floor(in_along), 0.0)) & 7u;
    if (((pattern >> bit) & 1u) == 0u) {
      discard;
    }
  }
  out_color = in_color;
}
