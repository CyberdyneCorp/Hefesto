// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold (openspec/changes/m5-mask-gds-cif/design.md D3/D4): the symbol
// table works; cif::read fails with CORE-NOTIMPL-005 until CIF ports the
// parser (tasks.md section 11). Owner: CIF.

#include "hefesto/cif/read/read.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::cif {

db::CellDef* SymbolTable::find(std::int64_t number) const noexcept {
  const auto it = cells_.find(number);
  return it == cells_.end() ? nullptr : it->second;
}

void SymbolTable::bind(std::int64_t number, db::CellDef& cell) {
  cells_[number] = &cell;
}

void SymbolTable::delete_from(std::int64_t number, bool magic_compat) {
  if (magic_compat) {
    cells_.erase(number);
    return;
  }
  cells_.erase(cells_.lower_bound(number), cells_.end());
}

cmd::CommandResult read(editor::EditorState& /*editor*/, const platform::Path& /*path*/,
                        const CifReadOptions& /*options*/, const mask::Progress* /*progress*/) {
  return cmd::not_implemented("cif read");
}

}  // namespace hefesto::cif
