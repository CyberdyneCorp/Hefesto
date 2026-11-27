// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-reports: log and console routing, the two-column layout, the
// summary, mismatch, pin and property sections, the list result, the JSON
// file, the classification, the canonical form and object references.
// Every netlist is Hefesto-authored from the spec.
//
// Owner: LVSREP.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "../hier/hier_fixture.hpp"
#include "hefesto/lvs/report/codes.hpp"
#include "hefesto/lvs/report/format.hpp"

namespace {

using lvs_hier_test::circuit;
using lvs_hier_test::contains;
using lvs_hier_test::kInv;
using lvs_hier_test::log_of;
using lvs_hier_test::run;
namespace lvs = hefesto::lvs;
namespace cmd = hefesto::cmd;

std::string padded(std::string text, std::size_t width) {
  text.resize(width, ' ');
  return text;
}

std::string inv_top() {
  return std::string{kInv} + ".subckt top A Y VDD VSS\nXinv1 A n VDD VSS inv\nXinv2 n Y VDD VSS inv\n.ends\n";
}

std::string read_data(const std::string& name) {
  std::ifstream in{std::string{HEFESTO_TEST_DATA_DIR} + "/" + name, std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

/// The key words of one pair of a list result.
std::vector<std::string> keys_of(const cmd::Value& pair) {
  std::vector<std::string> keys;
  const auto& list = std::get<cmd::Value::List>(pair.data());
  for (std::size_t i = 0; i < list.size(); i += 2) {
    keys.push_back(std::get<std::string>(list[i].data()));
  }
  return keys;
}

}  // namespace

// ------------------------------------------------------------ routing

TEST_CASE("Contents block not in log", "[cap:lvs-reports][req:Log and console routing]") {
  const auto result = run(kInv, kInv);
  const auto log = log_of(result);
  CHECK_FALSE(contains(log, "Contents of circuit 1:"));
  CHECK(contains(log, "Subcircuit summary:"));
  const auto console = lvs::console_text(result, true);
  CHECK(contains(console, "Contents of circuit 1:"));
  CHECK_FALSE(contains(console, "Subcircuit summary:"));  // echo off until the final result
  CHECK(contains(console, "Final result: Circuits match uniquely.\n"));
  CHECK(contains(lvs::console_text(result, false), "Subcircuit summary:"));
}

// ------------------------------------------------------------ layout

TEST_CASE("Default widths", "[cap:lvs-reports][req:Two-column layout]") {
  const auto line = lvs::header_row("inv", "inv", {});
  CHECK(line == padded("Circuit 1: inv", 43) + "|" + padded("Circuit 2: inv", 43) + "\n");
  CHECK(line.size() == 88);
  CHECK(lvs::divider({}, false) == std::string(87, '-') + "\n");
  const auto column = lvs::divider({}, true);
  CHECK(column[43] == '|');
  CHECK(lvs::blank_row({}) == std::string(43, ' ') + "|" + std::string(43, ' ') + "\n");
}

TEST_CASE("Column texts are truncated at their own width (Q-LVS-16)", "[cap:lvs-reports][req:Two-column layout]") {
  const std::string long_name(100, 'x');
  const auto row = lvs::column_row(long_name, long_name, {});
  CHECK(row.size() == 88);
  CHECK(row.substr(0, 42) == std::string(42, 'x'));
  CHECK(row[42] == ' ');
  // Unequal widths: the right column is cut at its own width (w2 + 1).
  const lvs::ColumnWidths narrow{10, 5};
  const auto cut = lvs::column_row(long_name, long_name, narrow);
  CHECK(cut.size() == static_cast<std::size_t>(lvs::right_end(narrow)) + 1);
  CHECK(cut.substr(13, 6) == std::string(6, 'x'));
  CHECK(cut[19] == ' ');
  CHECK_FALSE(lvs::validate_widths({0, 41}).ok());
  CHECK(lvs::validate_widths({0, 41}).error().code == lvs::report_codes::kBadWidth);
  CHECK_FALSE(lvs::validate_widths({41, -3}).ok());
  CHECK(lvs::validate_widths({41, 41}).ok());
}

TEST_CASE("A percent sign in a name is written literally (Q-LVS-12)", "[cap:lvs-reports][req:Two-column layout]") {
  const auto row = lvs::header_row("a%s%n%d", "b%%", {});
  CHECK(contains(row, "Circuit 1: a%s%n%d"));
  CHECK(contains(row, "Circuit 2: b%%"));
}

TEST_CASE("Bad widths end the run as an input error", "[cap:lvs-reports][req:Two-column layout]") {
  lvs::RunOptions options;
  options.widths = {41, 0};
  const auto result = run(kInv, kInv, options);
  CHECK(result.input_error);
}

// ------------------------------------------------------------ summary

TEST_CASE("Device count mismatch", "[cap:lvs-reports][req:Subcircuit summary section]") {
  const std::string one = ".subckt top a b c d e f g h i j k l\nM1 a b c c nfet\nM2 d e f f nfet\nM3 g h i i nfet\n"
                          "M4 a d g g pfet\nM5 b e h h pfet\n.ends\n";
  const std::string two = ".subckt top a b c d e f g h i j k l\nM1 a b c c nfet\nM2 d e f f nfet\n"
                          "M4 a d g g pfet\nM5 b e h h pfet\n.ends\n";
  const auto log = log_of(run(one, two));
  CHECK(contains(log, "\nSubcircuit summary:\n" + lvs::header_row("top", "top", {}) + lvs::divider({}, true)));
  CHECK(contains(log, lvs::column_row("nfet (3)", "nfet (2) **Mismatch**", {})));
  CHECK(contains(log, lvs::column_row("pfet (2)", "pfet (2)", {})));
  CHECK(contains(log, lvs::column_row("Number of devices: 5 **Mismatch**", "Number of devices: 4 **Mismatch**", {})));
}

TEST_CASE("Classes of one circuit only", "[cap:lvs-reports][req:Subcircuit summary section]") {
  const auto log = log_of(run(".subckt top a b\nR1 a b 1k\n.ends\n", ".subckt top a b\nC1 a b 1f\n.ends\n"));
  CHECK(contains(log, lvs::column_row("r (1)", "(no matching element)", {})));
  CHECK(contains(log, lvs::column_row("(no matching element)", "c (1)", {})));
}

// ------------------------------------------------------------ mismatch reports

TEST_CASE("Swapped inputs", "[cap:lvs-reports][req:Net mismatch report]") {
  const std::string one = ".subckt nand A B Y VDD VSS\nM1 Y A VDD VDD pfet\nM2 Y B VDD VDD pfet\n"
                          "M3 Y A n VSS nfet\nM4 n B VSS VSS nfet\n.ends\n";
  const std::string two = ".subckt nand A B Y VDD VSS\nM1 Y A VDD VDD pfet\nM2 Y A VDD VDD pfet\n"
                          "M3 Y B n VSS nfet\nM4 n B VSS VSS nfet\n.ends\n";
  const auto result = run(one, two);
  const auto log = log_of(result);
  CHECK(contains(log, "NET mismatches: Class fragments follow (with fanout counts):\n" +
                          lvs::header_row("nand", "nand", {})));
  CHECK(contains(log, lvs::column_row("Net: A", "Net: A", {})));
  CHECK(contains(log, "DEVICE mismatches: Class fragments follow (with node fanout counts):\n"));
  REQUIRE_FALSE(result.reports.empty());
  CHECK(result.reports.back().converged);
  CHECK_FALSE(result.reports.back().bad_nets.empty());
}

TEST_CASE("Permutable pins shown grouped", "[cap:lvs-reports][req:Device mismatch report]") {
  CHECK(lvs::pin_fanout_text({{"drain", "source"}, {1, 3}}) == "  (drain,source) = (3,1)");
  CHECK(lvs::pin_fanout_text({{"gate"}, {2}}) == "  gate = 2");
  lvs::MismatchClass<lvs::InstanceFragment> cls;
  cls.first.push_back({"M1", {{{"gate"}, {2}}}});
  const auto text = lvs::device_report("a", "b", {cls}, {});
  CHECK(contains(text, lvs::column_row("Instance: M1", "(no matching instance)", {})));
  CHECK(contains(text, lvs::column_row("  gate = 2", "", {})));
}

// ------------------------------------------------------------ pins

TEST_CASE("Matching pin table", "[cap:lvs-reports][req:Pin table]") {
  const auto log = log_of(run(kInv, kInv));
  const std::string table = "\nSubcircuit pins:\n" + lvs::header_row("inv", "inv", {}) + lvs::divider({}, true) +
                            lvs::column_row("A", "A", {}) + lvs::column_row("Y", "Y", {}) +
                            lvs::column_row("VDD", "VDD", {}) + lvs::column_row("VSS", "VSS", {}) +
                            lvs::divider({}, false) + "Cell pin lists are equivalent.\n";
  CHECK(contains(log, table));
  CHECK(contains(log, "Device classes inv and inv are equivalent.\n"));
}

TEST_CASE("Rows named (no pins) are dropped", "[cap:lvs-reports][req:Pin table]") {
  const auto text = lvs::pin_table("a", "b", {{"(no pins)", "(no pins)"}, {"x", "x"}}, {});
  CHECK_FALSE(contains(text, "(no pins)"));
  CHECK(contains(text, lvs::column_row("x", "x", {})));
}

// ------------------------------------------------------------ properties

TEST_CASE("Property error block", "[cap:lvs-reports][req:Property error report]") {
  auto one = circuit(".subckt top a b c\nM1 a b c c nfet L=0.15\n.ends\n", "one.spice", 0);
  auto two = circuit(".subckt top a b c\nM5 a b c c nfet L=0.18\n.ends\n", "two.spice", 1);
  const auto result = run(std::move(one), std::move(two));
  const auto log = log_of(result);
  CHECK(contains(log, "Netlists match uniquely with property errors.\n"));
  CHECK(contains(log, " vs. "));
  REQUIRE_FALSE(result.reports.empty());
  REQUIRE_FALSE(result.reports.back().properties.empty());
  const auto& p = result.reports.back().properties.front();
  REQUIRE_FALSE(p.values1.empty());
  CHECK(p.values1.front().key == "L");
}

// ------------------------------------------------------------ list result

TEST_CASE("Keys of a clean pair", "[cap:lvs-reports][req:Structured list result]") {
  const auto list = lvs::to_list(run(kInv, kInv));
  REQUIRE(list.ok());
  const auto& pairs = std::get<cmd::Value::List>(list.value().data());
  REQUIRE(pairs.size() == 1);
  CHECK(keys_of(pairs[0]) == std::vector<std::string>{"name", "devices", "nets", "badnets", "badelements", "pins"});
  const auto& values = std::get<cmd::Value::List>(pairs[0].data());
  CHECK(values[1].to_string() == "inv inv");
  CHECK(values[5].to_string() == "4 4");
  CHECK(values[7].to_string().empty());  // badnets
  CHECK(values[9].to_string().empty());  // badelements
}

TEST_CASE("A black-box pair has no name, devices or nets", "[cap:lvs-reports][req:Structured list result]") {
  const std::string text = ".subckt bb a b\n.ends\n.subckt top a b\nXbb1 a b bb\nR1 a b 1k\n.ends\n";
  const auto result = run(circuit(text, "one.spice", 0, true), circuit(text, "two.spice", 1, true));
  const auto list = lvs::to_list(result);
  const auto& pairs = std::get<cmd::Value::List>(list.value().data());
  REQUIRE(pairs.size() == 2);
  CHECK(keys_of(pairs[0]) == std::vector<std::string>{"pins"});
}

// ------------------------------------------------------------ JSON

TEST_CASE("JSON name", "[cap:lvs-reports][req:JSON file]") {
  CHECK(lvs::json_file_name("comp.out") == "comp.json");
  CHECK(lvs::json_file_name("run/comp.out") == "run/comp.json");
  CHECK(lvs::json_file_name("a.b.c") == "a.b.json");
  // Q-LVS-13: a name without an extension gets .json appended.
  CHECK(lvs::json_file_name("comp") == "comp.json");
  CHECK(lvs::json_file_name("out.d/comp") == "out.d/comp.json");
}

TEST_CASE("Nets block", "[cap:lvs-reports][req:JSON file]") {
  const auto json = lvs::write_json(run(kInv, kInv)).value();
  CHECK(contains(json, "   \"nets\": [\n    4,\n    4\n   ],\n"));
  CHECK(json.starts_with("[\n  {\n   \"name\": [\n      \"inv\",\n      \"inv\"\n   ],\n"));
  CHECK(json.ends_with("  }\n]\n"));
  // An empty badnets list is its opening line and its closing line.
  CHECK(contains(json, "   \"badnets\": [\n   ],\n"));
}

TEST_CASE("JSON devices close with and without a space", "[cap:lvs-reports][req:JSON file]") {
  const auto json = lvs::write_json(run(kInv, kInv)).value();
  CHECK(contains(json, "   \"devices\": [\n       [\n         [\"pfet\", 1],\n         [\"nfet\", 1 ]\n"
                       "       ], [\n         [\"pfet\", 1 ],\n         [\"nfet\", 1 ]\n       ]\n   ],\n"));
}

TEST_CASE("JSON strings are escaped (Q-LVS-13)", "[cap:lvs-reports][req:JSON file]") {
  lvs::LvsResult result;
  lvs::PairReport report;
  report.has_devices = true;
  report.cell1 = "a\"b\\c";
  report.cell2 = std::string{"d\x01"};
  result.reports.push_back(report);
  const auto json = lvs::write_json(result).value();
  CHECK(contains(json, "\"a\\\"b\\\\c\""));
  CHECK(contains(json, "\"d\\u0001\""));
}

// ------------------------------------------------------------ classification

TEST_CASE("Clean status", "[cap:lvs-reports][req:Result classification]") {
  const auto result = run(kInv, kInv);
  CHECK(lvs::overall_status(result) == lvs::OverallStatus::clean);
  CHECK(lvs::overall_status_name(lvs::overall_status(result)) == "clean");
  REQUIRE(result.pairs.size() == 1);
  CHECK(lvs::pair_status_name(result.pairs[0].status) == "match");
}

TEST_CASE("Statuses of the classification", "[cap:lvs-reports][req:Result classification]") {
  CHECK(lvs::pair_status_name(lvs::status_of_unique(-3)) == "match-property-errors");
  CHECK(lvs::pair_status_name(lvs::status_of_unique(-2)) == "match-port-errors");
  CHECK(lvs::pair_status_name(lvs::status_of_unique(-4)) == "match-port-property-errors");
  CHECK(lvs::pair_status_name(lvs::status_of_unique(0)) == "symmetric");
  CHECK(lvs::pair_status_name(lvs::status_of_unique(-1)) == "black-box");
  CHECK(lvs::pair_status_name(lvs::PairStatus::mismatch) == "mismatch");
  lvs::LvsResult result;
  result.input_error = true;
  CHECK(lvs::overall_status(result) == lvs::OverallStatus::error);
  result.input_error = false;
  result.cancelled = true;
  CHECK(lvs::overall_status(result) == lvs::OverallStatus::cancelled);
  CHECK(lvs::overall_status(run(kInv, ".subckt inv A Y VDD VSS\nM1 Y A VDD VDD pfet\n.ends\n")) ==
        lvs::OverallStatus::mismatch);
}

// ------------------------------------------------------------ canonical form

TEST_CASE("Hefesto's comp.out and comp.json for the canonical check are current",
          "[cap:lvs-reports][req:Canonical golden comparison]") {
  // tests/data/lvs/report/top.{out,json} are this run's outputs; the
  // lvs_canonical test (canonical_test.py) canonicalizes them and a
  // reordered copy. Regenerate them when the report text changes.
  const auto result = run(inv_top(), inv_top());
  CHECK(log_of(result) == read_data("top.out"));
  CHECK(lvs::write_json(result).value() == read_data("top.json"));
}

TEST_CASE("Sections are written in one deterministic order", "[cap:lvs-reports][req:Canonical golden comparison]") {
  const auto first = log_of(run(inv_top(), inv_top()));
  for (int i = 0; i < 20; ++i) {
    CHECK(log_of(run(inv_top(), inv_top())) == first);
  }
}

// ------------------------------------------------------------ references

TEST_CASE("Unmatched net points into the layout",
          "[cap:lvs-reports][req:Layout object references in structured results]") {
  const std::string one = ".subckt top a b c\nM1 a b c c nfet\nM2 a c b b pfet\n.ends\n";
  const std::string two = ".subckt top a b c\nM1 a b c c nfet\nM2 c a b b pfet\n.ends\n";
  lvs::RunOptions with;
  with.references = [](int circuit, lvs::ObjectReference::Kind kind, std::string_view name) {
    lvs::ObjectReference ref;
    ref.circuit = circuit;
    ref.kind = kind;
    ref.name = std::string{name};
    ref.use_path = {"x1"};
    ref.point = {3, 4};
    return std::optional<lvs::ObjectReference>{ref};
  };
  const auto plain = run(one, two);
  const auto referenced = run(one, two, with);
  CHECK(log_of(plain) == log_of(referenced));
  CHECK(lvs::write_json(plain).value() == lvs::write_json(referenced).value());
  CHECK(lvs::to_list(plain).value() == lvs::to_list(referenced).value());
  REQUIRE_FALSE(referenced.reports.empty());
  const auto& refs = referenced.reports.back().references;
  REQUIRE_FALSE(refs.empty());
  const bool net = std::any_of(refs.begin(), refs.end(), [](const lvs::ObjectReference& r) {
    return r.kind == lvs::ObjectReference::Kind::net && r.circuit == 1 && r.use_path == std::vector<std::string>{"x1"};
  });
  CHECK(net);
  CHECK(plain.reports.back().references.empty());
}
