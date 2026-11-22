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

#include <cstddef>
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

// The standard-cell inputs of the M7 PDK golden cases (openspec/changes/m7-extraction design D21, Open
// Question 4 (a), task 2.1): sky130_fd_sc_hd and gf180mcu_fd_sc_mcu7t5v0 from the .mag sets, sg13g2_stdcell
// from the ihp-gds-sc set. The path checks run only on a leg that has the set; the counts are the pins'.
namespace {

std::size_t count_mag_files(const fs::path& dir) {
  std::size_t n = 0;
  for (const auto& entry : fs::directory_iterator(dir)) {
    n += entry.is_regular_file() && entry.path().extension() == ".mag" ? 1U : 0U;
  }
  return n;
}

}  // namespace

TEST_CASE("standard-cell helpers name the M7 PDK libraries and their extract defaults",
          "[cap:hefesto-testing-strategy][req:Golden compatibility suite]") {
  CHECK(tst::pinned_stdcell("sky130_fd_sc_hd") == &tst::kSky130FdScHd);
  CHECK(tst::pinned_stdcell("gf180mcu_fd_sc_mcu7t5v0") == &tst::kGf180mcu7t5v0);
  CHECK(tst::pinned_stdcell("sg13g2_stdcell") == &tst::kSg13g2Stdcell);
  CHECK(tst::pinned_stdcell("sky130_fd_sc_hs") == nullptr);
  for (const auto* cell : {&tst::kSky130FdScHd, &tst::kGf180mcu7t5v0, &tst::kSg13g2Stdcell}) {
    CHECK(cell->extract_style == "ngspice()");
    CHECK(cell->tech_format == 35);
  }
  CHECK(tst::kSky130FdScHd.cells == 446);
  CHECK(tst::kGf180mcu7t5v0.cells == 229);
  CHECK(tst::kSg13g2Stdcell.cells == 84);
  // IHP ships no .mag standard cells, and an unknown library has no path.
  CHECK_FALSE(tst::pinned_stdcell_mag("sg13g2_stdcell"));
  CHECK_FALSE(tst::pinned_stdcell_mag("sky130_fd_sc_hs"));
  CHECK_FALSE(tst::pinned_stdcell_gds("sky130_fd_sc_hs"));
}

TEST_CASE("standard-cell .mag helpers return the pinned libraries with the confirmed cell counts",
          "[cap:hefesto-testing-strategy][req:Golden compatibility suite]") {
  int found = 0;
  for (const auto* cell : {&tst::kSky130FdScHd, &tst::kGf180mcu7t5v0}) {
    const auto dir = tst::pinned_stdcell_mag(cell->gds->library);
    if (!dir) {
      continue;
    }
    ++found;
    CHECK(*dir == *tst::pinned_library(*cell->pdk));
    CHECK(dir->filename() == "mag");
    CHECK(count_mag_files(*dir) == cell->cells);
  }
  if (found == 0) {
    SKIP("neither .mag set in .cache/pdks (scripts/fetch_pdk.py sky130 gf180mcu)");
  }
}

TEST_CASE("standard-cell GDS helpers return the pinned *-gds-sc files",
          "[cap:hefesto-testing-strategy][req:Golden compatibility suite]") {
  int found = 0;
  for (const auto* cell : {&tst::kSky130FdScHd, &tst::kGf180mcu7t5v0, &tst::kSg13g2Stdcell}) {
    const std::string lib{cell->gds->library};
    const auto file = tst::pinned_stdcell_gds(lib);
    if (!file) {
      continue;
    }
    ++found;
    CHECK(*file == *tst::pinned_gds(*cell->gds));
    CHECK(file->filename() == lib + ".gds");
  }
  if (found == 0) {
    SKIP("no *-gds-sc set in .cache/pdks/gds (scripts/fetch_pdk.py sky130-gds-sc gf180mcu-gds-sc ihp-gds-sc)");
  }
}
