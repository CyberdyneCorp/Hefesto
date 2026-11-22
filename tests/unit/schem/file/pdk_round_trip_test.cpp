// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SCHEM-RT-02: every `.sch` and `.sym` of the three PDK xschem trees, read
// and written, matches xschem 3.4.8RC's first save byte for byte (the
// SHA-256 digests in tests/golden/SCHEM/cases/SCHEM-RT-02/expected/
// first.digest), and the saved file is a fixpoint (capability
// schematic-file-format "Round-trip guarantee"; openspec/changes/
// m8-schematic tasks.md 6.4). Trees that were not fetched are skipped
// (scripts/fetch_pdk.py sky130-xschem gf180mcu-xschem ihp-xschem).
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "file_test_util.hpp"
#include "hefesto/pdk/sha256.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/file/save.hpp"
#include "reference_data.hpp"

namespace schem = hefesto::schem;
namespace fs = std::filesystem;

namespace {

struct Root {
  std::string tag;  // the capture's directory name under out/first/
  std::optional<fs::path> tree;
};

std::vector<Root> roots() {
  using hefesto::test::kGf180mcuD;
  using hefesto::test::kIhpSg13g2;
  using hefesto::test::kIhpStdcellXschemSymbols;
  using hefesto::test::kSky130A;
  using hefesto::test::pinned_xschem;
  return {{"sky130A_libs.tech_xschem", pinned_xschem(kSky130A)},
          {"gf180mcuD_libs.tech_xschem", pinned_xschem(kGf180mcuD)},
          {"ihp-sg13g2_libs.tech_xschem", pinned_xschem(kIhpSg13g2)},
          {"ihp-sg13g2_libs.ref_sg13g2_stdcell_sym_xschem", pinned_xschem(kIhpSg13g2, kIhpStdcellXschemSymbols)}};
}

// first.digest: "<sha256> <size> out/first/<tag>/<relative path>" per line.
std::map<std::string, std::string> expected_digests() {
  std::map<std::string, std::string> out;
  std::istringstream lines{schem::test::slurp(schem::test::golden("SCHEM-RT-02", "expected/first.digest"))};
  std::string digest;
  std::string size;
  std::string path;
  while (lines >> digest >> size >> path) {
    out[path.substr(std::string_view{"out/first/"}.size())] = digest;
  }
  return out;
}

std::string sha256(std::string_view bytes) {
  hefesto::pdk::Sha256 sha;
  sha.update(bytes);
  return sha.finish_hex();
}

}  // namespace

TEST_CASE("SCHEM-RT-02 every PDK file round-trips as xschem saves it",
          "[cap:schematic-file-format][req:Round-trip guarantee][req:Brace strings][req:Numeric fields]") {
  const auto digests = expected_digests();
  REQUIRE(digests.size() == 1375);
  std::size_t checked = 0;
  std::vector<std::string> mismatches;
  std::vector<std::string> not_fixpoints;
  for (const auto& root : roots()) {
    if (!root.tree) {
      continue;
    }
    for (const auto& [key, digest] : digests) {
      if (key.rfind(root.tag + "/", 0) != 0) {
        continue;
      }
      const fs::path file = *root.tree / key.substr(root.tag.size() + 1);
      if (!fs::is_regular_file(file)) {
        mismatches.push_back("missing " + key);
        continue;
      }
      const schem::FileKind kind = schem::file_kind_of(file);
      const std::string first = schem::test::round_trip(schem::test::slurp(file), kind);
      ++checked;
      if (sha256(first) != digest) {
        mismatches.push_back(key);
      }
      if (schem::test::round_trip(first, kind) != first) {
        not_fixpoints.push_back(key);
      }
    }
  }
  if (checked == 0) {
    SKIP("no xschem PDK tree in .cache/pdks/xschem");
  }
  INFO(checked << " files checked");
  for (const auto& m : mismatches) {
    UNSCOPED_INFO("differs from xschem's first save: " << m);
  }
  for (const auto& m : not_fixpoints) {
    UNSCOPED_INFO("second save differs: " << m);
  }
  CHECK(mismatches.empty());
  CHECK(not_fixpoints.empty());
}
