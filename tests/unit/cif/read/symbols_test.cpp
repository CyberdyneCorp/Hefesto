// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-read CIF symbol table (capability cif-format-io "Symbol-to-cell
// naming on read"; Q-MASK-318 fix, DD compat). Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <set>
#include <string>

#include "hefesto/cif/read/symbols.hpp"

using hefesto::cif::SymbolTable;

namespace {

SymbolTable with_library(const std::set<std::string, std::less<>>& library) {
  return SymbolTable([&library](std::string_view name) { return library.contains(name); });
}

}  // namespace

TEST_CASE("Symbol n names a new cell n, or n_k when the name is taken",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  const std::set<std::string, std::less<>> library{"5", "5_1"};
  SymbolTable table = with_library(library);
  const auto five = table.define(5);
  REQUIRE(five);
  CHECK(table.cell(five->cell).name == "5_2");  // the library cells keep their names
  CHECK_FALSE(five->renamed_to);
  const auto six = table.call(6);
  REQUIRE(six);
  CHECK(table.cell(*six).name == "6");
  CHECK_FALSE(table.cell(*six).defined);
}

TEST_CASE("A forward call and the later definition name the same cell",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SymbolTable table;
  const auto called = table.call(7);
  const auto defined = table.define(7);
  REQUIRE(called);
  REQUIRE(defined);
  CHECK(defined->cell == *called);
  CHECK(table.cell(*called).defined);
  CHECK(table.cells().size() == 1);
}

TEST_CASE("A redefinition renames the earlier cell of this read and makes a fresh one",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SymbolTable table;
  const auto first = table.define(5);
  const auto second = table.define(5);
  REQUIRE(first);
  REQUIRE(second);
  CHECK(second->cell != first->cell);
  REQUIRE(second->renamed_to);
  CHECK(second->renamed_from == "5");
  CHECK(*second->renamed_to == "5_1");
  CHECK(table.cell(first->cell).name == "5_1");
  CHECK(table.cell(second->cell).name == "5");
}

TEST_CASE("A redefinition after 9 renamed the symbol leaves the renamed cell alone",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SymbolTable table;
  const auto first = table.define(1);
  REQUIRE(first);
  REQUIRE_FALSE(table.name_in_use("inv"));
  table.rename(first->cell, "inv");
  const auto second = table.define(1);
  REQUIRE(second);
  CHECK_FALSE(second->renamed_to);
  CHECK(table.cell(first->cell).name == "inv");
  CHECK(table.cell(second->cell).name == "1");
}

TEST_CASE("DD forgets one symbol with magic-compat ON", "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SymbolTable table;
  REQUIRE(table.define(3));
  REQUIRE(table.define(7));
  table.forget(3, true);
  CHECK_FALSE(table.find(3));
  CHECK(table.find(7));
}

TEST_CASE("DD forgets every higher symbol with magic-compat OFF",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SymbolTable table;
  REQUIRE(table.define(2));
  REQUIRE(table.define(3));
  REQUIRE(table.define(7));
  table.forget(3, false);
  CHECK(table.find(2));
  CHECK_FALSE(table.find(3));
  CHECK_FALSE(table.find(7));
  const auto again = table.call(7);  // a new, undefined cell
  REQUIRE(again);
  CHECK(table.cell(*again).name == "7_1");
  CHECK_FALSE(table.cell(*again).defined);
}

TEST_CASE("A name this read gave out is not asked of the library",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  // The library already lists this read's cells (the engine creates them),
  // and learns of a rename only after the table made it.
  std::set<std::string, std::less<>> library;
  SymbolTable table = with_library(library);
  const auto first = table.define(5);
  REQUIRE(first);
  library.insert("5");
  const auto second = table.define(5);  // "5" moves to "5_1" before the library hears of it
  REQUIRE(second);
  CHECK(table.cell(second->cell).name == "5");
  library.insert("top");
  CHECK(table.name_in_use("top"));
  table.taken_outside("5");  // e.g. a top-level 9 renamed the edit cell
  CHECK(table.name_in_use("5"));
}

TEST_CASE("The cell cap stops new cells", "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SymbolTable table(nullptr, 2);
  CHECK(table.define(1));
  CHECK(table.call(2));
  CHECK_FALSE(table.call(3));
  CHECK_FALSE(table.define(4));
  CHECK(table.define(2));  // a forward call's cell can still be defined
  CHECK(table.cells().size() == 2);
}

TEST_CASE("Redefining one symbol many times stays linear and names n_1, n_2, ...",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SymbolTable table;
  constexpr int kTimes = 50000;  // about 1.25e9 name lookups when each search starts at _1
  const auto started = std::chrono::steady_clock::now();
  for (int i = 0; i < kTimes; ++i) {
    REQUIRE(table.define(1));
  }
  const auto elapsed = std::chrono::steady_clock::now() - started;
  CHECK(elapsed < std::chrono::seconds{20});
  REQUIRE(table.cells().size() == kTimes);
  CHECK(table.cells().front().name == "1_1");
  CHECK(table.cells()[kTimes - 2].name == "1_" + std::to_string(kTimes - 1));
  CHECK(table.cells().back().name == "1");
}

TEST_CASE("A suffix freed by a rename is given out again",
          "[cap:cif-format-io][req:Symbol-to-cell naming on read]") {
  SymbolTable table;
  for (int i = 0; i < 3; ++i) {
    REQUIRE(table.define(5));
  }
  REQUIRE(table.cells()[0].name == "5_1");
  REQUIRE(table.cells()[1].name == "5_2");
  table.rename(0, "x");  // `9 x` inside the first definition frees 5_1
  const auto again = table.define(5);
  REQUIRE(again);
  REQUIRE(again->renamed_to);
  CHECK(*again->renamed_to == "5_1");
  const auto more = table.define(5);
  REQUIRE(more);
  REQUIRE(more->renamed_to);
  CHECK(*more->renamed_to == "5_3");
}
