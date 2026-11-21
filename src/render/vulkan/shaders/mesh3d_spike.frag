// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450

//
// M10 renderer spike: writes the color (attachment 0, the sRGB view of the
// color image) and the pick id (attachment 1, R32_UINT; 0 is the background).

layout(location = 0) in vec4 in_color;
layout(location = 1) flat in uint in_pick;

layout(location = 0) out vec4 out_color;
layout(location = 1) out uint out_pick;

void main() {
  out_color = in_color;
  out_pick = in_pick;
}
