// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Schematics used as symbols (capability schematic-symbols-and-libraries
// "Schematic used as symbol"), built from hand-made documents; the recursion
// limit runs through the symbol cache once FORMAT's reader lands.
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <optional>
#include <string>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/lib/schematic_symbol.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;

namespace {

constexpr const char* kSchematicSymbol = "[cap:schematic-symbols-and-libraries][req:Schematic used as symbol]";

bool format_ready() {
  return schem::get_token("a=1", "a") == std::optional<std::string>{"1"} &&
         schem::read_document("v {xschem version=3.4.8RC file_version=1.3}\n", {}).ok();
}

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
  if (!format_ready()) SKIP("FORMAT's property reader is not implemented yet");
  CHECK(symbol.pins[0].name == "A");
  CHECK(symbol.pins[1].name == "Y");
}

TEST_CASE("floaters and symbol_ignore objects are dropped", kSchematicSymbol) {
  if (!format_ready()) SKIP("FORMAT's property reader is not implemented yet");
  const Library library;
  const schem::Symbol symbol =
      schem::schematic_symbol("inv.sch", "/lib/inv.sch", inverter(), library.resolver(), nullptr);
  for (const auto& text : symbol.document.texts) CHECK(text.text != "W=@W");
  for (const auto& box : symbol.document.boxes) CHECK(box.props != "symbol_ignore=true");
}

TEST_CASE("a .sym next to the schematic orders the generated pins", kSchematicSymbol) {
  if (!format_ready()) SKIP("FORMAT's property reader is not implemented yet");
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
  if (!format_ready()) SKIP("FORMAT's reader is not implemented yet");
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
