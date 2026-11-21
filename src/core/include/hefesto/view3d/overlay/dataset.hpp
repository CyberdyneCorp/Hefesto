// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Overlay datasets and color maps (P3; hefesto-3d-visualization "Parasitic
// and field overlays"; openspec/changes/m10-3d-view/design.md D16).
// Datasets are read, never recomputed: node and coupling capacitance from
// M7's `.ext` data, resistance networks from `extresist-*` output, currents
// and voltages from M9 results, IR drop and EM from
// `reliability-visualization`. Each is mapped onto pickable geometry through
// the net tiles of `connect` and the node positions of the extraction data;
// color maps and legends are the design system's.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); OVERLAY fills in
// src/core/src/view3d/overlay/ (design D3, section 15).

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace hefesto::view3d::overlay {

/// What a dataset's values are keyed by.
enum class KeyKind : std::uint8_t { net, node, element };

struct Value {
  std::string key;  // net, node or element name
  double value = 0.0;
};

struct Dataset {
  std::string name;   // e.g. "Node capacitance"
  std::string unit;   // e.g. "fF"
  KeyKind key_kind = KeyKind::net;
  std::vector<Value> values;
  double min = 0.0;   // the legend's range
  double max = 0.0;
  double threshold = 0.0;  // markers below it are filtered (coupling)
};

/// An sRGB color, 8 bits per channel.
using Rgb = std::array<std::uint8_t, 3>;

struct ColorMap {
  std::string name;
  std::vector<Rgb> stops;  // evenly spaced from min to max
};

/// The color of `value` on `map` over [min, max].
[[nodiscard]] Rgb color_of(const ColorMap& map, double value, double min, double max);

}  // namespace hefesto::view3d::overlay
