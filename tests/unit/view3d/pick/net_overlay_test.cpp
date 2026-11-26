// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layout-net overlay of mask-layer mode (M10 task 10.4; openspec/
// changes/m10-3d-view/design.md D10): the net found through `connect`
// drawn as prisms in the mask-layer mesh's units over the drawn stack's z
// range, clipped by the cut box. The inverter of highlight_test.cpp in a
// `wind3d` session whose output style has scalefactor 10. Owner: CORE
// (round 6).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string_view>

#include "../client/client_fixture.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/select/selection.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "hefesto/view3d/pick/pick.hpp"

namespace connect = hefesto::connect;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace view3d = hefesto::view3d;
using Catch::Approx;
using db::Rect;

namespace {

// The inverter technology with a mask style: POLY drawn from 0.4 to 0.5,
// MET1 from 1.0 to 1.5 (`render`, µm); 0.1 µm per internal unit.
// (metal2 is there for the session fixture's paint.)
constexpr std::string_view kInvTech = R"(tech
    format 35
    v3dinv
end
planes
    active,a
    metal1,m1
    metal2,m2
end
types
    active  ndiffusion,ndiff
    active  pdiffusion,pdiff
    active  polysilicon,poly
    active  ndcontact,ndc
    active  pdcontact,pdc
    active  polycontact,pc
    metal1  metal1,m1
    metal2  metal2,m2
end
styles
    styletype mos
    poly 1
    metal1 20
    metal2 21
end
contact
    ndc ndiff metal1
    pdc pdiff metal1
    pc poly metal1
end
compose
end
connect
    ndiff,ndc  ndiff,ndc
    pdiff,pdc  pdiff,pdc
    poly,pc    poly,pc
    metal1,ndc,pdc,pc  metal1,ndc,pdc,pc
    metal2  metal2
end
cifoutput
style gdsii
    scalefactor 10
    layer POLY poly,pc
        calma 66 20
    layer MET1 m1,ndc,pdc,pc
        calma 68 20
    render POLY metal1 0.4 0.1
    render MET1 metal1 1.0 0.5
end
cifinput
end
drc
end
extract
style default
    lambda 1
    step 100
    sidehalo 0
    planeorder active 0
    planeorder metal1 1
    planeorder metal2 2
    height poly 0.2 0.1
    height metal1 0.5 0.3
end
)";

constexpr db::Coord kX = 200;  // the inverter's offset, clear of the fixture's metal1 block

struct InvSession : view3d::test::ClientSession {
  explicit InvSession(bool compat) : ClientSession(compat, kInvTech) {
    paint("ndiff", at(Rect{0, 0, 30, 10}));
    paint("pdiff", at(Rect{0, 40, 30, 50}));
    paint("poly", at(Rect{12, -5, 18, 55}));
    paint("metal1", at(Rect{0, -10, 10, 10}));  // GND
    paint("metal1", at(Rect{0, 40, 10, 60}));   // VDD
    paint("metal1", at(Rect{20, 0, 30, 50}));   // OUT
    paint("ndc", at(Rect{2, 2, 8, 8}));
    paint("ndc", at(Rect{22, 2, 28, 8}));
    paint("pdc", at(Rect{2, 42, 8, 48}));
    paint("pdc", at(Rect{22, 42, 28, 48}));
    ok("specialopen wind3d c3d");
    REQUIRE(window().mode == view3d::Mode::mask_layer);
  }

  static Rect at(const Rect& r) { return Rect{r.x0 + kX, r.y0, r.x1 + kX, r.y1}; }

  tech::TileType type(std::string_view name) { return *editor.technology->find_type(name).id; }

  view3d::MeshHandle mesh() {
    view3d::BuildContext ctx;
    const view3d::Scene scene = view3d::build_scene(editor, window(), project.compat.magic_compat, ctx);
    REQUIRE(scene.mesh);
    return scene.mesh;
  }

  view3d::PickTarget seed(std::string_view type_name, const Rect& material) {
    view3d::PickTarget t;
    t.type = type(type_name);
    t.cell = cell;
    t.material = material;
    return t;
  }

  view3d::NetOptions options(view3d::NetScope scope = view3d::NetScope::flat) const {
    view3d::NetOptions o;
    o.scope = scope;
    o.magic_compat = project.compat.magic_compat;
    return o;
  }
};

/// The net's area per type (tiles on their home planes) within `clip`, from
/// connect::search_flat.
struct NetArea final : connect::NetVisitor {
  const tech::Technology* tech;
  Rect clip;
  std::map<tech::TileType, std::int64_t> area;

  NetArea(const tech::Technology& t, const Rect& c) : tech(&t), clip(c) {}

  bool on_tile(const connect::NetTile& t) override {
    if (tech->home_plane(t.tile.type) != t.plane) {
      return true;
    }
    const Rect& r = t.tile.area;
    const std::int64_t w = std::int64_t{std::min(r.x1, clip.x1)} - std::max(r.x0, clip.x0);
    const std::int64_t h = std::int64_t{std::min(r.y1, clip.y1)} - std::max(r.y0, clip.y0);
    if (w > 0 && h > 0) {
      area[t.tile.type] += w * h;
    }
    return true;
  }
};

std::map<tech::TileType, std::int64_t> expected_area(InvSession& s, const view3d::PickTarget& seed,
                                                     const Rect& clip = db::whole_plane()) {
  NetArea net(*s.editor.technology, clip);
  connect::SearchStart start;
  start.area = seed.material;
  start.types = tech::TypeMask::of(*seed.type);
  connect::SearchOptions o;
  o.magic_compat = s.project.compat.magic_compat;
  REQUIRE(connect::search_flat(*s.cell, *s.editor.technology, start, o, net).ok());
  return net.area;
}

/// The overlay's area per type, in internal units squared (mesh units
/// divided by `scale`).
std::map<tech::TileType, std::int64_t> overlay_area(const view3d::NetOverlay& overlay, std::int32_t scale) {
  REQUIRE(overlay.prisms.size() == overlay.types.size());
  std::map<tech::TileType, std::int64_t> out;
  const double s2 = static_cast<double>(scale) * scale;
  for (std::size_t i = 0; i < overlay.prisms.size(); ++i) {
    const view3d::NetOverlayPrism& p = overlay.prisms[i];
    out[overlay.types[i]] += static_cast<std::int64_t>((static_cast<double>(p.x1 - p.x0) * static_cast<double>(p.y1 - p.y0)) / s2);
  }
  return out;
}

void select_paint(InvSession& s, const Rect& area, tech::TileType type) {
  hefesto::select::Selection& sel = s.editor.selection;
  sel.set_root(s.cell, s.editor.technology->plane_count());
  REQUIRE(db::copy_paint(*s.cell, area, db::kIdentity, *sel.def(), *s.editor.technology, tech::TypeMask::of(type))
              .ok());
}

}  // namespace

TEST_CASE("Net highlighted over mask-layer geometry", "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const bool compat = GENERATE(true, false);
  INFO("magic-compat " << compat);
  {
    InvSession s(compat);
    const view3d::MeshHandle mesh = s.mesh();
    REQUIRE(mesh->units_per_root == 10);
    const view3d::PickTarget out = s.seed("metal1", InvSession::at(Rect{20, 0, 30, 50}));

    SECTION("the overlay covers the net's tiles, scaled to mask units") {
      const view3d::NetOverlay overlay = view3d::net_overlay(*mesh, s.editor, s.window(), out, s.options());
      REQUIRE_FALSE(overlay.empty());
      const auto expected = expected_area(s, out);
      CHECK(overlay_area(overlay, 10) == expected);
      // OUT: metal1 and both drain contacts; not GND, VDD or the gate.
      CHECK(expected.count(s.type("metal1")) == 1);
      CHECK(expected.count(s.type("ndc")) == 1);
      CHECK(expected.count(s.type("pdc")) == 1);
      CHECK(expected.count(s.type("poly")) == 0);
      // Right of the gate (OFF also reaches the drain diffusion, x >= 18).
      for (const view3d::NetOverlayPrism& p : overlay.prisms) {
        CHECK(p.x0 >= static_cast<float>(10 * (kX + 18)));
        CHECK(p.x1 <= static_cast<float>(10 * (kX + 30)));
      }
    }
    SECTION("the prisms span the drawn mask stack: Q-MISC-72 heights per mode") {
      const view3d::NetOverlay overlay = view3d::net_overlay(*mesh, s.editor, s.window(), out, s.options());
      REQUIRE_FALSE(overlay.empty());
      // POLY 0.4 + 0.1 and MET1 1.0 + 0.5 µm: as stored with magic-compat
      // ON, in internal units (0.1 µm each) with it OFF.
      const double unit = compat ? 1.0 : 10.0;
      for (const view3d::NetOverlayPrism& p : overlay.prisms) {
        CHECK(static_cast<double>(p.z_bottom) == Approx(0.4 * unit));
        CHECK(static_cast<double>(p.z_top) == Approx(1.5 * unit));
      }
    }
    SECTION("the cut box clips the overlay") {
      REQUIRE(s.w3d("cutbox 200 0 225 30").ok());
      const view3d::MeshHandle cut = s.mesh();
      const view3d::NetOverlay overlay = view3d::net_overlay(*cut, s.editor, s.window(), out, s.options());
      const Rect box{200, 0, 225, 30};
      CHECK(overlay_area(overlay, 10) == expected_area(s, out, box));
      for (const view3d::NetOverlayPrism& p : overlay.prisms) {
        CHECK(p.x1 <= 2250.0F);
        CHECK(p.y1 <= 300.0F);
      }
    }
    SECTION("the 2D selection seeds the overlay; nothing selected gives none") {
      CHECK(view3d::net_overlay(*mesh, s.editor, s.window(), s.editor.selection, s.options()).empty());
      select_paint(s, InvSession::at(Rect{20, 20, 30, 30}), s.type("metal1"));
      const view3d::NetOverlay from_sel =
          view3d::net_overlay(*mesh, s.editor, s.window(), s.editor.selection, s.options());
      CHECK(overlay_area(from_sel, 10) == expected_area(s, out));
    }
    SECTION("a seed outside the rendered cell or without a tile gives none") {
      view3d::PickTarget none = out;
      none.material = Rect{};
      CHECK(view3d::net_overlay(*mesh, s.editor, s.window(), none, s.options()).empty());
      view3d::PickTarget no_type = out;
      no_type.type.reset();
      CHECK(view3d::net_overlay(*mesh, s.editor, s.window(), no_type, s.options()).empty());
      view3d::PickTarget other = out;
      other.cell = nullptr;
      CHECK(view3d::net_overlay(*mesh, s.editor, s.window(), other, s.options()).empty());
    }
    SECTION("the hierarchical search gives the same net in a flat cell") {
      const view3d::NetOverlay overlay =
          view3d::net_overlay(*mesh, s.editor, s.window(), out, s.options(view3d::NetScope::hierarchical));
      CHECK(overlay_area(overlay, 10) == expected_area(s, out));
    }
  }
}
