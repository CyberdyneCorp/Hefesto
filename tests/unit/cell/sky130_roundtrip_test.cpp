// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M1 exit criterion (CELL-WR-10): every .mag of the cached
// sky130_fd_sc_hd library reads and writes back byte-identically. Skips
// when .cache/pdks or Magic's fonts (build/ref/magic) are absent.
//
// Two library files are not in the form Magic fc5daa9e writes, and Magic
// itself does not write them back unchanged: the golden case CELL-WR-10
// captured Magic's own rewrite of both (tests/golden/CELL/cases/CELL-WR-10/
// expected/), and Hefesto's output must equal those bytes:
//  - tapvpwrvgnd_1: the file lists MASKHINTS_PSDM before MASKHINTS_NSDM;
//    Magic (and Hefesto, "Writer properties") writes the keys in natural order.
//  - a21bo_1: the file's pwell is not the maximal-horizontal-strip
//    decomposition; Magic (and Hefesto, layout-geometry-model) re-splits it.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

#include "cell_fixture.hpp"
#include "reference_data.hpp"

namespace cell = hefesto::cell;
namespace fs = std::filesystem;

namespace {

/// The sky130A install of the pinned PDK cache (the exact pinned version), if present.
std::optional<fs::path> sky130a_root() {
  return hefesto::test::pinned_sky130a();
}

/// Library files whose expected output is Magic's own rewrite (CELL-WR-10).
const std::set<std::string> kMagicRewrites = {
    "sky130_fd_sc_hd__tapvpwrvgnd_1.mag",
    "sky130_fd_sc_hd__a21bo_1.mag",
};

fs::path magic_rewrite(const std::string& file) {
  return fs::path{HEFESTO_SOURCE_DIR} / "tests/golden/CELL/cases/CELL-WR-10/expected" / file;
}

std::string slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

/// The first differing line, for the failure message.
std::string first_difference(const std::string& a, const std::string& b) {
  std::istringstream sa(a);
  std::istringstream sb(b);
  std::string la;
  std::string lb;
  for (int line = 1;; ++line) {
    const bool ga = static_cast<bool>(std::getline(sa, la));
    const bool gb = static_cast<bool>(std::getline(sb, lb));
    if (!ga && !gb) {
      return "(no line difference)";
    }
    if (la != lb || ga != gb) {
      return "line " + std::to_string(line) + ": expected \"" + la + "\" got \"" + lb + "\"";
    }
  }
}

}  // namespace

TEST_CASE("every sky130_fd_sc_hd cell round-trips byte-identically",
          "[cap:cell-file-format][req:Overall file grammar][req:Writer paint sections][golden:CELL-WR-10]") {
  const auto root = sky130a_root();
  if (!root) {
    SKIP("sky130A with sky130_fd_sc_hd not in .cache/pdks");
  }
  if (!cell::test::magic_font_dir()) {
    SKIP("Magic's default fonts (build/ref/magic/graphics) not present");
  }
  hefesto::platform::LocalFileSystem local;
  hefesto::diag::CollectingSink tech_sink;
  auto tech = hefesto::tech::load_technology(local, (*root / "libs.tech/magic/sky130A.tech").string(),
                                             hefesto::tech::LoadOptions{}, tech_sink);
  REQUIRE(tech.ok());

  std::vector<fs::path> files;
  for (const auto& entry : fs::directory_iterator(*root / "libs.ref/sky130_fd_sc_hd/mag")) {
    if (entry.path().extension() == ".mag") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  REQUIRE(files.size() >= 400);

  std::size_t identical = 0;
  for (const fs::path& file : files) {
    cell::test::CellIo io{tech.value(), &local};
    REQUIRE(io.load_fonts());
    const std::string original = slurp(file);
    hefesto::diag::Status status = hefesto::diag::Done{};
    const auto& def = io.read(file.stem().string(), original, &status);
    INFO(file.filename().string());
    REQUIRE(status.ok());
    const std::string written = io.write(def);
    const std::string expected =
        kMagicRewrites.contains(file.filename().string()) ? slurp(magic_rewrite(file.filename().string())) : original;
    REQUIRE_FALSE(expected.empty());
    if (written == expected) {
      ++identical;
    } else {
      FAIL_CHECK(first_difference(expected, written));
    }
  }
  CHECK(identical == files.size());
}
