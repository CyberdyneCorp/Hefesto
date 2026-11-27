// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the lvs/hier tests: Hefesto-authored SPICE netlists
// read into one circuit each and compared with run_lvs.
//
// Owner: LVSHIER.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "hefesto/lvs/hier/hierarchy.hpp"
#include "hefesto/lvs/hier/run.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/lvs/report/report.hpp"

namespace lvs_hier_test {

namespace lvs = hefesto::lvs;

/// `text` read as file `name` (file number `file`: 0 for circuit 1, 1 for
/// circuit 2, as netgen numbers the two netlists) into a new circuit, its
/// top cell set.
inline lvs::Circuit circuit(std::string_view text, std::string name, lvs::FileNumber file = 0,
                            bool blackbox = false) {
  lvs::Circuit c;
  c.source = lvs::Source{std::move(name), std::string{text}};
  lvs::ReadOptions options;
  options.auto_blackbox = blackbox;
  options.file_number = file;
  const auto top = lvs::read_netlist(c.db, c.source, lvs::Format::spice, options);
  REQUIRE(top.ok());
  c.top = top.value();
  return c;
}

inline lvs::LvsResult run(lvs::Circuit first, lvs::Circuit second, const lvs::RunOptions& options = {}) {
  lvs::NullProgress progress;
  return lvs::run_lvs(std::move(first), std::move(second), {}, options, progress);
}

inline lvs::LvsResult run(std::string_view first, std::string_view second, const lvs::RunOptions& options = {}) {
  return run(circuit(first, "one.spice", 0), circuit(second, "two.spice", 1), options);
}

inline std::string log_of(const lvs::LvsResult& result) {
  return lvs::write_comp_out(result).value();
}

inline std::string console_of(const lvs::LvsResult& result) {
  std::string text;
  for (const auto& piece : result.output) {
    if (piece.console_only) {
      text += piece.text;
    }
  }
  return text;
}

inline bool contains(const std::string& text, std::string_view part) {
  return text.find(part) != std::string::npos;
}

/// The cell named `name` of a circuit read as file `file`.
inline lvs::CellId cell_id(const lvs::Circuit& c, std::string_view name, lvs::FileNumber file = 0) {
  const auto id = c.db.find_cell(name, file);
  REQUIRE(id.has_value());
  return *id;
}

inline lvs::Cell& cell(lvs::Circuit& c, std::string_view name, lvs::FileNumber file = 0) {
  return c.db.cells[cell_id(c, name, file)];
}

// An inverter.
inline constexpr std::string_view kInv = R"(.subckt inv A Y VDD VSS
M1 Y A VDD VDD pfet W=2 L=0.15
M2 Y A VSS VSS nfet W=1 L=0.15
.ends
)";

}  // namespace lvs_hier_test
