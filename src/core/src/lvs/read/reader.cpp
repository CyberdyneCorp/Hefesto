// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): both functions fail with CORE-NOTIMPL-009.
//
// Owner: LVSREAD.

#include "hefesto/lvs/read/reader.hpp"

#include "hefesto/lvs/not_implemented.hpp"

namespace hefesto::lvs {

diag::Outcome<CellId> read_netlist(NetlistDb& /*db*/, const Source& /*source*/, Format /*format*/,
                                   const ReadOptions& /*options*/) {
  return not_implemented("lvs::read_netlist");
}

diag::Outcome<Format> detect_format(const Source& /*source*/) {
  return not_implemented("lvs::detect_format");
}

}  // namespace hefesto::lvs
