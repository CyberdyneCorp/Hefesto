// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Schematics used as symbols (capability schematic-symbols-and-libraries
// "Schematic used as symbol"), built from hand-made documents; the recursion
// limit runs through the symbol cache once FORMAT's reader lands.
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <map>
#include <optional>
#include <string>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/file/transform.hpp"
#include "hefesto/schem/lib/schematic_symbol.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;

namespace {

constexpr const char* kSchematicSymbol = "[cap:schematic-symbols-and-libraries][req:Schematic used as symbol]";

schem::Symbol port_symbol(const char* type) {
  schem::Symbol symbol;
  symbol.document.kind = schem::FileKind::symbol;
  symbol.document.globals.k = std::string{"type="} + type + " template=\"name=p1 lab=xxx\"";
  symbol.document.boxes.push_back({5, {-2.5, -2.5}, {2.5, 2.5}, "name=p dir=in"});
  symbol.document.lines.push_back({4, {0, 0}, {20, 0}, ""});
  symbol.pins = schem::make_symbol("", "", symbol.document).pins;
  return symbol;
}

struct Library {
  std::map<std::string, schem::Symbol> symbols;
  schem::Symbol missing;

  Library() {
    missing.placeholder = true;
    symbols["ipin.sym"] = port_symbol("ipin");
    symbols["opin.sym"] = port_symbol("opin");
    symbols["title.sym"] = port_symbol("logo");
  }

  [[nodiscard]] schem::NestedSymbol resolver() const {
    return [this](const schem::InstanceRecord& instance) -> const schem::Symbol& {
      const auto it = symbols.find(instance.symbol);
      return it == symbols.end() ? missing : it->second;
    };
  }
};

schem::Document inverter() {
  schem::Document sch;
  sch.globals.k = "type=subcircuit format=\"@name @pinlist @symname\"";
  sch.lines.push_back({4, {0, 0}, {10, 10}, ""});
  sch.wires.push_back({{0, 0}, {50, 0}, "lab=A"});
  sch.texts.push_back({"INV", {0, -20}, 0, 0, 0.4, 0.4, ""});
  sch.texts.push_back({"W=@W", {0, -30}, 0, 0, 0.4, 0.4, "name=M1"});            // floater: dropped
  sch.boxes.push_back({4, {0, 0}, {5, 5}, "symbol_ignore=true"});                // dropped
  sch.instances.push_back({"ipin.sym", {100, 200}, 1, 0, "name=p1 lab=A"});
  sch.instances.push_back({"opin.sym", {300, 200}, 0, 1, "name=p2 lab=Y"});
  sch.instances.push_back({"title.sym", {0, 0}, 0, 0, "name=l1"});               // logo: left out
  return sch;
}

}  // namespace

TEST_CASE("a schematic used as a symbol takes its K record, graphics and wires", kSchematicSymbol) {
  const Library library;
  const schem::Document sch = inverter();
  const schem::Symbol symbol = schem::schematic_symbol("inv.sch", "/lib/inv.sch", sch, library.resolver(), nullptr);
  CHECK(symbol.document.kind == schem::FileKind::symbol);
  CHECK(symbol.property() == sch.globals.k);
  CHECK(symbol.file == "/lib/inv.sch");
  CHECK(symbol.document.path == "/lib/inv.sch");
  // The wire is a layer-1 line after the schematic's own line.
  REQUIRE(symbol.document.lines.size() >= 2);
  CHECK(symbol.document.lines[0] == sch.lines[0]);
  CHECK(symbol.document.lines[1] == schem::LineRecord{1, {0, 0}, {50, 0}, "lab=A"});
}

TEST_CASE("instances inside are drawn transformed and their pin boxes move to layer 7", kSchematicSymbol) {
  const Library library;
  const schem::Symbol symbol =
      schem::schematic_symbol("inv.sch", "/lib/inv.sch", inverter(), library.resolver(), nullptr);
  // ipin at (100,200) r=1: its line (0,0)-(20,0) becomes (100,200)-(100,220).
  CHECK(symbol.document.lines[2] == schem::LineRecord{4, {100, 200}, {100, 220}, ""});
  // opin at (300,200) flipped: (0,0)-(20,0) becomes (300,200)-(280,200).
  CHECK(symbol.document.lines[3] == schem::LineRecord{4, {300, 200}, {280, 200}, ""});
  CHECK(symbol.document.lines.size() == 4);  // the logo's line is left out
  int nested_pins = 0;
  for (const auto& box : symbol.document.boxes) nested_pins += box.layer == 7 ? 1 : 0;
  CHECK(nested_pins == 2);
}

TEST_CASE("pins come from the port instances at their positions", kSchematicSymbol) {
  const Library library;
  const schem::Symbol symbol =
      schem::schematic_symbol("inv.sch", "/lib/inv.sch", inverter(), library.resolver(), nullptr);
  REQUIRE(symbol.pins.size() == 2);
  CHECK(symbol.pins[0].direction == "in");
  CHECK(symbol.pins[1].direction == "out");
  CHECK(symbol.pins[0].box.layer == 5);
  CHECK(symbol.pins[0].box.p1 == schem::Point{97.5, 197.5});
  CHECK(symbol.pins[0].box.p2 == schem::Point{102.5, 202.5});
  CHECK(symbol.pins[1].box.p1 == schem::Point{297.5, 197.5});
  CHECK(symbol.pins[0].name == "A");
  CHECK(symbol.pins[1].name == "Y");
}

TEST_CASE("floaters and symbol_ignore objects are dropped", kSchematicSymbol) {
  const Library library;
  const schem::Symbol symbol =
      schem::schematic_symbol("inv.sch", "/lib/inv.sch", inverter(), library.resolver(), nullptr);
  for (const auto& text : symbol.document.texts) CHECK(text.text != "W=@W");
  for (const auto& box : symbol.document.boxes) CHECK(box.props != "symbol_ignore=true");
}

TEST_CASE("a .sym next to the schematic orders the generated pins", kSchematicSymbol) {
  const Library library;
  schem::Symbol sibling;
  sibling.pins.resize(2);
  sibling.pins[0].name = "Y";
  sibling.pins[1].name = "A";
  const schem::Symbol symbol =
      schem::schematic_symbol("inv.sch", "/lib/inv.sch", inverter(), library.resolver(), &sibling);
  REQUIRE(symbol.pins.size() == 2);
  CHECK(symbol.pins[0].name == "Y");
  CHECK(symbol.pins[1].name == "A");
}

TEST_CASE("Recursion limit", kSchematicSymbol) {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/w/a.sch",
                          "v {xschem version=3.4.8RC file_version=1.3}\nK {type=subcircuit}\n"
                          "C {a.sch} 0 0 0 0 {name=x1}\nC {a.sch} 0 100 0 0 {name=x2}\n")
              .ok());
  const schem::SearchPath path{{"/w"}};
  schem::Document top;
  top.path = "/w/top.sch";
  diag::CollectingSink sink;
  schem::SymbolCache cache;
  const schem::Symbol& a = cache.resolve({"a.sch"}, top, {path, fs, &sink});
  CHECK_FALSE(a.placeholder);
  const auto warnings = sink.take();
  REQUIRE(warnings.size() == 1);
  CHECK(warnings[0].code == "SCHEM-LIB-005");
  CHECK(warnings[0].text == "Symbol recursively instantiating symbol: max depth reached, skipping");
}

// Regression: SYMLIB and NETLIST had their own copies of the orientation
// transform, which disagreed with FORMAT's for flip values other than 0
// and 1. All three now use hefesto/schem/file/transform.hpp; the graphics a
// schematic symbol draws must match it for every rotation and flip.
TEST_CASE("placed graphics follow FORMAT's orientation transform",
          "[cap:schematic-file-format][req:Orientation transform]") {
  schem::Symbol shape;
  shape.document.kind = schem::FileKind::symbol;
  shape.document.globals.k = "type=subcircuit";
  shape.document.lines.push_back({4, {0, 0}, {20, 10}, ""});
  shape.document.arcs.push_back({4, {5, -5}, 10, 30, 90, ""});
  shape.document.texts.push_back({"t", {-10, 3}, 1, 0, 0.2, 0.2, ""});
  Library library;
  library.symbols["shape.sym"] = shape;
  for (int rotation = 0; rotation < 4; ++rotation) {
    for (int flip = 0; flip < 3; ++flip) {
      INFO("rotation " << rotation << " flip " << flip);
      schem::Document sch;
      sch.instances.push_back({"shape.sym", {100, 50}, rotation, flip, "name=x1"});
      const schem::Symbol placed = schem::schematic_symbol("s.sch", "/lib/s.sch", sch, library.resolver(), nullptr);
      const schem::Placement at{{100, 50}, rotation, flip};
      REQUIRE(placed.document.lines.size() == 1);
      CHECK(placed.document.lines[0].p1 == schem::transform_point(at, {0, 0}));
      CHECK(placed.document.lines[0].p2 == schem::transform_point(at, {20, 10}));
      REQUIRE(placed.document.arcs.size() == 1);
      CHECK(placed.document.arcs[0].centre == schem::transform_point(at, {5, -5}));
      CHECK(placed.document.arcs[0].start_angle == schem::transform_arc_start(at, 30, 90));
      REQUIRE(placed.document.texts.size() == 1);
      CHECK(placed.document.texts[0].origin == schem::transform_point(at, {-10, 3}));
      const schem::TextOrientation text = schem::transform_text(at, 1, 0);
      CHECK(placed.document.texts[0].rotation == text.rotation);
      CHECK(placed.document.texts[0].flip == text.flip);
    }
  }
  // "x' = -x if f = 1, else x": a flip of 2 does not mirror.
  CHECK(schem::transform_point({{0, 0}, 0, 2}, {3, 7}) == schem::Point{3, 7});
}

// Regression: ordering the generated pins like the sibling .sym searched
// every generated pin for each sibling pin, O(pins^2) on counts taken from
// hostile files. Pins are now bucketed by name; duplicates keep their order.
TEST_CASE("a .sym with many pins orders the generated pins in near-linear time", kSchematicSymbol) {
  const Library library;
  constexpr int kPins = 100000;  // quadratic: about 5 x 10^9 name comparisons
  schem::Document sch;
  schem::Symbol sibling;
  for (int i = 0; i < kPins; ++i) {
    sch.instances.push_back({"ipin.sym", {0, 10.0 * i}, 0, 0, "name=p" + std::to_string(i) + " lab=N" + std::to_string(i)});
    schem::Pin pin;
    pin.name = "N" + std::to_string(kPins - 1 - i);
    sibling.pins.push_back(pin);
  }
  sch.instances.push_back({"ipin.sym", {5, 5}, 0, 0, "name=dup lab=N0"});  // a second N0, kept after the first
  const auto start = std::chrono::steady_clock::now();
  const schem::Symbol symbol = schem::schematic_symbol("big.sch", "/lib/big.sch", sch, library.resolver(), &sibling);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(elapsed < std::chrono::seconds{10});
  REQUIRE(symbol.pins.size() == kPins + 1);
  CHECK(symbol.pins.front().name == "N" + std::to_string(kPins - 1));
  CHECK(symbol.pins[kPins - 1].name == "N0");
  CHECK(symbol.pins[kPins - 1].box.p1.y == -2.5);  // the first N0, not the duplicate
  CHECK(symbol.pins.back().name == "N0");
  CHECK(symbol.pins.back().box.p1.x == 2.5);
}
