// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Feedback area model (ui-feedback-and-highlights "Feedback area model",
// "Feedback display", "Clearing feedback"). Owner: VIEW.
// Behavior follows Magic 8.3.694 dbwind/DBWfdback.c, Copyright (C) 1985,
// 1990 Regents of the University of California, used under Magic's
// permissive license (see THIRD_PARTY_NOTICES.md).

#include "hefesto/view/feedback.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include "hefesto/view/highlights.hpp"

namespace hefesto::view {

namespace {

/// C integer division, as Magic's rounding code uses it.
db::Coord ceil_div(db::Coord v, std::int32_t f) noexcept {
  return v > 0 ? static_cast<db::Coord>((std::int64_t{v} + f - 1) / f) : v / f;
}

db::Coord floor_div(db::Coord v, std::int32_t f) noexcept {
  return v > 0 ? v / f : static_cast<db::Coord>((std::int64_t{v} - f + 1) / f);
}

bool shown(const db::CellDef& def, const WindowManager& windows) {
  bool found = false;
  windows.for_each([&](LayoutWindow& w) { found = found || w.root() == &def; });
  return found;
}

/// DBSrRoots: depth first through the parents, most recent instance first.
std::optional<FeedbackRoot> search_roots(const db::CellDef& def, const db::Transform& to_def,
                                         const WindowManager& windows,
                                         std::unordered_set<const db::CellDef*>& seen) {
  if (!seen.insert(&def).second || def.flags().internal) {
    return std::nullopt;
  }
  if (shown(def, windows)) {
    return FeedbackRoot{&def, to_def};
  }
  for (const db::CellUse* use : def.instances()) {
    if (use == nullptr || use->parent == nullptr) {
      continue;
    }
    if (auto found = search_roots(*use->parent, db::compose(use->transform, to_def), windows, seen)) {
      return found;
    }
  }
  return std::nullopt;
}

/// x scaled by f, rounded to nearest pixel through the window's origin
/// (DBWFeedbackRedraw's arithmetic).
std::int32_t to_pixel(db::Coord v, std::int64_t world_lo, std::int64_t scale, std::int64_t origin, std::int32_t f) {
  const std::int64_t half = f / 2;
  const std::int64_t x = (std::int64_t{v} - world_lo) * scale + half;
  return static_cast<std::int32_t>((x / f + origin) >> 16);
}

}  // namespace

FeedbackStyle tile_feedback_style(std::size_t index, const db::TileView& tile) noexcept {
  FeedbackStyle s{index};
  if (tile.split) {
    s.diagonal = true;
    s.side = tile.right_side;
    s.direction = tile.diagonal == db::Diagonal::nw_se;
  }
  return s;
}

const std::string& FeedbackArea::message() const noexcept {
  static const std::string kEmpty;
  return text ? *text : kEmpty;
}

db::Rect round_feedback_area(const db::Rect& scaled, std::int32_t scale) noexcept {
  const std::int32_t f = std::max(scale, 1);
  db::Rect r{floor_div(scaled.x0, f), floor_div(scaled.y0, f), ceil_div(scaled.x1, f), ceil_div(scaled.y1, f)};
  constexpr db::Coord lo = db::kPlaneMin + 10;  // 10 units inside TiPlaneRect
  constexpr db::Coord hi = db::kPlaneMax - 10;
  r.x0 = std::clamp(r.x0, lo, hi);
  r.y0 = std::clamp(r.y0, lo, hi);
  r.x1 = std::clamp(r.x1, lo, hi);
  r.y1 = std::clamp(r.y1, lo, hi);
  return r;
}

std::optional<FeedbackRoot> find_feedback_root(const db::CellDef& cell, const WindowManager& windows) {
  std::unordered_set<const db::CellDef*> seen;
  return search_roots(cell, db::kIdentity, windows, seen);
}

std::optional<ScreenRect> feedback_screen_rect(const FeedbackArea& area, const ViewMapping& m) {
  const std::int64_t f = std::max(area.scale, 1);
  const std::int64_t wx0 = std::int64_t{m.surface.x0} * f;
  const std::int64_t wy0 = std::int64_t{m.surface.y0} * f;
  const std::int64_t wx1 = std::int64_t{m.surface.x1} * f;
  const std::int64_t wy1 = std::int64_t{m.surface.y1} * f;
  db::Rect r = area.scaled;
  if (!area.style.diagonal) {
    r.x0 = static_cast<db::Coord>(std::max<std::int64_t>(r.x0, wx0));
    r.y0 = static_cast<db::Coord>(std::max<std::int64_t>(r.y0, wy0));
    r.x1 = static_cast<db::Coord>(std::min<std::int64_t>(r.x1, wx1));
    r.y1 = static_cast<db::Coord>(std::min<std::int64_t>(r.y1, wy1));
  }
  if (r.x1 < r.x0 || r.y1 < r.y0) {
    return std::nullopt;
  }
  const auto fi = static_cast<std::int32_t>(f);
  return ScreenRect{to_pixel(r.x0, wx0, m.scale, m.origin_x, fi), to_pixel(r.y0, wy0, m.scale, m.origin_y, fi),
                    to_pixel(r.x1, wx0, m.scale, m.origin_x, fi), to_pixel(r.y1, wy0, m.scale, m.origin_y, fi)};
}

bool FeedbackList::add(const WindowManager& windows, const db::CellDef& cell, const db::Rect& rect,
                       std::int32_t scale, FeedbackStyle style, std::string_view text, HighlightLayer* highlights) {
  const auto root = find_feedback_root(cell, windows);
  if (!root) {
    return false;
  }
  const std::int32_t f = std::max(scale, 1);
  db::Transform t = root->to_root;  // translations are in layout units: scale them
  t.c *= f;
  t.f *= f;
  FeedbackArea a;
  a.root = root->root;
  a.scaled = db::apply(t, rect);
  a.scale = f;
  a.area = round_feedback_area(a.scaled, f);
  a.style = style;
  if (!areas_.empty() && areas_.back().message() == text) {
    a.text = areas_.back().text;
  } else {
    a.text = std::make_shared<const std::string>(text);
  }
  areas_.push_back(std::move(a));
  if (highlights != nullptr) {
    highlights->request(areas_.back().root, areas_.back().area, false);
  }
  return true;
}

std::size_t FeedbackList::clear(std::optional<std::string_view> text, HighlightLayer* highlights) {
  std::vector<FeedbackArea> kept;
  const db::CellDef* run_root = nullptr;
  std::optional<db::Rect> run_area;
  const auto flush = [&] {
    if (highlights != nullptr && run_root != nullptr && run_area) {
      highlights->request(run_root, *run_area, true);
    }
  };
  std::size_t removed = 0;
  for (FeedbackArea& a : areas_) {
    if (text && a.message().find(*text) == std::string::npos) {
      kept.push_back(std::move(a));
      continue;
    }
    if (a.root != run_root) {
      flush();
      run_root = a.root;
      run_area.reset();
    }
    run_area = run_area ? db::Rect{std::min(run_area->x0, a.area.x0), std::min(run_area->y0, a.area.y0),
                                   std::max(run_area->x1, a.area.x1), std::max(run_area->y1, a.area.y1)}
                        : a.area;
    ++removed;
  }
  flush();
  areas_ = std::move(kept);
  return removed;
}

void FeedbackList::clear() noexcept {
  areas_.clear();
  find_cursor_ = 0;
}

}  // namespace hefesto::view
