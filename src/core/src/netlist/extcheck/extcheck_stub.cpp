// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub of netlist::extcheck::run (netlist/extcheck/extcheck.hpp):
// prints CORE-NOTIMPL-008's text and returns 1 until SIM lands it
// (openspec/changes/m7-extraction tasks.md, SIM). SIM replaces this file
// with its own sources.
//
// Owner: SIM (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/netlist/extcheck/extcheck.hpp"

namespace hefesto::netlist::extcheck {

int run(std::span<const std::string> /*args*/, std::ostream& /*out*/, std::ostream& err,
        const std::string& /*program*/) {
  err << diag::format_text(cmd::m7_not_implemented_diagnostic("extcheck")) << '\n';
  return 1;
}

}  // namespace hefesto::netlist::extcheck
