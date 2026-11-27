// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Two circuits read from Hefesto-authored SPICE text, and a setup run over
// them, for the lvs-setup-file tests.
//
// Owner: LVSSETUP.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/lvs/setup/setup.hpp"

namespace setup_test {

namespace lvs = hefesto::lvs;

/// A MOS inverter subcircuit with model cells nfet and pfet.
inline constexpr std::string_view kInverter =
    "* inverter\n"
    ".subckt inv A Y VDD VSS\n"
    "M1 Y A VSS VSS nfet W=1 L=0.15 as=1 ad=1\n"
    "M2 Y A VDD VDD pfet W=2 L=0.15\n"
    ".ends\n";

struct Pair {
  lvs::Circuit one;
  lvs::Circuit two;

  explicit Pair(std::string_view first = kInverter, std::string_view second = {}) {
    read(one, "layout.spice", first, 0);
    read(two, "schematic.spice", second.empty() ? first : second, 1);
  }

  static void read(lvs::Circuit& circuit, std::string name, std::string_view text, lvs::FileNumber number) {
    lvs::ReadOptions options;
    options.file_number = number;
    circuit.source = lvs::Source{name, std::nullopt};
    const auto top = lvs::read_netlist(circuit.db, lvs::Source{std::move(name), std::string{text}},
                                       lvs::Format::spice, options);
    REQUIRE(top.ok());
    circuit.top = top.value();
  }

  [[nodiscard]] lvs::SetupContext context(bool compat = true) {
    lvs::SetupContext ctx;
    ctx.first = &one;
    ctx.second = &two;
    ctx.netgen_compat = compat;
    return ctx;
  }

  hefesto::diag::Outcome<lvs::SetupState> run(std::string_view setup, bool compat = true,
                                              std::string name = "setup.tcl") {
    auto ctx = context(compat);
    return lvs::evaluate_setup(lvs::Source{std::move(name), std::string{setup}}, ctx);
  }

  /// Runs `setup`, which must evaluate; returns the state.
  lvs::SetupState ok(std::string_view setup, bool compat = true) {
    auto state = run(setup, compat);
    if (!state.ok()) {
      FAIL(state.error().text);
    }
    return std::move(state).value();
  }

  lvs::Cell& cell(int side, std::string_view name) {
    lvs::Circuit& c = side == 1 ? one : two;
    const auto file = c.db.cells.at(*c.top).file;
    const auto id = c.db.find_cell(name, file);
    REQUIRE(id.has_value());
    return c.db.cells.at(*id);
  }
};

/// The text of every log line, stdout and stderr, joined by newlines.
inline std::string log_text(const lvs::SetupState& state) {
  std::string text;
  for (const auto& message : state.log) {
    text += message.text + "\n";
  }
  return text;
}

inline bool logged(const lvs::SetupState& state, std::string_view line) {
  return std::any_of(state.log.begin(), state.log.end(),
                     [line](const lvs::SetupMessage& message) { return message.text == line; });
}

inline bool logged_error(const lvs::SetupState& state, std::string_view line) {
  return std::any_of(state.log.begin(), state.log.end(), [line](const lvs::SetupMessage& message) {
    return message.stream == lvs::SetupMessage::Stream::err && message.text == line;
  });
}

inline const lvs::PropertyDef* property(const lvs::Cell& cell, std::string_view key) {
  return cell.find_property_def(key);
}

}  // namespace setup_test
