// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the mask/gen unit tests: a small technology (a well plane,
// diffusion and poly composing a transistor, a poly contact, two metals),
// an editor session over it (the interaction finder runs through
// drc::interaction_area) and a hand-built output style, so the scenarios'
// output units are exact (STYLES' reader would reduce the units).
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/gen/classify.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/tech/technology.hpp"

template <>
struct Catch::StringMaker<hefesto::db::Rect> {
  static std::string convert(const hefesto::db::Rect& r) {
    return "(" + std::to_string(r.x0) + "," + std::to_string(r.y0) + ")-(" + std::to_string(r.x1) + "," +
           std::to_string(r.y1) + ")";
  }
};

namespace hefesto::mask::test {

using db::Rect;
using Rects = std::vector<Rect>;

inline LayerOp op(OpKind kind, std::int32_t distance = 0) {
  LayerOp o;
  o.kind = kind;
  o.distance = distance;
  return o;
}
inline LayerOp types_op(OpKind kind, std::initializer_list<tech::TileType> types) {
  LayerOp o = op(kind);
  for (const tech::TileType t : types) {
    o.types.set(t);
  }
  return o;
}
inline LayerOp layers_op(OpKind kind, std::initializer_list<std::size_t> layers) {
  LayerOp o = op(kind);
  o.cif_layers = layers;
  return o;
}
inline LayerOp squares_op(std::int32_t border, std::int32_t size, std::int32_t sep) {
  LayerOp o = op(OpKind::squares);
  o.values = {border, size, sep};
  return o;
}

/// The canonical strips of the union of `rects`.
inline Rects strips(const Rects& rects) {
  MaskPlane p;
  for (const Rect& r : rects) {
    REQUIRE(p.paint(r).ok());
  }
  return p.solid_rects();
}

struct GenFixture {
  editor::EditorState ed;
  std::shared_ptr<const tech::Technology> tech;
  compat::CompatSettings compat;
  tech::TileType nwell = 0;
  tech::TileType ndiff = 0;
  tech::TileType pdiff = 0;
  tech::TileType poly = 0;
  tech::TileType nfet = 0;
  tech::TileType pc = 0;
  tech::TileType m1 = 0;
  tech::TileType m2 = 0;
  OutputStyle style;
  diag::CollectingSink sink;
  GenerationContext ctx;

  explicit GenFixture(bool magic_compat = true) {
    tech::TechnologyBuilder b;
    b.set_name("gent");
    REQUIRE(b.add_plane("well").ok());
    REQUIRE(b.add_plane("active").ok());
    REQUIRE(b.add_plane("metal1").ok());
    REQUIRE(b.add_plane("metal2").ok());
    nwell = b.add_type("well", "nwell").value();
    ndiff = b.add_type("active", "ndiff").value();
    pdiff = b.add_type("active", "pdiff").value();
    poly = b.add_type("active", "poly").value();
    nfet = b.add_type("active", "nfet").value();
    pc = b.add_type("active", "pc").value();
    m1 = b.add_type("metal1", "m1").value();
    m2 = b.add_type("metal2", "m2").value();
    const std::array<std::string_view, 2> pc_res{"poly", "m1"};
    REQUIRE(b.add_contact("pc", pc_res).ok());
    const std::array<std::string_view, 3> compose{"nfet", "poly", "ndiff"};
    REQUIRE(b.add_compose_rule("compose", compose).ok());
    tech = b.finalize().value();
    ed.technology = tech;
    compat.magic_compat = magic_compat;
    ed.drc.compat = &compat;
    style.name = "test";
    style.scale.scale_factor = 10;
    ctx.sink = &sink;
  }

  void set_compat(bool on) { compat.magic_compat = on; }

  /// Appends a layer and recomputes the radius (as STYLES does at load).
  std::size_t layer(std::string name, std::vector<LayerOp> ops, bool temporary = false) {
    OutputLayer l;
    l.name = std::move(name);
    l.ops = std::move(ops);
    l.temporary = temporary;
    style.layers.push_back(std::move(l));
    style.radius = style.compute_halo();
    return style.layers.size() - 1;
  }

  db::CellDef& cell(const std::string& name) {
    auto made = ed.cells.create(name, tech->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    return *made.value();
  }

  void paint(db::CellDef& def, tech::TileType type, const Rect& r) {
    REQUIRE(db::paint_type(def, *tech, r, type).ok());
    db::recompute_bbox(def);
  }

  db::CellUse& place(db::CellDef& parent, db::CellDef& child, db::Transform t = {}, db::ArrayInfo array = {}) {
    db::PlaceOptions o;
    o.transform = t;
    o.array = array;
    auto u = db::place(parent, child, o);
    REQUIRE(u.ok());
    db::recompute_bbox(parent);
    return *u.value();
  }

  [[nodiscard]] GenerationSource source() const { return generation_source(ed); }

  /// The canonical strips of layer `layer` of `planes`.
  static Rects rects(const LayerPlanes& planes, std::size_t layer) {
    REQUIRE(planes[layer] != nullptr);
    return strips(planes[layer]->solid_rects());
  }

  /// The hierarchical output of `cell` (subcells and arrays on).
  LayerPlanes hierarchical(const db::CellDef& cell, HierarchyOptions options = {}) {
    return generate_hierarchical(source(), cell, style, ctx, options);
  }

  /// The codes of the diagnostics emitted so far (and forgets them).
  std::vector<std::string> codes() {
    std::vector<std::string> out;
    for (const auto& d : sink.take()) {
      out.push_back(d.code);
    }
    return out;
  }
};

inline db::Transform translate(db::Coord dx, db::Coord dy) {
  return db::translation(dx, dy);
}

}  // namespace hefesto::mask::test
