// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Network construction from layout (openspec/specs/extresist-network
// "Device annotation", "Start tile and origin", "Breakpoints", "Tile
// processing order", "Tile resistors", "Near-device cutting", "Contact
// resistors", "Construction-time reductions"; tasks.md 13.2-13.4;
// Q-NETLIST-43). The values are derived by hand from Magic's routines
// (resis/ResMakeRes.c, ResMerge.c); areas are single precision.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "extresist/network/builder.hpp"
#include "resnet_fixture.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace extresist = hefesto::extresist;
namespace detail = hefesto::extresist::detail;
using hefesto::test::resnet::at;
using hefesto::test::resnet::between;
using hefesto::test::resnet::Box;
using hefesto::test::resnet::build;
using hefesto::test::resnet::cell;
using hefesto::test::resnet::env;
using hefesto::test::resnet::Run;

namespace {

Run wire_run(db::Point start, const char* type, db::Rect port) {
  Run run;
  run.start = start;
  run.type = type;
  run.driver = extresist::NetDriver{start, env().type(type), true};
  run.ports = {extresist::NetPort{"out", port, env().type(type), false}};
  return run;
}

}  // namespace

TEST_CASE("a straight wire is one east-west resistor", "[cap:extresist-network][req:Tile resistors]") {
  // NL-RESNET-01: metal2 (80 mOhm/sq) 100x4; the origin at (0,2) and the
  // port at (99,1): 80 * 99 / 4.
  auto src = cell({Box{"metal2", {0, 0, 100, 4}}});
  const auto net = build(*src, wire_run({0, 2}, "metal2", {99, 1, 100, 3}));
  REQUIRE(net.resistors.size() == 1);
  CHECK(net.resistors[0].resistance == static_cast<double>(80.0F * 99.0F / 4.0F));
  CHECK(net.resistors[0].centerline == 2);
  CHECK(net.resistors[0].width == 4);
  CHECK((net.resistors[0].status & extresist::kStatusEW) != 0);
  REQUIRE(at(net, {0, 2}) != nullptr);
  REQUIRE(at(net, {99, 1}) != nullptr);
  CHECK(at(net, {0, 2})->cap == 198.0);   // (99*4)/2
  CHECK(at(net, {99, 1})->cap == 202.0);  // 198 + 4*(100-99)
  CHECK(at(net, {99, 1})->name == "out");
  CHECK(at(net, {0, 2})->path_resistance == 0.0);
}

TEST_CASE("the half areas of a tile resistor are truncated", "[cap:extresist-network][req:Tile resistors]") {
  // NL-RESNET-09: a 7x3 metal1 tile, breakpoints at (0,1) and (5,2):
  // 100*5/3 in single precision, (5*3)/2 = 7 to each end.
  auto src = cell({Box{"metal1", {0, 0, 7, 3}}});
  const auto net = build(*src, wire_run({0, 1}, "metal1", {5, 2, 7, 3}));
  REQUIRE(net.resistors.size() == 1);
  CHECK(net.resistors[0].resistance == static_cast<double>(100.0F * 5.0F / 3.0F));
  CHECK(at(net, {0, 1})->cap == 7.0);
  CHECK(at(net, {5, 2})->cap == 13.0);  // 7 + 3*(7-5): 1 of the 21 is lost
}

TEST_CASE("a tall tile is cut north-south", "[cap:extresist-network][req:Tile resistors]") {
  // NL-RESNET-09: [0,4]x[0,100] metal1, breakpoints at (2,0) and (1,99).
  auto src = cell({Box{"metal1", {0, 0, 4, 100}}});
  const auto net = build(*src, wire_run({2, 0}, "metal1", {1, 99, 3, 100}));
  REQUIRE(net.resistors.size() == 1);
  CHECK(net.resistors[0].resistance == static_cast<double>(100.0F * 99.0F / 4.0F));
  CHECK(net.resistors[0].centerline == 2);
  CHECK(net.resistors[0].width == 4);
  CHECK((net.resistors[0].status & extresist::kStatusNS) != 0);
}

TEST_CASE("an L-shaped wire is reduced to one resistor",
          "[cap:extresist-network][req:Construction-time reductions]"
          "[cap:extresist-network][req:Breakpoints]"
          "[cap:extresist-network][req:Tile processing order]") {
  // NL-RESNET-11 (the scenario's values, with the port at (95,99)):
  // S = [0,100]x[0,10], T = [90,100]x[10,100]; the junction J at (95,10)
  // gets R1 = 950 (O 475, J 525) and R2 = 890 (J +445, P 455); J is a
  // series node: O-P = 1840, and J's area 970 is shared.
  auto src = cell({Box{"metal1", {0, 0, 100, 10}}, Box{"metal1", {90, 10, 100, 100}}});
  const auto net = build(*src, wire_run({0, 5}, "metal1", {95, 99, 96, 100}));
  REQUIRE(net.nodes.size() == 2);
  REQUIRE(net.resistors.size() == 1);
  CHECK(net.resistors[0].resistance == 1840.0);
  // The origin finished last, so it heads the done list.
  CHECK(net.nodes[0].location == db::Point{0, 5});
  CHECK(net.origin == std::size_t{0});
  const float j = 525.0F + 445.0F;
  CHECK(net.nodes[0].cap == static_cast<double>(475.0F + 890.0F * j / 1840.0F));
  CHECK(net.nodes[1].cap == static_cast<double>(455.0F + 950.0F * j / 1840.0F));
  CHECK(net.nodes[1].name == "out");
}

TEST_CASE("a device driver starts at its S/D edge",
          "[cap:extresist-network][req:Start tile and origin]"
          "[cap:extresist-network][req:Device annotation]") {
  // ndiff [0,10] and [20,30] around an nfet [10,20]x[0,10] with gate poly
  // above and below: the net is the left ndiff. The nfet's left neighbour is in
  // S/D class 0, so the origin is at (10, 5); the device's source terminal
  // merges into it.
  auto src = cell({Box{"ndiff", {0, 0, 10, 10}}, Box{"nfet", {10, 0, 20, 10}}, Box{"ndiff", {20, 0, 30, 10}},
                   Box{"poly", {10, -5, 20, 0}}, Box{"poly", {10, 10, 20, 15}}});
  Run run;
  run.start = {2, 5};
  run.type = "ndiff";
  run.driver = extresist::NetDriver{{10, 0}, env().type("nfet"), false};
  extresist::NetDevice dev;
  dev.model = "nfet";
  dev.location = {10, 0};
  dev.type = env().type("nfet");
  run.devices.devices = {dev};
  const auto net = build(*src, run);
  REQUIRE(net.nodes.size() == 1);
  CHECK(net.nodes[0].location == db::Point{10, 5});
  CHECK(net.nodes[0].path_resistance == 0.0);
  CHECK(net.nodes[0].cap == 100.0);
  REQUIRE(net.devices.size() == 1);
  const auto& d = net.devices[0];
  CHECK(d.type == env().type("nfet"));
  REQUIRE(d.terminals.size() == 4);
  CHECK(d.terminals[2] == 0);
  CHECK(d.terminals[0] == extresist::kNoNode);
  // perimeter 40, overlap with the gate poly 20: L = 20/2, W = (40-20)/2.
  CHECK(d.length == 10);
  CHECK(d.width == 10);
  REQUIRE(net.input_devices.size() == 1);
  CHECK(net.input_devices[0] == std::size_t{0});
  CHECK(net.driver_source == std::size_t{0});
}

TEST_CASE("a missing wire at the drive point fails the net", "[cap:extresist-network][req:Start tile and origin]") {
  auto src = cell({Box{"metal1", {0, 0, 10, 10}}});
  Run run = wire_run({2, 2}, "metal1", {8, 8, 9, 9});
  run.driver.location = {50, 50};
  const auto& e = env();
  extresist::YankRequest req;
  req.area = {0, 0, 4, 4};
  req.start_types = e.technology->connects_to(e.type("metal1"));
  auto geometry = extresist::yank_net(*src, *e.technology, req);
  REQUIRE(geometry.ok());
  extresist::BuildInput input;
  input.technology = e.technology.get();
  input.start = run.start;
  input.driver = run.driver;
  const auto net = extresist::build_network(geometry.value(), e.style, {}, input);
  REQUIRE(net.ok());
  CHECK(net.value().failed);
  REQUIRE_FALSE(net.value().messages.empty());
  CHECK(net.value().messages.back() == "Couldn't find wire at 50 50");
}

TEST_CASE("a two-cut via is one contact resistor", "[cap:extresist-network][req:Contact resistors]") {
  // NL-RESNET-03: an 8x4 via, CIF scale 10, squares border 10 size 20
  // separation 20 (minside 40): nx = (80-40)/40 + 1 = 2, ny = 1, 10000/2.
  auto src = cell({Box{"metal1", {0, 0, 48, 4}}, Box{"metal2", {40, 0, 100, 4}}, Box{"via", {40, 0, 48, 4}}});
  Run run = wire_run({0, 2}, "metal1", {99, 1, 100, 3});
  run.ports[0].type = env().type("metal2");
  const auto& e = env();
  extresist::YankRequest req;
  req.area = {0, 0, 4, 4};
  req.start_types = e.technology->connects_to(e.type("metal1"));
  auto geometry = extresist::yank_net(*src, *e.technology, req);
  REQUIRE(geometry.ok());
  extresist::BuildInput input;
  input.technology = e.technology.get();
  input.cif = e.cif ? &*e.cif : nullptr;
  input.start = run.start;
  input.driver = run.driver;
  input.ports = run.ports;
  input.geometry = true;  // no construction-time reductions: the via stays
  const auto net = extresist::build_network(geometry.value(), e.style, {}, input);
  REQUIRE(net.ok());
  const extresist::NetResistor* via = between(net.value(), {44, 2}, {44, 2});
  REQUIRE(via != nullptr);
  CHECK(via->resistance == 5000.0);
  CHECK(via->width == 2);       // nx
  CHECK(via->centerline == 1);  // ny
  CHECK(via->type == e.type("via"));
}

namespace {

/// A Builder over one ndiff tile [0,20]x[0,10] with breakpoints set by hand
/// (ResCalcNearDevice).
struct NearDevice {
  extresist::NetGeometry geometry;
  extresist::BuildInput input;
  extresist::DeviceAnnotations devices;
  std::unique_ptr<detail::Builder> b;
  detail::Id tile = detail::kNil;

  explicit NearDevice(bool magic_compat) {
    const auto& e = env();
    geometry.scratch = std::make_unique<db::CellDef>("__RESIS__", e.technology->plane_count());
    const tech::TileType nd = e.type("ndiff");
    const tech::PlaneId active = *e.technology->home_plane(nd);
    REQUIRE(geometry.scratch->plane(active).paint({0, 0, 20, 10}, db::paint_row(*e.technology, active, nd)).ok());
    input.technology = e.technology.get();
    input.magic_compat = magic_compat;
    b = std::make_unique<detail::Builder>(geometry, e.style, devices, input);
    b->open_editors();
    tile = b->add_field(b->goto_point(active, {5, 5}));
    b->g.tiles[tile].status |= detail::kTileDone;
  }

  detail::Id node(db::Point p, std::uint32_t why) {
    const detail::Id n = b->g.new_node(p, why);
    b->g.add_to_queue(n, b->g.pending);
    b->g.new_break(tile, n, p, detail::kNil);
    return n;
  }
};

}  // namespace

TEST_CASE("a lone device breakpoint drops the tile area with magic-compat ON",
          "[cap:extresist-network][req:Near-device cutting]") {
  // Q-NETLIST-43 (compat): as Magic, no area and no resistor.
  NearDevice t(true);
  const detail::Id d = t.node({20, 5}, extresist::kNodeDevice);
  CHECK_FALSE(t.b->calc_tile_resistance(t.tile));
  CHECK(t.b->g.nodes[d].area == 0.0F);
  CHECK(t.b->g.res.empty());
}

TEST_CASE("a lone device breakpoint keeps the tile area with magic-compat OFF",
          "[cap:extresist-network][req:Near-device cutting]") {
  // Q-NETLIST-43 (compat): width x height = 200 goes to the node.
  NearDevice t(false);
  const detail::Id d = t.node({20, 5}, extresist::kNodeDevice);
  CHECK_FALSE(t.b->calc_tile_resistance(t.tile));
  CHECK(t.b->g.nodes[d].area == 200.0F);
  CHECK(t.b->g.res.empty());
}

TEST_CASE("a device on the right edge cuts the tile east-west", "[cap:extresist-network][req:Near-device cutting]") {
  // A junction at (0,5) and a device terminal at (20,5): dx = 20, dy = 0.
  NearDevice t(true);
  const detail::Id j = t.node({0, 5}, extresist::kNodeJunction);
  const detail::Id d = t.node({20, 5}, extresist::kNodeDevice);
  t.b->calc_tile_resistance(t.tile);
  REQUIRE(t.b->g.res.size() == 1);
  CHECK(t.b->g.res[0].value == 1000.0F * 20.0F / 10.0F);
  CHECK((t.b->g.res[0].status & extresist::kStatusEW) != 0);
  CHECK(t.b->g.nodes[j].area == 100.0F);
  CHECK(t.b->g.nodes[d].area == 100.0F);
}

TEST_CASE("device breakpoints on the top and bottom edges cut north-south",
          "[cap:extresist-network][req:Near-device cutting]") {
  NearDevice t(true);
  t.node({10, 10}, extresist::kNodeDevice);
  t.node({10, 0}, extresist::kNodeDevice);
  t.b->calc_tile_resistance(t.tile);
  REQUIRE(t.b->g.res.size() == 1);
  CHECK((t.b->g.res[0].status & extresist::kStatusNS) != 0);
  CHECK(t.b->g.res[0].value == 1000.0F * 10.0F / 20.0F);
}

TEST_CASE("a contact without a CIF cut joins its planes without resistance",
          "[cap:extresist-network][req:Contact resistors]") {
  // Standing rule (undefined behaviour fixed in both modes): Magic's
  // ResDoContacts reads CIFGetContactSize's unset cut size when the output
  // style has no cut for the type (and the scale factor of a missing
  // style). Without a cut the contact is one node on both planes.
  auto src = cell({Box{"metal1", {0, 0, 48, 4}}, Box{"metal2", {40, 0, 100, 4}}, Box{"via", {40, 0, 48, 4}}});
  const auto& e = env();
  extresist::YankRequest req;
  req.area = {0, 0, 4, 4};
  req.start_types = e.technology->connects_to(e.type("metal1"));
  for (const bool compat : {true, false}) {
    auto geometry = extresist::yank_net(*src, *e.technology, req);
    REQUIRE(geometry.ok());
    extresist::BuildInput input;
    input.technology = e.technology.get();
    input.cif = nullptr;
    input.start = {0, 2};
    input.driver = extresist::NetDriver{{0, 2}, e.type("metal1"), true};
    input.ports = {extresist::NetPort{"out", {99, 1, 100, 3}, e.type("metal2"), false}};
    input.geometry = true;
    input.magic_compat = compat;
    const auto net = extresist::build_network(geometry.value(), e.style, {}, input);
    REQUIRE(net.ok());
    CHECK(between(net.value(), {44, 2}, {44, 2}) == nullptr);
    std::size_t contact_nodes = 0;
    for (const auto& n : net.value().nodes) {
      contact_nodes += n.kind == extresist::kNodeContact ? 1U : 0U;
    }
    CHECK(contact_nodes == 1);
  }
}
