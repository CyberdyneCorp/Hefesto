// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450

//
// Mesh3dRenderer, multisampled color pass (level >= 1): the color only; the
// pick ids come from the single-sample pass of the same frame.

layout(location = 0) in vec4 in_color;
layout(location = 1) flat in uint in_pick;

layout(location = 0) out vec4 out_color;

void main() {
  out_color = in_color;
}
