// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract_cell: ExtCell's pipeline for one cell (capability
// extract-hierarchy; openspec/changes/m7-extraction/design.md D5, D11):
// header, basic extraction (nodes, caps, devices), subtree interactions,
// arrays and path length. ExtractContext holds everything one run needs
// (style, options, yank buffers), with no globals, so per-cell parallelism
// can be added later (Open Question 8 (a)).
//
// M7 R4 (EXTCMD, tasks.md 8.1): extract_cell() runs extCellFile's flat part
// in extBasic's order (the device regions, the node pass with the substrate
// seeds, labels, duplicate-label feedback, substrate and coupling
// capacitance, the `device` property, `parameters`, `abstract`, the node,
// coupling and device records) and writes the header and `use` records.
// The subtree and array passes are EXTHIER's (extract_subtree, one marked
// call site); interactions() is EXTHIER's and still reports
// CORE-NOTIMPL-008.
//
// Owner: INTEGRATION (this header); EXTCMD (extract_cell, tasks.md 8.1);
// EXTHIER (interactions and the subtree passes).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::mask {
struct Progress;
}

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::extract {

class ExtWriter;

/// The scratch cells of one run's interaction yanks (ExtYank.c); owned by
/// the context, never registered in the cell table.
struct YankBuffers {
  std::unique_ptr<db::CellDef> cumulative;
  std::unique_ptr<db::CellDef> interaction;
};

/// One run's state.
struct ExtractContext {
  std::shared_ptr<const ExtStyle> style;
  ExtractOptions options;
  Overrides overrides;
  LengthLists length;
  compat::CompatSettings compat;
  const mask::Progress* progress = nullptr;
  YankBuffers yank;
  /// Added in M7 R4 (EXTCMD): the technology (planes, contacts, names);
  /// required.
  const tech::Technology* technology = nullptr;
  /// Added in M7 R4: Magic's console messages of the pass (TxPrintf and
  /// TxError, by code severity); nullptr = not reported.
  diag::DiagnosticSink* sink = nullptr;
  /// Added in M7 R4: the Tcl variable lookup of a `$NAME` substrate name
  /// (extSubsName, Q-EXTRACT-9); empty = no variable is defined.
  std::function<std::optional<std::string>(std::string_view)> variable;
  /// Added in M7 R4: the driver has already run the temporary unique pass
  /// and swapped the isolated substrate plane in for every cell it hands
  /// over (design D12's prepare phase); otherwise extract_cell() does both
  /// for the cell and undoes them before it returns.
  bool prepared = false;
  /// Added in M7 R4: the cells marked "don't use" (CDDONTUSE: empty cells
  /// whose children are all empty); their uses get no `use` record.
  std::unordered_set<const db::CellDef*> dont_use;
};

/// Added in M7 R4: a feedback area left by the pass (DBWFeedbackAdd with
/// scale 1), in the cell's coordinates.
struct CellFeedback {
  db::Rect area;
  std::string text;
  bool medium = false;  // STYLE_MEDIUMHIGHLIGHTS (else pale)

  friend bool operator==(const CellFeedback&, const CellFeedback&) = default;
};

/// What extracting one cell reports.
struct CellResult {
  std::int32_t errors = 0;
  std::int32_t warnings = 0;
  bool cancelled = false;
  /// Added in M7 R4: the feedback areas, in creation order.
  std::vector<CellFeedback> feedback;
  /// Added in M7 R4: with magic-compat ON, a record Magic writes without
  /// its newline (the line of a `device` cell property, ExtBasic.c:519-528,
  /// so the next record continues it); unterminate() applies it to the
  /// finished text. Proposed Q-EXTRACT-39; OFF ends the line.
  std::optional<std::string> unterminated;
};

/// Added in M7 R4: removes the newline after the first occurrence of
/// `line` (a whole line) in `text`.
void unterminate(std::string& text, std::string_view line);

/// The interaction areas of a cell (halo and step squares; ExtSubtree.c,
/// ExtInter.c).
struct InteractionAreas {
  std::vector<db::Rect> areas;
};

/// extCellFile: writes `cell`'s records to `writer` (header first). Fails
/// with EXTRACT-CMD-001 without a style or technology. A cancelled pass
/// (ctx.progress) returns with CellResult::cancelled set; the writer then
/// holds a partial file, which the caller abandons.
[[nodiscard]] diag::Outcome<CellResult> extract_cell(ExtractContext& ctx, db::CellDef& cell, ExtWriter& writer,
                                                     bool is_top);

[[nodiscard]] diag::Outcome<InteractionAreas> interactions(const db::CellDef& cell, const ExtStyle& style,
                                                           const Overrides& overrides);

}  // namespace hefesto::extract
