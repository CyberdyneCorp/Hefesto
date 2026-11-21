// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450

//
// M10 renderer spike (openspec/changes/m10-3d-view tasks.md 5.1): a test mesh
// of colored triangles with a pick id per vertex, transformed by one matrix.
// The real 3D pipelines (instanced rectangles, side segments, triangles with
// z from h, t and s_z) replace it in task 5.3.

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;  // linear RGB; the sRGB view encodes it
layout(location = 2) in uint in_pick;

layout(location = 0) out vec4 out_color;
layout(location = 1) flat out uint out_pick;

layout(push_constant) uniform Frame {
  mat4 transform;
} frame;

void main() {
  gl_Position = frame.transform * vec4(in_position, 1.0);
  out_color = in_color;
  out_pick = in_pick;
}
