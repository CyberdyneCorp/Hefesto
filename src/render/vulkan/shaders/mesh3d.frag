// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450

//
// Mesh3dRenderer, single-sample pass: the color (attachment 0, the sRGB view
// of the color image; already lit and linear) and the pick id (attachment
// 1, R32_UINT). The pipeline's write masks decide which of them a pass keeps.

layout(location = 0) flat in vec4 in_color;
layout(location = 1) flat in uint in_pick;

layout(location = 0) out vec4 out_color;
layout(location = 1) out uint out_pick;

void main() {
  out_color = in_color;
  out_pick = in_pick;
}
