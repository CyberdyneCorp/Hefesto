// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "layout_canvas.hpp"

#include <cmath>
#include <cstdio>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/render/layout_scene.hpp"

namespace hefesto::shell {

namespace {


render::Bounds to_bounds(const db::Rect& r) {
  if (r.is_null()) {
    return {};
  }
  return render::Bounds{r.x0, r.y0, r.x1, r.y1, true};
}

db::Rect to_rect(const render::Bounds& b) {
  if (b.empty()) {
    return db::Rect{0, 0, 0, 0};
  }
  return db::Rect{static_cast<db::Coord>(b.x0), static_cast<db::Coord>(b.y0), static_cast<db::Coord>(b.x1),
                  static_cast<db::Coord>(b.y1)};
}

/// One LayeredRect per scene instance; the batch index is the layer index.
std::vector<render::LayeredRect> layered(const render::RectScene& scene) {
  std::vector<render::LayeredRect> out;
  out.reserve(scene.size());
  const auto instances = scene.instances();
  for (const render::LayerBatch& batch : scene.batches()) {
    for (std::uint32_t i = 0; i < batch.count; ++i) {
      out.push_back(render::LayeredRect{instances[batch.first + i], static_cast<std::uint8_t>(batch.layer)});
    }
  }
  return out;
}

display::SceneOptions scene_options(const view::LayoutWindow& window, const db::CellDef& root) {
  display::SceneOptions options;
  // Everything the root holds (its bounding box includes its subcells).
  if (!root.bbox().is_null()) options.area = root.bbox();
  options.visible = window.visible_types();
  options.show_labels = window.labels_visible();
  options.expanded = [&window](const db::CellUse& use) { return window.is_expanded(use); };
  return options;
}

}  // namespace

LayoutCanvas::LayoutCanvas(ScreenContext& ctx, std::mutex& queue_mutex, std::function<float()> pixel_ratio)
    : ctx_(ctx), pixel_ratio_(std::move(pixel_ratio)), viewport_(ui::Viewport::create()) {
  render::QueueLock lock{[&queue_mutex] { queue_mutex.lock(); }, [&queue_mutex] { queue_mutex.unlock(); }};
  renderer_ = std::make_unique<render::CanvasRenderer>(ctx_.runtime.vulkan_device(), std::move(lock));
  painted_ = ui::Painted::create(ctx_.runtime, [this](ui::Painter& p, float w, float h) { paint_overlay(p, w, h); });
  // The overlay never takes input: the viewport below gets every event and
  // hit-tests the zoom control and the minimap itself (handle_overlay_click).
  painted_->set_hit_testable(false);
  viewport_->set_accessible_name("Layout canvas");
  viewport_->set_tooltip("Layout canvas: wheel zooms, middle or right drag pans, F fits");
  overlay_.add(*viewport_);
  overlay_.add(*painted_);
  overlay_.set_accessible_name("Layout canvas");

  viewport_->on_draw([this](const ui::ViewportDraw& target) { draw(target); });
  viewport_->on_resize([this](int width, int height) {
    camera_.set_viewport(width, height);
    controller_.set_pixel_ratio(static_cast<double>(pixel_ratio_()));
    if (auto_fit_) camera_.fit(bounds_);
    camera_changed(true);
  });
  viewport_->on_input([this](const CanvasInput& input) { return handle_input(input); });
  controller_.set_commands(&ctx_.core.dispatcher());
  ctx_.editor().windows.add_observer(*this);
  ensure_window();
}

LayoutCanvas::~LayoutCanvas() {
  ctx_.editor().windows.remove_observer(*this);
}

view::LayoutWindow* LayoutCanvas::window() const {
  return window_id_ >= 0 ? ctx_.editor().windows.find(window_id_) : nullptr;
}

void LayoutCanvas::ensure_window() {
  if (window() != nullptr) return;
  view::WindowManager& windows = ctx_.editor().windows;
  if (view::LayoutWindow* current = windows.current()) {
    window_id_ = current->id();
    return;
  }
  // The desktop app shows one layout window from the start ("Opening and
  // closing windows"); its root is set when a cell is loaded.
  auto opened = windows.open(ctx_.editor().edit_cell, view::ScreenRect{0, 0, 999, 999});
  if (opened) {
    window_id_ = opened.value()->id();
    windows.set_current(window_id_);
  } else {
    ctx_.core.dispatcher().report(opened.error());
  }
}

void LayoutCanvas::show_rects(const std::vector<render::LayeredRect>& rects,
                              const std::vector<render::LayerStyle>& styles) {
  upload(rects, styles);
  synthetic_ = true;
  overlay_state_ = paint::OverlayState{};
  overlay_state_.bounds = to_rect(bounds_);
  empty_hint_.clear();
  camera_.fit(bounds_);
  camera_changed(true);
}

void LayoutCanvas::upload(const std::vector<render::LayeredRect>& rects,
                          const std::vector<render::LayerStyle>& styles) {
  auto index = std::make_shared<const render::CanvasIndex>(render::CanvasIndex::build(rects));
  renderer_->set_layer_styles(styles);
  (void)renderer_->upload(index->instances());
  bounds_ = index->bounds();
  controller_.set_bounds(bounds_);
  const std::lock_guard guard(view_mutex_);
  index_ = std::move(index);
}

void LayoutCanvas::refresh_scene() {
  ensure_window();
  view::LayoutWindow* win = window();
  editor::EditorState& ed = ctx_.editor();
  if (win != nullptr && win->root() == nullptr && ed.edit_cell != nullptr) {
    // "Loading a cell into a window": the first loaded cell fills the empty window.
    win->set_root(ed.edit_cell);
  }
  const db::CellDef* root = win != nullptr ? win->root() : nullptr;
  if (root == nullptr && synthetic_) return;  // the benchmark's rectangles
  synthetic_ = false;
  if (root == nullptr || !ed.styles || !ed.technology) {
    upload({}, {});
    overlay_state_.cells.clear();
    overlay_state_.labels.clear();
    overlay_state_.crosses.clear();
    empty_hint_ = root == nullptr ? "No cell loaded: open a .mag file (Open) or type  load <cell>"
                                  : "No display styles for this technology: layers cannot be drawn";
    refresh_overlay();
    return;
  }
  const bool first = bounds_.empty();
  auto built = display::build_scene(*root, *ed.technology, *ed.styles, ed.fonts, scene_options(*win, *root));
  if (!built) {
    ctx_.core.dispatcher().report(built.error());
    return;
  }
  const display::LayoutScene& scene = built.value();
  const render::LayoutBatches batches = render::to_batches(scene);
  upload(layered(batches.rects), batches.styles);
  // The fit covers the drawn scene and the root's bounding box (subcells).
  bounds_ = to_bounds(scene.bounds.is_null() ? root->bbox() : db::include(scene.bounds, root->bbox()));
  controller_.set_bounds(bounds_);
  set_scene_overlay(scene);
  empty_hint_.clear();
  if (first) {
    camera_.fit(bounds_);
    auto_fit_ = true;
  }
  camera_changed(first);
}

void LayoutCanvas::set_scene_overlay(const display::LayoutScene& scene) {
  overlay_state_.labels = scene.labels;
  overlay_state_.cells = scene.cells;
  overlay_state_.crosses.clear();
  for (const display::SceneLayer& layer : scene.layers) {
    if (layer.style.fill == display::FillKind::cross && !layer.rects.empty()) overlay_state_.crosses.push_back(layer);
  }
  overlay_state_.bounds = to_rect(bounds_);
}

void LayoutCanvas::refresh_overlay() {
  painted_->invalidate();
}

void LayoutCanvas::pan_pixels(double dx, double dy) {
  camera_.pan_pixels(dx, dy);
  auto_fit_ = false;
  camera_changed(true);
}

void LayoutCanvas::set_realtime(bool realtime) {
  viewport_->set_realtime(realtime);
}

std::size_t LayoutCanvas::instance_count() const {
  const std::lock_guard guard(view_mutex_);
  return index_ ? index_->instances().size() : 0;
}

UnitScale LayoutCanvas::unit_scale() const {
  const editor::EditorState& ed = ctx_.editor();
  const auto cif = ed.technology ? ed.technology->cif_output_grid() : std::optional<tech::CifGridInfo>();
  return UnitScale{ed.grid.internal, ed.grid.lambda, microns_per_lambda(cif)};
}

void LayoutCanvas::on_window_changed(view::WindowId id, view::Change change) {
  if (id != window_id_) {
    if (change == view::Change::opened && window() == nullptr) window_id_ = id;
    return;
  }
  switch (change) {
    case view::Change::view:
      // A core command (zoom, view, findbox, scroll) moved the view.
      if (const view::LayoutWindow* win = window()) camera_from_mapping(win->mapping(), camera_);
      auto_fit_ = false;
      camera_changed(false);
      break;
    case view::Change::closed:
      window_id_ = -1;
      refresh_scene();
      break;
    default:
      refresh_scene();
      break;
  }
}

void LayoutCanvas::camera_changed(bool to_core) {
  {
    const std::lock_guard guard(view_mutex_);
    snapshot_ = Snapshot{camera_.transform(), camera_.visible_window(), camera_.units_per_pixel()};
  }
  if (to_core && camera_.viewport_width() > 0) {
    if (view::LayoutWindow* win = window()) win->set_mapping(mapping_from_camera(camera_));
  }
  viewport_->invalidate();
  painted_->invalidate();
  if (view_changed_) view_changed_();
}

bool LayoutCanvas::handle_input(const CanvasInput& input) {
  track_cursor(input);
  if (input.kind == InputKind::pointer_down && input.button == PointerButton::left &&
      handle_overlay_click(input.x, input.y)) {
    return true;
  }
  const bool changed = controller_.handle(input);
  if (changed) {
    auto_fit_ = false;
    camera_changed(true);
  }
  return changed;
}

bool LayoutCanvas::handle_overlay_click(double x, double y) {
  const auto width = static_cast<double>(camera_.viewport_width()) / static_cast<double>(pixel_ratio_());
  const auto height = static_cast<double>(camera_.viewport_height()) / static_cast<double>(pixel_ratio_());
  const OverlayLayout layout = overlay_layout(width, height);
  switch (hit_test(layout, x, y)) {
    // Magic's `zoom`: a factor below 1 zooms in, above 1 zooms out.
    case OverlayHit::zoom_in: ctx_.run("zoom 0.5"); return true;
    case OverlayHit::zoom_out: ctx_.run("zoom 2"); return true;
    case OverlayHit::zoom_label: ctx_.run("view"); return true;
    case OverlayHit::minimap: {
      if (bounds_.empty()) return true;
      const db::Point p = minimap_to_world(to_rect(bounds_), layout.minimap.w, layout.minimap.h,
                                           x - layout.minimap.x, y - layout.minimap.y);
      camera_ = render::Camera(p.x, p.y, camera_.pixels_per_unit());
      const auto ratio = static_cast<double>(pixel_ratio_());
      camera_.set_viewport(static_cast<int>(width * ratio), static_cast<int>(height * ratio));
      auto_fit_ = false;
      camera_changed(true);
      return true;
    }
    case OverlayHit::none: return false;
  }
  return false;
}

void LayoutCanvas::track_cursor(const CanvasInput& input) {
  if (input.kind != InputKind::pointer_move && input.kind != InputKind::pointer_down) return;
  const double ratio = static_cast<double>(pixel_ratio_());
  cursor_ = db::Point{static_cast<db::Coord>(std::lround(camera_.world_x(input.x * ratio))),
                      static_cast<db::Coord>(std::lround(camera_.world_y(input.y * ratio)))};
  if (view_changed_) view_changed_();
}

void LayoutCanvas::paint_overlay(ui::Painter& p, float w, float h) {
  overlay_state_.camera = camera_;
  overlay_state_.pixel_ratio = static_cast<double>(pixel_ratio_());
  overlay_state_.scale = unit_scale();
  overlay_state_.box = ctx_.editor().box;
  overlay_state_.empty_hint = empty_hint_;
  render::Camera fitted = camera_;
  fitted.fit(bounds_);
  overlay_state_.fit_pixels_per_unit = fitted.pixels_per_unit();
  paint::canvas_overlay(p, w, h, overlay_state_, ctx_.mode);
}

void LayoutCanvas::draw(const ui::ViewportDraw& target) {
  Snapshot view;
  std::shared_ptr<const render::CanvasIndex> index;
  {
    const std::lock_guard guard(view_mutex_);
    view = snapshot_;
    index = index_;
  }
  if (index) {
    index->visible_ranges(view.window, view.units_per_pixel, ranges_);
  } else {
    ranges_.clear();
  }
  // The canvas keeps the design-system "canvas" color in both themes.
  const Rgba& bg = theme_tokens(ThemeMode::dark).canvas;
  render::DrawTarget draw_target{target.command_buffer, target.render_pass, target.x, target.y, target.width,
                                 target.height, false, true, {bg.r, bg.g, bg.b, 1.0F}};
  renderer_->record(draw_target, view.transform, ranges_);
}

}  // namespace hefesto::shell
