// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Networks pinned to Magic's captured `.res.ext` of the NL-RESNET cases
// (golden-capture run 45555921742; tasks.md 17.3): the case's own
// technology (tests/golden/NETLIST/cases/NL-RESNET-*/input/exttest.tech),
// its layout, the port labels as drive points (Magic's driver is the last
// port, the head of the drive-point list) and the net capacitance as the
// sum of the captured rnode capacitances. Resistances are compared exactly
// as printed (%g), capacitances to the printed precision.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "resnet_fixture.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace extract = hefesto::extract;
namespace extresist = hefesto::extresist;
namespace diag = hefesto::diag;
using Catch::Approx;

namespace {

struct CaseEnv {
  std::shared_ptr<const tech::Technology> technology;
  extract::ExtStyle style;
  std::optional<hefesto::mask::OutputStyle> cif;
};

const CaseEnv& case_env() {
  static const CaseEnv e = [] {
    std::ifstream in(std::string(HEFESTO_SOURCE_DIR) + "/tests/golden/NETLIST/cases/NL-RESNET-01/input/exttest.tech");
    REQUIRE(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    hefesto::platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    auto loaded = tech::load_technology_text(ss.str(), "/t/exttest.tech", fs, tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    CaseEnv out;
    out.technology = loaded.value();
    const tech::RawSection* section = nullptr;
    for (const tech::RawSection& raw : out.technology->raw_sections()) {
      if (raw.name == "extract") {
        section = &raw;
      }
    }
    REQUIRE(section != nullptr);
    const extract::StyleTable table =
        extract::load_styles(*section, *out.technology, sink, hefesto::compat::CompatSettings{});
    const auto style = table.select(table.names().front());
    REQUIRE(style != nullptr);
    out.cif = hefesto::mask::read_first_output_style(*out.technology);
    out.style = style->micron_convert(out.cif ? &*out.cif : nullptr);
    return out;
  }();
  return e;
}

/// One metal1 net with ports (label rectangles in label order); the last
/// port is the driver.
extresist::Network run(const std::vector<db::Rect>& metal1, const std::vector<db::Rect>& ports, double node_cap) {
  const CaseEnv& e = case_env();
  const tech::TileType m1 = *e.technology->find_type("metal1").id;
  db::CellDef src("rn", e.technology->plane_count());
  for (const db::Rect& r : metal1) {
    REQUIRE(db::paint(src, *e.technology, r, m1).ok());
  }
  const db::Point start{ports.back().x0, ports.back().y0};
  extresist::YankRequest req;
  req.area = {start.x - 2, start.y - 2, start.x + 2, start.y + 2};
  req.start_types = e.technology->connects_to(m1);
  auto geometry = extresist::yank_net(src, *e.technology, req);
  REQUIRE(geometry.ok());
  extresist::BuildInput input;
  input.technology = e.technology.get();
  input.cif = e.cif ? &*e.cif : nullptr;
  input.start = start;
  input.driver = extresist::NetDriver{start, m1, true};
  for (std::size_t i = 0; i < ports.size(); ++i) {
    input.ports.push_back(extresist::NetPort{"p" + std::to_string(i), ports[i], m1, false});
  }
  input.node_cap = node_cap;
  auto built = extresist::build_network(geometry.value(), e.style, {}, input);
  REQUIRE(built.ok());
  extresist::Network net = built.value();
  REQUIRE_FALSE(net.failed);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.min_delay = 0.0;
  REQUIRE(extresist::reduce(net, tol, heap, hefesto::compat::CompatSettings{}).ok());
  return net;
}

double cap_at(const extresist::Network& n, db::Point p) {
  for (const auto& node : n.nodes) {
    if (node.location == p) {
      return node.cap;
    }
  }
  FAIL("no node at " << p.x << "," << p.y);
  return 0.0;
}

}  // namespace

TEST_CASE("NL-RESNET-01: a straight wire", "[cap:extresist-network][req:Tile resistors]") {
  const auto n = run({{0, 0, 100, 4}}, {{0, 0, 0, 4}, {100, 0, 100, 4}}, 4000.0);
  REQUIRE(n.resistors.size() == 1);
  CHECK(n.resistors[0].resistance == 2500.0);
  CHECK(cap_at(n, {0, 0}) == Approx(2000.0));
  CHECK(cap_at(n, {100, 0}) == Approx(2000.0));
}

TEST_CASE("NL-RESNET-09: the truncated 7x3 tile", "[cap:extresist-network][req:Tile resistors]") {
  const auto n = run({{0, 0, 7, 3}}, {{0, 1, 0, 1}, {7, 2, 7, 2}}, 210.0);
  REQUIRE(n.resistors.size() == 1);
  CHECK(n.resistors[0].resistance == Approx(233.333).margin(5e-4));
  CHECK(cap_at(n, {0, 1}) == Approx(105.0));
  CHECK(cap_at(n, {7, 2}) == Approx(105.0));
}

TEST_CASE("NL-RESNET-11: the L-wire reduces to 1850 mOhm",
          "[cap:extresist-network][req:Construction-time reductions]") {
  const auto n = run({{0, 0, 100, 10}, {90, 10, 100, 100}}, {{0, 5, 0, 5}, {95, 100, 95, 100}}, 19000.0);
  REQUIRE(n.resistors.size() == 1);
  CHECK(n.resistors[0].resistance == 1850.0);
  CHECK(cap_at(n, {0, 5}) == Approx(9493.24).margin(0.01));
  CHECK(cap_at(n, {95, 100}) == Approx(9506.76).margin(0.01));
}

TEST_CASE("NL-RESNET-07: the wide branch is fractured", "[cap:extresist-network][req:Tile fracturing]") {
  const auto n = run({{0, 0, 100, 10}, {0, 10, 30, 100}}, {{0, 0, 0, 10}, {0, 100, 30, 100}}, 37000.0);
  REQUIRE(n.resistors.size() == 1);
  CHECK(n.resistors[0].resistance == Approx(333.333).margin(5e-4));
  CHECK(cap_at(n, {0, 0}) == Approx(21650.0).margin(0.01));
  CHECK(cap_at(n, {0, 100}) == Approx(15350.0).margin(0.01));
}

TEST_CASE("NL-RESNET-02: the T-shaped wire", "[cap:extresist-network][req:Breakpoints]") {
  const auto n = run({{200, 0, 300, 10}, {240, 10, 260, 100}},
                     {{200, 0, 200, 10}, {300, 0, 300, 10}, {240, 100, 260, 100}}, 28000.0);
  REQUIRE(n.resistors.size() == 1);
  CHECK(n.resistors[0].resistance == 875.0);
  CHECK(cap_at(n, {200, 0}) == Approx(10957.1).margin(0.05));
  CHECK(cap_at(n, {240, 100}) == Approx(17042.9).margin(0.05));
}

TEST_CASE("NL-RESNET-08: the staircase and the U", "[cap:extresist-network][req:Tile processing order]") {
  const auto st =
      run({{400, 0, 430, 10}, {420, 10, 450, 20}, {440, 20, 470, 30}}, {{400, 0, 400, 10}, {470, 20, 470, 30}}, 9000.0);
  REQUIRE(st.resistors.size() == 1);
  CHECK(st.resistors[0].resistance == 700.0);
  const auto u = run({{200, 0, 300, 10}, {200, 10, 210, 60}, {290, 10, 300, 60}},
                     {{200, 60, 210, 60}, {290, 60, 300, 60}}, 20000.0);
  CHECK(u.resistors.empty());
  REQUIRE(u.nodes.size() == 1);
  CHECK(u.nodes[0].location == db::Point{290, 60});
}
