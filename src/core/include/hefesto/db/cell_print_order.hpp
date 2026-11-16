// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The order of `cellname list allcells|topcells|modified` with magic-compat
// ON (capability cmd-hierarchy "Cellname and instance commands"). Magic's
// DBCellPrint collects the names in cell-table hash order, qsorts them with
// qcompare (the negated strcmpbynum) and prints the array from its end.
// strcmpbynum is not a total order: when one name is a proper prefix of the
// other it returns 1 both ways ("inv" and "inv_1" each compare greater than
// the other), so the result depends on the sort algorithm and on the hash
// order. The reference Magic runs on glibc (2.41 in the capture image),
// whose qsort is the top-down merge sort msort_with_tmp; this file ports
// both. With magic-compat OFF the names are sorted by the total natural
// order util::compare_natural instead.
//
// Magic ref: database/DBcellname.c:676-730 (strcmpbynum, qcompare), 784-880
// (DBCellPrint); glibc stdlib/qsort.c (msort_with_tmp).
//
// Owner: MODEL.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hefesto::db {

/// Magic's strcmpbynum, byte for byte: digit runs compare by value (strtoul,
/// saturating), other bytes by tolower in the C locale as signed chars; a
/// name that ends first compares greater ("" vs "a" is 1, and so is "a" vs "").
[[nodiscard]] int magic_strcmpbynum(std::string_view s1, std::string_view s2) noexcept;

/// DBCellPrint's order: `names` given in cell-table hash order, merge sorted
/// as glibc's qsort does with qcompare, then reversed.
[[nodiscard]] std::vector<std::string> magic_cell_print_order(std::vector<std::string> names);

}  // namespace hefesto::db
