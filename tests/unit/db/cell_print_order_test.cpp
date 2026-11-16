// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The order of `cellname list allcells|topcells|modified` (cmd-hierarchy
// "Cellname and instance commands"): Magic's strcmpbynum, glibc's merge sort
// and the cell-table hash order with magic-compat ON (MASK-GR-008).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/db/cell_print_order.hpp"

namespace db = hefesto::db;
using Names = std::vector<std::string>;

TEST_CASE("strcmpbynum compares like Magic's, prefix quirk included",
          "[cap:cmd-hierarchy][req:Cellname and instance commands]") {
  CHECK(db::magic_strcmpbynum("b9", "b10") < 0);
  CHECK(db::magic_strcmpbynum("b10", "b9") > 0);
  CHECK(db::magic_strcmpbynum("Top", "top") == 0);
  CHECK(db::magic_strcmpbynum("a007", "a7") == 0);
  CHECK(db::magic_strcmpbynum("", "") == 0);
  // A proper prefix compares greater both ways (strcmpbynum returns 1 when
  // either name ends first).
  CHECK(db::magic_strcmpbynum("inv", "inv_1") == 1);
  CHECK(db::magic_strcmpbynum("inv_1", "inv") == 1);
  // strtoul saturates: two runs past 2^64 - 1 are equal.
  CHECK(db::magic_strcmpbynum("x99999999999999999999", "x199999999999999999999") == 0);
}

TEST_CASE("the print order is glibc's merge sort of the hash order, reversed",
          "[cap:cmd-hierarchy][req:Cellname and instance commands]") {
  CHECK(db::magic_cell_print_order({"b10", "top", "b9"}) == Names{"b9", "b10", "top"});
  // With the non-transitive prefix case the input order decides.
  CHECK(db::magic_cell_print_order({"inv", "inv_1"}) == Names{"inv_1", "inv"});
  CHECK(db::magic_cell_print_order({"inv_1", "inv"}) == Names{"inv", "inv_1"});
  CHECK(db::magic_cell_print_order({}).empty());
}
