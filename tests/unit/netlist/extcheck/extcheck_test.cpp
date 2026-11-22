// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tests of extcheck (netlist/extcheck/extcheck.hpp, counts.hpp; capability
// netlist-extcheck; openspec/changes/m7-extraction/design.md D15). The
// end-to-end runs read NL-CHK-01's inputs (tests/golden/NETLIST/cases/
// NL-CHK-01/input) and need EXTFLAT's read and flattening (tasks.md 10.x);
// they are skipped while those are scaffold stubs.

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <vector>

#include "hefesto/extflat/args.hpp"
#include "hefesto/netlist/extcheck/counts.hpp"
#include "hefesto/netlist/extcheck/extcheck.hpp"

namespace extcheck = hefesto::netlist::extcheck;

namespace {

constexpr const char* kUsage =
    "Standard arguments: [-R] [-C] [-r rthresh] [-c cthresh] [-v]\n"
    "[-p searchpath] [-s sym=value] [-S symfile] [-t trimchars]\n"
    "[-T techname] rootfile\n";

struct Result {
  int exit = 0;
  std::string out;
  std::string err;
};

Result run(const std::vector<std::string>& args) {
  std::ostringstream out;
  std::ostringstream err;
  Result r;
  r.exit = extcheck::run(args, out, err, "hefesto-extcheck");
  r.out = out.str();
  r.err = err.str();
  return r;
}

std::string case_input() {
  return std::string{HEFESTO_SOURCE_DIR} + "/tests/golden/NETLIST/cases/NL-CHK-01/input";
}

// The report without the informative memory line.
std::string counts_of(const std::string& out) {
  const std::size_t memory = out.find("Memory used: ");
  REQUIRE(memory != std::string::npos);
  return out.substr(out.find('\n', memory) + 1);
}

bool common_args_available() {
  const std::vector<std::string> args{"inv"};
  return hefesto::extflat::parse_common_args(args, {}).ok();
}

// EXTFLAT's read of the case: true once the inputs read and flatten.
bool reader_available() {
  if (!common_args_available()) {
    return false;
  }
  return run({"-p", case_input(), "top"}).exit == 0;
}

}  // namespace

TEST_CASE("extcheck counts with the default thresholds", "[cap:netlist-extcheck][req:Counts and report format]") {
  extcheck::CheckInput input;
  input.devices = 2;
  input.nodes = {{400, 7500, false}, {5000, 1850, false}, {1200, 0, true}, {2400, 0, true}};
  const extcheck::Counts c = extcheck::count(input, {});
  CHECK(c.devices == 2);
  CHECK(c.nodes == 4);
  CHECK(c.global_nodes == 2);
  CHECK(c.node_caps == 2);  // (2400 + 500) / 1000 = 2.9 > 2 and 5.5; 1.7 and 0.9 are not
  CHECK(c.node_resists == 0);
  CHECK(extcheck::report(c, "1k") ==
        "Memory used: 1k\n2 devices\n4 nodes (2 global, 2 local)\n2 nodes above capacitance threshold\n"
        "0 nodes above resistance threshold\n0 internodal capacitors (0 above threshold)\n"
        "0 explicit resistors (0 above threshold)\n");
}

TEST_CASE("extcheck rounding at the thresholds", "[cap:netlist-extcheck][req:Counts and report format]") {
  extcheck::CheckInput input;
  input.nodes = {{2400, 10499, false}, {1499, 10500, false}};
  input.caps = {2000, 2001};
  input.resists = {10000, 10001};
  const extcheck::Counts c = extcheck::count(input, {});
  CHECK(c.node_caps == 1);     // 2.9 > 2; 1.999 is not
  CHECK(c.node_resists == 1);  // (10500 + 500) / 1000 = 11 > 10; (10499 + 500) / 1000 = 10 is not
  CHECK(c.caps == 2);
  CHECK(c.thresh_caps == 1);   // 2.001 > 2; 2.0 is not
  CHECK(c.resists == 2);
  CHECK(c.thresh_resists == 1);
}

TEST_CASE("extcheck skips couplings and resistors at an infinite threshold",
          "[cap:netlist-extcheck][req:Counts and report format]") {
  extcheck::CheckInput input;
  input.caps = {5000};
  input.resists = {20000};
  input.nodes = {{1.0E40, 0, false}};
  const extcheck::Counts c = extcheck::count(input, {extcheck::kInfiniteCap, extcheck::kInfiniteResist});
  CHECK(c.caps == 0);
  CHECK(c.resists == 0);
  CHECK(c.node_caps == 0);
}

TEST_CASE("extcheck with no arguments prints the usage text", "[cap:netlist-extcheck][req:Invocation]") {
  if (!common_args_available()) {
    SKIP("extflat::parse_common_args is a scaffold stub until EXTFLAT lands it");
  }
  const Result r = run({});
  CHECK(r.exit == 1);
  CHECK(r.out.empty());
  CHECK(r.err == kUsage);
  const Result bad = run({"-x", "top"});
  CHECK(bad.exit == 1);
  CHECK(bad.err == kUsage);
  const Result help = run({"-h"});
  CHECK(help.exit == 1);
  CHECK(help.out == kUsage);
}

TEST_CASE("extcheck reports a read failure with its program name", "[cap:netlist-extcheck][req:Invocation]") {
  if (!common_args_available()) {
    SKIP("extflat::parse_common_args is a scaffold stub until EXTFLAT lands it");
  }
  const Result r = run({"-p", case_input(), "missing.ext"});
  CHECK(r.exit == 1);
  CHECK(r.err.starts_with("hefesto-extcheck: "));
}

// Q-NETLIST-50 (fix, both modes): the inputs start with `scale 1000 1000 5`
// and no technology is loaded; Magic's standalone program crashes there.
TEST_CASE("extcheck reads a scale record without a technology", "[cap:netlist-extcheck][req:Invocation]") {
  if (!reader_available()) {
    SKIP("extflat's read and flattening are scaffold stubs until EXTFLAT lands them");
  }
  const Result r = run({"-p", case_input(), "top.ext"});
  CHECK(r.exit == 0);
  CHECK(r.err.empty());
  CHECK(counts_of(r.out) ==
        "4 devices\n6 nodes (2 global, 4 local)\n4 nodes above capacitance threshold\n"
        "0 nodes above resistance threshold\n2 internodal capacitors (2 above threshold)\n"
        "2 explicit resistors (2 above threshold)\n");
  CHECK(r.out.starts_with("*** Global name vdd! not fully connected:\n"));
}

TEST_CASE("extcheck thresholds from the command line", "[cap:netlist-extcheck][req:Counts and report format]") {
  if (!reader_available()) {
    SKIP("extflat's read and flattening are scaffold stubs until EXTFLAT lands them");
  }
  CHECK(counts_of(run({"-p", case_input(), "-c", "0", "-r", "0", "top"}).out) ==
        "4 devices\n6 nodes (2 global, 4 local)\n6 nodes above capacitance threshold\n"
        "4 nodes above resistance threshold\n2 internodal capacitors (2 above threshold)\n"
        "2 explicit resistors (2 above threshold)\n");
  CHECK(counts_of(run({"-p", case_input(), "-C", "-r", "0", "top"}).out) ==
        "4 devices\n6 nodes (2 global, 4 local)\n0 nodes above capacitance threshold\n"
        "4 nodes above resistance threshold\n0 internodal capacitors (0 above threshold)\n"
        "2 explicit resistors (2 above threshold)\n");
}
