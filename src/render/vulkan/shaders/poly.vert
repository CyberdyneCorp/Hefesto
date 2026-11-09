// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Filled triangles (split tiles, glyphs of font labels): one float vertex
// per corner, relative to an integer base point, so precision does not
// depend on the absolute coordinate.
#version 450

layout(location = 0) in vec2 in_position;

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

layout(location = 0) out vec4 out_color;
layout(location = 1) noperspective out float out_along;

void main() {
  vec2 p = (vec2(view.base.xy - view.origin) + in_position) * view.scale + view.offset;
  gl_Position = vec4(p, 0.0, 1.0);
  out_color = view.color;
  out_along = 0.0;
}
