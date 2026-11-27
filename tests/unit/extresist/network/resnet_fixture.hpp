// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixtures of the extresist-network tests (openspec/changes/m7-extraction
// tasks.md 13.1-13.6): the technology tests/data/extresist/network/resnet.tech
// (metal1 100 and metal2 80 mOhm/sq, ndiff 1000, a metal1-metal2 via of
// 10000 mOhm with CIF squares 10 20 20 at scale factor 10, an nfet), a
// source cell painted from rectangles, and a run of yank_net, build_network
// and reduce on it. Hand-built working networks (detail::Graph) test the
// reductions directly.
//
// Owner: RESNET.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/extresist/network/network.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::test::resnet {

namespace extract = hefesto::extract;
namespace extresist = hefesto::extresist;
namespace tech = hefesto::tech;
namespace db = hefesto::db;
namespace diag = hefesto::diag;

struct Env {
  std::shared_ptr<const tech::Technology> technology;
  extract::ExtStyle style;
  std::optional<mask::OutputStyle> cif;

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }
};

inline const Env& env() {
  static const Env e = [] {
    std::ifstream in(std::string(HEFESTO_TEST_DATA_DIR) + "/resnet.tech");
    REQUIRE(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    auto loaded = tech::load_technology_text(ss.str(), "/t/resnet.tech", fs, tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    Env out;
    out.technology = loaded.value();
    const tech::RawSection* section = nullptr;
    for (const tech::RawSection& raw : out.technology->raw_sections()) {
      if (raw.name == "extract") {
        section = &raw;
      }
    }
    REQUIRE(section != nullptr);
    const extract::StyleTable table = extract::load_styles(*section, *out.technology, sink, compat::CompatSettings{});
    const auto style = table.select(table.names().front());
    REQUIRE(style != nullptr);
    out.cif = mask::read_first_output_style(*out.technology);
    out.style = style->micron_convert(out.cif ? &*out.cif : nullptr);
    return out;
  }();
  return e;
}

struct Box {
  const char* type;
  db::Rect rect;
};

inline std::unique_ptr<db::CellDef> cell(const std::vector<Box>& boxes) {
  const Env& e = env();
  auto c = std::make_unique<db::CellDef>("src", e.technology->plane_count());
  for (const Box& b : boxes) {
    REQUIRE(db::paint(*c, *e.technology, b.rect, e.type(b.type)).ok());
  }
  return c;
}

/// One net: the yank from `start` on `type`, the network, and (optionally)
/// its reduction.
struct Run {
  db::Point start;
  const char* type = "metal1";
  extresist::NetDriver driver;
  std::vector<extresist::NetPort> ports;
  extresist::DeviceAnnotations devices;
  double node_cap = 0.0;
  bool magic_compat = true;
  const extract::ExtStyle* style = nullptr;  // env().style when null
};

inline extresist::Network build(const db::CellDef& src, const Run& run) {
  const Env& e = env();
  extresist::YankRequest req;
  req.area = db::Rect{run.start.x - 2, run.start.y - 2, run.start.x + 2, run.start.y + 2};
  req.start_types = e.technology->connects_to(e.type(run.type));
  req.magic_compat = run.magic_compat;
  auto geometry = extresist::yank_net(src, *e.technology, req);
  REQUIRE(geometry.ok());
  extresist::BuildInput input;
  input.technology = e.technology.get();
  input.cif = e.cif ? &*e.cif : nullptr;
  input.start = run.start;
  input.driver = run.driver;
  input.ports = run.ports;
  input.node_cap = run.node_cap;
  input.magic_compat = run.magic_compat;
  auto net = extresist::build_network(geometry.value(), run.style != nullptr ? *run.style : e.style, run.devices, input);
  REQUIRE(net.ok());
  for (const std::string& m : net.value().messages) {
    UNSCOPED_INFO("message: " << m);
  }
  return net.value();
}

/// The resistor between nodes named or located as given, or nullptr.
inline const extresist::NetResistor* between(const extresist::Network& n, db::Point a, db::Point b) {
  for (const auto& r : n.resistors) {
    const db::Point pa = n.nodes[r.a].location;
    const db::Point pb = n.nodes[r.b].location;
    if ((pa == a && pb == b) || (pa == b && pb == a)) {
      return &r;
    }
  }
  return nullptr;
}

inline const extresist::NetNode* at(const extresist::Network& n, db::Point p) {
  for (const auto& node : n.nodes) {
    if (node.location == p) {
      return &node;
    }
  }
  return nullptr;
}

}  // namespace hefesto::test::resnet
