// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Polygon painting for the LEF and DEF readers (LefPaintPolygon,
// lef/lefRead.c:1037-1066; PaintPolygon, cif/CIFrdpt.c:604-636; the port
// labels of LefReadPort, lef/lefRead.c:1380-1460; Q-LEFDEF-23, 66, 70).
// Magic is (c) the Regents of the University of California, BSD-style
// licence (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFVIA (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include "hefesto/lef/via/polygon.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/polygon.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

/// PaintPolygon's CIFPath: the points prepended one by one, so reversed.
std::vector<db::Point> path_of(std::span<const db::Point> vertices) {
  return {vertices.rbegin(), vertices.rend()};
}

std::vector<db::Rect> strips_of(const std::vector<db::PolygonPiece>& pieces) {
  std::vector<db::Rect> rects;
  for (const db::PolygonPiece& piece : pieces) {
    if (!piece.triangle) {
      rects.push_back(piece.area);
    }
  }
  return rects;
}

/// The index of a port label named `name` already in `cell`, if any.
std::optional<std::uint32_t> existing_port(const db::CellDef& cell, std::string_view name) {
  for (const db::Label& label : cell.labels()) {
    if (label.is_port() && label.text == name) {
      return label.port_index;
    }
  }
  return std::nullopt;
}

diag::Status put_port(db::CellDef& cell, const db::Rect& rect, tech::TileType type, const PortLabel& port) {
  db::Label label;
  label.text = std::string{port.name};
  label.type = type;
  label.rect = rect;
  label.sticky = true;
  label.port_directions = db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest;
  label.port_index = existing_port(cell, port.name).value_or(port.index);
  auto placed = db::put_label(cell, std::move(label), /*auto_justify=*/true);
  if (!placed.ok()) {
    return diag::Status{placed.error()};
  }
  return diag::Done{};
}

}  // namespace

std::vector<db::Rect> polygon_rects(std::span<const db::Point> vertices) {
  const std::vector<db::Point> path = path_of(vertices);
  return strips_of(db::decompose_read_polygon(path).pieces);
}

diag::Outcome<std::vector<db::Rect>> paint_polygon_rects(db::CellDef& cell, const tech::Technology& tech,
                                                         tech::TileType type, std::span<const db::Point> vertices) {
  const std::vector<db::Point> path = path_of(vertices);
  auto pieces = db::decompose_polygon(path);
  if (!pieces.ok()) {
    return pieces.error();
  }
  for (const db::PolygonPiece& piece : pieces.value()) {
    std::optional<db::TriangleSpec> triangle;
    if (piece.triangle) {
      triangle = db::TriangleSpec{piece.triangle->diagonal, piece.triangle->right_side};
    }
    if (auto st = db::paint_type(cell, tech, piece.area, type, triangle); !st.ok()) {
      return st.error();
    }
  }
  return strips_of(pieces.value());
}

diag::Status paint_polygon(db::CellDef& cell, const tech::Technology& tech, tech::TileType type,
                           std::span<const db::Point> vertices, const std::optional<PortLabel>& port) {
  auto rects = paint_polygon_rects(cell, tech, type, vertices);
  if (!rects.ok()) {
    return diag::Status{rects.error()};
  }
  if (!port) {
    return diag::Done{};
  }
  for (const db::Rect& rect : rects.value()) {
    if (auto st = put_port(cell, rect, type, *port); !st.ok()) {
      return st;
    }
  }
  return diag::Done{};
}

}  // namespace hefesto::lef
