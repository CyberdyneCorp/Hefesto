// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Net collection and tile fracturing (openspec/specs/extresist-network
// "Net geometry collection", "Tile fracturing"; tasks.md 13.1; Q-NETLIST-23,
// Q-NETLIST-53; design D6/R6: only scratch planes reach the stitch editor).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <ostream>
#include <tuple>
#include <vector>

#include "hefesto/db/plane.hpp"
#include "hefesto/db/plane_stitch.hpp"
#include "resnet_fixture.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace extresist = hefesto::extresist;
using hefesto::test::resnet::Box;
using hefesto::test::resnet::env;

#define FRACTURE "[cap:extresist-network][req:Tile fracturing]"
#define COLLECT "[cap:extresist-network][req:Net geometry collection]"

namespace {

struct Piece {
  db::Rect rect;
  bool split = false;
  tech::TileType left = 0;

  friend bool operator==(const Piece&, const Piece&) = default;
};

std::ostream& operator<<(std::ostream& os, const Piece& x) {
  return os << "[" << x.rect.x0 << "," << x.rect.y0 << "," << x.rect.x1 << "," << x.rect.y1
            << (x.split ? " split " : " ") << x.left << "]";
}

std::vector<Piece> pieces(const db::Plane& p) {
  std::vector<Piece> out;
  tech::TypeMask mask = tech::TypeMask::all();
  mask.clear_type(tech::kSpace);
  p.enumerate(db::whole_plane(), mask, [&](const db::TileView& v) {
    if (!v.split || !v.right_side) {
      out.push_back(Piece{v.area, v.split, v.split ? v.left_type : v.type});
    }
    return true;
  });
  std::sort(out.begin(), out.end(), [](const Piece& a, const Piece& b) {
    return std::tie(a.rect.x0, a.rect.y0) < std::tie(b.rect.x0, b.rect.y0);
  });
  return out;
}

std::vector<db::Rect> rects(const db::Plane& p) {
  std::vector<db::Rect> out;
  for (const Piece& x : pieces(p)) {
    out.push_back(x.rect);
  }
  return out;
}

db::Plane plane_of(const std::vector<db::Rect>& rs) {
  const auto& e = env();
  const tech::TileType m1 = e.type("metal1");
  db::Plane p;
  for (const db::Rect& r : rs) {
    REQUIRE(p.paint(r, db::paint_row(*e.technology, *e.technology->home_plane(m1), m1)).ok());
  }
  return p;
}

void fracture(db::Plane& p, bool compat = true) {
  db::StitchEditor ed(p);
  REQUIRE(extresist::fracture(ed, p.bounding_box(), compat).ok());
}

}  // namespace

TEST_CASE("a wide vertical branch is cut vertically at the concave corner", FRACTURE) {
  // NL-RESNET-07's first L: [0,100]x[0,10] u [0,30]x[10,100]; corner #2 at
  // (30,10) with xl = 30 > yl = 10.
  db::Plane p = plane_of({{0, 0, 100, 10}, {0, 10, 30, 100}});
  REQUIRE(rects(p) == std::vector<db::Rect>{{0, 0, 100, 10}, {0, 10, 30, 100}});
  fracture(p);
  CHECK(rects(p) == std::vector<db::Rect>{{0, 0, 30, 100}, {30, 0, 100, 10}});
}

TEST_CASE("equal runs keep the horizontal strips", FRACTURE) {
  // NL-RESNET-07's second L: [0,100]x[0,10] u [0,10]x[10,100]; xl = yl = 10.
  db::Plane p = plane_of({{0, 0, 100, 10}, {0, 10, 10, 100}});
  const std::vector<db::Rect> before = rects(p);
  fracture(p);
  CHECK(rects(p) == before);
}

TEST_CASE("a T is cut at both concave corners", FRACTURE) {
  // [0,100]x[0,10] with a stem [40,60]x[10,100]: corner #2 at (60,10) and
  // corner #4 at (40,10), each with a run of 40 against a height of 10.
  db::Plane p = plane_of({{0, 0, 100, 10}, {40, 10, 60, 100}});
  fracture(p);
  CHECK(rects(p) == std::vector<db::Rect>{{0, 0, 40, 10}, {40, 0, 60, 100}, {60, 0, 100, 10}});
}

namespace {

/// A split metal1 tile [0,40]x[0,10] (metal1 in the upper-left triangle)
/// under [0,30]x[10,100]: corner #2 at (30,10) cuts the split tile.
db::Plane split_under_branch() {
  const auto& e = env();
  const tech::TileType m1 = e.type("metal1");
  db::Plane p;
  REQUIRE(p.paint_triangle({0, 0, 40, 10}, db::Diagonal::ne_sw, false,
                           db::paint_row(*e.technology, *e.technology->home_plane(m1), m1), true, nullptr,
                           db::PaintMerge::none)
              .ok());
  REQUIRE(p.paint({0, 10, 30, 100}, db::paint_row(*e.technology, *e.technology->home_plane(m1), m1)).ok());
  return p;
}

}  // namespace

// Regression (review of PR #119): the split tile was made Manhattan before
// the cut, which is not what Magic does.
TEST_CASE("a split tile at a cut is cut as ResSplitX cuts it with magic-compat ON", FRACTURE) {
  // Q-NETLIST-53 (compat): TiSplitX leaves the left part [0,30]x[0,10] a
  // split tile with the whole body, the new right part [30,40]x[0,10] gets
  // the left type (metal1), and TiJoinY1 joins the branch above into the
  // split tile (the branch is not resSrTile), stretching the diagonal
  // over [0,30]x[0,100] (resis/ResFract.c:436-490, tiles/tile.c:251-300).
  db::Plane p = split_under_branch();
  const tech::TileType m1 = env().type("metal1");
  fracture(p, true);
  CHECK(pieces(p) == std::vector<Piece>{{{0, 0, 30, 100}, true, m1}, {{30, 0, 40, 10}, false, m1}});
}

TEST_CASE("a split tile at a cut is kept whole with magic-compat OFF", FRACTURE) {
  // Q-NETLIST-53 (compat): the cut is skipped and the diagonal stays.
  db::Plane p = split_under_branch();
  const std::vector<Piece> before = pieces(p);
  fracture(p, false);
  CHECK(pieces(p) == before);
  bool diagonal = false;
  for (const Piece& x : pieces(p)) {
    diagonal = diagonal || x.split;
  }
  CHECK(diagonal);
}

TEST_CASE("only a scratch cell of yank_net reaches the stitch editor", FRACTURE) {
  // Design D6 / R6: a cell's own planes are never fractured.
  const auto& e = env();
  auto src = hefesto::test::resnet::cell({Box{"metal1", {0, 0, 10, 10}}});
  extresist::NetGeometry geometry;
  geometry.scratch = std::make_unique<db::CellDef>("not_scratch", e.technology->plane_count());
  geometry.source = src.get();
  extresist::BuildInput input;
  input.technology = e.technology.get();
  const auto net = extresist::build_network(geometry, e.style, {}, input);
  REQUIRE_FALSE(net.ok());
  CHECK(net.error().code == "EXTRESIST-NET-002");
  // The scaffold overloads have no technology.
  CHECK(extresist::build_network(geometry, e.style, {}).error().code == "EXTRESIST-NET-001");
  CHECK(extresist::yank_net(*src, {0, 0}, 0, {}).error().code == "EXTRESIST-NET-001");
}

TEST_CASE("skip removes a type from the copied net", COLLECT) {
  // Q-NETLIST-23: metal1 -> pc -> poly; with poly skipped the poly part of
  // the net is not copied.
  const auto& e = env();
  auto src = hefesto::test::resnet::cell(
      {Box{"metal1", {0, 0, 50, 10}}, Box{"pc", {50, 0, 60, 10}}, Box{"poly", {60, 0, 100, 10}}});
  extresist::YankRequest req;
  req.area = {0, 3, 4, 7};
  req.start_types = e.technology->connects_to(e.type("metal1"));
  const auto poly_on_active = [&](const extresist::NetGeometry& g) {
    bool found = false;
    g.scratch->plane(1 + tech::kFirstUserPlane)
        .enumerate(db::whole_plane(), tech::TypeMask::of(e.type("poly")), [&](const db::TileView&) {
          found = true;
          return false;
        });
    return found;
  };
  auto all = extresist::yank_net(*src, *e.technology, req);
  REQUIRE(all.ok());
  CHECK(poly_on_active(all.value()));
  req.skip = tech::TypeMask::of(e.type("poly"));
  auto skipped = extresist::yank_net(*src, *e.technology, req);
  REQUIRE(skipped.ok());
  CHECK_FALSE(poly_on_active(skipped.value()));
  CHECK(skipped.value().source == src.get());
}
