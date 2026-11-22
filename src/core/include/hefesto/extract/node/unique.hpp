// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// UniqueRenamer: ExtUnique.c's renaming of duplicate labels (capability
// extract-nodes-and-labels "Unique naming of duplicate labels";
// openspec/changes/m7-extraction design.md D8, D12). Q-EXTRACT-23
// (replicate) and Q-EXTRACT-6 (fix: the temporary labels are reverted in
// every renamed cell, the top cell included).
//
// Scaffold state (tasks.md 3.3): apply() reports CORE-NOTIMPL-008 and
// revert() does nothing until EXTNODE lands them (tasks.md 5.4).
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::extract {

/// EXT_UNIQ_* (extract.h): the `extract unique` options, in Magic's order.
enum class UniqueMode : std::uint8_t { all, tagged, noports, notopports };

class UniqueRenamer {
 public:
  /// Renames the duplicate labels of `cell` (temporary `_uqN` labels when
  /// the run asked for it); the renames are remembered for revert().
  [[nodiscard]] diag::Status apply(db::CellDef& cell, UniqueMode mode);
  /// Restores every temporary label apply() made, in every cell.
  void revert();

 private:
  struct Rename {
    db::CellDef* cell = nullptr;
    std::string original;
    std::string renamed;
  };
  std::vector<Rename> renames_;
};

}  // namespace hefesto::extract
