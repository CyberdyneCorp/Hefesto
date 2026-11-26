// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResSortByGate (extresist/command/cutoff.hpp sort_by_gate; capability
// extresist-command "Net selection and first cutoff"; Q-NETLIST-69). Owner:
// RESCMD.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "hefesto/extresist/command/cutoff.hpp"
#include "hefesto/extresist/output/output.hpp"

namespace extresist = hefesto::extresist;

namespace {

extresist::InputDevice fet(std::size_t gate, std::size_t source, std::size_t drain) {
  extresist::InputDevice d;
  d.gate = gate;
  d.source = source;
  d.drain = drain;
  return d;
}

std::vector<std::size_t> order(const std::vector<extresist::NodeDevice>& devices) {
  std::vector<std::size_t> out;
  for (const extresist::NodeDevice& d : devices) {
    out.push_back(d.device);
  }
  return out;
}

}  // namespace

// Regression (Q-NETLIST-69): Magic's qsort compares node addresses; the
// order is the nets' reverse first-mention order (a higher index first),
// sorted as glibc's merge sort sorts with devSortFunc (M7 R5: NL-RES-01's
// `out.t2` is the nfet at x 400, the later gate entry of the list).
TEST_CASE("sort_by_gate groups by gate net, then the other S/D net, gate entries last",
          "[cap:extresist-command][req:Net selection and first cutoff]") {
  using extresist::Terminal;
  extresist::InputModel m;
  m.nodes.resize(7);  // net 0 is the net being sorted
  m.devices = {
      fet(2, 0, 3),  // 0: gate net 2, other S/D 3
      fet(1, 0, 4),  // 1: gate net 1, other 4
      fet(0, 5, 6),  // 2: net 0 is its gate
      fet(1, 5, 0),  // 3: gate net 1, net 0 on the drain, other 5
      fet(1, 0, 4),  // 4: the same nets as device 1
      fet(0, 6, 5),  // 5: net 0 is its gate
  };
  const std::vector<extresist::NodeDevice> on_net = {
      {5, Terminal::gate},   {0, Terminal::source}, {2, Terminal::gate},
      {1, Terminal::source}, {4, Terminal::source}, {3, Terminal::drain},
  };
  const auto sorted = extresist::sort_by_gate(m, on_net);
  // Gate 2 (device 0), gate 1 (3 with other net 5, then devices 1 and 4,
  // which tie on other net 4 and keep their order), then the gate entries 5
  // and 2 reversed (devSortFunc says a gate entry is greater than another).
  CHECK(order(sorted) == std::vector<std::size_t>{0, 3, 1, 4, 2, 5});
  CHECK(sorted[1].terminal == Terminal::drain);
}
