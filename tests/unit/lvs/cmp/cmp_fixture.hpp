// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the lvs/cmp tests: two Hefesto-authored SPICE netlists
// read into one database (files 0 and 1) and compared.
//
// Owner: LVSALG.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/read/reader.hpp"

namespace lvs_cmp_test {

namespace lvs = hefesto::lvs;

/// Reads `text` as file `name` into `db` and returns its top cell.
inline lvs::CellId read(lvs::NetlistDb& db, std::string_view text, std::string name) {
  const auto top = lvs::read_netlist(db, lvs::Source{std::move(name), std::string{text}}, lvs::Format::spice, {});
  REQUIRE(top.ok());
  return top.value();
}

struct Pair {
  lvs::NetlistDb db;
  lvs::CellId one = 0;
  lvs::CellId two = 0;

  Pair(std::string_view first, std::string_view second) {
    one = read(db, first, "one.spice");
    two = read(db, second, "two.spice");
  }

  /// The class cell `name` of file `file` (0: circuit 1, 1: circuit 2).
  lvs::Cell& klass(std::string_view name, lvs::FileNumber file) {
    const auto id = db.find_cell(name, file);
    REQUIRE(id.has_value());
    return db.cells.at(*id);
  }

  /// Declares `a`/`b` permutable on class `name` in both circuits.
  void permute(std::string_view name, std::string a, std::string b) {
    for (lvs::FileNumber f : {0, 1}) {
      klass(name, f).permutes.push_back(lvs::PinPair{a, b});
    }
  }

  [[nodiscard]] lvs::PairResult compare(const lvs::CompareOptions& options = {}) const {
    return lvs::compare_pair(db, one, db, two, options);
  }

  [[nodiscard]] lvs::Comparison comparison(lvs::CompareOptions options = {}) const {
    return lvs::Comparison{db, one, db, two, std::move(options)};
  }
};

[[nodiscard]] inline bool logged(const lvs::PairResult& result, std::string_view text) {
  for (const auto& line : result.log) {
    if (line.text == text) {
      return true;
    }
  }
  return false;
}

// An inverter, its ports first.
inline constexpr std::string_view kInverter = R"(.subckt inv A Y VDD VSS
M1 Y A VDD VDD pfet W=2 L=0.15
M2 Y A VSS VSS nfet W=1 L=0.15
.ends
)";

}  // namespace lvs_cmp_test
