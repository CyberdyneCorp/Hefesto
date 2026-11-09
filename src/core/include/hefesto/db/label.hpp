// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Labels and ports (capability layout-labels-ports, requirement "Label data
// model"; font attributes from capability font-labels).
// (Magic ref: database/database.h.in:219-319, database/DBlabel.c)
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {

enum class PortClass : std::uint8_t { default_class, input, output, tristate, bidirectional, feedthrough };
enum class PortUse : std::uint8_t { default_use, signal, analog, power, ground, clock, reset, scan, tieoff };
enum class PortShape : std::uint8_t { default_shape, abutment, ring, feedthrough };

/// Port direction bits; a label is a port iff any bit is set.
enum PortDirection : std::uint8_t { kPortNorth = 1, kPortEast = 2, kPortSouth = 4, kPortWest = 8 };

struct Label {
  std::string text;                       // never empty in a written file
  tech::TileType type = tech::kSpace;     // attached layer
  Rect rect;                              // point / line / area label
  Position justification = 0;             // 0 center, 1..8 N..NW
  std::int32_t font = -1;                 // -1 = default font (font-labels)
  std::int32_t size = 0;                  // 1/8 database units; 0 for the default font
  std::int32_t rotation = 0;              // degrees
  Point offset;                           // 1/8 database units
  bool sticky = false;
  std::uint8_t port_directions = 0;       // PortDirection bits
  PortClass port_class = PortClass::default_class;
  PortUse port_use = PortUse::default_use;
  PortShape port_shape = PortShape::default_shape;
  std::uint32_t port_index = 0;
  /// Rendered text box of an outline-font label in cell coordinates
  /// (lab_bbox, "Rendered text box of outline-font labels"), filled by the
  /// font code (display::font_label_corners); nullopt for the default font.
  /// The extended bounding box includes it.
  std::optional<Rect> text_bbox;

  [[nodiscard]] bool is_port() const noexcept { return port_directions != 0; }

  friend bool operator==(const Label&, const Label&) = default;
};

/// Magic's DefaultLabel, created by the first `setlabel -default` and used
/// by later `label` commands (cmd-labels-properties "Label command"). Its
/// initial values mean "not set": justification -1 (automatic), font -1,
/// type nullopt (layer chosen from the paint).
/// (Magic ref: commands/CmdRS.c:2207-2222)
struct LabelDefaults {
  Position justification = -1;
  std::int32_t font = -1;
  std::int32_t size = 0;      // 1/8 database units
  std::int32_t rotation = 0;  // degrees
  Point offset;               // 1/8 database units
  bool sticky = false;
  std::optional<tech::TileType> type;

  friend bool operator==(const LabelDefaults&, const LabelDefaults&) = default;
};

}  // namespace hefesto::db
