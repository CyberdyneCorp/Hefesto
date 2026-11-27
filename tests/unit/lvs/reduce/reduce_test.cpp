// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-device-reduction: combination flags, the schedule, parallel and
// series passes (Q-LVS-9, Q-LVS-26), permutable pins, zero-valued removal,
// merged counts, preservation of pins and names, and the cost bound.
// Netlists are Hefesto-authored.
//
// Owner: LVSRED.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <string_view>

#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/lvs/reduce/codes.hpp"
#include "hefesto/lvs/reduce/reduce.hpp"

namespace {

namespace lvs = hefesto::lvs;

struct Reduced {
  lvs::NetlistDb db;
  lvs::CellId cell = 0;

  explicit Reduced(std::string_view text, std::string name = "in.spice") {
    const auto top = lvs::read_netlist(db, lvs::Source{std::move(name), std::string{text}}, lvs::Format::spice, {});
    REQUIRE(top.ok());
    cell = top.value();
  }

  lvs::Cell& klass(std::string_view name) {
    const auto id = db.find_cell(name, 0);
    REQUIRE(id.has_value());
    return db.cells.at(*id);
  }

  lvs::Cell& self() { return db.cells.at(cell); }

  lvs::ReductionSummary run(const lvs::ReduceOptions& options = {}) { return lvs::reduce(db, cell, options); }
};

std::string records_text(const lvs::Instance& instance) {
  std::string text;
  for (const auto& record : instance.properties) {
    text += "{";
    for (const auto& property : record) {
      text += property.key + "=" + property.value + ",";
    }
    text += "}";
  }
  return text;
}

bool logged(const lvs::ReductionSummary& summary, std::string_view text) {
  for (const auto& line : summary.log) {
    if (line == text) {
      return true;
    }
  }
  return false;
}

}  // namespace

// ------------------------------------------------------------- flags

TEST_CASE("Subcircuits merge in parallel by default", "[cap:lvs-device-reduction][req:Combination flags]") {
  Reduced r{".subckt buf A Y\n.ends\n.subckt top A Y\nX1 A Y buf\nX2 A Y buf\n.ends\n"};
  const auto summary = r.run();
  CHECK(summary.parallel_merged == 1);
  REQUIRE(r.self().instances.size() == 1);
  CHECK(r.self().instances[0].properties.size() == 2);
}

TEST_CASE("A class without parallel-allowed is not merged", "[cap:lvs-device-reduction][req:Combination flags]") {
  Reduced r{".subckt buf A Y\n.ends\n.subckt top A Y\nX1 A Y buf\nX2 A Y buf\n.ends\n"};
  r.klass("buf").flags.no_parallel = true;
  CHECK(r.run().parallel_merged == 0);
  CHECK(r.self().instances.size() == 2);
}

// ---------------------------------------------------------- schedule

TEST_CASE("Series then parallel", "[cap:lvs-device-reduction][req:Reduction schedule]") {
  Reduced r{".subckt top a b\nR1 a m1 1k\nR2 m1 b 1k\nR3 a m2 1k\nR4 m2 b 1k\n.ends\n"};
  r.klass("r").flags.series_combinable = true;
  const auto summary = r.run();
  CHECK(summary.series_merged == 2);
  CHECK(summary.parallel_merged == 1);
  CHECK(summary.modified());
  REQUIRE(r.self().instances.size() == 1);
  CHECK(r.self().instances[0].properties.size() == 4);
  CHECK(logged(summary, "Class top (0):  Merged 2 series devices."));
  CHECK(logged(summary, "Class top (0):  Merged 1 parallel devices."));
}

TEST_CASE("Reduction ends after a series phase that merges nothing",
          "[cap:lvs-device-reduction][req:Reduction schedule]") {
  Reduced r{".subckt top a b\nR1 a b 1k\nR2 a b 1k\n.ends\n"};
  const auto summary = r.run();
  CHECK(summary.parallel_merged == 1);
  CHECK(summary.series_merged == 0);
  CHECK(summary.log.size() == 1);
}

// ---------------------------------------------------------- parallel

TEST_CASE("Two fingers", "[cap:lvs-device-reduction][req:Parallel combination]") {
  Reduced r{".subckt cell y a vss\nM1 y a vss vss nfet W=1\nM2 y a vss vss nfet W=1\n.ends\n"};
  const auto summary = r.run();
  REQUIRE(r.self().instances.size() == 1);
  CHECK(records_text(r.self().instances[0]) == "{W=1,}{W=1,}");
  CHECK(logged(summary, "Class cell (0):  Merged 1 parallel devices."));
}

TEST_CASE("A device without records contributes {M=1}", "[cap:lvs-device-reduction][req:Parallel combination]") {
  Reduced r{".subckt buf A Y\n.ends\n.subckt top A Y\nX1 A Y buf\nX2 A Y buf\n.ends\n"};
  for (auto& instance : r.self().instances) {
    instance.properties.clear();
  }
  (void)r.run();
  REQUIRE(r.self().instances.size() == 1);
  CHECK(records_text(r.self().instances[0]) == "{M=1,}{M=1,}");
}

TEST_CASE("Permuted parallel", "[cap:lvs-device-reduction][req:Parallel combination]") {
  for (const bool compat : {true, false}) {
    Reduced r{".subckt c a b g x\nM1 a g b x nfet\nM2 b g a x nfet\n.ends\n"};
    r.klass("nfet").permutes.push_back(lvs::PinPair{"drain", "source"});
    lvs::ReduceOptions options;
    options.netgen_compat = compat;
    CHECK(r.run(options).parallel_merged == 1);
  }
}

namespace {

// A four-pin class with two permutable pairs; the two devices differ only
// by the second pair.
constexpr std::string_view kTwoPairs = R"(.subckt quad p1 p2 p3 p4
.ends
.subckt top a b c d
X1 a b c d quad
X2 a b d c quad
.ends
)";

}  // namespace

TEST_CASE("Only the first permutable pair gives an alternate signature (netgen-compat ON)",
          "[cap:lvs-device-reduction][req:Parallel combination]") {
  // Q-LVS-9 (compat) ON: netgen merges with the first declared pair only.
  Reduced r{kTwoPairs};
  r.klass("quad").permutes = {lvs::PinPair{"p1", "p2"}, lvs::PinPair{"p3", "p4"}};
  lvs::ReduceOptions options;
  options.netgen_compat = true;
  CHECK(r.run(options).parallel_merged == 0);
  CHECK(r.self().instances.size() == 2);
}

TEST_CASE("Every permutation-equivalent pin order merges (netgen-compat OFF)",
          "[cap:lvs-device-reduction][req:Parallel combination]") {
  // Q-LVS-9 (compat) OFF: every permutable pair takes part.
  Reduced r{kTwoPairs};
  r.klass("quad").permutes = {lvs::PinPair{"p1", "p2"}, lvs::PinPair{"p3", "p4"}};
  lvs::ReduceOptions options;
  options.netgen_compat = false;
  CHECK(r.run(options).parallel_merged == 1);
  CHECK(r.self().instances.size() == 1);
}

TEST_CASE("No-connect pins count as one net", "[cap:lvs-device-reduction][req:Parallel combination]") {
  // Q-LVS-26 (replicate): open mode merges devices whose open pins differ;
  // `property parallel connected` and top cells do not.
  const std::string cell = ".subckt c y a vss\nM1 y a nc1 vss nfet\nM2 y a nc2 vss nfet\n.ends\n";
  Reduced open{cell};
  CHECK(open.run().parallel_merged == 1);
  REQUIRE(open.self().instances.size() == 1);
  CHECK(open.self().instances[0].open_pins.at(2) == lvs::OpenPin::no_connects);

  Reduced connected{cell};
  lvs::ReduceOptions options;
  options.parallel_open = false;
  CHECK(connected.run(options).parallel_merged == 0);

  Reduced top{"M1 y a nc1 vss nfet\nM2 y a nc2 vss nfet\n"};
  CHECK(top.self().flags.top);
  CHECK(top.run().parallel_merged == 0);
}

TEST_CASE("Property values are not part of the parallel test", "[cap:lvs-device-reduction][req:Parallel combination]") {
  Reduced r{".subckt c y a vss\nM1 y a vss vss nfet W=1\nM2 y a vss vss nfet W=2\n.ends\n"};
  CHECK(r.run().parallel_merged == 1);
  CHECK(records_text(r.self().instances[0]) == "{W=1,}{W=2,}");
}

// ------------------------------------------------------------ series

TEST_CASE("Resistor chain", "[cap:lvs-device-reduction][req:Series combination]") {
  Reduced r{".subckt c a b\nR1 a m 1k\nR2 m b 2k\n.ends\n"};
  r.klass("r").flags.series_combinable = true;
  const auto summary = r.run();
  CHECK(summary.series_merged == 1);
  REQUIRE(r.self().instances.size() == 1);
  const auto& device = r.self().instances[0];
  CHECK(records_text(device) == "{value=1k,}{_tag=+,value=2k,}");
  CHECK(r.self().find_net("m") == lvs::kNoNet);
  CHECK(r.self().net(device.connections[0]).name == "a");
  CHECK(r.self().net(device.connections[1]).name == "b");
  CHECK(logged(summary, "Class c (0):  Merged 1 series devices."));
}

TEST_CASE("Port blocks series merge", "[cap:lvs-device-reduction][req:Series combination]") {
  Reduced r{".subckt c a b m\nR1 a m 1k\nR2 m b 2k\n.ends\n"};
  r.klass("r").flags.series_combinable = true;
  CHECK(r.run().series_merged == 0);
  CHECK(r.self().instances.size() == 2);
}

TEST_CASE("A chain reduces in one pass", "[cap:lvs-device-reduction][req:Series combination]") {
  Reduced r{".subckt c a b\nR1 a m1 1k\nR2 m1 m2 2k\nR3 m2 b 3k\n.ends\n"};
  r.klass("r").flags.series_combinable = true;
  lvs::ReductionSummary summary;
  CHECK(lvs::series_pass(r.db, r.cell, {}, summary) == 2);
  REQUIRE(r.self().instances.size() == 1);
  CHECK(records_text(r.self().instances[0]) == "{value=1k,}{_tag=+,value=2k,}{_tag=+,value=3k,}");
}

TEST_CASE("A marked record gets S prepended", "[cap:lvs-device-reduction][req:Series combination]") {
  Reduced r{".subckt c a b\nR1 a m 1k\nR2 m b 2k\n.ends\n"};
  r.klass("r").flags.series_combinable = true;
  r.self().instances[1].properties[0].insert(r.self().instances[1].properties[0].begin(), lvs::Property{"_tag", "+"});
  (void)r.run();
  CHECK(records_text(r.self().instances[0]) == "{value=1k,}{_tag=S+,value=2k,}");
}

TEST_CASE("Series needs the class flag and the first two pins", "[cap:lvs-device-reduction][req:Series combination]") {
  Reduced off{".subckt c a b\nR1 a m 1k\nR2 m b 2k\n.ends\n"};
  CHECK(off.run().series_merged == 0);
  Reduced gate{".subckt c a b g\nM1 a g m b nfet\nM2 m g b b nfet\nM3 x m x b nfet\n.ends\n"};
  gate.klass("nfet").flags.series_combinable = true;
  CHECK(gate.run().series_merged == 0);  // m also reaches a gate
}

// ------------------------------------------------------- permutable

TEST_CASE("Swapped source and drain", "[cap:lvs-device-reduction][req:Permutable pins in matching]") {
  lvs::NetlistDb db;
  const auto one = lvs::read_netlist(db, lvs::Source{"l.spice", std::string{".subckt c out g gnd b\nM1 out g gnd b nfet\n.ends\n"}},
                                     lvs::Format::spice, {});
  const auto two = lvs::read_netlist(db, lvs::Source{"s.spice", std::string{".subckt c out g gnd b\nM1 gnd g out b nfet\n.ends\n"}},
                                     lvs::Format::spice, {});
  REQUIRE(one.ok());
  REQUIRE(two.ok());
  for (lvs::FileNumber f : {0, 1}) {
    db.cells.at(*db.find_cell("nfet", f)).permutes.push_back(lvs::PinPair{"drain", "source"});
  }
  CHECK(lvs::compare_pair(db, one.value(), db, two.value(), {}).status == lvs::PairStatus::match);
}

TEST_CASE("A missing permutable pin stops the permutations", "[cap:lvs-device-reduction][req:Permutable pins in matching]") {
  // Regression (Q-LVS-9 tier 1, both modes): a pin name the class lacks
  // leaves no unset position; the class is merged without permutations.
  for (const bool compat : {true, false}) {
    Reduced r{".subckt c a b g x\nM1 a g b x nfet\nM2 b g a x nfet\n.ends\n"};
    r.klass("nfet").permutes.push_back(lvs::PinPair{"drain", "nosuch"});
    lvs::ReduceOptions options;
    options.netgen_compat = compat;
    const auto summary = r.run(options);
    CHECK(summary.parallel_merged == 0);
    CHECK(logged(summary, "Class nfet does not have pin nosuch."));
    REQUIRE_FALSE(summary.diagnostics.empty());
    CHECK(summary.diagnostics.front().code == lvs::reduce_codes::kMissingPin);
  }
}

TEST_CASE("Shorted ports become permutable", "[cap:lvs-device-reduction][req:Permutable pins in matching]") {
  Reduced r{".subckt s a b c\nR1 a c 1k\n.ends\n"};
  auto& cell = r.db.cells.at(*r.db.find_cell("s", 0));
  cell.ports[1].net = cell.ports[0].net;  // a and b on one net
  const auto messages = lvs::declare_shorted_ports(r.db, *r.db.find_cell("s", 0));
  REQUIRE(messages.size() == 1);
  CHECK(messages[0] == "Pins a and b are shorted in cell s (0)");
  REQUIRE(cell.permutes.size() == 1);
  CHECK(lvs::declare_shorted_ports(r.db, *r.db.find_cell("s", 0)).empty());
}

// ------------------------------------------------------ zero-valued

TEST_CASE("Zero-ohm jumper", "[cap:lvs-device-reduction][req:Zero-valued device removal]") {
  lvs::NetlistDb db;
  const auto schematic = lvs::read_netlist(
      db, lvs::Source{"s.spice", std::string{".subckt c a b x\nR1 a b 0\nM1 a x b b nfet\n.ends\n"}}, lvs::Format::spice, {});
  const auto layout = lvs::read_netlist(
      db, lvs::Source{"l.spice", std::string{".subckt c a x\nM1 a x a a nfet\n.ends\n"}}, lvs::Format::spice, {});
  REQUIRE(schematic.ok());
  REQUIRE(layout.ok());
  const auto summary = lvs::remove_zero_valued(db, schematic.value(), db, layout.value(), false);
  CHECK(summary.zero_valued_removed == 1);
  const auto& cell = db.cells.at(schematic.value());
  REQUIRE(cell.instances.size() == 1);
  CHECK(cell.instances[0].connections[0] == cell.instances[0].connections[2]);  // a and b merged
  CHECK(cell.find_net("b") == lvs::kNoNet);
  bool removed = false;
  bool warned = false;
  for (const auto& line : summary.log) {
    removed = removed || line == "Removing zero-valued device r from cell c (0) makes a better match";
    warned = warned || line.starts_with("Warning: zero-valued device connects port a to another port");
  }
  CHECK(removed);
  CHECK(warned);  // a and b are both ports, and the pair is not the queue top
}

TEST_CASE("Zero-valued removal needs differing counts", "[cap:lvs-device-reduction][req:Zero-valued device removal]") {
  lvs::NetlistDb db;
  const std::string text = ".subckt c a b\nR1 a b 0\n.ends\n";
  const auto one = lvs::read_netlist(db, lvs::Source{"one.spice", text}, lvs::Format::spice, {});
  const auto two = lvs::read_netlist(db, lvs::Source{"two.spice", text}, lvs::Format::spice, {});
  REQUIRE(one.ok());
  REQUIRE(two.ok());
  CHECK(lvs::remove_zero_valued(db, one.value(), db, two.value(), true).zero_valued_removed == 0);
}

// ----------------------------------------------------- merged counts

TEST_CASE("Four fingers merged", "[cap:lvs-device-reduction][req:Merged-device counts in the summary]") {
  Reduced r{".subckt c d g s b\nM1 d g s b nfet\nM2 d g s b nfet\nM3 d g s b nfet\nM4 d g s b nfet\nR1 d s 1k\n.ends\n"};
  for (auto& instance : r.self().instances) {
    instance.properties.clear();
  }
  (void)r.run();
  const auto counts = lvs::class_counts(r.db, r.cell);
  REQUIRE(counts.size() == 2);
  CHECK(lvs::format_class_count(counts[0]) == "nfet (4->1)");
  CHECK(lvs::format_class_count(counts[1]) == "r (1)");
}

// -------------------------------------------------- pins and names

TEST_CASE("Cross-probe merged fingers", "[cap:lvs-device-reduction][req:Reduction preserves pin order and names]") {
  Reduced r{".subckt c d g s b\nM1 d g s b nfet\nM2 d g s b nfet\nM3 d g s b nfet\nM4 d g s b nfet\n.ends\n"};
  const auto ports = r.self().ports.size();
  (void)r.run();
  REQUIRE(r.self().instances.size() == 1);
  const auto& device = r.self().instances[0];
  CHECK(device.name == "nfet:1");  // the first in instance order survives
  CHECK(device.merged == std::vector<std::string>{"nfet:2", "nfet:3", "nfet:4"});
  CHECK(r.self().ports.size() == ports);
  CHECK(r.self().net(device.connections[0]).name == "d");
}

// ------------------------------------------------------------- cost

namespace {

/// A cell with `n` identical transistors between the same four nets,
/// built directly (no reader) so the test measures reduction only.
lvs::CellId fingers(lvs::NetlistDb& db, std::size_t n) {
  lvs::Cell klass{"nfet"};
  klass.device_class = lvs::DeviceClass::fet;
  for (const char* pin : {"drain", "gate", "source", "bulk"}) {
    klass.ports.push_back(lvs::Port{pin, klass.add_net(pin), false});
  }
  const auto class_id = db.add_cell(std::move(klass));
  lvs::Cell cell{"block"};
  std::vector<lvs::NetId> nets;
  for (const char* net : {"d", "g", "s", "b"}) {
    nets.push_back(cell.add_net(net));
  }
  cell.instances.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    lvs::Instance instance;
    instance.cell = class_id;
    instance.name = "m" + std::to_string(i);
    instance.connections = nets;
    cell.instances.push_back(std::move(instance));
  }
  return db.add_cell(std::move(cell));
}

}  // namespace

TEST_CASE("Many parallel fingers", "[cap:lvs-device-reduction][req:Reduction cost]") {
  // 100 000 fingers here; the 1 000 000-finger scenario runs as [.slow].
  lvs::NetlistDb db;
  const auto cell = fingers(db, 100'000);
  const auto summary = lvs::reduce(db, cell, {});
  CHECK(summary.parallel_merged == 99'999);
  const auto counts = lvs::class_counts(db, cell);
  REQUIRE(counts.size() == 1);
  CHECK(counts[0].multiplicity == 100'000);
  CHECK(counts[0].count == 1);
}

TEST_CASE("Many parallel fingers (1M)", "[.][slow][cap:lvs-device-reduction][req:Reduction cost]") {
  lvs::NetlistDb db;
  const auto cell = fingers(db, 1'000'000);
  const auto summary = lvs::reduce(db, cell, {});
  CHECK(summary.parallel_merged == 999'999);
  CHECK(lvs::class_counts(db, cell).at(0).multiplicity == 1'000'000);
}

TEST_CASE("Reduction polls the cancel token", "[cap:lvs-device-reduction][req:Reduction cost]") {
  lvs::NetlistDb db;
  const auto cell = fingers(db, 70'000);
  std::atomic<bool> flag{true};
  lvs::ReduceOptions options;
  options.cancel.flag = &flag;
  const auto summary = lvs::reduce(db, cell, options);
  CHECK(summary.cancelled);
  CHECK(summary.parallel_merged == 0);
}

TEST_CASE("The result does not depend on the device order", "[cap:lvs-device-reduction][req:Reduction cost]") {
  Reduced a{".subckt c x y z\nR1 x y 1k\nR2 y z 1k\nR3 x y 2k\n.ends\n"};
  Reduced b{".subckt c x y z\nR3 x y 2k\nR2 y z 1k\nR1 x y 1k\n.ends\n"};
  (void)a.run();
  (void)b.run();
  const auto ca = lvs::class_counts(a.db, a.cell);
  const auto cb = lvs::class_counts(b.db, b.cell);
  REQUIRE(ca.size() == cb.size());
  CHECK(lvs::format_class_count(ca[0]) == lvs::format_class_count(cb[0]));
}
