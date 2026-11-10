// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `wire segment` (wiring-tool "wire segment"): a centerline path of a given
// width painted on every plane of a layer in the edit cell, independent of
// the wire state. Ported from Magic VLSI 8.3.694 (fc5daa9e)
// commands/CmdTZ.c:1995-2150 and cif/CIFrdpt.c:661-703 (PaintWireList),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). The usage text lists the coordinates in order
// in both magic-compat modes (Q-LEGACY-53). Owner: WIRE.
//
// Differences: the pieces are painted with db::paint_type (contact images
// resolved) where Magic paints each plane with DBStdPaintTbl; the DRC
// re-check over the changed area waits for M3.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/polygon.hpp"
#include "hefesto/wiring/diag_codes.hpp"
#include "wiring/commands_internal.hpp"
#include "wiring/text.hpp"

namespace hefesto::wiring {

namespace {

constexpr std::string_view kUsageText =
    "Usage: wire segment layer width x1 y1 x2 y2\n (or)  wire segment layer width filename";

// The rectangle of a four-vertex axis-parallel polygon (painted without the
// general decomposer).
std::optional<db::Rect> as_rectangle(const std::vector<db::Point>& poly) {
  if (poly.size() != 4) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i < 4; ++i) {
    const db::Point a = poly[i];
    const db::Point b = poly[(i + 1) % 4];
    if (a.x != b.x && a.y != b.y) {
      return std::nullopt;
    }
  }
  db::Rect r{poly[0].x, poly[0].y, poly[0].x, poly[0].y};
  for (const db::Point& p : poly) {
    r = db::Rect{std::min(r.x0, p.x), std::min(r.y0, p.y), std::max(r.x1, p.x), std::max(r.y1, p.y)};
  }
  return r;
}

diag::Outcome<std::vector<db::PolygonPiece>> pieces_of(const std::vector<db::Point>& poly) {
  if (const auto r = as_rectangle(poly)) {
    return std::vector<db::PolygonPiece>{db::PolygonPiece{*r, std::nullopt}};
  }
  return db::decompose_polygon(poly);
}

diag::Status paint_polygons(const WireSession& s, const std::vector<std::vector<db::Point>>& polygons,
                            tech::TileType type) {
  for (const auto& poly : polygons) {
    auto pieces = pieces_of(poly);
    if (!pieces.ok()) {
      return pieces.error();
    }
    for (const db::PolygonPiece& piece : pieces.value()) {
      if (piece.area.is_null()) {
        continue;
      }
      std::optional<db::TriangleSpec> tri;
      if (piece.triangle) {
        tri = db::TriangleSpec{piece.triangle->diagonal, piece.triangle->right_side};
      }
      if (auto st = db::paint_type(*s.ed->edit_cell, *s.tech, piece.area, type, tri); !st.ok()) {
        return st;
      }
    }
  }
  return diag::Done{};
}

diag::Outcome<std::vector<db::Point>> file_points(const WireSession& s, const std::string& name) {
  editor::EditorState& ed = *s.ed;
  cell::LookupContext lookup{s.ctx->services().files, ed.search_paths, ed.path_variables, s.ctx->sink()};
  const auto found = cell::search_file(lookup, name, ed.search_paths.path, &ed.search_paths.library);
  if (!found) {
    return diag::make(codes::kFileOpen, "No such file or error opening " + name);
  }
  const auto text = s.ctx->services().files.read(found->path);
  if (!text.ok()) {
    return diag::make(codes::kFileOpen, "No such file or error opening " + name);
  }
  return read_segment_points(text.value(), name, s.units(), &s.ctx->sink());
}

std::vector<db::Point> argument_points(const WireSession& s, const std::vector<std::string>& args, std::size_t count) {
  std::vector<db::Point> points;
  const cmd::CoordinateUnits units = s.units();
  for (std::size_t i = 0; i < count; ++i) {
    const std::string& x = args[3 + 2 * i];
    const std::string& y = args[4 + 2 * i];
    points.push_back(db::Point{scale_coord(x, units, false, true, 2, &s.ctx->sink()),
                               scale_coord(y, units, false, false, 2, &s.ctx->sink())});
  }
  return points;
}

// The area to redisplay: the points' bounding box grown by the width times
// sqrt(2), in half units, halved with C truncation.
db::Rect changed_area(const std::vector<db::Point>& points, db::Coord width) {
  db::Rect r{points[0].x, points[0].y, points[0].x, points[0].y};
  for (const db::Point& p : points) {
    r = db::Rect{std::min(r.x0, p.x), std::min(r.y0, p.y), std::max(r.x1, p.x), std::max(r.y1, p.y)};
  }
  const auto grow = static_cast<std::int64_t>(0.5 + static_cast<double>(static_cast<float>(width)) * 1.414214);
  const auto half = [](std::int64_t v) {  // C truncation, kept inside the plane
    return static_cast<db::Coord>(std::clamp<std::int64_t>(v / 2, db::kPlaneMin, db::kPlaneMax));
  };
  return db::Rect{half(r.x0 - grow), half(r.y0 - grow), half(r.x1 + grow), half(r.y1 + grow)};
}

}  // namespace

cmd::CommandResult segment_command(const WireSession& s, const std::vector<std::string>& args) {
  db::CellDef* def = s.ed->edit_cell;
  if (def == nullptr) {
    return fail(codes::kNoBox, "No cell being edited");
  }
  std::size_t argc = args.size() + 1;  // Magic's locargc: "wire" "segment" ...
  bool endcap = true;
  if (args.back() == "-noendcap") {
    endcap = false;
    --argc;
  }
  if ((argc < 8 || (argc & 1U) != 0) && argc != 5) {
    return fail(codes::kUsage, std::string{kUsageText});
  }
  const auto type = s.tech->find_type(args[1].substr(0, args[1].find('/')));
  if (!type.ok()) {
    return fail(codes::kBadLayer, std::string{type.error == tech::LookupError::ambiguous ? "Ambiguous" : "Unrecognized"} +
                                      " layer (type) name \"" + args[1] + "\"");
  }
  if (!str_is_numeric(args[2])) {
    return fail(codes::kUsage, "Route segment width must be a numeric value");
  }
  const db::Coord width = s.parse_coord(args[2], false, true);
  auto points = argc == 5 ? file_points(s, args[3])
                          : diag::Outcome<std::vector<db::Point>>{argument_points(s, args, (argc - 4) / 2)};
  if (!points.ok()) {
    return cmd::CommandResult::failure(points.error());
  }
  std::vector<db::Point> path = points.value();
  if (path.empty()) {
    return cmd::CommandResult::success();  // an empty file: nothing to paint
  }
  std::reverse(path.begin(), path.end());  // PaintWireList builds its list backwards
  db::mark_modified(*def);
  const auto polygons = wire_path_polygons(path, width, endcap, &s.ctx->sink());
  if (auto st = paint_polygons(s, polygons, *type.id); !st.ok()) {
    return cmd::CommandResult::failure(st.error());
  }
  s.ed->windows.area_changed(*def, changed_area(path, width), ~tech::TypeMask::of(tech::kSpace));
  db::recompute_bbox(*def, s.magic_compat);
  return cmd::CommandResult::success();
}

}  // namespace hefesto::wiring
