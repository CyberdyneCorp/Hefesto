// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of extract/cap (caps.hpp): CORE-NOTIMPL-008 or 0 until
// EXTCAP lands them (openspec/changes/m7-extraction tasks.md 6.1-6.7).
// EXTCAP replaces this file with its own sources.
//
// Owner: EXTCAP (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extract/cap/caps.hpp"

namespace hefesto::extract {

diag::Status substrate_caps(NodeRegionList& /*nodes*/, const CapInputs& /*in*/) {
  return cmd::m7_not_implemented_status("extract substrate capacitance");
}

diag::Status overlap_caps(const NodeRegionList& /*nodes*/, const CapInputs& /*in*/, CouplingTable& /*out*/) {
  return cmd::m7_not_implemented_status("extract overlap capacitance");
}

diag::Status sidewall_caps(const NodeRegionList& /*nodes*/, const CapInputs& /*in*/, CouplingTable& /*out*/) {
  return cmd::m7_not_implemented_status("extract sidewall capacitance");
}

diag::Status sideoverlap_caps(NodeRegionList& /*nodes*/, const CapInputs& /*in*/, FringeModel /*model*/,
                              CouplingTable& /*out*/) {
  return cmd::m7_not_implemented_status("extract sideoverlap capacitance");
}

diag::Status corner_caps(NodeRegionList& /*nodes*/, const CapInputs& /*in*/, double /*factor*/,
                         CouplingTable& /*out*/) {
  return cmd::m7_not_implemented_status("extract corner capacitance");
}

std::int64_t lumped_resistance(const NodeRegion& /*node*/, const ExtStyle& /*style*/) {
  return 0;
}

double fringe_fraction(double /*mult*/, double /*d*/) {
  return 0.0;
}

}  // namespace hefesto::extract
