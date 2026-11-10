// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Feedback areas (capability ui-feedback-and-highlights, requirements
// "Feedback area model", "Feedback display", "Clearing feedback", "feedback
// command", "Feedback producers"): rectangles with a style and a text that
// commands and later subsystems (DRC, extraction, LVS) leave on a root cell
// for the user. Kept in insertion order and numbered 1..N for users.
// Producers call FeedbackList::add with the cell the area is in, its scale
// factor and one of the producer styles below; the list finds the displayed
// root, rounds the area outward to layout units and asks the highlight layer
// to show it at the next update.
// (Magic ref: dbwind/DBWfdback.c, commands/CmdFI.c:146-505)
//
// Owner: VIEW (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::view {

class HighlightLayer;

/// `feedback add` reads points in 1/20 layout unit (Magic's FEEDMAGNIFY).
inline constexpr std::int32_t kFeedbackMagnify = 20;

/// The highlight styles of utils/styles.h used by feedback.
inline constexpr std::size_t kStyleSolidHighlights = 1;
inline constexpr std::size_t kStyleMediumHighlights = 2;
inline constexpr std::size_t kStylePaleHighlights = 3;
inline constexpr std::size_t kStyleOutlineHighlights = 6;
inline constexpr std::size_t kStyleDottedHighlights = 7;

/// A feedback style: the display style-table index (the low-order style
/// bits) and Magic's TT_DIAGONAL / TT_SIDE / TT_DIRECTION flags.
/// `diagonal`: the triangle of the rectangle on `side` (right when set)
/// with `direction` set for `\` (lower-left to upper-right clear).
/// `side` without `diagonal`: a single line, `/` or, with `direction`, `\`.
struct FeedbackStyle {
  std::size_t index = kStylePaleHighlights;
  bool diagonal = false;
  bool side = false;
  bool direction = false;

  friend bool operator==(const FeedbackStyle&, const FeedbackStyle&) = default;
};

/// "Feedback producers": the default style of each producer.
namespace producer_style {
inline constexpr FeedbackStyle kExtraction{kStylePaleHighlights};
inline constexpr FeedbackStyle kExtractionHierarchical{kStyleMediumHighlights};
inline constexpr FeedbackStyle kMaskGeneration{kStylePaleHighlights};
inline constexpr FeedbackStyle kAntenna{kStylePaleHighlights};
inline constexpr FeedbackStyle kPaintError{kStyleMediumHighlights};
inline constexpr FeedbackStyle kNetlist{kStylePaleHighlights};
}  // namespace producer_style

/// The style of a split tile shown as feedback (`cif see`-style and
/// selection-based displays): the requested style with the tile's diagonal
/// flags (Magic ORs TT_DIAGONAL | TT_DIRECTION | TT_SIDE of the tile).
[[nodiscard]] FeedbackStyle tile_feedback_style(std::size_t index, const db::TileView& tile) noexcept;

struct FeedbackArea {
  const db::CellDef* root = nullptr;
  db::Rect scaled;         // in scaled coordinates of `root`
  std::int32_t scale = 1;  // f >= 1 scaled units per layout unit
  db::Rect area;           // rounded outward to layout units, clipped inside the plane
  FeedbackStyle style;
  /// Consecutive areas with identical text share one instance.
  std::shared_ptr<const std::string> text;

  [[nodiscard]] const std::string& message() const noexcept;
};

/// "Feedback area model" step 3: xtop' = ceil(xtop/f), xbot' = floor(xbot/f)
/// (and the same for y), then step 4: clipped to 10 units inside the plane.
[[nodiscard]] db::Rect round_feedback_area(const db::Rect& scaled, std::int32_t scale) noexcept;

/// The displayed root of `cell` (dbwfbGetTransform through DBSrRoots): the
/// cell itself when a window shows it, else the first ancestor reached
/// through its instances (most recent first, depth first) that a window
/// shows, with the transform from `cell` to it. Internal cells never count.
struct FeedbackRoot {
  const db::CellDef* root = nullptr;
  db::Transform to_root;
};
[[nodiscard]] std::optional<FeedbackRoot> find_feedback_root(const db::CellDef& cell, const WindowManager& windows);

/// "Feedback display": the area on screen in `mapping` (an extra division by
/// f, rounding to nearest; clipped to the window surface except for
/// diagonal styles). nullopt when clipped out of existence.
[[nodiscard]] std::optional<ScreenRect> feedback_screen_rect(const FeedbackArea& area, const ViewMapping& mapping);

class FeedbackList {
 public:
  /// DBWFeedbackAdd: `rect` is in scaled coordinates (factor `scale` >= 1)
  /// of `cell`. Returns false, adding nothing, when no ancestor of `cell` is
  /// shown in a window. The new area is shown at the next update through
  /// `highlights` (when given).
  bool add(const WindowManager& windows, const db::CellDef& cell, const db::Rect& rect, std::int32_t scale,
           FeedbackStyle style, std::string_view text, HighlightLayer* highlights = nullptr);

  /// DBWFeedbackClear: with no text every area, otherwise every area whose
  /// text contains `text`; the others keep their order. The removed areas
  /// are erased (one request per run of the same root). Returns the number
  /// removed. The `find` cursor is the command's (it resets it).
  std::size_t clear(std::optional<std::string_view> text, HighlightLayer* highlights = nullptr);
  /// Removes everything without touching the display (session reset).
  void clear() noexcept;

  [[nodiscard]] const std::vector<FeedbackArea>& areas() const noexcept { return areas_; }
  [[nodiscard]] std::size_t size() const noexcept { return areas_.size(); }

  /// The `feedback find` cursor (1-based area last shown; 0 = none yet).
  [[nodiscard]] std::size_t find_cursor() const noexcept { return find_cursor_; }
  void set_find_cursor(std::size_t n) noexcept { find_cursor_ = n; }

 private:
  std::vector<FeedbackArea> areas_;
  std::size_t find_cursor_ = 0;
};

}  // namespace hefesto::view
