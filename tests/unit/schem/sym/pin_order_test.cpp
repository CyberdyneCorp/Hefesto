// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pins and pin order (capability schematic-symbols-and-libraries "Pins",
// "Pin order"; Q-SCHEM-5).
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;

namespace {

constexpr const char* kPinOrder = "[cap:schematic-symbols-and-libraries][req:Pin order]";
constexpr const char* kPins = "[cap:schematic-symbols-and-libraries][req:Pins]";

schem::Pin pin(std::string name, std::optional<std::string> sim_pinnumber) {
  schem::Pin p;
  p.name = std::move(name);
  p.sim_pinnumber = std::move(sim_pinnumber);
  return p;
}

std::vector<std::string> names_in_order(const schem::Symbol& symbol, diag::DiagnosticSink* sink = nullptr) {
  std::vector<std::string> names;
  for (const schem::PinIndex index : schem::pin_order(symbol, sink)) names.push_back(symbol.pins[index].name);
  return names;
}

}  // namespace

TEST_CASE("Sorted by sim_pinnumber", kPinOrder) {
  schem::Symbol symbol;
  symbol.pins = {pin("B", "3"), pin("A", "1"), pin("C", "2")};
  CHECK(names_in_order(symbol) == std::vector<std::string>{"A", "C", "B"});
}

TEST_CASE("Partial numbering", kPinOrder) {
  schem::Symbol symbol;
  symbol.pins = {pin("B", "2"), pin("A", std::nullopt)};
  diag::CollectingSink sink;
  CHECK(names_in_order(symbol, &sink) == std::vector<std::string>{"B", "A"});
  CHECK(sink.snapshot().empty());
}

TEST_CASE("Duplicate sim_pinnumber", kPinOrder) {
  schem::Symbol symbol;
  symbol.reference = "dup.sym";
  symbol.pins = {pin("B", "1"), pin("A", "1"), pin("C", "0")};
  diag::CollectingSink sink;
  CHECK(names_in_order(symbol, &sink) == std::vector<std::string>{"C", "B", "A"});
  const auto warnings = sink.take();
  REQUIRE(warnings.size() == 1);
  CHECK(warnings[0].code == "SCHEM-SYM-001");
  CHECK(warnings[0].severity == diag::Severity::warning);
  CHECK(warnings[0].text.find("dup.sym") != std::string::npos);
  CHECK(warnings[0].text.find("sim_pinnumber 1") != std::string::npos);
}

TEST_CASE("Non-integer sim_pinnumber values", kPinOrder) {
  schem::Symbol symbol;
  symbol.pins = {pin("A", "2a"), pin("B", "1.9"), pin("C", "x")};
  CHECK(schem::sim_pinnumber_value("2a") == 2);
  CHECK(schem::sim_pinnumber_value("1.9") == 1);
  CHECK(schem::sim_pinnumber_value("x") == 0);
  CHECK(names_in_order(symbol) == std::vector<std::string>{"C", "B", "A"});
}

TEST_CASE("Empty sim_pinnumber disables sorting", kPinOrder) {
  // `sim_pinnumber=""` decodes to the empty value.
  schem::Document document;
  document.boxes = {{5, {0, 0}, {5, 5}, "name=B sim_pinnumber=2"}, {5, {10, 0}, {15, 5}, "name=A sim_pinnumber=\"\""}};
  const schem::Symbol symbol = schem::make_symbol("e.sym", {}, document);
  REQUIRE(symbol.pins[1].sim_pinnumber == std::optional<std::string>{""});
  diag::CollectingSink sink;
  CHECK(names_in_order(symbol, &sink) == std::vector<std::string>{"B", "A"});
  CHECK(sink.snapshot().empty());
}

TEST_CASE("ties keep file order however many pins share a number", kPinOrder) {
  schem::Symbol symbol;
  for (int i = 0; i < 40; ++i) symbol.pins.push_back(pin("P" + std::to_string(i), i % 2 == 0 ? "5" : "4"));
  const auto order = schem::pin_order(symbol);
  REQUIRE(order.size() == 40);
  for (std::size_t i = 0; i < 20; ++i) {
    CHECK(order[i] == 2 * i + 1);
    CHECK(order[20 + i] == 2 * i);
  }
}

TEST_CASE("sim_pinnumber values are read as atoi reads them and clamp to 32 bits", kPinOrder) {
  CHECK(schem::sim_pinnumber_value("12") == 12);
  CHECK(schem::sim_pinnumber_value("  -3") == -3);
  CHECK(schem::sim_pinnumber_value("\v\f\r 3") == 3);
  CHECK(schem::sim_pinnumber_value("+7x") == 7);
  CHECK(schem::sim_pinnumber_value("") == 0);
  CHECK(schem::sim_pinnumber_value("abc") == 0);
  CHECK(schem::sim_pinnumber_value("-") == 0);
  CHECK(schem::sim_pinnumber_value("2147483647") == 2147483647);
  CHECK(schem::sim_pinnumber_value("2147483648") == 2147483647);
  CHECK(schem::sim_pinnumber_value("-2147483648") == -2147483648LL);
  CHECK(schem::sim_pinnumber_value("-2147483649") == -2147483648LL);
  CHECK(schem::sim_pinnumber_value("99999999999999999999999") == std::numeric_limits<std::int32_t>::max());
  CHECK(schem::sim_pinnumber_value("-99999999999999999999999") == std::numeric_limits<std::int32_t>::min());
}

TEST_CASE("a symbol without pins has an empty pin order", kPinOrder) {
  CHECK(schem::pin_order(schem::Symbol{}).empty());
}

TEST_CASE("pins are the layer-5 boxes in file order", kPins) {
  schem::Document document;
  document.kind = schem::FileKind::symbol;
  document.boxes = {{5, {-22.5, -2.5}, {-17.5, 2.5}, "name=G dir=in sim_pinnumber=2"},
                    {4, {0, 0}, {10, 10}, "name=notapin"},
                    {5, {17.5, -2.5}, {22.5, 2.5}, "name=D dir=out"}};
  const schem::Symbol symbol = schem::make_symbol("m.sym", "/lib/m.sym", document);
  REQUIRE(symbol.pins.size() == 2);
  CHECK(symbol.pins[0].box == document.boxes[0]);
  CHECK(symbol.pins[1].box == document.boxes[2]);
  CHECK(symbol.reference == "m.sym");
  CHECK(symbol.file == "/lib/m.sym");
  CHECK(symbol.pins[0].name == "G");
  CHECK(symbol.pins[0].direction == "in");
  CHECK(symbol.pins[0].sim_pinnumber == std::optional<std::string>{"2"});
  CHECK(symbol.pins[1].name == "D");
  CHECK(symbol.pins[1].direction == "out");
  CHECK_FALSE(symbol.pins[1].sim_pinnumber.has_value());
}
