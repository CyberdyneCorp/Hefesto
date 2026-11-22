// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of extract/hier (extract_cell.hpp): CORE-NOTIMPL-008 until
// EXTHIER lands them (openspec/changes/m7-extraction tasks.md, EXTHIER).
// EXTHIER replaces this file with its own sources.
//
// Owner: EXTHIER (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"

namespace hefesto::extract {

diag::Outcome<CellResult> extract_cell(ExtractContext& /*ctx*/, db::CellDef& /*cell*/, ExtWriter& /*writer*/,
                                       bool /*is_top*/) {
  return cmd::m7_not_implemented_diagnostic("extract cell pipeline");
}

diag::Outcome<InteractionAreas> interactions(const db::CellDef& /*cell*/, const ExtStyle& /*style*/,
                                             const Overrides& /*overrides*/) {
  return cmd::m7_not_implemented_diagnostic("extract interactions");
}

}  // namespace hefesto::extract
