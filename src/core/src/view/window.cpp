// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layout windows (capability ui-windows-and-views). The view arithmetic is
// ported from Magic 8.3.694 windows/windView.c (WindMove, WindZoom,
// WindView, WindScroll, windFixSurfaceArea), windows/windTrans.c
// (WindPointToSurface, WindScreenToSurface), windows/windMove.c
// (WindCreate, WindOutToIn), dbwind/DBWprocs.c (DBWloadWindow framing) and
// commands/CmdSubrs.c (cmdSetWindCaption), Copyright (C) 1985, 1990 Regents
// of the University of California, used under Magic's permissive license
// (see THIRD_PARTY_NOTICES.md).

#include "hefesto/view/window.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/view/diag_codes.hpp"

namespace hefesto::view {

namespace {

constexpr std::int64_t kSubpixel = 65536;      // SUBPIXEL
constexpr std::int64_t kHalfSubpixel = 32768;  // HSUBPIXEL

/// One axis of WindMove: the scale candidate of this axis.
std::int64_t axis_scale(std::int32_t lo_px, std::int32_t hi_px, db::Coord lo, db::Coord hi) {
  const std::int64_t size = std::int64_t{hi} - lo + 1;
  return size <= 0 ? kSubpixel : (std::int64_t{hi_px} - lo_px + 1) * kSubpixel / size;
}

struct AxisView {
  db::Coord surface_lo = 0;
  db::Coord surface_hi = 0;
  std::int64_t origin = 0;
};

/// One axis of WindMove after the scale is known.
AxisView axis_view(std::int32_t lo_px, std::int32_t hi_px, db::Coord lo, db::Coord hi, std::int64_t scale) {
  const std::int64_t half_pixels = (std::int64_t{hi_px} - lo_px) * kHalfSubpixel;
  const std::int64_t half_units = half_pixels / scale + 1;
  AxisView out;
  out.surface_lo = static_cast<db::Coord>((std::int64_t{lo} + hi) / 2 - half_units);
  out.surface_hi = static_cast<db::Coord>(out.surface_lo + 2 * half_units + 1);
  out.origin = (std::int64_t{hi_px} + lo_px) * kHalfSubpixel - half_units * scale;
  return out;
}

/// WindPointToSurface, point form (rounded half away from zero).
db::Coord point_axis(std::int32_t px, std::int64_t origin, db::Coord surface_lo, std::int64_t scale) {
  std::int64_t tmp = kSubpixel * px - origin;
  const std::int64_t adjust = scale / 2;
  tmp += tmp >= 0 ? adjust : -adjust;
  return static_cast<db::Coord>(surface_lo + tmp / scale);
}

/// WindPointToSurface, box form: the lower edge of the box around a pixel.
db::Coord box_axis(std::int32_t px, std::int64_t origin, db::Coord surface_lo, std::int64_t scale) {
  std::int64_t tmp = kSubpixel * px - origin;
  if (tmp < 0) {
    tmp -= scale - 1;
  }
  return static_cast<db::Coord>(surface_lo + tmp / scale);
}

/// One axis of the screen-offset part of WindScroll.
void scroll_axis(std::int32_t screen_offset, std::int64_t scale, db::Coord& lo, db::Coord& hi, std::int64_t& origin) {
  const std::int64_t pixels = std::int64_t{screen_offset} * kSubpixel;
  const std::int64_t units = pixels / scale;
  lo = static_cast<db::Coord>(lo - units);
  hi = static_cast<db::Coord>(hi - units);
  origin += pixels - scale * units;
}

std::string shorten(std::string_view name, std::size_t limit) {
  if (name.size() < limit) {
    return std::string(name);
  }
  return "..." + std::string(name.substr(name.size() - (limit - 3)));
}

db::Coord grow(db::Coord lo, db::Coord hi) {
  return (hi - lo + 1) / 10;  // SLOP
}

/// WindSurfaceToScreen of a rectangle.
ScreenRect to_screen(const ViewMapping& m, const db::Rect& r) {
  return ScreenRect{m.screen_x(r.x0), m.screen_y(r.y0), m.screen_x(r.x1), m.screen_y(r.y1)};
}

/// GeoClip; nullopt when nothing is left (inclusive corners).
std::optional<ScreenRect> clip(const ScreenRect& r, const ScreenRect& to) {
  const ScreenRect c{std::max(r.x0, to.x0), std::max(r.y0, to.y0), std::min(r.x1, to.x1), std::min(r.y1, to.y1)};
  if (c.x0 > c.x1 || c.y0 > c.y1) {
    return std::nullopt;
  }
  return c;
}

std::uint32_t window_bit(WindowId id) {
  return std::uint32_t{1} << static_cast<std::uint32_t>(id);
}

/// DBWAreaChanged's step from a child to its parent: the changed area of
/// `def` in the parent of `use`, per array element or as the span of all
/// elements when the area is more than half the cell's width or height.
std::vector<db::Rect> parent_areas(const db::CellDef& def, const db::CellUse& use, const db::Rect& area) {
  const db::ArrayInfo& a = use.array;
  if (!a.is_array()) {
    return {db::apply(use.transform, area)};
  }
  const db::Rect& bbox = def.bbox();
  if (2 * area.width() > bbox.width() || 2 * area.height() > bbox.height()) {
    const db::Rect lo = db::apply(db::element_transform(use, a.xlo, a.ylo), area);
    const db::Rect hi = db::apply(db::element_transform(use, a.xhi, a.yhi), area);
    return {db::Rect{std::min(lo.x0, hi.x0), std::min(lo.y0, hi.y0), std::max(lo.x1, hi.x1), std::max(lo.y1, hi.y1)}};
  }
  std::vector<db::Rect> out;
  for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
    for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
      out.push_back(db::apply(db::element_transform(use, x, y), area));
    }
  }
  return out;
}

}  // namespace

// --- ViewMapping ---------------------------------------------------------------------

ViewMapping ViewMapping::fit(const ScreenRect& screen, const db::Rect& area) {
  ViewMapping m;
  m.screen = screen;
  m.scale = std::min(axis_scale(screen.x0, screen.x1, area.x0, area.x1),
                     axis_scale(screen.y0, screen.y1, area.y0, area.y1));
  if (m.scale < 1) {
    m.scale = 1;
    m.at_minimum = true;
  }
  const AxisView x = axis_view(screen.x0, screen.x1, area.x0, area.x1, m.scale);
  const AxisView y = axis_view(screen.y0, screen.y1, area.y0, area.y1, m.scale);
  m.surface = db::Rect{x.surface_lo, y.surface_lo, x.surface_hi, y.surface_hi};
  m.origin_x = x.origin;
  m.origin_y = y.origin;
  return m;
}

std::int32_t ViewMapping::screen_x(db::Coord x) const noexcept {
  return static_cast<std::int32_t>((origin_x + (std::int64_t{x} - surface.x0) * scale) >> 16);
}

std::int32_t ViewMapping::screen_y(db::Coord y) const noexcept {
  return static_cast<std::int32_t>((origin_y + (std::int64_t{y} - surface.y0) * scale) >> 16);
}

db::Point ViewMapping::to_layout(std::int32_t px, std::int32_t py) const noexcept {
  return db::Point{point_axis(px, origin_x, surface.x0, scale), point_axis(py, origin_y, surface.y0, scale)};
}

db::Rect ViewMapping::screen_to_surface(const ScreenRect& r) const noexcept {
  const auto units_per_pixel = static_cast<db::Coord>(kSubpixel / scale);
  const db::Coord xhi = box_axis(r.x1, origin_x, surface.x0, scale) + units_per_pixel + 1;
  const db::Coord yhi = box_axis(r.y1, origin_y, surface.y0, scale) + units_per_pixel + 1;
  return db::Rect{box_axis(r.x0, origin_x, surface.x0, scale), box_axis(r.y0, origin_y, surface.y0, scale), xhi, yhi};
}

ViewMapping ViewMapping::scrolled(db::Point surface_offset, db::Point screen_offset) const noexcept {
  ViewMapping m = *this;
  m.surface = db::Rect{surface.x0 + surface_offset.x, surface.y0 + surface_offset.y, surface.x1 + surface_offset.x,
                       surface.y1 + surface_offset.y};
  scroll_axis(screen_offset.x, scale, m.surface.x0, m.surface.x1, m.origin_x);
  scroll_axis(screen_offset.y, scale, m.surface.y0, m.surface.y1, m.origin_y);
  // windFixSurfaceArea: the surface covering the screen grown by one pixel.
  const ScreenRect grown{screen.x0 - 1, screen.y0 - 1, screen.x1 + 1, screen.y1 + 1};
  const db::Rect fixed = m.screen_to_surface(grown);
  m.origin_x += (std::int64_t{fixed.x0} - m.surface.x0) * scale;
  m.origin_y += (std::int64_t{fixed.y0} - m.surface.y0) * scale;
  m.surface = fixed;
  return m;
}

ScreenRect inner_area(const ScreenRect& frame, const ChromeFlags& flags, const ChromeMetrics& metrics) {
  const std::int32_t thin = flags.border ? metrics.border : 0;
  const std::int32_t side = flags.scrollbars ? 2 * thin + metrics.scrollbar : 2 * thin;
  const std::int32_t top = flags.caption ? metrics.caption : 2 * thin;
  return ScreenRect{frame.x0 + side, frame.y0 + side, frame.x1 - 2 * thin, frame.y1 - top};
}

// --- framing -------------------------------------------------------------------------

db::Rect load_frame(const db::Rect& bbox) {
  auto margin = [](db::Coord size) { return std::max<db::Coord>(0, (60 - size) / 2) + (size + 1) / 10; };
  const db::Coord mx = margin(bbox.x1 - bbox.x0);
  const db::Coord my = margin(bbox.y1 - bbox.y0);
  return db::Rect{bbox.x0 - mx, bbox.y0 - my, bbox.x1 + mx, bbox.y1 + my};
}

db::Rect view_frame(const db::Rect& bbox) {
  db::Rect r = bbox;
  r.x0 -= grow(r.x0, r.x1);
  r.x1 += grow(r.x0, r.x1);
  r.y0 -= grow(r.y0, r.y1);
  r.y1 += grow(r.y0, r.y1);
  return r;
}

db::Rect zoom_area(const db::Rect& surface, float factor) {
  const db::Coord cx = (surface.x0 + surface.x1) / 2;
  const db::Coord cy = (surface.y0 + surface.y1) / 2;
  auto lo = [factor](db::Coord c, db::Coord v) {
    return static_cast<db::Coord>(static_cast<float>(c) - static_cast<float>(c - v) * factor);
  };
  auto hi = [factor](db::Coord c, db::Coord v) {
    return static_cast<db::Coord>(static_cast<float>(c) + static_cast<float>(v - c) * factor);
  };
  return db::Rect{lo(cx, surface.x0), lo(cy, surface.y0), hi(cx, surface.x1), hi(cy, surface.y1)};
}

db::Rect box_frame(const db::Rect& box) {
  const db::Coord mx = std::max<db::Coord>(2, (box.x1 - box.x0) / 20);
  const db::Coord my = std::max<db::Coord>(2, (box.y1 - box.y0) / 20);
  return db::Rect{box.x0 - mx, box.y0 - my, box.x1 + mx, box.y1 + my};
}

std::string window_caption(std::string_view root, std::string_view edit, bool editing) {
  constexpr std::size_t kCaptionMax = 199;
  std::string caption = editing ? shorten(root, 90) + " EDITING " + shorten(edit, 90)
                                : shorten(root, 175) + " [NOT BEING EDITED]";
  if (caption.size() > kCaptionMax) {
    caption.resize(kCaptionMax);
  }
  return caption;
}

// --- LayoutWindow --------------------------------------------------------------------

LayoutWindow::LayoutWindow(WindowId id, db::CellDef* root) : id_(id) {
  set_root(root);
}

LayoutWindow::~LayoutWindow() = default;

void LayoutWindow::set_root(db::CellDef* def) {
  expanded_.clear();
  if (def == nullptr) {
    root_use_.reset();
  } else {
    root_use_ = std::make_unique<db::CellUse>();
    root_use_->def = def;
    root_use_->bbox = def->bbox();
  }
  // A cell that does not exist on disk frames as (0,0)-(1,1).
  const bool framed = def != nullptr && !def->flags().not_found;
  move_to(load_frame(framed ? def->bbox() : db::Rect{0, 0, 1, 1}));
}

bool LayoutWindow::is_expanded(const db::CellUse& use) const {
  return &use == root_use_.get() || expanded_.contains(&use);
}

void LayoutWindow::set_expanded(const db::CellUse& use, bool expanded) {
  if (expanded) {
    expanded_.insert(&use);
  } else {
    expanded_.erase(&use);
  }
}

void LayoutWindow::mark_redisplay(const db::Rect& root_area, bool labels) {
  if (redisplay_.all) {
    return;
  }
  redisplay_.areas.push_back(root_area);
  ScreenRect r = to_screen(mapping_, root_area);
  if (const auto clipped = clip(r, mapping_.screen)) {
    r = *clipped;
    if (labels) {
      r = ScreenRect{r.x0 + label_extent_.x0, r.y0 + label_extent_.y0, r.x1 + label_extent_.x1,
                     r.y1 + label_extent_.y1};
    }
    redisplay_.screen.push_back(r);
  }
}

void LayoutWindow::mark_redisplay_all() {
  redisplay_ = Redisplay{true, {}, {mapping_.screen}};
}

Redisplay LayoutWindow::take_redisplay() {
  return std::exchange(redisplay_, Redisplay{});
}

void LayoutWindow::include_label_extent(const ScreenRect& amounts) noexcept {
  label_extent_ = ScreenRect{std::min(label_extent_.x0, amounts.x0), std::min(label_extent_.y0, amounts.y0),
                             std::max(label_extent_.x1, amounts.x1), std::max(label_extent_.y1, amounts.y1)};
}

// --- WindowManager -------------------------------------------------------------------

WindowManager::WindowManager() = default;
WindowManager::~WindowManager() = default;

diag::Outcome<LayoutWindow*> WindowManager::insert(db::CellDef* root, const ScreenRect& frame,
                                                   const ScreenRect& screen) {
  if (windows_.size() + reserved_.size() >= kMaxWindows) {
    return diag::make(codes::kWindowLimit, "Can't have more than 31 windows.");
  }
  const WindowId id = free_id();
  auto window = std::make_unique<LayoutWindow>(id, nullptr);
  window->set_frame(frame);
  window->set_chrome(chrome_defaults_);
  window->set_mapping(ViewMapping::fit(screen, db::Rect{0, 0, 1, 1}));
  window->set_root(root);
  LayoutWindow* raw = window.get();
  windows_.insert(windows_.begin(), std::move(window));
  current_ = id;
  notify(id, Change::opened);
  return raw;
}

diag::Outcome<LayoutWindow*> WindowManager::open(db::CellDef* root, const ScreenRect& screen) {
  return insert(root, screen, screen);
}

diag::Outcome<LayoutWindow*> WindowManager::open_frame(db::CellDef* root, const ScreenRect& frame) {
  return insert(root, frame, inner_area(frame, chrome_defaults_, chrome_metrics_));
}

diag::Status WindowManager::close(WindowId id) {
  const auto it = std::find_if(windows_.begin(), windows_.end(), [id](const auto& w) { return w->id() == id; });
  if (it == windows_.end()) {
    return diag::make(codes::kWindowUnknown, "Window " + std::to_string(id) + " cannot be found");
  }
  windows_.erase(it);
  if (current_ == id) {
    current_ = windows_.empty() ? std::nullopt : std::optional<WindowId>{windows_.front()->id()};
  }
  notify(id, Change::closed);
  return diag::Done{};
}

void WindowManager::scale_all(const std::function<db::Point(db::Point)>& scale_point) {
  for (const auto& w : windows_) {
    const db::Rect area = w->mapping().surface;
    const db::Point ll = scale_point(db::Point{area.x0, area.y0});
    const db::Point ur = scale_point(db::Point{area.x1, area.y1});
    w->move_to(db::Rect{ll.x, ll.y, ur.x, ur.y});
    notify(w->id(), Change::view);
  }
}

WindowId WindowManager::free_id() const {
  WindowId id = 0;
  while (find(id) != nullptr || id_reserved(id)) {
    ++id;
  }
  return id;
}

diag::Outcome<WindowId> WindowManager::reserve_id() {
  if (windows_.size() + reserved_.size() >= kMaxWindows) {
    return diag::make(codes::kWindowLimit, "Can't have more than 31 windows.");
  }
  const WindowId id = free_id();
  reserved_.push_back(id);
  return id;
}

void WindowManager::release_id(WindowId id) {
  reserved_.erase(std::remove(reserved_.begin(), reserved_.end(), id), reserved_.end());
}

bool WindowManager::id_reserved(WindowId id) const {
  return std::find(reserved_.begin(), reserved_.end(), id) != reserved_.end();
}

LayoutWindow* WindowManager::find(WindowId id) const {
  for (const auto& w : windows_) {
    if (w->id() == id) {
      return w.get();
    }
  }
  return nullptr;
}

LayoutWindow* WindowManager::find_named(std::string_view name) const {
  for (const auto& w : windows_) {
    if (!w->name().empty() && w->name() == name) {
      return w.get();
    }
  }
  return nullptr;
}

LayoutWindow* WindowManager::current() const {
  return current_ ? find(*current_) : nullptr;
}

void WindowManager::set_current(WindowId id) {
  if (find(id) != nullptr) {
    current_ = id;
  }
}

void WindowManager::for_each(const std::function<void(LayoutWindow&)>& visit) const {
  for (const auto& w : windows_) {
    visit(*w);
  }
}

std::size_t WindowManager::size() const noexcept {
  return windows_.size();
}

void WindowManager::raise(WindowId id) {
  const auto it = std::find_if(windows_.begin(), windows_.end(), [id](const auto& w) { return w->id() == id; });
  if (it != windows_.end()) {
    std::rotate(windows_.begin(), it, std::next(it));
  }
}

void WindowManager::lower(WindowId id) {
  const auto it = std::find_if(windows_.begin(), windows_.end(), [id](const auto& w) { return w->id() == id; });
  if (it != windows_.end()) {
    std::rotate(it, std::next(it), windows_.end());
  }
}

void WindowManager::toggle_full_screen(WindowId id) {
  const auto it = std::find_if(windows_.begin(), windows_.end(), [id](const auto& w) { return w->id() == id; });
  if (it == windows_.end()) {
    return;
  }
  LayoutWindow& w = **it;
  const bool host_managed = w.mapping().screen == w.frame();
  const ScreenRect frame = w.full_screen_ ? w.saved_frame_ : screen_;
  std::size_t depth = 0;
  if (w.full_screen_) {
    depth = w.saved_depth_;
  } else {
    w.saved_frame_ = w.frame_;
    w.saved_depth_ = static_cast<std::size_t>(it - windows_.begin());
  }
  w.full_screen_ = !w.full_screen_;
  // WindSetWindowAreas, then WindMove onto the surface area it showed.
  const db::Rect surface = w.mapping().surface;
  w.frame_ = frame;
  w.mapping_ = ViewMapping::fit(host_managed ? frame : inner_area(frame, w.chrome_, chrome_metrics_), surface);
  auto owned = std::move(*it);
  windows_.erase(it);
  windows_.insert(windows_.begin() + static_cast<std::ptrdiff_t>(std::min(depth, windows_.size())), std::move(owned));
  w.mark_redisplay_all();
  notify(id, Change::view);
}

void WindowManager::area_changed(const db::CellDef& def, const db::Rect& area,
                                 const std::optional<tech::TypeMask>& layers, std::optional<WindowId> only) {
  if (area.x0 == area.x1 || area.y0 == area.y1) {
    return;
  }
  std::uint32_t all = 0;
  for (const auto& w : windows_) {
    if (!only || *only == w->id()) {
      all |= window_bit(w->id());
    }
  }
  propagate_change(def, area, all, layers);
}

void WindowManager::propagate_change(const db::CellDef& def, const db::Rect& area, std::uint32_t windows,
                                     const std::optional<tech::TypeMask>& layers) {
  for (const auto& w : windows_) {
    if ((windows & window_bit(w->id())) == 0 || w->root() != &def) {
      continue;
    }
    if (!layers || !(*layers & w->visible_types()).empty()) {
      w->mark_redisplay(area, !layers.has_value());
    }
  }
  for (const db::CellUse* use : def.instances()) {
    if (use == nullptr || use->parent == nullptr) {
      continue;
    }
    std::uint32_t expanded = 0;
    for (const auto& w : windows_) {
      if ((windows & window_bit(w->id())) != 0 && w->is_expanded(*use)) {
        expanded |= window_bit(w->id());
      }
    }
    if (expanded == 0) {
      continue;
    }
    for (const db::Rect& parent_area : parent_areas(def, *use, area)) {
      propagate_change(*use->parent, parent_area, expanded, layers);
    }
  }
}

void WindowManager::redisplay_all() {
  for (const auto& w : windows_) {
    w->mark_redisplay_all();
  }
}

void WindowManager::update_display() {
  if (suspended_) {
    return;
  }
  for (const auto& w : windows_) {
    if (!w->pending_redisplay().empty()) {
      notify(w->id(), Change::content);
    }
  }
}

void WindowManager::set_crosshair(const db::CellDef* root, std::optional<db::Point> point) {
  // One crosshair per session: windows on other roots lose theirs.
  for (const auto& w : windows_) {
    const std::optional<db::Point> shown = point && w->root() == root ? point : std::nullopt;
    if (shown != w->crosshair()) {
      w->set_crosshair(shown);
      notify(w->id(), Change::view);
    }
  }
}

void WindowManager::add_observer(ViewObserver& observer) {
  observers_.push_back(&observer);
}

void WindowManager::remove_observer(const ViewObserver& observer) {
  std::erase(observers_, &observer);
}

void WindowManager::notify(WindowId id, Change change) {
  for (auto* observer : observers_) {
    observer->on_window_changed(id, change);
  }
}

}  // namespace hefesto::view
