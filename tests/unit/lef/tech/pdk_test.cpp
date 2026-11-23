// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-tech-section on the pinned PDK technologies (tasks.md 3.4): the
// `lef` sections of sky130A, gf180mcuD and ihp-sg13g2 load with no
// diagnostic, and the names and canonical names match the tech files. Each
// test skips when its data set is not in .cache/pdks (scripts/fetch_pdk.py).
//
// Owner: LEFTECH (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/lef/tech/lifecycle.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace lef = hefesto::lef;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;

#define PDK_TAGS "[cap:lef-tech-section][req:Lef section line grammar][pdk]"

namespace {

struct Expected {
  std::size_t names = 0;                      // LEF names, aliases included
  std::vector<std::string> canonical_names;  // one per record, sorted
};

struct Loaded {
  lef::LayerTable table;
  std::vector<diag::Diagnostic> diagnostics;  // of the lef section only
};

Loaded load(const std::filesystem::path& file) {
  hefesto::platform::LocalFileSystem fs;
  diag::NullSink load_sink;
  auto loaded = tech::load_technology(fs, file.string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const hefesto::compat::CompatSettings compat{};
  diag::NullSink quiet;
  const auto rules = hefesto::drc::read_rules(*loaded.value(), compat, quiet).rules;
  diag::CollectingSink sink;
  Loaded out;
  out.table = lef::read_layer_section(*loaded.value(), rules.get(), compat, sink);
  out.diagnostics = sink.take();
  return out;
}

void check(const Loaded& loaded, const Expected& expected) {
  for (const auto& d : loaded.diagnostics) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  CHECK(loaded.diagnostics.empty());
  CHECK(loaded.table.size() == expected.names);
  std::set<std::string> canonical;
  std::size_t visited = 0;
  loaded.table.enumerate([&](std::string_view name, const lef::LayerRecord& r) {
    ++visited;
    canonical.insert(r.canonical_name);
    CHECK(loaded.table.find(r.canonical_name) == &r);  // the canonical name is one of the aliases
    CHECK(loaded.table.find(name) == &r);
  });
  CHECK(visited == expected.names);
  CHECK(std::vector<std::string>(canonical.begin(), canonical.end()) == expected.canonical_names);
}

std::vector<std::string> sorted(std::vector<std::string> v) {
  std::sort(v.begin(), v.end());
  return v;
}

}  // namespace

TEST_CASE("the sky130A lef section loads at the pin", PDK_TAGS) {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("sky130A.tech not in .cache/pdks");
  }
  const Loaded loaded = load(*file);
  // 2 masterslice (5 names), li (4), met1-met5 (3 each), met6 (5), mcon (3),
  // via (6), via2-via4 (4 each); the obs lines only add obstruction types.
  check(loaded, Expected{50, sorted({"pwell", "nwell", "li1", "met1", "met2", "met3", "met4", "met5", "met6", "mcon",
                                     "via", "via2", "via3", "via4"})});
  const lef::LayerRecord* via = loaded.table.find("VIA1");
  REQUIRE(via != nullptr);
  CHECK(via->canonical_name == "via");
  CHECK(via->layer_class == lef::LayerClass::cut);
  CHECK(via->obstruction_type != tech::kSpace);  // obs obsm1 via
  CHECK(loaded.table.find("substrate") == loaded.table.find("pwell"));
}

TEST_CASE("the gf180mcuD lef section loads at the pin", PDK_TAGS) {
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("gf180mcuD.tech not in .cache/pdks");
  }
  const Loaded loaded = load(*file);
  // 2 masterslice (2 names each), Metal1-5 (6 each), Via1-4 (6 each).
  check(loaded, Expected{58, sorted({"Pwell", "Nwell", "Metal1", "Metal2", "Metal3", "Metal4", "Metal5", "Via1", "Via2",
                                     "Via3", "Via4"})});
  CHECK(loaded.table.find("Via1")->obstruction_types.count() == 1);
}

TEST_CASE("the ihp-sg13g2 lef section loads at the pin", PDK_TAGS) {
  const auto file = hefesto::test::pinned_ihp_sg13g2_tech();
  if (!file) {
    SKIP("ihp-sg13g2.tech not in .cache/pdks");
  }
  const Loaded loaded = load(*file);
  check(loaded, Expected{14, sorted({"GatPoly", "Metal1", "Metal2", "Metal3", "Metal4", "Metal5", "TopMetal1",
                                     "TopMetal2", "Via1", "Via2", "Via3", "Via4", "TopVia1", "TopVia2"})});
  // obs obsm1,obsm2 Via1: the cut keeps both obstruction types.
  CHECK(loaded.table.find("Via1")->obstruction_types.count() == 2);
}
