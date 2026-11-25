// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
#version 450

//
// Mesh3dRenderer, multisampled color pass (level >= 1): the color only; the
// pick ids come from the single-sample pass of the same frame.

layout(location = 0) flat in vec4 in_color;
layout(location = 1) flat in uint in_pick;
layout(location = 2) in float in_clip;

// The clipped pipelines (DrawParams::section_plane) set this: fragments on
// the removed side are discarded, so no pass writes color, pick or depth
// there.
layout(constant_id = 0) const bool kClip = false;

layout(location = 0) out vec4 out_color;

void main() {
  if (kClip && in_clip < 0.0) {
    discard;
  }
  out_color = in_color;
}
