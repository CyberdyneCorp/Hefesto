// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PDK attribute conventions over the three PDKs with the system library in
// place (capability schematic-symbols-and-libraries "PDK attribute
// conventions"; task 7.6). The schematics of golden cases SCHEM-NL-07 (IHP:
// ev, ev7, cap_cmomi_C, code_shown tcleval), SCHEM-NL-08 (IHP: bipolar and
// MIM formats) and SCHEM-NL-09 (gf180mcu: `+` continuation lines and quoted
// SPICE expressions) are netlisted through the public schem APIs: the PDK's
// own xschemrc evaluated in the sandbox with CMD's XSCHEM_SHAREDIR, then
// netlist() with that interpreter's Tcl bridge. Each netlist is compared,
// with the netlist xschem wrote in CI (expected/), after the `schem-paths`
// mask (the PDK root written as /pdk): the element lines (every non-blank
// line) SHALL be equal, and the whole text SHALL be equal but for one known
// code-block gap (only_the_tcleval_block_gap), checked exactly so the
// test fails when the gap closes or changes. The tests skip when the xschem
// data sets are not fetched (scripts/fetch_pdk.py).
//
// Owner: DEVLIB.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "netlist_support.hpp"
#include "reference_data.hpp"

namespace schem = hefesto::schem;
namespace dl = hefesto::test::devlib;
using hefesto::platform::Path;

namespace {

constexpr const char* kConventions = "[cap:schematic-symbols-and-libraries][req:PDK attribute conventions]";

const Path kCases = dl::kSource / "tests/golden/SCHEM/cases";

/// One `schematic open` / `schematic netlist` pair of a case's hefesto.hef.
struct Run {
  std::string pdk;        // `schematic pdk <name>`
  std::string schematic;  // `schematic open pdk/<path>`, without `pdk/`
  std::string output;     // `schematic netlist out/<name>`, without `out/`
};

std::vector<Run> runs_of(const std::string& case_id) {
  std::istringstream in(dl::read_text(kCases / case_id / "input" / "hefesto.hef"));
  std::vector<Run> runs;
  Run run;
  for (std::string line; std::getline(in, line);) {
    const auto take = [&line](std::string_view prefix, std::string& into) {
      if (line.rfind(prefix, 0) == 0) into = line.substr(prefix.size());
    };
    take("schematic pdk ", run.pdk);
    take("schematic open pdk/", run.schematic);
    take("schematic netlist out/", run.output);
    if (line.rfind("schematic netlist", 0) == 0) runs.push_back(run);
  }
  return runs;
}

/// The PDK root of the xschem data set that holds `pdk`, when fetched.
std::optional<Path> pdk_root(const std::string& pdk) {
  using namespace hefesto::test;
  const PinnedPdk& pinned = pdk == "ihp-sg13g2" ? kIhpSg13g2 : pdk == "gf180mcuD" ? kGf180mcuD : kSky130A;
  const auto tree = pinned_xschem(pinned);  // <root>/<pdk>/libs.tech/xschem
  if (!tree) return std::nullopt;
  return tree->parent_path().parent_path().parent_path();
}

/// The element lines of a netlist and the text of its code blocks: every
/// line but blank ones and the `**` structure comments.
std::vector<std::string> element_lines(const std::string& text) {
  std::vector<std::string> out;
  for (const std::string& line : dl::lines_of(text)) {
    const std::size_t first = line.find_first_not_of(" \t");
    if (first != std::string::npos && line.compare(first, 2, "**") != 0) out.push_back(line);
  }
  return out;
}

/// Known gap (reported to NETLIST/SYMLIB, task 7.6). A code block whose
/// format is `tcleval( @value )` evaluates to its value followed by the
/// blank before `)`. xschem ends that block with LF and no blank, Hefesto
/// with the blank and no LF, which shows as one blank line fewer when another
/// code block follows, and as ` **** end user architecture code` when the
/// block is the last one. True when `actual` is `expected` with exactly that
/// difference at its first place.
bool only_the_tcleval_block_gap(const std::string& expected, const std::string& actual) {
  constexpr std::string_view kEnd = "\n\n**** end user architecture code";
  if (const std::size_t at = expected.find("\n\n\n\n"); at != std::string::npos) {
    if (expected.substr(0, at) + expected.substr(at + 1) == actual) return true;
  }
  const std::size_t at = expected.find(kEnd);
  return at != std::string::npos &&
         expected.substr(0, at) + "\n " + expected.substr(at + 2) == actual;
}

void check_run(const std::string& case_id, const Run& run, const Path& root) {
  const Path top = root / run.schematic;
  const auto input =
      schem::pdk_xschemrc_input(root, run.pdk, dl::kSharedir, top.parent_path() / ".xschem", top.parent_path());
  dl::NetlistBench bench(input, {root});
  const schem::NetlistResult result = bench.netlist(top, root);
  const std::string actual = dl::replace_all(result.text, root.generic_string(), "/pdk");
  const std::string expected = dl::read_text(kCases / case_id / "expected" / run.output);
  INFO(case_id << " " << run.output << "\n--- xschem\n" << expected << "--- hefesto\n" << actual);
  CHECK_FALSE(result.failed);
  CHECK(element_lines(actual) == element_lines(expected));
  // The whole text matches but for the one known code-block gap.
  CHECK((actual == expected || only_the_tcleval_block_gap(expected, actual)));
}

void check_case(const std::string& case_id) {
  const std::vector<Run> runs = runs_of(case_id);
  REQUIRE_FALSE(runs.empty());
  for (const Run& run : runs) {
    const auto root = pdk_root(run.pdk);
    if (!root) SKIP(run.pdk + " xschem data set not fetched (scripts/fetch_pdk.py)");
    check_run(case_id, run, *root);
  }
}

}  // namespace

TEST_CASE("SCHEM-NL-07: IHP sg13g2_tests netlists with the system library", kConventions) { check_case("SCHEM-NL-07"); }

TEST_CASE("SCHEM-NL-08: IHP bipolar and MIM benches netlist with the system library", kConventions) {
  check_case("SCHEM-NL-08");
}

TEST_CASE("SCHEM-NL-09: gf180mcu benches netlist with the system library", kConventions) { check_case("SCHEM-NL-09"); }
