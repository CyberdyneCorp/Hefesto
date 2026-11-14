// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A small technology, a layout cell and a hand-built output style for the
// operator kernel tests (openspec/changes/m5-mask-gds-cif/design.md D2: OPS
// works against a hand-built OutputStyle, not STYLES' parser).
//
// Owner: OPS.

#pragma once

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/ops/compat.hpp"
#include "hefesto/mask/ops/evaluate.hpp"
#include "hefesto/mask/ops/operators.hpp"
#include "hefesto/tech/technology.hpp"

template <>
struct Catch::StringMaker<hefesto::db::Rect> {
  static std::string convert(const hefesto::db::Rect& r) {
    return "[" + std::to_string(r.x0) + "," + std::to_string(r.x1) + "]x[" + std::to_string(r.y0) + "," +
           std::to_string(r.y1) + "]";
  }
};

namespace hefesto::mask::test {

using db::Rect;
using Rects = std::vector<Rect>;

/// Operator constructors (the operand layouts of mask/ops/operators.hpp).
/// `operands` are written in tech-file order: the distance first for the
/// operators that have one (grow, shrink, grow-grid, grow-min, close,
/// bridge, bridge-lim), then the client values.
inline LayerOp op(OpKind kind, std::initializer_list<std::int64_t> operands = {}) {
  LayerOp o;
  o.kind = kind;
  std::vector<std::int32_t> all;
  for (const std::int64_t v : operands) {
    all.push_back(static_cast<std::int32_t>(v));
  }
  const bool has_distance = kind == OpKind::grow || kind == OpKind::shrink || kind == OpKind::grow_grid ||
                            kind == OpKind::grow_min || kind == OpKind::close || kind == OpKind::bridge ||
                            kind == OpKind::bridge_lim;
  if (has_distance && !all.empty()) {
    o.distance = all.front();
    all.erase(all.begin());
  }
  o.values = std::move(all);
  return o;
}
/// The co_client flag: maxrect `external`, orthogonal `fill`, bbox `top`.
inline LayerOp with_option(LayerOp o) {
  o.option = true;
  return o;
}
inline LayerOp with_types(LayerOp o, std::initializer_list<tech::TileType> types) {
  for (const tech::TileType t : types) {
    o.types.set(t);
  }
  return o;
}
inline LayerOp with_layers(LayerOp o, std::initializer_list<std::size_t> layers) {
  o.cif_layers = layers;
  return o;
}
inline LayerOp with_text(LayerOp o, std::string text) {
  o.text = std::move(text);
  return o;
}

/// The canonical strips of the union of `rects` (a plane painted with them).
inline Rects strips(const Rects& rects) {
  MaskPlane p;
  for (const Rect& r : rects) {
    REQUIRE(p.paint(r).ok());
  }
  return p.solid_rects();
}

struct OpsFixture {
  std::shared_ptr<const tech::Technology> tech;
  tech::PlaneId active = 0;
  tech::PlaneId metal1 = 0;
  tech::PlaneId metal2 = 0;
  tech::TileType ndiff = 0;
  tech::TileType pdiff = 0;
  tech::TileType poly = 0;
  tech::TileType nfet = 0;
  tech::TileType m1 = 0;
  tech::TileType m2 = 0;
  tech::TileType ndc = 0;
  tech::TileType via = 0;
  std::unique_ptr<db::CellDef> cell;
  OutputStyle style;
  PlanePool pool;
  diag::CollectingSink sink;
  OperatorContext ctx;
  std::vector<MaskPlane*> layers;  // the layer planes read by cif_layers

  OpsFixture() {
    tech::TechnologyBuilder b;
    b.set_name("opst");
    active = b.add_plane("active").value();
    metal1 = b.add_plane("metal1").value();
    metal2 = b.add_plane("metal2").value();
    ndiff = b.add_type("active", "ndiff").value();
    pdiff = b.add_type("active", "pdiff").value();
    poly = b.add_type("active", "poly").value();
    nfet = b.add_type("active", "nfet").value();
    m1 = b.add_type("metal1", "m1").value();
    m2 = b.add_type("metal2", "m2").value();
    ndc = b.add_type("active", "ndc").value();
    via = b.add_type("metal1", "via").value();
    const std::array<std::string_view, 2> ndc_res{"ndiff", "m1"};
    const std::array<std::string_view, 2> via_res{"m1", "m2"};
    REQUIRE(b.add_contact("ndc", ndc_res).ok());
    REQUIRE(b.add_contact("via", via_res).ok());
    REQUIRE(b.add_connect("m1,ndc,via", "m1,ndc,via").ok());
    REQUIRE(b.add_connect("m2,via", "m2,via").ok());
    tech = b.finalize().value();
    cell = std::make_unique<db::CellDef>("top", tech->plane_count());
    style.name = "test";
    ctx.pool = &pool;
    ctx.sink = &sink;
    ctx.cell = cell.get();
  }

  void compat(bool on) { ctx.compat.magic_compat = on; }
  void scale(std::int32_t s) { style.scale.scale_factor = s; }

  void paint(tech::TileType type, const Rect& r) {
    REQUIRE(db::paint_type(*cell, *tech, r, type).ok());
    db::recompute_bbox(*cell);
  }

  /// A mask layer holding `rects`, appended to the style (as a templayer)
  /// and readable through cif_layers; returns its index.
  std::size_t mask_layer(const Rects& rects) {
    MaskPlane& p = pool.acquire();
    for (const Rect& r : rects) {
      REQUIRE(p.paint(r).ok());
    }
    OutputLayer l;
    l.name = "L" + std::to_string(style.layers.size());
    l.temporary = true;
    style.layers.push_back(l);
    layers.push_back(&p);
    return layers.size() - 1;
  }

  [[nodiscard]] LayerInputs inputs() const {
    return LayerInputs{cell.get(), std::span<MaskPlane* const>{layers}, tech.get()};
  }

  /// Evaluates a layer made of `ops` over `area` (internal units) and
  /// returns the plane (released by the fixture's pool at exit).
  MaskPlane& eval(const std::vector<LayerOp>& ops, const Rect& area = Rect{-1000, -1000, 1000, 1000}) {
    OutputLayer l;
    l.name = "OUT";
    l.ops = ops;
    style.layers.push_back(l);
    layers.push_back(nullptr);
    const std::size_t index = style.layers.size() - 1;
    const LayerInputs in = inputs();
    MaskPlane& out = evaluate_layer(style, index, in, area, ctx);
    style.layers.pop_back();
    layers.pop_back();
    return out;
  }

  /// The canonical strips of `ops` evaluated over `area`.
  Rects run(const std::vector<LayerOp>& ops, const Rect& area = Rect{-1000, -1000, 1000, 1000}) {
    return eval(ops, area).solid_rects();
  }

  /// The plane `rects` as the layer's current plane, then `ops` applied.
  Rects apply(const Rects& cur, const std::vector<LayerOp>& ops) {
    const std::size_t l = mask_layer(cur);
    std::vector<LayerOp> all{with_layers(op(OpKind::op_or), {l})};
    all.insert(all.end(), ops.begin(), ops.end());
    return run(all);
  }
};

}  // namespace hefesto::mask::test
