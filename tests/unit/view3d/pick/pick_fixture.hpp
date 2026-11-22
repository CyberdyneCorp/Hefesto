// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the view3d/pick tests: an inverter technology (planes
// active and metal1; ndiff, pdiff, poly, metal1 and the contacts ndc, pdc,
// pc), an editor session holding it, a hand-built layout-layer stack (one
// layer per type, in type order) and lookups of mesh instances in root
// coordinates.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d::test {

struct InvTech {
  std::shared_ptr<const tech::Technology> tech;
  tech::TileType ndiff = 0;
  tech::TileType pdiff = 0;
  tech::TileType poly = 0;
  tech::TileType ndc = 0;
  tech::TileType pdc = 0;
  tech::TileType pc = 0;
  tech::TileType metal1 = 0;
  std::vector<tech::TileType> order;  // the stack's types, in layer order
};

inline InvTech build_inverter_tech() {
  tech::TechnologyBuilder b;
  b.set_name("pick");
  REQUIRE(b.add_plane("active").ok());
  REQUIRE(b.add_plane("metal1").ok());
  InvTech t;
  const auto type = [&](std::string_view plane, std::string_view name) {
    auto r = b.add_type(plane, name);
    REQUIRE(r.ok());
    return r.value();
  };
  t.ndiff = type("active", "ndiff");
  t.pdiff = type("active", "pdiff");
  t.poly = type("active", "poly");
  t.ndc = type("active", "ndc");
  t.pdc = type("active", "pdc");
  t.pc = type("active", "pc");
  t.metal1 = type("metal1", "metal1");
  REQUIRE(b.add_contact("ndc", std::array<std::string_view, 2>{"ndiff", "metal1"}).ok());
  REQUIRE(b.add_contact("pdc", std::array<std::string_view, 2>{"pdiff", "metal1"}).ok());
  REQUIRE(b.add_contact("pc", std::array<std::string_view, 2>{"poly", "metal1"}).ok());
  REQUIRE(b.add_connect("ndiff,ndc", "ndiff,ndc").ok());
  REQUIRE(b.add_connect("pdiff,pdc", "pdiff,pdc").ok());
  REQUIRE(b.add_connect("poly,pc", "poly,pc").ok());
  REQUIRE(b.add_connect("metal1,ndc,pdc,pc", "metal1,ndc,pdc,pc").ok());
  auto fin = b.finalize();
  REQUIRE(fin.ok());
  t.tech = fin.value();
  t.order = {t.ndiff, t.pdiff, t.poly, t.ndc, t.pdc, t.pc, t.metal1};
  return t;
}

/// One layer per type of `t.order`, h = 1 + index, t = 0.5.
inline LayerStack stack_of(const InvTech& t) {
  std::vector<StackLayer> layers;
  for (std::size_t i = 0; i < t.order.size(); ++i) {
    StackLayer l;
    l.name = std::string(t.tech->type_name(t.order[i]));
    l.type = t.order[i];
    l.height = 1.0F + static_cast<float>(i);
    l.thickness = 0.5F;
    l.source = ValueSource::extract;
    layers.push_back(l);
  }
  return LayerStack{Mode::layout_layer, std::move(layers), true, 0.01};
}

/// The stack layer of a type of `t.order`.
inline LayerId layer_of(const InvTech& t, tech::TileType type) {
  for (std::size_t i = 0; i < t.order.size(); ++i) {
    if (t.order[i] == type) {
      return static_cast<LayerId>(i);
    }
  }
  FAIL("type not in the stack");
  return 0;
}

/// An editor session on the inverter technology.
struct Session {
  InvTech t = build_inverter_tech();
  editor::EditorState editor;
  LayerStack stack = stack_of(t);

  Session() { editor.technology = t.tech; }

  db::CellDef& cell(const std::string& name) {
    db::CellDef* def = editor.cells.create(name, t.tech->plane_count()).value();
    def->flags().available = true;
    return *def;
  }

  void paint(db::CellDef& def, const db::Rect& r, tech::TileType type) {
    REQUIRE(db::paint(def, *t.tech, r, type).ok());
    db::recompute_bbox(def);
  }

  db::CellUse& place(db::CellDef& parent, db::CellDef& child, const std::string& id, db::Coord dx, db::Coord dy) {
    db::PlaceOptions o;
    o.id = id;
    o.transform = db::translation(dx, dy);
    auto u = db::place(parent, child, o);
    REQUIRE(u.ok());
    db::recompute_bbox(parent);
    return *u.value();
  }

  [[nodiscard]] MeshModel mesh(const db::CellDef& root) const {
    BuildContext ctx;
    const db::Rect bb = root.bbox();
    const db::Rect area{bb.x0 - 1, bb.y0 - 1, bb.x1 + 1, bb.y1 + 1};
    auto m = build_from_layout(stack, root, ClipArea{area, false, 0}, ctx);
    REQUIRE(m.has_value());
    return *m;
  }

  [[nodiscard]] PickContext context(db::CellDef& root) const { return PickContext{&stack, &root, true}; }
};

/// The id of the rectangle instance of `layer` whose root rectangle is `r`.
inline std::optional<PickId> rect_id(const MeshModel& mesh, LayerId layer, const db::Rect& r) {
  for (std::size_t c = 0; c < mesh.chunks.size(); ++c) {
    for (std::size_t i = 0; i < mesh.chunks[c].rects.size(); ++i) {
      const PickRef ref{c, PickKind::rect, i};
      const auto target = resolve(mesh, encode_pick(mesh, ref));
      if (target.has_value() && target->layer == layer && target->root_rect == r) {
        return encode_pick(mesh, ref);
      }
    }
  }
  return std::nullopt;
}

/// The id of the first rectangle instance of `layer` containing point (x, y).
inline std::optional<PickId> rect_id_at(const MeshModel& mesh, LayerId layer, db::Coord x, db::Coord y) {
  for (std::size_t c = 0; c < mesh.chunks.size(); ++c) {
    for (std::size_t i = 0; i < mesh.chunks[c].rects.size(); ++i) {
      const PickId id = encode_pick(mesh, PickRef{c, PickKind::rect, i});
      const auto target = resolve(mesh, id);
      const db::Rect& r = target->root_rect;
      if (target->layer == layer && r.x0 <= x && x < r.x1 && r.y0 <= y && y < r.y1) {
        return id;
      }
    }
  }
  return std::nullopt;
}

/// Every instance id of the model, in order.
inline std::vector<PickId> all_ids(const MeshModel& mesh) {
  std::vector<PickId> out;
  std::uint64_t n = 0;
  for (const Chunk& c : mesh.chunks) {
    n += pick_instance_count(c);
  }
  for (std::uint64_t id = 1; id <= n; ++id) {
    out.push_back(static_cast<PickId>(id));
  }
  return out;
}

}  // namespace hefesto::view3d::test
