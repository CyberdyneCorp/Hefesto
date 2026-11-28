// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for LVS setup files (capability lvs-setup-file;
// openspec/changes/m9-lvs-simulation design.md D7, D20, hefesto-testing-
// strategy "Parser fuzzing"): the input is a setup file evaluated through
// the sandbox against two small fixed circuits (subcircuits, MOS, R, C and
// placeholder devices), so every setup command, the cell reference syntax
// and the line accumulation see crafted words. The first byte picks
// netgen-compat and the seed; the step and time budgets are lowered so that
// a loop ends quickly. Seeds are Hefesto-authored (corpus/fuzz_lvs_setup/).
//
// Owner: LVSSETUP.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/lvs/setup/setup.hpp"

namespace {

namespace lvs = hefesto::lvs;

constexpr std::string_view kFirst =
    "* layout\n"
    ".subckt inv A Y VDD VSS\nM1 Y A VSS VSS nfet W=1 L=0.15\nM2 Y A VDD VDD pfet W=2 L=0.15\n.ends\n"
    ".subckt cell a b c\nX1 a b inv_x\nR1 a b 1k\nC1 b c 1p\nX2 a b c rpoly w=1 l=2\n.ends\n"
    ".subckt top a y vdd vss\nX1 a y vdd vss inv\nX2 a y vss cell\n.ends\n";
constexpr std::string_view kSecond =
    "* schematic\n"
    ".global VDD\n.subckt inv A Y VSS\nM1 Y A VSS VSS nfet W=1 L=0.15\nM2 Y A VDD VDD pfet W=2 L=0.15\n.ends\n"
    ".subckt rpoly p n c\n.ends\n"
    ".subckt top a y vdd vss\nX1 a y vss inv\nX2 a y vss rpoly w=1 l=2\nC1 y vss 1p\n.ends\n";

void read(lvs::Circuit& circuit, std::string name, std::string_view text, lvs::FileNumber number) {
  lvs::ReadOptions options;
  options.file_number = number;
  const auto top = lvs::read_netlist(circuit.db, lvs::Source{std::move(name), std::string{text}},
                                     lvs::Format::spice, options);
  if (top.ok()) {
    circuit.top = top.value();
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, setup] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  lvs::Circuit first;
  lvs::Circuit second;
  read(first, "layout.spice", kFirst, 0);
  read(second, "schematic.spice", kSecond, 1);
  lvs::SetupContext context;
  context.first = &first;
  context.second = &second;
  context.netgen_compat = (mode & 1U) != 0;
  context.seed = mode >> 1U;
  context.step_budget = 200'000;
  context.time_budget = std::chrono::milliseconds{2'000};
  const auto state = lvs::evaluate_setup(lvs::Source{"fuzz.tcl", std::string{setup}}, context);
  if (state.ok()) {
    // The circuits stay consistent: every connection names a net of its cell.
    for (const lvs::Circuit* circuit : {&first, &second}) {
      for (const auto& cell : circuit->db.cells) {
        for (const auto& instance : cell.instances) {
          for (const lvs::NetId net : instance.connections) {
            if (net > cell.nets.size()) {
              __builtin_trap();
            }
          }
        }
      }
    }
  }
  return 0;
}
