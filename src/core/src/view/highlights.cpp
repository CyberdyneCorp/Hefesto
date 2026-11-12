// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// view::HighlightLayer and the built-in highlight clients
// (ui-feedback-and-highlights "Highlight layer and clients"; cmd-selection
// "Selection model" display). Owner: VIEW.
// Behavior follows Magic 8.3.694 dbwind/DBWhlights.c and select/selDisplay.c,
// Copyright (C) 1985, 1990 Regents of the University of California, used
// under Magic's permissive license (see THIRD_PARTY_NOTICES.md).

#include "hefesto/view/highlights.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/view/feedback.hpp"
#include "view/command_util.hpp"

namespace hefesto::view {

namespace {

using display::HighlightItem;
using display::HighlightScene;
using display::HighlightShape;
using display::TextPlace;

db::Rect widened(db::Rect r) noexcept {
  if (r.x0 >= r.x1) {
    r.x1 = r.x0 + 1;
    --r.x0;
  }
  if (r.y0 >= r.y1) {
    r.y1 = r.y0 + 1;
    --r.y0;
  }
  return r;
}

ScreenRect to_screen(const ViewMapping& m, const db::Rect& r) noexcept {
  return ScreenRect{m.screen_x(r.x0), m.screen_y(r.y0), m.screen_x(r.x1), m.screen_y(r.y1)};
}

/// GrCrossRect with the window's label extent included (GeoInclude).
ScreenRect expansion(const LayoutWindow& w) noexcept {
  const ScreenRect& l = w.label_extent();
  return ScreenRect{std::min(-kCrossSize, l.x0), std::min(-kCrossSize, l.y0), std::max(kCrossSize, l.x1),
                    std::max(kCrossSize, l.y1)};
}

bool touches(const db::Rect& a, const db::Rect& b) noexcept {
  return a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1;
}

db::Rect bounding(const std::vector<db::Rect>& rects) noexcept {
  db::Rect out = rects.front();
  for (const db::Rect& r : rects) {
    out = db::Rect{std::min(out.x0, r.x0), std::min(out.y0, r.y0), std::max(out.x1, r.x1), std::max(out.y1, r.y1)};
  }
  return out;
}

HighlightItem item(HighlightShape shape, std::size_t style, const db::Rect& rect) {
  HighlightItem it;
  it.shape = shape;
  it.style = style;
  it.rect = rect;
  return it;
}

HighlightItem line_item(std::size_t style, db::Point a, db::Point b) {
  HighlightItem it = item(HighlightShape::line, style, db::Rect{});
  it.line = display::SceneSegment{a, b};
  return it;
}

HighlightItem text_item(std::size_t style, const db::Rect& rect, std::string text,
                        TextPlace place = TextPlace::center) {
  HighlightItem it = item(HighlightShape::text, style, rect);
  it.text = std::move(text);
  it.place = place;
  return it;
}

db::Rect shifted(const db::Rect& r, db::Point d) noexcept {
  return db::Rect{r.x0 + d.x, r.y0 + d.y, r.x1 + d.x, r.y1 + d.y};
}

// --- box ---------------------------------------------------------------------

/// The edit cell's bounding box in the window's root coordinates, when the
/// window shows the edit cell.
std::optional<db::Rect> edit_bbox_in(const editor::EditorState& e, const LayoutWindow& w) {
  if (e.edit_cell == nullptr || w.root() == nullptr) {
    return std::nullopt;
  }
  if (e.edit_cell == w.root()) {
    return e.edit_cell->bbox();
  }
  if (e.edit_root == w.root()) {
    return db::apply(e.edit_transform, e.edit_cell->bbox());
  }
  return std::nullopt;
}

// --- feedback ----------------------------------------------------------------

/// Nearest layout unit of a scaled coordinate (half up), for drawing.
db::Coord unscale(db::Coord v, std::int32_t f) noexcept {
  const std::int64_t num = 2 * std::int64_t{v} + f;
  const std::int64_t den = 2 * std::int64_t{f};
  const std::int64_t q = num / den;
  return static_cast<db::Coord>((num % den != 0 && num < 0) ? q - 1 : q);
}

db::Rect drawn_area(const FeedbackArea& a) noexcept {
  return db::Rect{unscale(a.scaled.x0, a.scale), unscale(a.scaled.y0, a.scale), unscale(a.scaled.x1, a.scale),
                  unscale(a.scaled.y1, a.scale)};
}

HighlightItem feedback_item(const FeedbackArea& a) {
  const db::Rect r = drawn_area(a);
  const FeedbackStyle& s = a.style;
  if (s.diagonal) {
    HighlightItem it = item(HighlightShape::triangle, s.index, r);
    it.diagonal = s.direction ? db::Diagonal::nw_se : db::Diagonal::ne_sw;
    it.right_side = s.side;
    return it;
  }
  if (s.side) {
    return s.direction ? line_item(s.index, {r.x0, r.y1}, {r.x1, r.y0}) : line_item(s.index, {r.x0, r.y0}, {r.x1, r.y1});
  }
  const bool point = r.x0 == r.x1 && r.y0 == r.y1;
  return item(point ? HighlightShape::cross : HighlightShape::box, s.index, r);
}

// --- elements ----------------------------------------------------------------

std::size_t element_style(const editor::EditorState& e, const db::Element& el) {
  if (e.styles && !el.styles.empty()) {
    if (const auto index = display::style_from_name(e.styles->table, el.styles.front())) {
      return *index;
    }
  }
  return kStyleOutlineHighlights;
}

// --- selection ---------------------------------------------------------------

/// The two straight sides of a split tile's reported triangle.
void triangle_legs(const db::TileView& t, std::vector<display::SceneSegment>& out) {
  const db::Rect& r = t.area;
  const bool upper = (t.diagonal == db::Diagonal::ne_sw) != t.right_side;
  const db::Coord y = upper ? r.y1 : r.y0;
  const db::Coord x = t.right_side ? r.x1 : r.x0;
  out.push_back({{r.x0, y}, {r.x1, y}});
  out.push_back({{x, r.y0}, {x, r.y1}});
}

void diagonal_of(const db::TileView& t, std::vector<display::SceneSegment>& out) {
  const db::Rect& r = t.area;
  if (t.diagonal == db::Diagonal::nw_se) {
    out.push_back({{r.x0, r.y1}, {r.x1, r.y0}});
  } else {
    out.push_back({{r.x0, r.y0}, {r.x1, r.y1}});
  }
}

/// The selected paint's edges on one plane: boundaries between different
/// material (selRedisplayFunc), split-tile diagonals and legs.
void selection_plane_edges(const db::Plane& plane, const db::Rect& area, std::vector<display::SceneSegment>& out) {
  std::map<tech::TileType, std::vector<db::Rect>> by_type;
  tech::TypeMask all = tech::TypeMask::all();
  all.clear_type(tech::kSpace);
  (void)plane.enumerate(area, all, [&](const db::TileView& t) {
    if (t.split) {
      triangle_legs(t, out);
      diagonal_of(t, out);
    } else {
      by_type[t.type].push_back(t.area);
    }
    return true;
  });
  for (const auto& [type, rects] : by_type) {
    auto edges = display::boundary_segments(rects);
    out.insert(out.end(), edges.begin(), edges.end());
  }
}

void selection_labels(const db::CellDef& def, const db::Rect& area, db::Point d, HighlightScene& out) {
  for (const db::Label& label : def.labels()) {
    if (!touches(widened(label.rect), area)) {
      continue;
    }
    const db::Rect r = shifted(label.rect, d);
    const bool point = r.x0 == r.x1 && r.y0 == r.y1;
    out.items.push_back(item(point ? HighlightShape::cross : HighlightShape::outline, kStyleOutlineHighlights, r));
    out.items.push_back(text_item(kStyleOutlineHighlights, r, label.text));
  }
}

void selection_uses(const db::CellDef& def, const db::Rect& area, db::Point d, HighlightScene& out) {
  for (const auto& use : def.uses()) {
    if (!use || use->def == nullptr || !touches(use->bbox, area)) {
      continue;
    }
    const db::Rect r = shifted(use->bbox, d);
    out.items.push_back(item(HighlightShape::outline, kStyleOutlineHighlights, r));
    // selRedisplayCellFunc: the definition name and the instance id in the
    // solid-highlights style, at 2/3 and 1/3 of the height (not both centred).
    out.items.push_back(text_item(kStyleSolidHighlights, r, use->def->name(), TextPlace::cell_name));
    out.items.push_back(text_item(kStyleSolidHighlights, r, use->id, TextPlace::cell_id));
  }
}

}  // namespace

// --- built-in clients -----------------------------------------------------------

void draw_box_highlights(const HighlightContext& ctx, HighlightScene& out) {
  const auto box = detail::box_in_window(ctx.editor, ctx.window);
  if (!box || !touches(*box, ctx.area)) {
    return;
  }
  const ViewMapping& m = ctx.window.mapping();
  const std::int64_t px_w = (std::int64_t{box->x1 - box->x0} * m.scale) >> 16;
  const std::int64_t px_h = (std::int64_t{box->y1 - box->y0} * m.scale) >> 16;
  const display::BoxDrawing d = display::box_drawing(*box, px_w, px_h, edit_bbox_in(ctx.editor, ctx.window));
  if (d.shape == display::BoxShape::point) {
    out.items.push_back(item(HighlightShape::cross, d.style, *box));
  }
  out.items.push_back(item(HighlightShape::outline, d.style, *box));
}

void draw_feedback_highlights(const HighlightContext& ctx, HighlightScene& out) {
  const db::CellDef* root = ctx.window.root();
  for (const FeedbackArea& a : ctx.editor.feedback.areas()) {
    if (a.root == root && touches(a.area, ctx.area)) {
      out.items.push_back(feedback_item(a));
    }
  }
}

void draw_element_highlights(const HighlightContext& ctx, HighlightScene& out) {
  const db::CellDef* root = ctx.window.root();
  if (root == nullptr) {
    return;
  }
  for (const db::Element& el : root->elements()) {
    if (!touches(el.area, ctx.area)) {
      continue;
    }
    const std::size_t style = element_style(ctx.editor, el);
    switch (el.kind) {
      case db::Element::Kind::rectangle:
        out.items.push_back(item(HighlightShape::box, style, el.area));
        break;
      case db::Element::Kind::line:
        out.items.push_back(line_item(style, {el.area.x0, el.area.y0}, {el.area.x1, el.area.y1}));
        break;
      case db::Element::Kind::text:
        out.items.push_back(text_item(style, el.area, el.text));
        break;
    }
  }
}

void draw_crosshair_highlights(const HighlightContext& ctx, HighlightScene& out) {
  const auto& p = ctx.window.crosshair();
  if (!p) {
    return;
  }
  const db::Rect& s = ctx.window.mapping().surface;
  out.items.push_back(line_item(display::kStyleCrosshair, {s.x0, p->y}, {s.x1, p->y}));
  out.items.push_back(line_item(display::kStyleCrosshair, {p->x, s.y0}, {p->x, s.y1}));
}

void draw_selection_highlights(const HighlightContext& ctx, HighlightScene& out) {
  const select::Selection& sel = ctx.editor.selection;
  const db::CellDef* def = sel.def();
  if (def == nullptr || sel.root() == nullptr || sel.root() != ctx.window.root()) {
    return;
  }
  const db::Point d = sel.floating_offset().value_or(db::Point{0, 0});
  const db::Rect area = shifted(ctx.area, db::Point{-d.x, -d.y});  // in the selection cell's coordinates
  std::vector<display::SceneSegment> edges;
  for (tech::PlaneId p = 0; p < def->plane_count(); ++p) {
    selection_plane_edges(def->plane(p), area, edges);
  }
  for (const auto& e : edges) {
    out.items.push_back(line_item(kStyleOutlineHighlights, {e.a.x + d.x, e.a.y + d.y}, {e.b.x + d.x, e.b.y + d.y}));
  }
  selection_labels(*def, area, d, out);
  selection_uses(*def, area, d, out);
}

// --- HighlightLayer -------------------------------------------------------------

HighlightLayer::HighlightLayer() {
  (void)add_client("box", draw_box_highlights);
  (void)add_client("feedback", draw_feedback_highlights);
  (void)add_client("elements", draw_element_highlights);
  (void)add_client("crosshair", draw_crosshair_highlights);
}

HighlightAreas highlight_areas(const LayoutWindow& window, const db::Rect& area, bool erase) {
  // dbwhlRedrawFunc: the erase area grows by the expansion once, the redraw
  // area once more, then it goes back to layout units plus one unit.
  const ViewMapping& m = window.mapping();
  const ScreenRect ex = expansion(window);
  ScreenRect s = to_screen(m, widened(area));
  HighlightAreas out;
  if (erase) {
    s = ScreenRect{s.x0 + ex.x0, s.y0 + ex.y0, s.x1 + ex.x1, s.y1 + ex.y1};
    out.erase = s;
  }
  const db::Rect r = m.screen_to_surface(ScreenRect{s.x0 - ex.x1, s.y0 - ex.y1, s.x1 - ex.x0, s.y1 - ex.y0});
  out.redraw = db::Rect{r.x0 - 1, r.y0 - 1, r.x1 + 1, r.y1 + 1};
  return out;
}

void HighlightLayer::request(const db::CellDef* root, const db::Rect& area, bool erase) {
  if (root == nullptr) {
    return;
  }
  by_root_[root].push_back(Request{area, erase});
  by_root_areas_[root].push_back(widened(area));
}

void HighlightLayer::request_window(WindowId window, const db::Rect& area, bool erase) {
  by_window_[window].push_back(Request{area, erase});
}

std::vector<HighlightLayer::Request> HighlightLayer::requests_for(const LayoutWindow& window) const {
  std::vector<Request> out;
  if (const auto it = by_root_.find(window.root()); it != by_root_.end()) {
    out = it->second;
  }
  if (const auto it = by_window_.find(window.id()); it != by_window_.end()) {
    out.insert(out.end(), it->second.begin(), it->second.end());
  }
  return out;
}

bool HighlightLayer::add_client(std::string name, HighlightClient client) {
  if (clients_.size() >= kMaxHighlightClients || !client) {
    return false;
  }
  for (const Client& c : clients_) {
    if (c.name == name) {
      return false;
    }
  }
  clients_.push_back(Client{std::move(name), std::move(client)});
  return true;
}

std::vector<std::string> HighlightLayer::client_names() const {
  std::vector<std::string> out;
  out.reserve(clients_.size());
  for (const Client& c : clients_) {
    out.push_back(c.name);
  }
  return out;
}

const std::vector<db::Rect>* HighlightLayer::pending(const db::CellDef* root) const {
  const auto it = by_root_areas_.find(root);
  return it == by_root_areas_.end() ? nullptr : &it->second;
}

void HighlightLayer::ensure_selection_client(const editor::EditorState& editor) {
  if (!selection_registered_ && !editor.selection.empty()) {
    selection_registered_ = add_client("selection", draw_selection_highlights);
  }
}

display::HighlightScene HighlightLayer::build(const editor::EditorState& editor, const LayoutWindow& window,
                                              const db::Rect& area) {
  ensure_selection_client(editor);
  HighlightScene scene;
  const HighlightContext ctx{editor, window, area};
  for (const Client& c : clients_) {
    c.draw(ctx, scene);
  }
  const display::StyleSet* styles = editor.styles ? &*editor.styles : nullptr;
  for (HighlightItem& it : scene.items) {
    display::resolve_highlight(it, styles);
  }
  return scene;
}

display::HighlightScene HighlightLayer::build(const editor::EditorState& editor, const LayoutWindow& window) {
  return build(editor, window, window.mapping().surface);
}

void HighlightLayer::redraw(editor::EditorState& editor) {
  WindowManager& windows = editor.windows;
  std::vector<LayoutWindow*> touched;
  windows.for_each([&](LayoutWindow& w) {
    const std::vector<Request> requests = requests_for(w);
    // "Tile structure overlay": any change in a watched window redraws it whole.
    if (editor.tile_overlays.contains(w.id()) && (!requests.empty() || !w.pending_redisplay().empty())) {
      w.mark_redisplay_all();
    }
    if (requests.empty()) {
      return;
    }
    HighlightFrame frame;
    for (const Request& r : requests) {
      const HighlightAreas a = highlight_areas(w, r.area, r.erase);
      if (a.erase) frame.erase.push_back(*a.erase);
      frame.redraw.push_back(a.redraw);
    }
    frame.scene = build(editor, w, bounding(frame.redraw));
    frames_[w.id()] = std::move(frame);
    touched.push_back(&w);
  });
  by_root_.clear();
  by_root_areas_.clear();
  by_window_.clear();
  for (LayoutWindow* w : touched) {
    windows.notify(w->id(), Change::highlights);
  }
}

void HighlightLayer::redraw(WindowManager& windows) {
  std::vector<WindowId> touched;
  windows.for_each([&](LayoutWindow& w) {
    if (!requests_for(w).empty()) touched.push_back(w.id());
  });
  by_root_.clear();
  by_root_areas_.clear();
  by_window_.clear();
  for (const WindowId id : touched) {
    windows.notify(id, Change::highlights);
  }
}

const HighlightFrame* HighlightLayer::frame(WindowId window) const {
  const auto it = frames_.find(window);
  return it == frames_.end() ? nullptr : &it->second;
}

HighlightFrame HighlightLayer::take_frame(WindowId window) {
  const auto it = frames_.find(window);
  if (it == frames_.end()) {
    return {};
  }
  HighlightFrame out = std::move(it->second);
  frames_.erase(it);
  return out;
}

}  // namespace hefesto::view
