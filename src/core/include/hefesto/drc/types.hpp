// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The compiled DRC rule table and the error sink: the shared types between
// the rule compiler (RULES, drc/rules/), the check kernel (CHECK,
// drc/check/), the engine (ENGINE, drc/engine/) and the `drc` command
// (COMMANDS, drc/commands/) (openspec/changes/m3-drc/design.md D4/D5).
//
//   - Rule is Magic's DRCCookie (drc/drc.h): one primitive rule of an edge
//     pair (A, B), A the type on the left/bottom of the edge, B on the
//     right/top. A trigger rule (flag kTrigger) is always immediately
//     followed by the rule it triggers in the pair's span.
//   - RuleSet is the frozen table of one loaded DRC style (DRCStyle):
//     per-pair rule spans in a CSR layout indexed by A * type_count() + B
//     (design D5), the why-text table, the exception names, halo and step
//     size, option flags, the DRC paint table, the exact-overlap types and
//     the RULES-private details (StyleDetails, drc/rules/details.hpp).
//     It is immutable: `drc style`, `tech load`, grid rescale and a
//     cifoutput style change build a new one (DrcSession::rules), so the
//     background worker always reads one consistent table.
//   - RuleSetBuilder assembles a RuleSet from rules already in final order.
//     RULES calls it after ordering, pruning and covered-rule elimination;
//     CHECK, ENGINE and COMMANDS tests build small rule sets with it without
//     the tech-file parser.
//   - ErrorSink receives every error rectangle the kernel finds
//     (drc-rule-semantics "Error rectangle reporting"); the engine paints a
//     temporary error plane, `drc why` collects why indices, `*flatcheck`
//     counts.
//
// (Magic ref: drc/drc.h DRCCookie, DRCStyle, DRC_* flags, DRC_*_TAG)
//
// Owner: INTEGRATION (shared file); the contents are filled by RULES (rule
// table, why table, details) and used by CHECK, ENGINE and COMMANDS. A
// change to this header is an open item for INTEGRATION.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::drc {

// --- Rule flags (drcc_flags, drc/drc.h) ----------------------------------

using RuleFlags = std::uint32_t;

namespace flags {
inline constexpr RuleFlags kForward = 0x0000;        // DRC_FORWARD: left to right / bottom to top
inline constexpr RuleFlags kReverse = 0x0001;        // DRC_REVERSE
inline constexpr RuleFlags kBothCorners = 0x0002;    // DRC_BOTHCORNERS
inline constexpr RuleFlags kTrigger = 0x0004;        // DRC_TRIGGER: the next rule is the triggered one
inline constexpr RuleFlags kBends = 0x0008;          // DRC_BENDS (maxwidth bend_ok / bend_illegal)
inline constexpr RuleFlags kOutside = 0x0010;        // DRC_OUTSIDE
inline constexpr RuleFlags kArea = 0x0020;           // DRC_AREA
inline constexpr RuleFlags kOffGrid = 0x0040;        // DRC_OFFGRID
inline constexpr RuleFlags kMaxWidth = 0x0080;       // DRC_MAXWIDTH
inline constexpr RuleFlags kMaxWidthBoth = 0x0100;   // DRC_MAXWIDTH_BOTH
inline constexpr RuleFlags kRectSize = 0x0200;       // DRC_RECTSIZE
inline constexpr RuleFlags kAngles45 = 0x0400;       // DRC_ANGLES_45
inline constexpr RuleFlags kAngles90 = 0x0800;       // DRC_ANGLES_90
inline constexpr RuleFlags kSplitTile = 0x1000;      // DRC_SPLITTILE
inline constexpr RuleFlags kRunLength = 0x2000;      // DRC_RUNLENGTH: cdist is a run length
inline constexpr RuleFlags kManhattan = 0x4000;      // DRC_MANHATTAN
inline constexpr RuleFlags kCifRule = 0x8000;        // DRC_CIFRULE
inline constexpr RuleFlags kDerivedEdge = 0x10000;   // DRC_DERIVED_EDGE
inline constexpr RuleFlags kDerivedTarget = 0x20000; // DRC_DERIVED_TARGET
/// DRC_NONSTANDARD: rules that do not use the standard edge processing.
inline constexpr RuleFlags kNonStandard = kArea | kMaxWidth | kRectSize | kAngles90 | kOffGrid;
}  // namespace flags

// --- Why-text indices (drcc_tag) ------------------------------------------

/// Index into WhyTable; 0 is "no text".
using WhyIndex = std::uint32_t;

/// The predefined why texts 1-5 (drc-rule-language "Why-text table";
/// DRC_*_TAG in drc/drc.h).
namespace why {
inline constexpr WhyIndex kArrayOverlap = 1;    // "This layer can't abut or partially overlap between array elements"
inline constexpr WhyIndex kOverlap = 2;         // "Can't overlap those layers"
inline constexpr WhyIndex kSubcellOverlap = 3;  // "This layer can't abut or partially overlap between subcells"
inline constexpr WhyIndex kInSubcell = 4;       // "See error definition in the subcell"
inline constexpr WhyIndex kOffGrid = 5;         // "This position does not align with the manufacturing grid"
inline constexpr WhyIndex kFirstUser = 6;
}  // namespace why

// --- Exception tags (drcc_exception) --------------------------------------

/// DRC_EXCEPTION_NONE: the rule has no exception or exemption.
inline constexpr std::uint8_t kNoException = 0xff;
/// DRC_EXCEPTION_MASK: set for an exemption, clear for an exception; the
/// low 7 bits index RuleSet::exception_names().
inline constexpr std::uint8_t kExemptionBit = 0x80;

/// One primitive rule (DRCCookie). Distances are in internal units after
/// scaling (drc-rule-language "Scale factor and distance units"); for
/// kArea rules `cdist` is an area in internal units squared and `dist` the
/// horizon.
struct Rule {
  std::int32_t dist = 0;           // drcc_dist: extent of the rule from the edge
  std::uint8_t dist_mod = 0;       // drcc_mod: Magic's one-byte remainder (Q-DRC-1, magic-compat ON)
  std::int32_t cdist = 0;          // drcc_cdist: corner extension (run length with kRunLength; area with kArea)
  std::uint8_t cdist_mod = 0;      // drcc_cmod
  tech::TypeMask ok;               // drcc_mask: legal types on the checked side
  tech::TypeMask corner;           // drcc_corner: types that trigger the corner check
  RuleFlags flags = flags::kForward;
  std::uint8_t exception = kNoException;  // drcc_exception
  tech::PlaneId edge_plane = 0;    // drcc_edgeplane: plane of the edge
  tech::PlaneId check_plane = 0;   // drcc_plane: plane on which `ok` is checked
  WhyIndex why = 0;                // drcc_tag
  /// The tech-file values before scaling (Q-DRC-1 with magic-compat OFF
  /// recomputes from them); 0 when not applicable.
  std::int32_t file_dist = 0;
  std::int32_t file_cdist = 0;

  [[nodiscard]] bool has(RuleFlags f) const noexcept { return (flags & f) == f && f != 0; }
  [[nodiscard]] bool reverse() const noexcept { return (flags & flags::kReverse) != 0; }
  [[nodiscard]] bool nonstandard() const noexcept { return (flags & flags::kNonStandard) != 0; }

  friend bool operator==(const Rule&, const Rule&) = default;
};

/// The why-text table of a style: index 1..5 predefined, then the style's
/// strings in order of first registration; identical strings share an index.
class WhyTable {
 public:
  /// A table holding the five predefined texts.
  WhyTable();

  /// The index of `text`, appending it when new (drcWhyCreate).
  WhyIndex intern(std::string_view text);
  /// The text of `index`; empty for 0 or an unknown index.
  [[nodiscard]] std::string_view text(WhyIndex index) const noexcept;
  /// The index of `text`, or 0 when absent.
  [[nodiscard]] WhyIndex find(std::string_view text) const noexcept;
  /// Number of entries including the predefined ones (DRCWhySize - 1).
  [[nodiscard]] std::size_t size() const noexcept { return texts_.size(); }

  friend bool operator==(const WhyTable&, const WhyTable&) = default;

 private:
  std::vector<std::string> texts_;  // texts_[i] is index i + 1
};

/// Halo of a style (drc-rule-language "Halo and step size"): the tech-file
/// part and the mask-layer (cif rule) part kept apart so both Q-DRC-2 modes
/// can be computed; `value` is the halo in effect (internal units).
struct Halo {
  std::int32_t value = 0;
  std::int32_t tech_contribution = 0;  // max tech-file-unit contribution (before D'/N')
  std::int32_t mask_contribution = 0;  // max cif-rule contribution (internal units)

  friend bool operator==(const Halo&, const Halo&) = default;
};

/// Style option flags (DRCFlags; drc-rule-language "option statement").
struct StyleOptions {
  bool wide_width_noninclusive = false;  // `option wide-width-noninclusive`

  friend bool operator==(const StyleOptions&, const StyleOptions&) = default;
};

/// The reduced conversion factor of a style: distances in tech-file units
/// times D'/N' give internal units (drc-rule-language "Scale factor and
/// distance units"; DRCScaleFactorN/D combined with the grid scale).
struct ScaleFactor {
  std::int32_t n = 1;
  std::int32_t d = 1;

  friend bool operator==(const ScaleFactor&, const ScaleFactor&) = default;
};

/// RULES-private parts of a compiled style (mask-layer rule records,
/// printrules and rulestats data, the per-rule restore data of Q-DRC-1,
/// ...), defined in hefesto/drc/rules/details.hpp. CHECK and ENGINE never
/// need it; COMMANDS reads it through the RULES query functions.
struct StyleDetails;

class RuleSetBuilder;

/// The frozen rule table of one DRC style. Cheap to share as
/// std::shared_ptr<const RuleSet>.
class RuleSet {
 public:
  /// An empty table (no rules) over `type_count` tile types.
  explicit RuleSet(std::size_t type_count = tech::kMaxTileTypes);
  ~RuleSet();
  RuleSet(const RuleSet&);
  RuleSet& operator=(const RuleSet&);
  RuleSet(RuleSet&&) noexcept;
  RuleSet& operator=(RuleSet&&) noexcept;

  /// Name of the style ("default" for the implicit style).
  [[nodiscard]] const std::string& style_name() const noexcept { return style_name_; }
  /// Number of tile types the table is indexed by (pairs are A * N + B).
  [[nodiscard]] std::size_t type_count() const noexcept { return type_count_; }

  /// The rules of edge pair (left/bottom type a, right/top type b) in check
  /// order; empty for types out of range.
  [[nodiscard]] std::span<const Rule> rules(tech::TileType a, tech::TileType b) const noexcept;
  /// Every rule of every pair, pair by pair (A-major).
  [[nodiscard]] std::span<const Rule> all_rules() const noexcept { return rules_; }
  /// Whether any rule exists for an edge between a and b.
  [[nodiscard]] bool has_rules(tech::TileType a, tech::TileType b) const noexcept { return !rules(a, b).empty(); }

  [[nodiscard]] const WhyTable& why() const noexcept { return why_; }
  /// Exception/exemption names in order of first use (at most 126); a
  /// rule's exception tag & ~kExemptionBit indexes this list, the property
  /// is "MASKHINTS_" + name.
  [[nodiscard]] std::span<const std::string> exception_names() const noexcept { return exception_names_; }

  [[nodiscard]] const Halo& halo() const noexcept { return halo_; }
  /// DRCStepSize in internal units.
  [[nodiscard]] std::int32_t step_size() const noexcept { return step_size_; }
  [[nodiscard]] const StyleOptions& options() const noexcept { return options_; }
  [[nodiscard]] const ScaleFactor& scale() const noexcept { return scale_; }

  /// The DRC paint table (DRCPaintTable[plane][paint][have]) used to
  /// flatten for interaction checks (drc-rule-language "Overlap rules and
  /// the DRC paint table"): the row of `paint` on `plane`, over every
  /// `have`. Planes or types out of range give the identity "paint wins"
  /// row of an empty style.
  [[nodiscard]] const db::PaintRow& paint_row(tech::PlaneId plane, tech::TileType paint) const noexcept;
  [[nodiscard]] tech::TileType paint_result(tech::PlaneId plane, tech::TileType have,
                                            tech::TileType paint) const noexcept {
    return paint_row(plane, paint)[have];
  }
  /// DRCExactOverlapTypes.
  [[nodiscard]] const tech::TypeMask& exact_overlap_types() const noexcept { return exact_overlap_; }

  /// RULES-private details; nullptr for a table built without them (tests).
  [[nodiscard]] const StyleDetails* details() const noexcept { return details_.get(); }

 private:
  friend class RuleSetBuilder;

  std::string style_name_{"default"};
  std::size_t type_count_;
  std::vector<Rule> rules_;               // CSR values
  std::vector<std::uint32_t> offsets_;    // CSR offsets, type_count_^2 + 1 entries (empty: no rules)
  WhyTable why_;
  std::vector<std::string> exception_names_;
  Halo halo_;
  std::int32_t step_size_ = 64;
  StyleOptions options_;
  ScaleFactor scale_;
  std::vector<db::PaintRow> paint_rows_;  // [plane * kMaxTileTypes + paint]; empty: identity
  tech::TypeMask exact_overlap_;
  std::shared_ptr<const StyleDetails> details_;
};

/// Builds a RuleSet. Rules are added pair by pair in final check order
/// (ordering, pruning and covered-rule elimination are the caller's job);
/// a trigger rule must be added immediately before the rule it triggers.
class RuleSetBuilder {
 public:
  explicit RuleSetBuilder(std::size_t type_count = tech::kMaxTileTypes);

  RuleSetBuilder& style_name(std::string name);
  /// Appends `rule` to the list of edge pair (a, b). Out-of-range types are ignored.
  RuleSetBuilder& add(tech::TileType a, tech::TileType b, const Rule& rule);
  /// The why-text table to intern texts in (starts with the predefined five).
  [[nodiscard]] WhyTable& why() noexcept { return why_; }
  /// Shorthand: interns `text` and returns its index.
  WhyIndex why(std::string_view text) { return why_.intern(text); }
  RuleSetBuilder& exception_names(std::vector<std::string> names);
  RuleSetBuilder& halo(const Halo& halo);
  RuleSetBuilder& step_size(std::int32_t step);
  RuleSetBuilder& options(const StyleOptions& options);
  RuleSetBuilder& scale(const ScaleFactor& scale);
  /// Sets DRCPaintTable[plane][paint][have] = result for every `have` of `row`.
  RuleSetBuilder& paint_row(tech::PlaneId plane, tech::TileType paint, const db::PaintRow& row);
  RuleSetBuilder& exact_overlap_types(const tech::TypeMask& types);
  RuleSetBuilder& details(std::shared_ptr<const StyleDetails> details);

  /// Freezes the table. The builder is left empty.
  [[nodiscard]] std::shared_ptr<const RuleSet> build();

 private:
  std::size_t type_count_;
  std::map<std::size_t, std::vector<Rule>> pairs_;  // a * type_count_ + b -> rules (sparse)
  RuleSet set_;
  WhyTable why_;
};

/// The row of a paint table where painting `paint` over any type gives `paint`.
[[nodiscard]] db::PaintRow identity_paint_row(tech::TileType paint) noexcept;

// --- Error reporting -----------------------------------------------------

/// Counters of the basic checker for `drc statistics` (DRCstat* in
/// drc/DRCmain.c). The kernel increments tiles, edges, rules, slow; the
/// engine increments squares, interactions, interaction/cif/array tiles.
struct CheckStatistics {
  std::uint64_t squares = 0;            // DRCstatSquares: step squares processed
  std::uint64_t tiles = 0;              // DRCstatTiles: tiles processed by the basic checker
  std::uint64_t edges = 0;              // DRCstatEdges: edge pieces processed
  std::uint64_t rules = 0;              // DRCstatRules: constraint areas checked
  std::uint64_t slow = 0;               // DRCstatSlow: multi-tile constraints
  std::uint64_t interactions = 0;       // DRCstatInteractions
  std::uint64_t interaction_tiles = 0;  // DRCstatIntTiles
  std::uint64_t cif_tiles = 0;          // DRCstatCifTiles
  std::uint64_t array_tiles = 0;        // DRCstatArrayTiles

  CheckStatistics& operator+=(const CheckStatistics& other) noexcept;
  friend bool operator==(const CheckStatistics&, const CheckStatistics&) = default;
};

/// Receives the error rectangles of a check (dCD_function in Magic). The
/// kernel calls report() once per error rectangle, already clipped to the
/// check's clip area L, with the rule that failed (its `why` names the
/// text). Implementations: the engine's temporary error plane, `drc why`
/// collection, `*flatcheck` counting.
class ErrorSink {
 public:
  ErrorSink() = default;
  ErrorSink(const ErrorSink&) = delete;
  ErrorSink& operator=(const ErrorSink&) = delete;
  virtual ~ErrorSink() = default;

  /// Records one error and counts it, then calls on_error().
  void report(const db::Rect& area, const Rule& rule) {
    ++count_;
    on_error(area, rule);
  }
  /// Number of report() calls so far (*dCD_errors).
  [[nodiscard]] std::size_t count() const noexcept { return count_; }
  /// Statistics counters the kernel adds to; nullptr = not collected.
  [[nodiscard]] CheckStatistics* statistics() const noexcept { return statistics_; }
  void set_statistics(CheckStatistics* statistics) noexcept { statistics_ = statistics; }

 protected:
  virtual void on_error(const db::Rect& area, const Rule& rule) = 0;

 private:
  std::size_t count_ = 0;
  CheckStatistics* statistics_ = nullptr;
};

/// An ErrorSink that keeps every report in order (tests, `drc why`).
class CollectingErrorSink final : public ErrorSink {
 public:
  struct Error {
    db::Rect area;
    Rule rule;
    friend bool operator==(const Error&, const Error&) = default;
  };
  [[nodiscard]] const std::vector<Error>& errors() const noexcept { return errors_; }

 protected:
  void on_error(const db::Rect& area, const Rule& rule) override { errors_.push_back(Error{area, rule}); }

 private:
  std::vector<Error> errors_;
};

}  // namespace hefesto::drc
