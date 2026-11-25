// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// UniqueRenamer: ExtUnique.c's renaming of duplicate labels (capability
// extract-nodes-and-labels "Unique naming of duplicate labels";
// openspec/changes/m7-extraction design.md D8, D12). Q-EXTRACT-23
// (replicate) and Q-EXTRACT-6 (fix: the temporary labels are reverted in
// every renamed cell, the top cell included).
//
// A renamed label is removed and appended again with its new text (as
// DBRemoveLabel + DBPutFontLabel), so it moves to the end of the cell's
// label list; revert() truncates each temporary label at its last `_` and
// gives a renamed port the port number of another label with the restored
// text (ExtRevertUniqueCell), then restores the modified flags the cell
// had before its first rename.
//
// Owner: INTEGRATION (this header); EXTNODE (the implementation).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::extract {

/// EXT_UNIQ_* (extract.h): the `extract unique` options, in Magic's order.
enum class UniqueMode : std::uint8_t { all, tagged, noports, notopports };

class ExtStyle;

/// Added in M7 R3: what apply() needs besides the cell and the mode.
struct UniqueContext {
  const tech::Technology* technology = nullptr;
  const ExtStyle* style = nullptr;
  /// `extract do unique` (EXT_UNIQ_TEMP, or EXT_UNIQ_TEMP_NOPORTS with
  /// mode noports/notopports): the renamed labels are flagged for revert().
  bool temporary = false;
  /// The substrate marking of extFindNodes(subonly) (EXTDEV's seeds);
  /// substrate_only is forced on.
  NodePassOptions substrate;
  compat::CompatSettings compat;
  /// "Processing <cell>" (EXTRACT-NODE-002) and "<cell>: <n> warnings"
  /// (EXTRACT-NODE-003); nullptr = not reported.
  diag::DiagnosticSink* sink = nullptr;
};

class UniqueRenamer {
 public:
  /// Renames the duplicate labels of `cell` (temporary `_uqN` labels when
  /// the run asked for it); the renames are remembered for revert().
  /// Without a style and technology (this form) it fails with
  /// EXTRACT-NODE-004.
  [[nodiscard]] diag::Status apply(db::CellDef& cell, UniqueMode mode);
  /// Added in M7 R3: extUniqueCell with its inputs.
  [[nodiscard]] diag::Status apply(db::CellDef& cell, UniqueMode mode, const UniqueContext& context);
  /// Restores every temporary label apply() made, in every cell.
  void revert();

  /// Added in M7 R3: the warnings of the last apply() (one per feedback
  /// entry) and the feedback entries of all apply() calls since the last
  /// revert().
  [[nodiscard]] std::size_t warnings() const noexcept { return warnings_; }
  [[nodiscard]] const std::vector<NodeFeedback>& feedback() const noexcept { return feedback_; }

 private:
  struct Rename {
    db::CellDef* cell = nullptr;
    std::string original;
    std::string renamed;
    // Added in M7 R3: the renamed label (to find it again), whether it is
    // temporary, and the cell's flags before its first rename.
    db::Rect rect;
    tech::TileType type = 0;
    bool temporary = false;
    bool was_modified = false;
    bool needed_timestamp = false;
  };
  std::vector<Rename> renames_;
  std::vector<NodeFeedback> feedback_;
  std::size_t warnings_ = 0;

  friend class UniquePass;
};

}  // namespace hefesto::extract
