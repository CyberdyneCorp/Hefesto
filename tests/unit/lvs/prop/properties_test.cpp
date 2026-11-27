// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-property-matching: properties of interest, promotion, tolerance
// (Q-LVS-7), missing properties, timing, pin-associated properties,
// series/parallel reconciliation (Q-LVS-8, Q-LVS-18), property errors
// (Q-LVS-10) and topology. Values are Hefesto-authored.
//
// Owner: LVSPROP.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/prop/properties.hpp"
#include "hefesto/lvs/read/reader.hpp"

namespace {

namespace lvs = hefesto::lvs;
namespace rule = hefesto::lvs::merge_rule;

using Record = std::vector<std::pair<std::string, std::string>>;

lvs::Cell fet(std::initializer_list<lvs::PropertyDef> definitions) {
  lvs::Cell klass{"nfet"};
  klass.device_class = lvs::DeviceClass::fet;
  for (const char* pin : {"drain", "gate", "source", "bulk"}) {
    klass.ports.push_back(lvs::Port{pin, klass.add_net(pin), false});
  }
  klass.properties = definitions;
  return klass;
}

lvs::PropertyDef def(std::string key, lvs::PropertyType type, double tolerance, std::uint16_t merge = 0,
                     std::string fallback = {}) {
  lvs::PropertyDef definition;
  definition.key = std::move(key);
  definition.type = type;
  definition.tolerance = tolerance;
  definition.merge = merge;
  definition.default_value = std::move(fallback);
  return definition;
}

lvs::Instance device(std::string name, std::initializer_list<Record> records) {
  lvs::Instance instance;
  instance.name = std::move(name);
  instance.connections = {1, 2, 3, 4};
  for (const auto& record : records) {
    lvs::PropertyRecord converted;
    for (const auto& [key, value] : record) {
      converted.push_back(lvs::Property{key, value});
    }
    instance.properties.push_back(std::move(converted));
  }
  return instance;
}

struct Check {
  lvs::Cell class1;
  lvs::Cell class2;
  lvs::PropertySettings settings;

  explicit Check(lvs::Cell klass) : class1(klass), class2(std::move(klass)) {}

  lvs::PairCheck run(const lvs::Instance& a, const lvs::Instance& b, std::vector<std::uint32_t> keys1 = {},
                     std::vector<std::uint32_t> keys2 = {}) const {
    const lvs::DeviceSide one{a.name, &class1, &a, keys1};
    const lvs::DeviceSide two{b.name, &class2, &b, keys2};
    return lvs::check_device_pair(one, two, settings);
  }
};

bool reported(const lvs::PairCheck& check, std::string_view text) {
  for (const auto& line : check.report) {
    if (line == text) {
      return true;
    }
  }
  return false;
}

const lvs::PropertyDef kW = def("W", lvs::PropertyType::real, 0.01);
const lvs::PropertyDef kL = def("L", lvs::PropertyType::real, 0.01);

}  // namespace

// --------------------------------------------------- of interest

TEST_CASE("Unlisted property ignored", "[cap:lvs-property-matching][req:Properties of interest]") {
  const Check check{fet({kW, kL})};
  const auto result = check.run(device("M1", {{{"W", "1"}, {"L", "1"}, {"ad", "1p"}}}),
                                device("M2", {{{"W", "1"}, {"L", "1"}, {"ad", "2p"}}}));
  CHECK(result.outcome == lvs::PropertyOutcome::match);
  CHECK(result.errors.empty());
}

TEST_CASE("Missing multiplier is one", "[cap:lvs-property-matching][req:Properties of interest]") {
  const Check check{fet({kW})};
  CHECK(check.run(device("M1", {{{"W", "1"}, {"M", "1"}}}), device("M2", {{{"W", "1"}}})).outcome ==
        lvs::PropertyOutcome::match);
  const auto two = check.run(device("M1", {{{"W", "1"}, {"M", "2"}}}), device("M2", {{{"W", "1"}}}));
  CHECK(two.outcome == lvs::PropertyOutcome::mismatch);
  CHECK(reported(two, " M circuit1: 2   circuit2: 1   (delta=1, cutoff=0)"));
}

// -------------------------------------------------------- promotion

TEST_CASE("SI string promoted", "[cap:lvs-property-matching][req:Value types and promotion]") {
  const Check check{fet({kW})};
  CHECK(check.run(device("M1", {{{"W", "1u"}}}), device("M2", {{{"W", "1e-6"}}})).outcome ==
        lvs::PropertyOutcome::match);
}

TEST_CASE("Promotion toward the target type", "[cap:lvs-property-matching][req:Value types and promotion]") {
  Check integer{fet({def("nf", lvs::PropertyType::integer, 0)})};
  CHECK(integer.run(device("M1", {{{"nf", "2.0"}}}), device("M2", {{{"nf", "2"}}})).outcome ==
        lvs::PropertyOutcome::match);
  // Circuit 2 declares nf a double: the double wins as the target.
  integer.class2.properties = {def("nf", lvs::PropertyType::real, 0.5)};
  CHECK(integer.run(device("M1", {{{"nf", "2"}}}), device("M2", {{{"nf", "2.4"}}})).outcome ==
        lvs::PropertyOutcome::match);
  const Check text{fet({def("model", lvs::PropertyType::string, 0, 0, "x")})};
  const auto mismatch = text.run(device("M1", {{{"model", "lvt"}}}), device("M2", {{{"model", "1.5"}}}));
  CHECK(mismatch.outcome == lvs::PropertyOutcome::mismatch);
  CHECK(reported(mismatch, " model circuit1: \"lvt\"   circuit2: \"1.5\"   (exact match req'd)"));
  const Check integers{fet({def("nf", lvs::PropertyType::integer, 0)})};
  const auto typed = integers.run(device("M1", {{{"nf", "2.5"}}}), device("M2", {{{"nf", "abc"}}}));
  CHECK(reported(typed, " nf (property type mismatch)"));
}

// -------------------------------------------------------- tolerance

TEST_CASE("Width within tolerance", "[cap:lvs-property-matching][req:Tolerance rules]") {
  const Check check{fet({kW})};
  CHECK(check.run(device("M1", {{{"W", "1.000e-6"}}}), device("M2", {{{"W", "1.005e-6"}}})).outcome ==
        lvs::PropertyOutcome::match);
}

TEST_CASE("Length mismatch line", "[cap:lvs-property-matching][req:Tolerance rules]") {
  const Check check{fet({kL})};
  const auto result = check.run(device("/M1", {{{"L", "0.15"}}}), device("M5", {{{"L", "0.18"}}}));
  REQUIRE(result.report.size() == 2);
  CHECK(result.report[0] == "M1 vs. M5:");
  CHECK(result.report[1] == " L circuit1: 0.15   circuit2: 0.18   (delta=18.2%, cutoff=1%)");
  REQUIRE(result.errors.size() == 1);
  CHECK(result.errors[0].property == "L");
}

TEST_CASE("String prefix and expression rules", "[cap:lvs-property-matching][req:Tolerance rules]") {
  const Check prefix{fet({def("model", lvs::PropertyType::string, 3, 0, "x")})};
  CHECK(prefix.run(device("M1", {{{"model", "LVTa"}}}), device("M2", {{{"model", "lvtb"}}})).outcome ==
        lvs::PropertyOutcome::match);
  const auto differ = prefix.run(device("M1", {{{"model", "hvt"}}}), device("M2", {{{"model", "lvt"}}}));
  CHECK(reported(differ, " model circuit1: \"hvt\"   circuit2: \"lvt\"   (check to 3 chars.)"));
  const Check expression{fet({kW})};
  const auto unresolved = expression.run(device("M1", {{{"W", "'wp*2'"}}}), device("M2", {{{"W", "1"}}}));
  CHECK(reported(unresolved, " W (unresolved expressions.)"));
}

TEST_CASE("Relative tolerance follows netgen with netgen-compat ON", "[cap:lvs-property-matching][req:Tolerance rules]") {
  // Q-LVS-7 (compat) ON: 2|a-b|/(a+b); a negative sum gives a negative
  // ratio, which matches; 0 and 0 give NaN, which matches.
  CHECK(std::isnan(lvs::relative_delta(0.0, 0.0, true)));
  CHECK(lvs::within_relative_tolerance(0.0, 0.0, 0.01, true));
  CHECK(lvs::within_relative_tolerance(-1.0, -3.0, 0.01, true));
  CHECK_FALSE(lvs::within_relative_tolerance(1.0, -1.0, 0.01, true));  // +inf
  Check check{fet({kW})};
  check.settings.netgen_compat = true;
  CHECK(check.run(device("M1", {{{"W", "-1"}}}), device("M2", {{{"W", "-3"}}})).outcome ==
        lvs::PropertyOutcome::match);
}

TEST_CASE("Relative tolerance uses the larger magnitude with netgen-compat OFF",
          "[cap:lvs-property-matching][req:Tolerance rules]") {
  // Q-LVS-7 (compat) OFF: |a-b|/max(|a|,|b|); two zeros match.
  CHECK(lvs::relative_delta(0.0, 0.0, false) == 0.0);
  CHECK(lvs::within_relative_tolerance(0.0, 0.0, 0.01, false));
  CHECK_FALSE(lvs::within_relative_tolerance(-1.0, -3.0, 0.01, false));
  CHECK(lvs::relative_delta(1.0, 1.01, false) == std::fabs(1.0 - 1.01) / 1.01);
  Check check{fet({kW})};
  check.settings.netgen_compat = false;
  CHECK(check.run(device("M1", {{{"W", "-1"}}}), device("M2", {{{"W", "-3"}}})).outcome ==
        lvs::PropertyOutcome::mismatch);
}

// ---------------------------------------------------------- missing

TEST_CASE("A property missing on one side", "[cap:lvs-property-matching][req:Missing properties]") {
  const Check check{fet({kW, kL})};
  const auto result = check.run(device("M1", {{{"W", "1"}, {"L", "1"}}}), device("M2", {{{"W", "1"}}}));
  CHECK(reported(result, "Property L in circuit1 has no matching property in circuit2"));
  const auto mirror = check.run(device("M1", {{{"W", "1"}}}), device("M2", {{{"W", "1"}, {"L", "1"}}}));
  CHECK(reported(mirror, "Property L in circuit2 has no matching property in circuit1"));
}

TEST_CASE("A device with no records", "[cap:lvs-property-matching][req:Missing properties]") {
  const Check check{fet({kW})};
  CHECK(check.run(device("M1", {}), device("M2", {{{"M", "1"}}})).outcome == lvs::PropertyOutcome::match);
  const auto result = check.run(device("M1", {{{"W", "1"}}}), device("M2", {}));
  CHECK(result.outcome == lvs::PropertyOutcome::mismatch);
  CHECK(reported(result, "Circuit 1 nfet instance M1 property \"W\" has no match in circuit 2."));
}

// ------------------------------------------------------------ timing

TEST_CASE("Property error does not cause mismatch", "[cap:lvs-property-matching][req:When properties are checked]") {
  lvs::NetlistDb db;
  const auto one = lvs::read_netlist(
      db, lvs::Source{"one.spice", std::string{".subckt inv A Y VDD VSS\nM1 Y A VDD VDD pfet W=2\nM2 Y A VSS VSS nfet W=1\n.ends\n"}},
      lvs::Format::spice, {});
  const auto two = lvs::read_netlist(
      db, lvs::Source{"two.spice", std::string{".subckt inv A Y VDD VSS\nM1 Y A VDD VDD pfet W=2\nM2 Y A VSS VSS nfet W=1.5\n.ends\n"}},
      lvs::Format::spice, {});
  REQUIRE(one.ok());
  REQUIRE(two.ok());
  lvs::Comparison comparison{db, one.value(), db, two.value(), {}};
  const auto result = comparison.resolve();
  CHECK(result.status == lvs::PairStatus::match_property_errors);
  CHECK(result.property_state == lvs::PropertyState::errors);
  REQUIRE(result.property_errors.size() == 1);
  CHECK(result.property_errors[0].property == "W");
  bool verdict = false;
  for (const auto& line : result.log) {
    verdict = verdict || line.text == "Netlists match uniquely with property errors.";
  }
  CHECK(verdict);
  // The failed pair is checked once: verifying again adds no error.
  CHECK(comparison.verify(lvs::VerifyOption::unique, false).value == -3);
  CHECK(comparison.result().property_errors.size() == 1);
  const auto out = comparison.verify(lvs::VerifyOption::properties, false);
  REQUIRE(out.lines.size() >= 3);
  CHECK(out.lines[1].text == "Property errors were found.");
}

TEST_CASE("Parallelized instances disagree on pin connections",
          "[cap:lvs-property-matching][req:When properties are checked]") {
  const Check check{fet({})};
  auto a = device("M1", {{}});
  auto b = device("M2", {{}});
  a.open_pins = {lvs::OpenPin::none, lvs::OpenPin::none, lvs::OpenPin::no_connects, lvs::OpenPin::none};
  b.open_pins = {lvs::OpenPin::none, lvs::OpenPin::none, lvs::OpenPin::tied_together, lvs::OpenPin::none};
  const auto result = check.run(a, b);
  CHECK(result.outcome == lvs::PropertyOutcome::mismatch);
  CHECK(reported(result, "  Parallelized instances disagree on pin connections."));
  CHECK(reported(result, "    Circuit1 instance M1 pin source connections are no connects (1)"));
  CHECK(reported(result, "    Circuit2 instance M2 pin source connections are tied together (2)"));
  // A permutable alternative that agrees is accepted.
  Check permuted{fet({})};
  permuted.class1.permutes.push_back(lvs::PinPair{"drain", "source"});
  b.open_pins = {lvs::OpenPin::no_connects, lvs::OpenPin::none, lvs::OpenPin::tied_together, lvs::OpenPin::none};
  CHECK(permuted.run(a, b).outcome == lvs::PropertyOutcome::match);
}

// ---------------------------------------------------- pin associated

TEST_CASE("Source/drain area swap", "[cap:lvs-property-matching][req:Pin-associated properties]") {
  auto as = def("as", lvs::PropertyType::real, 0.01);
  as.pin = "source";
  auto ad = def("ad", lvs::PropertyType::real, 0.01);
  ad.pin = "drain";
  Check check{fet({as, ad})};
  check.class1.permutes.push_back(lvs::PinPair{"drain", "source"});
  check.class2.permutes.push_back(lvs::PinPair{"drain", "source"});
  const auto layout = device("M1", {{{"as", "1"}, {"ad", "2"}}});
  const auto schematic = device("M1", {{{"as", "2"}, {"ad", "1"}}});
  // Net keys: the layout's drain is on the schematic's source net.
  CHECK(check.run(layout, schematic, {10, 11, 12, 13}, {12, 11, 10, 13}).outcome == lvs::PropertyOutcome::match);
  CHECK(check.run(layout, schematic, {10, 11, 12, 13}, {10, 11, 12, 13}).outcome == lvs::PropertyOutcome::mismatch);
}

// --------------------------------------------------- reconciliation

TEST_CASE("Fingers versus wide device", "[cap:lvs-property-matching][req:Series/parallel network reconciliation]") {
  const Check check{fet({def("w", lvs::PropertyType::real, 0.01, rule::kParallelAdd),
                         def("l", lvs::PropertyType::real, 0.01, rule::kParallelCritical)})};
  const auto result = check.run(device("M1", {{{"w", "1"}, {"l", "0.15"}}, {{"w", "1"}, {"l", "0.15"}}}),
                                device("M1", {{{"w", "2"}, {"l", "0.15"}, {"M", "1"}}}));
  CHECK(result.outcome == lvs::PropertyOutcome::match);
  REQUIRE(result.console.size() == 1);
  CHECK(result.console[0] == "No more changes can be made to series/parallel networks.");
}

TEST_CASE("Critical width is not folded (verify against netgen)",
          "[cap:lvs-property-matching][req:Series/parallel network reconciliation]") {
  const Check check{fet({def("w", lvs::PropertyType::real, 0.01, rule::kParallelAdd | rule::kParallelCritical),
                         def("l", lvs::PropertyType::real, 0.01, rule::kParallelCritical)})};
  const auto result = check.run(device("M1", {{{"w", "1"}, {"l", "0.15"}}, {{"w", "1"}, {"l", "0.15"}}}),
                                device("M1", {{{"w", "2"}, {"l", "0.15"}, {"M", "1"}}}));
  CHECK(result.outcome == lvs::PropertyOutcome::mismatch);
}

TEST_CASE("Different finger counts with M", "[cap:lvs-property-matching][req:Series/parallel network reconciliation]") {
  const Check check{fet({def("W", lvs::PropertyType::real, 0.01)})};
  const auto layout = device("M1", {{{"W", "0.5"}}, {{"W", "0.5"}}, {{"W", "0.5"}}, {{"W", "0.5"}}});
  CHECK(check.run(layout, device("M1", {{{"W", "0.5"}, {"M", "4"}}})).outcome == lvs::PropertyOutcome::match);
  const auto three = check.run(layout, device("M1", {{{"W", "0.5"}, {"M", "3"}}}));
  CHECK(three.outcome == lvs::PropertyOutcome::mismatch);
}

TEST_CASE("Network size mismatch is reported", "[cap:lvs-property-matching][req:Series/parallel network reconciliation]") {
  const Check check{fet({def("W", lvs::PropertyType::real, 0.01)})};
  const auto result = check.run(device("M1", {{{"W", "1"}}, {{"W", "2"}}}), device("M2", {{{"W", "1"}}}));
  CHECK(result.outcome == lvs::PropertyOutcome::mismatch);
  CHECK(reported(result, "Circuit 1 parallel/series network does not match Circuit 2"));
}

TEST_CASE("Series records merge by S", "[cap:lvs-property-matching][req:Series/parallel network reconciliation]") {
  const Check check{fet({def("value", lvs::PropertyType::value, 0.01, rule::kSeriesAdd)})};
  const auto chain = device("R1", {{{"value", "1k"}}, {{"_tag", "+"}, {"value", "1k"}}});
  CHECK(check.run(chain, device("R1", {{{"value", "1k"}, {"S", "2"}}})).outcome == lvs::PropertyOutcome::match);
  CHECK(check.run(chain, device("R1", {{{"value", "2k"}}})).outcome == lvs::PropertyOutcome::match);
}

TEST_CASE("A missing double takes the definition's default while merging",
          "[cap:lvs-property-matching][req:Series/parallel network reconciliation]") {
  // Regression (Q-LVS-18 fix, both modes): the record without W merges as
  // W = 1u (the default), not with an unset value.
  for (const bool compat : {true, false}) {
    Check check{fet({def("W", lvs::PropertyType::real, 0.01, 0, "1u")})};
    check.settings.netgen_compat = compat;
    const auto layout = device("M1", {{{"W", "1u"}}, {{"L", "1"}}});
    CHECK(check.run(layout, device("M1", {{{"W", "1u"}, {"M", "2"}}})).outcome == lvs::PropertyOutcome::match);
  }
}

TEST_CASE("Record order is stable across platforms", "[cap:lvs-property-matching][req:Series/parallel network reconciliation]") {
  // Q-LVS-8 fix: a stable three-way order; records that differ only in a
  // non-key property keep their input order, so both orders give one result.
  const Check check{fet({def("w", lvs::PropertyType::real, 0.01, rule::kParallelAdd),
                         def("l", lvs::PropertyType::real, 0.01, rule::kParallelCritical)})};
  const auto ab = device("M1", {{{"w", "1"}, {"l", "0.2"}}, {{"w", "2"}, {"l", "0.1"}}});
  const auto ba = device("M1", {{{"w", "2"}, {"l", "0.1"}}, {{"w", "1"}, {"l", "0.2"}}});
  CHECK(check.run(ab, ba).outcome == lvs::PropertyOutcome::match);
  CHECK(check.run(ba, ab).outcome == lvs::PropertyOutcome::match);
}

// -------------------------------------------------- error results

TEST_CASE("A check that cannot compare fails with property-check-failed",
          "[cap:lvs-property-matching][req:Property error results]") {
  // Q-LVS-10 (replicate): the failure keeps netgen's outcome; the
  // structured result carries the detail.
  const auto a = device("M1", {});
  const lvs::DeviceSide one{a.name, nullptr, &a, {}};
  const lvs::DeviceSide two{a.name, nullptr, &a, {}};
  const auto result = lvs::check_device_pair(one, two, {});
  CHECK(result.outcome == lvs::PropertyOutcome::failure);
  REQUIRE(result.errors.size() == 1);
  CHECK(result.errors[0].detail == lvs::kPropertyCheckFailed);
}

TEST_CASE("Property error listed", "[cap:lvs-property-matching][req:Property error results]") {
  lvs::NetlistDb db;
  const std::string text1 = ".subckt inv A Y VDD VSS TEST\nM1 Y A VDD VDD pfet W=2\nM2 Y A VSS VSS nfet W=1\n.ends\n";
  const std::string text2 = ".subckt inv A Y VDD VSS\nM1 Y A VDD VDD pfet W=2\nM2 Y A VSS VSS nfet W=3\n.ends\n";
  const auto one = lvs::read_netlist(db, lvs::Source{"one.spice", text1}, lvs::Format::spice, {});
  const auto two = lvs::read_netlist(db, lvs::Source{"two.spice", text2}, lvs::Format::spice, {});
  REQUIRE(one.ok());
  REQUIRE(two.ok());
  lvs::Comparison comparison{db, one.value(), db, two.value(), {}};
  const auto result = comparison.resolve();
  CHECK(result.status == lvs::PairStatus::match_port_property_errors);
  CHECK(comparison.verify(lvs::VerifyOption::unique, false).value == -4);
}

// --------------------------------------------------------- topology

TEST_CASE("Strict refuses", "[cap:lvs-property-matching][req:Strict and relaxed topology]") {
  Check check{fet({def("W", lvs::PropertyType::real, 0.01)})};
  const auto layout = device("M1", {{{"W", "1"}, {"S", "2"}}, {{"W", "1"}, {"S", "2"}}});
  const auto schematic = device("M1", {{{"W", "1"}, {"S", "2"}, {"M", "2"}}});
  CHECK(check.run(layout, schematic).outcome == lvs::PropertyOutcome::match);  // relaxed
  check.settings.strict_topology = true;
  CHECK(check.run(layout, schematic).outcome == lvs::PropertyOutcome::mismatch);
}

// ------------------------------------------------------- platform

TEST_CASE("Tolerance edge on two platforms", "[cap:lvs-property-matching][req:Platform-independent property arithmetic]") {
  const Check check{fet({def("w", lvs::PropertyType::real, 0.01)})};
  CHECK(check.run(device("R1", {{{"w", "1.0"}}}), device("R1", {{{"w", "1.01"}}})).outcome ==
        lvs::PropertyOutcome::match);
  const auto result = check.run(device("R1", {{{"w", "1.0"}}}), device("R1", {{{"w", "1.03"}}}));
  CHECK(reported(result, " w circuit1: 1   circuit2: 1.03   (delta=2.96%, cutoff=1%)"));
  CHECK(lvs::format_g(1e-6) == "1e-06");
  CHECK(lvs::format_g(0.15) == "0.15");
  CHECK(lvs::format_g(18.1818, 3) == "18.2");
}
