// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Simulator vector names (openspec/changes/m8-schematic task 11.1, design
// D13; capability schematic-simulation-integration "Simulator vector
// names"), including golden case SCHEM-SIM-03: from inside x1 of its
// testbench, the names xschem's `resolved_net` printed (expected/stdout.txt,
// read only), lowercased as the simulator's node names are.
//
// Owner: SIMINT.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

#include "../cmd/cmd_fixture.hpp"
#include "hefesto/schem/sim/deck.hpp"
#include "sim_support.hpp"

namespace schem = hefesto::schem;
namespace tsim = hefesto::test::sim;
using hefesto::test::SchemSession;

#define NAMES "[cap:schematic-simulation-integration][req:Simulator vector names]"

namespace {

constexpr const char* kHeader = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nF {}\nE {}\n";

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string lower(std::string text) {
  for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

// The SCHEM-SIM-03 inputs in the session folder, with tb.sch open and x1
// descended into.
void open_sim03(SchemSession& s) {
  const auto input = tsim::kGolden / "SCHEM-SIM-03/input";
  for (const char* name : {"tb.sch", "stage.sch", "stage.sym"}) s.folder().write(name, read_file(input / name));
  tsim::copy_devices(s.folder().path(), {"ipin.sym", "opin.sym", "res.sym", "lab_pin.sym"});
  REQUIRE(s.schematic({"open", s.folder().str("tb.sch")}).ok());
  const auto descended = s.schematic({"descend", "x1"});
  INFO(SchemSession::message(descended));
  REQUIRE(descended.ok());
}

}  // namespace

TEST_CASE("SCHEM-SIM-03: vector names from inside x1", NAMES) {
  SchemSession s;
  open_sim03(s);
  // `resolved_net MID -> [x1.MID]` and so on, from xschem's capture.
  const std::string stdout_text = read_file(tsim::kGolden / "SCHEM-SIM-03/expected/stdout.txt");
  const std::regex line{R"(resolved_net (\S+) -> \[([^\]]*)\])"};
  std::map<std::string, std::string> expected;
  for (auto it = std::sregex_iterator(stdout_text.begin(), stdout_text.end(), line); it != std::sregex_iterator();
       ++it) {
    expected[(*it)[1]] = lower((*it)[2]);
  }
  REQUIRE(expected.size() == 3);
  const schem::HierPath x1{{"x1"}};
  for (const auto& [net, name] : expected) {
    INFO(net);
    CHECK(schem::vector_name(s.schem(), x1, net) == name);
  }
}

TEST_CASE("Internal node of a subcircuit", NAMES) {
  SchemSession s;
  open_sim03(s);
  CHECK(schem::vector_name(s.schem(), schem::HierPath{{"x1"}}, "net1") == "x1.net1");
}

TEST_CASE("Port net resolved upwards", NAMES) {
  SchemSession s;
  open_sim03(s);
  CHECK(schem::vector_name(s.schem(), schem::HierPath{{"x1"}}, "A") == "in");
  // At the top a net keeps its name, lowercased.
  CHECK(schem::vector_name(s.schem(), schem::HierPath{}, "OUT") == "out");
}

TEST_CASE("global nets and 0 keep their name; other components get the x prefix", NAMES) {
  SchemSession s;
  tsim::copy_devices(s.folder().path(), {"ipin.sym", "res.sym", "lab_pin.sym", "gnd.sym"});
  s.folder().write("cell.sym", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"
                               "K {type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=u1\"}\n"
                               "V {}\nS {}\nF {}\nE {}\nB 5 -42.5 -2.5 -37.5 2.5 {name=P dir=inout}\n");
  s.folder().write("cell.sch", std::string{kHeader} + "C {ipin.sym} 0 0 0 0 {name=p1 lab=P}\n"
                                                      "N 0 0 0 20 {}\nC {res.sym} 0 50 0 0 {name=R1 value=1k}\n"
                                                      "N 0 80 0 100 {}\nC {gnd.sym} 0 100 0 0 {name=l1 lab=GND}\n");
  s.folder().write("top.sch", std::string{kHeader} + "C {cell.sym} 300 0 0 0 {name=u1}\n"
                                                     "N 200 0 260 0 {}\nC {lab_pin.sym} 200 0 0 0 {name=l1 lab=SIG}\n");
  REQUIRE(s.schematic({"open", s.folder().str("top.sch")}).ok());
  REQUIRE(s.schematic({"descend", "u1"}).ok());
  const schem::HierPath u1{{"u1"}};
  CHECK(schem::vector_name(s.schem(), u1, "GND") == "gnd");
  CHECK(schem::vector_name(s.schem(), u1, "0") == "0");
  CHECK(schem::vector_name(s.schem(), u1, "P") == "sig");
  CHECK(schem::vector_name(s.schem(), u1, "Inner") == "xu1.inner");
  // A path the session has not descended along is only prefixed.
  CHECK(schem::vector_name(s.schem(), schem::HierPath{{"x2", "u3"}}, "P") == "x2.xu3.p");
  CHECK(schem::device_vector_name(u1, "R1") == "xu1.r1");
}

TEST_CASE("a net passed as an attribute of the parent instance", NAMES) {
  SchemSession s;
  tsim::copy_devices(s.folder().path(), {"res.sym", "lab_pin.sym"});
  s.folder().write("cell.sym", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"
                               "K {type=subcircuit\nformat=\"@name @symname\"\ntemplate=\"name=x1 supply=VDD\"}\n"
                               "V {}\nS {}\nF {}\nE {}\n");
  s.folder().write("cell.sch", std::string{kHeader} + "C {res.sym} 0 50 0 0 {name=R1 value=1k}\n");
  s.folder().write("top.sch", std::string{kHeader} + "C {cell.sym} 300 0 0 0 {name=x1 supply=VCC_A}\n");
  REQUIRE(s.schematic({"open", s.folder().str("top.sch")}).ok());
  REQUIRE(s.schematic({"descend", "x1"}).ok());
  CHECK(schem::vector_name(s.schem(), schem::HierPath{{"x1"}}, "@supply") == "vcc_a");
}
