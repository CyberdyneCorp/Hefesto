// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DRC reporting for the `drc` command and other consumers (the desktop
// browser through `drc listall why`, the M11 agent loop): why-text
// substitution, the ignore list, error counts, `drc find` and `drc why`
// (drc-commands; openspec/changes/m3-drc/design.md D9).
//
//   - why_text() is drcSubstitute: %d, %c and %a replaced by the rule's
//     compiled distances in the current units (microns with a suffix for
//     the default units); any other `%x` and a trailing `%` are copied
//     literally in both magic-compat settings (Q-DRC-16).
//   - count_errors() is DRCCount: error tiles of the root's error plane,
//     and in list-all mode of every loaded cell under the area, each cell
//     once, listed by name (X-1).
//   - why_errors() is DRCWhy / DRCWhyAll: rechecks the area (it does not
//     read the error plane), keeps errors overlapping the area with
//     positive area, resolves why-4 errors in the subtree under them and
//     groups the reports by why index (distinct mode, ignore list applied)
//     or by substituted text (all mode, `drc listall why`).
//   - find_error() is the `drc find` loop over DRCFind: the persistent
//     counter (Q-DRC-15), the text filter over the scanned why range
//     (Q-DRC-17), wrapping, and the why report of each error area tried.
//
// The recheck is ENGINE's drc::interaction_check(cell, area, area) into an
// ErrorSink (engine_recheck); callers and tests may pass their own.
//
// (Magic ref: drc/DRCmain.c drcSubstitute, drcPrintError, drcListError,
// drcListallError, DRCWhy, DRCWhyAll, drcWhyFunc, DRCCount, drcCountFunc,
// DRCFind, drcFindFunc; commands/CmdCD.c CmdDrc `find` and `ignore`;
// dbwind/DBWprocs.c dbwPrintValue0)
//
// Owner: COMMANDS (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/cmd/units.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/drc/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::drc {

/// drcSubstitute: the why text of `rule` with %d, %c and %a replaced by its
/// compiled dist / cdist / cdist-as-area formatted in `units` (the default
/// units print microns with `um`), each value cut to 19 characters; mask-
/// layer rules (flags::kCifRule) convert their mask units first
/// (drc-commands "Why-text substitution", Q-DRC-16 for other escapes).
[[nodiscard]] std::string why_text(const RuleSet& rules, const Rule& rule, const cmd::UnitsState& units,
                                   const cmd::CoordinateUnits& scale);

/// why_text() in the session's units and output scale.
[[nodiscard]] std::string why_text(const editor::EditorState& editor, const Rule& rule);

/// The `drc ignore` list (DRCIgnoreRules, a linked list in Magic): why
/// indices, most recently added first; adding an index twice lists it twice.
class IgnoreList {
 public:
  /// Adds `why` at the front.
  void add(WhyIndex why) { items_.insert(items_.begin(), why); }
  void clear() noexcept { items_.clear(); }
  [[nodiscard]] bool contains(WhyIndex why) const noexcept;
  [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
  /// Most recently added first.
  [[nodiscard]] const std::vector<WhyIndex>& items() const noexcept { return items_; }

  friend bool operator==(const IgnoreList&, const IgnoreList&) = default;

 private:
  std::vector<WhyIndex> items_;
};

/// The why indices whose text contains `text` (case-sensitive) in the range
/// `drc ignore` and `drc find <text>` scan, most recent (highest) first as
/// they would be added: 1 up to but excluding the last index with
/// magic-compat ON, up to and including it OFF (Q-DRC-17).
[[nodiscard]] IgnoreList matching_whys(const WhyTable& why, std::string_view text, bool magic_compat);

/// One cell's error-tile count (DRCCountList entry).
struct CellErrorCount {
  const db::CellDef* cell = nullptr;
  std::size_t count = 0;

  friend bool operator==(const CellErrorCount&, const CellErrorCount&) = default;
};

/// DRCCount: the error tiles (non-space tiles of the error plane over the
/// cell's bounding box) of `root` and, with `recurse`, of every cell
/// reached through uses overlapping `area` (root coordinates; unloaded
/// cells are counted but not descended into), each cell once. Only cells
/// with errors, sorted by name (X-1). Nothing when `root` is not loaded.
[[nodiscard]] std::vector<CellErrorCount> count_errors(const db::CellDef& root, const db::Rect& area, bool recurse);

/// The error tiles of `cell`'s own error plane over its bounding box in
/// area-enumeration order (what DRCFind numbers from 1).
[[nodiscard]] std::vector<db::Rect> error_tiles(const db::CellDef& cell);

/// DRCInteractionCheck(cell, area, area, sink): every error the check of
/// `area` in `cell` (cell coordinates) reports.
using RecheckFn =
    std::function<void(editor::EditorState& editor, const db::CellDef& cell, const db::Rect& area, ErrorSink& sink)>;

/// The default recheck: drc::interaction_check(editor, cell, area, area, sink).
void engine_recheck(editor::EditorState& editor, const db::CellDef& cell, const db::Rect& area, ErrorSink& sink);

/// While alive, the recheck used on this thread when none is given (tests of
/// the `drc why` and `drc find` commands stand in for the engine with it).
class ScopedRecheck {
 public:
  explicit ScopedRecheck(RecheckFn recheck);
  ~ScopedRecheck();
  ScopedRecheck(const ScopedRecheck&) = delete;
  ScopedRecheck& operator=(const ScopedRecheck&) = delete;

 private:
  RecheckFn previous_;
};

/// One why group of a `drc why` report: the substituted text and the
/// error rectangles (root coordinates, discovery order) reported with it.
struct WhyGroup {
  std::string text;
  WhyIndex why = 0;
  std::vector<db::Rect> areas;

  friend bool operator==(const WhyGroup&, const WhyGroup&) = default;
};

struct WhyOptions {
  /// `drc listall why` (DRCWhyAll): every error, grouped by substituted
  /// text, ignore list not applied. Otherwise DRCWhy: one group per why
  /// index, ignored indices skipped.
  bool all = false;
  /// DRCWhy's `findonly`: only the indices of `filter` are reported.
  bool only_filtered = false;
  /// The ignore list (or, with only_filtered, the searched list); nullptr =
  /// the session's ignore list.
  const IgnoreList* filter = nullptr;
  /// Empty: the ScopedRecheck in force, else engine_recheck().
  RecheckFn recheck;
};

/// DRCWhy / DRCWhyAll over `area` (root coordinates) of `root`: the groups
/// in first-discovery order; empty when nothing was reported ("No errors
/// found." is the command's text).
[[nodiscard]] std::vector<WhyGroup> why_errors(editor::EditorState& editor, const db::CellDef& root,
                                               const db::Rect& area, const WhyOptions& options = {});

/// One error area `drc find` moved the box to and its why report.
struct FindAttempt {
  std::int64_t index = 0;  // the `Error area #N:` number
  db::Rect area;           // root coordinates
  std::vector<WhyGroup> why;

  friend bool operator==(const FindAttempt&, const FindAttempt&) = default;
};

enum class FindStatus : unsigned char {
  found,         // the last attempt reported a why text
  no_errors,     // "There are no errors in <cell>." (Tcl result 0)
  out_of_range,  // "There aren't that many errors" (Tcl result -1)
  wrapped,       // the search came back to its first area without a match
};

struct FindResult {
  FindStatus status = FindStatus::no_errors;
  /// Every area tried, in order (each moves the box and prints its number).
  std::vector<FindAttempt> attempts;

  friend bool operator==(const FindResult&, const FindResult&) = default;
};

/// The `drc find [nth|text]` loop over `root`'s own error plane: `argument`
/// nullopt (next), an integer (that error) or a text (next error whose why
/// matches, Q-DRC-17 range). Updates DrcSession::find_counter (initial
/// value per Q-DRC-15). `recheck` empty: as WhyOptions::recheck.
[[nodiscard]] FindResult find_error(editor::EditorState& editor, const db::CellDef& root,
                                    std::optional<std::string_view> argument, const RecheckFn& recheck = {});

}  // namespace hefesto::drc
