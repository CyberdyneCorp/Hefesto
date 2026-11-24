// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-substrate on single cells (openspec/changes/m7-extraction
// tasks.md 7.6): the substrate seeds, the isolated substrate plane, naming
// with `$` variables (Q-EXTRACT-9; scripting-api "Script variables read by
// the engine", SCRIPT-API-16's input), the substrate record, device
// substrate terminals and coupling relocation, pinned to EX-044..048.
// "Subcell substrate connections" waits for EXTHIER (task 7.7).
// Owner: EXTDEV.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "device_fixture.hpp"
#include "hefesto/app/project_file.hpp"

using namespace hefesto::test::dev;  // NOLINT(google-build-using-namespace): test fixture helpers
namespace app = hefesto::app;

namespace {

std::optional<std::string> sub_is_vsubs(std::string_view name) {
  if (name == "SUB") {
    return std::string("VSUBS");
  }
  return std::nullopt;
}

}  // namespace

// --- Backward-compatible mode ------------------------------------------------------

TEST_CASE("No substrate declaration: no substrate node, the record's substrate name",
          "[cap:extract-substrate][req:Backward-compatible mode without a substrate declaration]") {
  const auto c = extract_case("EX-044");
  const extract::SubstrateSeeds seeds = extract::substrate_seeds(*c->cell, *c->tech, c->style);
  CHECK(seeds.none);
  CHECK(seeds.tiles.empty());
  CHECK_FALSE(c->nodes.substrate);
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].substrate_field == extract::SubstrateField::name);
  CHECK(c->result.devices[0].substrate_name == "GND!");
}

TEST_CASE("Backward-compatible mode substitutes a `$` device substrate name, and keeps it when undefined",
          "[cap:extract-substrate][req:Backward-compatible mode without a substrate declaration]") {
  const std::string tech = tech_with_extract("    device mosfet nfet nfet ndiff None $SUB\n");
  const std::string mag = read_text(case_dir("EX-026") + "ex.mag");
  Options o;
  o.variable = sub_is_vsubs;
  const auto c = extract_texts(tech, mag, o);
  REQUIRE(c->result.devices.size() == 1);
  CHECK(c->result.devices[0].substrate_name == "VSUBS");
  const auto undefined = extract_texts(tech, mag);
  REQUIRE(undefined->result.devices.size() == 1);
  CHECK(undefined->result.devices[0].substrate_name == "$SUB");
}

// --- Isolated substrate regions --------------------------------------------------------

TEST_CASE("EX-045: a dnwell ring on the substrate plane isolates nothing",
          "[cap:extract-substrate][req:Isolated substrate regions]") {
  const auto c = extract_case("EX-045");
  const auto plane = extract::isolated_substrate_plane(*c->cell, *c->tech, c->style);
  REQUIRE(plane);  // the shield paint marks the plane as modified
  const tech::TileType pwell = type_of(*c->tech, "pwell");
  const tech::TileType dnwell = type_of(*c->tech, "dnwell");
  CHECK(plane->type_at({13, 13}) == tech::kSpace);  // inside the ring: the dnwell erased its own area
  CHECK(plane->type_at({1, 1}) == dnwell);          // the cell's own paint is copied back
  bool any_pwell = false;
  (void)plane->enumerate(db::whole_plane(), tech::TypeMask::of(pwell), [&](const db::TileView&) {
    any_pwell = true;
    return false;
  });
  CHECK_FALSE(any_pwell);
}

TEST_CASE("Shield paint on another plane becomes the default substrate type, minus non-substrate types",
          "[cap:extract-substrate][req:Isolated substrate regions]") {
  const std::string tech = tech_with_extract("    substrate *psd,space,pwell well $SUB -thkox\n"
                                             "    device mosfet nfet nfet ndiff pwell,space VSUB\n");
  const auto c = extract_texts(tech, "<< thkox >>\nrect 0 0 10 10\n<< nwell >>\nrect 2 2 4 4\n"
                                     "<< metal1 >>\nrect 30 30 31 31\n");
  const auto plane = extract::isolated_substrate_plane(*c->cell, *c->tech, c->style);
  REQUIRE(plane);
  CHECK(plane->type_at({1, 1}) == type_of(*c->tech, "pwell"));
  CHECK(plane->type_at({3, 3}) == type_of(*c->tech, "nwell"));
  CHECK(plane->type_at({20, 20}) == tech::kSpace);
}

TEST_CASE("No default substrate type or no shield paint: no replacement plane",
          "[cap:extract-substrate][req:Isolated substrate regions]") {
  const auto no_default = extract_case("EX-046");  // `substrate space well VSUB`
  CHECK_FALSE(extract::isolated_substrate_plane(*no_default->cell, *no_default->tech, no_default->style));
  const auto no_shield = extract_texts(tech_with_extract("    substrate *psd,space,pwell well $SUB -thkox\n"),
                                       "<< metal1 >>\nrect 0 0 4 4\n");
  CHECK_FALSE(extract::isolated_substrate_plane(*no_shield->cell, *no_shield->tech, no_shield->style));
}

// --- Global substrate node discovery -----------------------------------------------------

TEST_CASE("EX-045: the substrate node is the psd tap over empty well-plane area",
          "[cap:extract-substrate][req:Global substrate node discovery]") {
  const auto c = extract_case("EX-045");
  const extract::SubstrateSeeds seeds = extract::substrate_seeds(*c->cell, *c->tech, c->style);
  REQUIRE(seeds.tiles.size() == 1);
  CHECK(seeds.tiles[0].second.area == db::Rect{60, 0, 64, 4});
  CHECK_FALSE(seeds.tileless);
  REQUIRE(c->nodes.substrate);
  CHECK(c->name(*c->nodes.substrate) == "@60_0");
}

TEST_CASE("Two separate taps form one substrate node", "[cap:extract-substrate][req:Global substrate node discovery]") {
  const auto c = extract_texts(tech_with_extract("    substrate *psd,space well\n"),
                               "<< psd >>\nrect 0 0 4 4\nrect 20 0 24 4\n");
  const extract::SubstrateSeeds seeds = extract::substrate_seeds(*c->cell, *c->tech, c->style);
  CHECK(seeds.tiles.size() == 2);
  REQUIRE(c->nodes.substrate);
  CHECK(c->nodes.list.size() == 1);
}

TEST_CASE("A tap over well-plane paint or under a shield is not substrate",
          "[cap:extract-substrate][req:Global substrate node discovery]") {
  const auto c = extract_texts(tech_with_extract("    substrate *psd,space well VSUB -thkox\n"),
                               "<< psd >>\nrect 0 0 4 4\nrect 20 0 24 4\n<< nwell >>\nrect -1 -1 5 5\n"
                               "<< thkox >>\nrect 19 -1 25 5\n");
  const extract::SubstrateSeeds seeds = extract::substrate_seeds(*c->cell, *c->tech, c->style);
  CHECK(seeds.tiles.empty());
  CHECK(seeds.tileless);
}

TEST_CASE("EX-046: a metal-only cell has a tile-less substrate node at MINFINITY + 3",
          "[cap:extract-substrate][req:Global substrate node discovery]") {
  const auto c = extract_case("EX-046");
  const extract::SubstrateSeeds seeds = extract::substrate_seeds(*c->cell, *c->tech, c->style);
  CHECK(seeds.tileless);
  CHECK(extract::kTilelessSubstrateCoord == -1073741817);
  REQUIRE(c->nodes.substrate);
  CHECK(c->nodes.list[*c->nodes.substrate].reference == db::Point{-1073741817, -1073741817});
}

// --- Substrate node naming --------------------------------------------------------------------

TEST_CASE("EX-046: the tile-less substrate node is named by the declaration",
          "[cap:extract-substrate][req:Substrate node naming]") {
  const auto c = extract_case("EX-046");
  const auto nodes = extract::prepare_substrate(*c->cell, c->style, app::GlobalNets{});
  REQUIRE(nodes.ok());
  CHECK(nodes.value().global_name == "VSUB");
}

TEST_CASE("Q-EXTRACT-9: `$SUB` from the project's global nets, the name without `$` when undefined",
          "[cap:extract-substrate][req:Substrate node naming]") {
  const auto c = extract_case("EX-045");  // `substrate *psd,space,pwell well $SUB -dnwell`
  app::GlobalNets nets;
  nets.sub = "VSUBS";
  CHECK(extract::prepare_substrate(*c->cell, c->style, nets).value().global_name == "VSUBS");
  CHECK(extract::prepare_substrate(*c->cell, c->style, app::GlobalNets{}).value().global_name == "SUB");
  CHECK(extract::substrate_name("$$SUB", sub_is_vsubs) == "$$SUB");  // `$$` is left alone
  CHECK(extract::substrate_name("VSUB", sub_is_vsubs) == "VSUB");
}

TEST_CASE("Substrate variable substitution", "[cap:scripting-api][req:Script variables read by the engine]") {
  CHECK(extract::substrate_name("$SUB", sub_is_vsubs) == "VSUBS");
}

TEST_CASE("Undefined substrate variable", "[cap:scripting-api][req:Script variables read by the engine]") {
  CHECK(extract::substrate_name("$SUB", [](std::string_view) { return std::optional<std::string>{}; }) == "SUB");
}

// --- Substrate record ---------------------------------------------------------------------------

TEST_CASE("EX-048: a LEFview cell writes its substrate node as an ordinary node",
          "[cap:extract-substrate][req:Substrate record]") {
  const auto abstract = extract_case("EX-048");
  CHECK_FALSE(extract::writes_substrate_record(*abstract->cell));
  const auto plain = extract_case("EX-046");
  CHECK(extract::writes_substrate_record(*plain->cell));
}

// --- Device substrate terminals ----------------------------------------------------------------

TEST_CASE("EX-045: devices over empty well-plane area take the global substrate node",
          "[cap:extract-substrate][req:Device substrate terminals]") {
  const auto c = extract_case("EX-045");
  CHECK(c->lines() ==
        std::vector<std::string>{
            "device mosfet nfet 50 12 51 13 2 4 \"@60_0\" \"gout\" 0 0 \"@47_12\" 4 12,14 \"@52_12\" 4 12,14",
            "device mosfet nfet 12 12 13 13 2 4 \"@60_0\" \"gin\" 0 0 \"@9_12\" 4 12,14 \"@14_12\" 4 12,14"});
}

TEST_CASE("A device over a well of its substrate types names that well's node",
          "[cap:extract-substrate][req:Device substrate terminals]") {
  // No default substrate type, so the pwell stays on the substrate plane.
  const auto c = extract_texts(tech_with_extract("    substrate *psd,space well $SUB\n"
                                                 "    device mosfet nfet nfet ndiff pwell,space VSUB\n"),
                               read_text(case_dir("EX-027") + "ex.mag"));  // an nfet in a pwell
  REQUIRE(c->result.devices.size() == 1);
  const auto& d = c->result.devices[0];
  REQUIRE(d.substrate_field == extract::SubstrateField::node);
  CHECK(c->name(*d.substrate) == "@-6_-4");
  CHECK(d.substrate != c->nodes.substrate);
}

// --- Substrate coupling relocation -----------------------------------------------------------------

TEST_CASE("Coupling to the substrate node becomes substrate capacitance of the other node",
          "[cap:extract-substrate][req:Substrate coupling relocation]") {
  extract::NodeRegionList nodes(3);
  extract::CouplingTable table;
  table.add(0, 1, 2.5);
  table.add(2, 0, 4.0);
  table.add(1, 2, 7.0);
  const extract::CouplingTable rest = extract::relocate_substrate_coupling(table, 0, nodes);
  CHECK(nodes[1].substrate_cap == 2.5);
  CHECK(nodes[2].substrate_cap == 4.0);
  CHECK(nodes[0].substrate_cap == 0.0);
  REQUIRE(rest.pairs().size() == 1);
  CHECK(rest.pairs().begin()->first == std::pair<std::size_t, std::size_t>{1, 2});
}
