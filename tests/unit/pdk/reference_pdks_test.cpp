// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M3 reference PDK data sets (openspec/changes/m3-drc design D15, task
// 3.4): the pinned gf180mcuD and ihp-sg13g2 tech files load, a
// gf180mcu_fd_sc_mcu7t5v0 cell reads, and the lookups in reference_data.hpp
// find only the exact pinned install, so a missing set skips. Data comes from
// scripts/fetch_pdk.py (docs/building.md, "Reference data for tests").
// Owner: PDK.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../cell/cell_fixture.hpp"
#include "reference_data.hpp"

namespace fs = std::filesystem;
namespace tst = hefesto::test;

namespace {

std::string slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

/// Loads `file` with a fresh sink; reports every error diagnostic on failure.
hefesto::diag::Outcome<std::shared_ptr<const hefesto::tech::Technology>> load_tech(const fs::path& file) {
  hefesto::platform::LocalFileSystem local;
  hefesto::diag::CollectingSink sink;
  auto loaded = hefesto::tech::load_technology(local, file.string(), hefesto::tech::LoadOptions{}, sink);
  for (const auto& d : sink.snapshot()) {
    if (d.severity == hefesto::diag::Severity::error) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
  }
  return loaded;
}

}  // namespace

TEST_CASE("the pinned gf180mcuD technology loads", "[cap:tech-loading-and-scaling][req:Full technology load]") {
  const auto file = tst::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("gf180mcuD not in .cache/pdks (scripts/fetch_pdk.py gf180mcu)");
  }
  auto loaded = load_tech(*file);
  REQUIRE(loaded.ok());
  CHECK(loaded.value()->name() == "gf180mcuD");
}

TEST_CASE("the pinned ihp-sg13g2 technology loads", "[cap:tech-loading-and-scaling][req:Full technology load]") {
  const auto file = tst::pinned_ihp_sg13g2_tech();
  if (!file) {
    SKIP("ihp-sg13g2 not in .cache/pdks (scripts/fetch_pdk.py ihp)");
  }
  auto loaded = load_tech(*file);
  REQUIRE(loaded.ok());
  CHECK(loaded.value()->name() == "ihp-sg13g2");
}

TEST_CASE("a pinned gf180mcu_fd_sc_mcu7t5v0 cell reads",
          "[cap:cell-file-format][req:Overall file grammar][req:Coordinate scaling on read]") {
  const auto tech_file = tst::pinned_gf180mcud_tech();
  const auto library = tst::pinned_gf180mcu_7t5v0_mag();
  if (!tech_file || !library) {
    SKIP("gf180mcuD with gf180mcu_fd_sc_mcu7t5v0 not in .cache/pdks (scripts/fetch_pdk.py gf180mcu)");
  }
  if (!hefesto::cell::test::magic_font_dir()) {
    SKIP("Magic's default fonts (build/ref/magic/graphics) not present; the cells use flabels");
  }
  auto tech = load_tech(*tech_file);
  REQUIRE(tech.ok());
  hefesto::platform::LocalFileSystem local;
  hefesto::cell::test::CellIo io{tech.value(), &local};
  REQUIRE(io.load_fonts());
  const fs::path file = *library / "gf180mcu_fd_sc_mcu7t5v0__inv_1.mag";
  const std::string text = slurp(file);
  REQUIRE_FALSE(text.empty());
  hefesto::diag::Status status = hefesto::diag::Done{};
  const auto& def = io.read(file.stem().string(), text, &status);
  for (const auto& d : io.sink.snapshot()) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  REQUIRE(status.ok());
  CHECK_FALSE(def.bbox().is_null());
  CHECK_FALSE(def.labels().empty());
}

TEST_CASE("reference data lookups use only the exact pinned install",
          "[cap:hefesto-pdk-manager][req:Install layout compatible with ciel]") {
  // Ciel's layout: <root>/ciel/<family>/versions/<pin>/<variant>.
  CHECK(tst::kGf180mcuD.version == tst::kSky130A.version);  // one open_pdks snapshot (design D15)
  CHECK(tst::kIhpSg13g2.family == "ihp-sg13");
  CHECK(tst::kIhpSg13g2.library.empty());
  CHECK_FALSE(tst::pinned_library(tst::kIhpSg13g2));

  tst::PinnedPdk other_pin = tst::kGf180mcuD;
  other_pin.version = "0000000000000000000000000000000000000000";
  CHECK_FALSE(tst::pinned_variant(other_pin));
  CHECK_FALSE(tst::pinned_tech(other_pin));

  if (const auto root = tst::pinned_gf180mcud()) {
    const fs::path expected = fs::path{HEFESTO_SOURCE_DIR} / ".cache/pdks/ciel/gf180mcu/versions" /
                              std::string{tst::kPdkVersion} / "gf180mcuD";
    CHECK(*root == expected);
    tst::PinnedPdk missing_library = tst::kGf180mcuD;
    missing_library.library = "gf180mcu_fd_sc_not_fetched";
    CHECK_FALSE(tst::pinned_variant(missing_library));
  }
}
