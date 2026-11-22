// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for bus and vector notation (capability
// schematic-netlisting "Bus and vector notation"; m8-schematic design D19).
// The input is one net, pin or instance name; it is expanded without and
// with a small total budget (Q-SCHEM-41), then its first line is used as a
// bus name and a tap label of a small schematic whose nets are named under
// that budget.
//
// Written by NETLIST; tests/fuzz/ is shared (INTEGRATION).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/connectivity.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace schem = hefesto::schem;
  const std::string_view text = hefesto::fuzz::as_text(data, size);
  const schem::BusExpansion bus = schem::parse_bus(text);
  if (bus.bits.empty() && !text.empty()) {
    __builtin_trap();  // an expansion always gives at least the input back
  }
  (void)schem::replace_bus_chars(schem::join_bits(bus.bits, ","), "<>");

  // Under a total budget (Q-SCHEM-41, sized from the input): the same names
  // when they fit, else the input unchanged, and the budget never grows.
  schem::BusBudget budget{size * 4};
  const std::uint64_t before = budget.remaining();
  const schem::BusExpansion limited = schem::parse_bus(text, &budget);
  const bool same = limited.bits == bus.bits;
  const bool unchanged = limited.bits.size() == 1 && limited.bits.front() == text && limited.width() == 1;
  if ((!same && !(unchanged && limited.over_budget)) || budget.remaining() > before) {
    __builtin_trap();
  }
  if (schem::bus_width(text, &budget) > bus.width()) {
    __builtin_trap();
  }

  const std::string_view name = text.substr(0, std::min<std::size_t>(text.find('\n'), 256));
  schem::SchematicNets s;
  s.wires = {{{0, 0}, {100, 0}, false}};
  schem::NetInstance label{"l", "label", false, false, std::string{name}, true, 1, {{"p", {0, 0}, 1}}};
  schem::NetInstance tap{"t", "bus_tap", false, false, std::string{name}, false, 1,
                         {{"tap", {50, -20}, 1}, {"bus", {50, 0}, 1}}};
  s.instances = {label, tap};
  s.bus_budget = &budget;
  const schem::NetNaming naming = schem::name_nets(s);
  for (const auto& nets : naming.pin_nets) {
    for (const auto& net : nets) {
      (void)schem::net_text(net, naming, "<>", &budget);
    }
  }
  return 0;
}
