// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "layout_canvas.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/render/layout_scene.hpp"
#include "hefesto/ui/state.hpp"
#include "hefesto/view/highlights.hpp"
#include "hefesto/view/tile_overlay.hpp"
#include "hefesto/ui/tools.hpp"

namespace hefesto::shell {

namespace {

#if defined(__APPLE__)
constexpr bool kMacOs = true;  // Key event normalization: any of Mod1-Mod5 is Meta
#else
constexpr bool kMacOs = false;
#endif

/// Scenes up to this many rectangles refresh the minimap with every edit.
constexpr std::size_t kMinimapSyncRects = 1'000'000;
/// How long the edits of a larger scene must pause before the minimap is redone.
constexpr std::chrono::milliseconds kMinimapDelay{750};

std::size_t scene_rect_count(const display::LayoutScene& scene) {
  std::size_t n = 0;
  for (const display::SceneLayer& layer : scene.layers) n += layer.rects.size();
  return n;
}

/// Half the side of the area the highlight pass covers (internal units).
constexpr db::Coord kHighlightReach = 1 << 28;

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

/// The options of a scene of `window`: everything the root holds (its
/// bounding box includes its subcells, the extended one font-label text), or
/// only `area` for a partial rebuild.
display::SceneOptions scene_options(const view::LayoutWindow& window, const db::CellDef& root,
                                    const std::optional<db::Rect>& area = std::nullopt) {
  display::SceneOptions options;
  const db::Rect all = db::include(root.bbox(), root.extended_bbox());
  if (area) {
    options.area = *area;
  } else if (!all.is_null()) {
    options.area = all;
  }
  options.visible = window.visible_types();
  options.show_labels = window.labels_visible();
  options.expanded = [&window](const db::CellUse& use) { return window.is_expanded(use); };
  return options;
}

}  // namespace

LayoutCanvas::LayoutCanvas(ScreenContext& ctx, std::mutex& queue_mutex, std::function<float()> pixel_ratio)
    : ctx_(ctx),
      pixel_ratio_(std::move(pixel_ratio)),
      viewport_(ui::Viewport::create()),
      router_(ctx.core.dispatcher(), ctx.core.editor(), kMacOs) {
  render::QueueLock lock{[&queue_mutex] { queue_mutex.lock(); }, [&queue_mutex] { queue_mutex.unlock(); }};
  render::QueueLock highlight_lock{[&queue_mutex] { queue_mutex.lock(); }, [&queue_mutex] { queue_mutex.unlock(); }};
  renderer_ = std::make_unique<render::CanvasRenderer>(ctx_.runtime.vulkan_device(), std::move(lock));
  highlight_renderer_ =
      std::make_unique<render::CanvasRenderer>(ctx_.runtime.vulkan_device(), std::move(highlight_lock));
  painted_ = ui::Painted::create(ctx_.runtime, [this](ui::Painter& p, float w, float h) { paint_overlay(p, w, h); });
  // The overlay never takes input: the viewport below gets every event and
  // hit-tests the zoom control and the minimap itself (handle_overlay_click).
  painted_->set_hit_testable(false);
  viewport_->set_accessible_name("Layout canvas");
  viewport_->set_tooltip(
      "Layout canvas: buttons and keys run the current tool's bindings; wheel or pinch zooms, two-finger "
      "scroll or Option-drag pans");
  overlay_.add(*viewport_);
  overlay_.add(*painted_);
  overlay_.set_accessible_name("Layout canvas");

  viewport_->on_draw([this](const ui::ViewportDraw& target) { draw(target); });
  viewport_->on_resize([this](int width, int height) {
    camera_.set_viewport(width, height);
    controller_.set_pixel_ratio(static_cast<double>(pixel_ratio_()));
    if (auto_fit_) fit_view();
    camera_changed(true);
  });
  viewport_->on_input([this](const CanvasInput& input) { return handle_input(input); });
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
  upload_polygons({});
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
    framed_root_ = nullptr;
    have_scene_ = false;
    upload({}, {});
    upload_polygons({});
    overlay_state_.cells.clear();
    overlay_state_.minimap.clear();
    overlay_state_.labels.clear();
    empty_hint_ = root == nullptr ? "No cell loaded: open a .mag file (Open) or type  load <cell>"
                                  : "No display styles for this technology: layers cannot be drawn";
    refresh_overlay();
    return;
  }
  // hefesto-architecture "Concurrency model": the scene is built as a
  // database reader, beside the background DRC worker (m3-drc design D8).
  auto built = [&] {
    const db::DatabaseLock::Shared reading(ed.database_lock);
    return display::build_scene(*root, *ed.technology, *ed.styles, ed.fonts, scene_options(*win, *root));
  }();
  if (!built) {
    ctx_.core.dispatcher().report(built.error());
    return;
  }
  scene_ = std::move(built).value();
  have_scene_ = true;
  built_root_ = root;
  built_tech_ = ed.technology.get();
  built_styles_ = true;
  (void)win->take_redisplay();  // the whole scene covers whatever was pending
  publish_scene(*root, true);
  // A new root takes the core's load frame (set_root moved the core mapping
  // there without a Change::view); keeping the old camera made the next core
  // zoom jump.
  const bool new_root = root != framed_root_;
  if (new_root) {
    framed_root_ = root;
    auto_fit_ = true;
    fit_view();
  }
  camera_changed(new_root);
}

void LayoutCanvas::publish_scene(const db::CellDef& root, bool full) {
  const render::LayoutBatches batches = render::to_batches(scene_);
  upload(layered(batches.rects), batches.styles);
  upload_polygons(batches.polygons);
  // The fit covers the drawn scene and the root's bounding box (subcells).
  bounds_ = to_bounds(scene_.bounds.is_null() ? root.bbox() : db::include(scene_.bounds, root.bbox()));
  controller_.set_bounds(bounds_);
  set_scene_overlay(scene_, full);
  empty_hint_.clear();
}

bool LayoutCanvas::needs_full_rebuild(const view::LayoutWindow& win) const {
  const editor::EditorState& ed = ctx_.editor();
  if (win.root() == nullptr) return true;  // the empty window or the first cell
  return !have_scene_ || synthetic_ || win.root() != built_root_ || ed.technology.get() != built_tech_ ||
         ed.styles.has_value() != built_styles_;
}

void LayoutCanvas::update_scene() {
  ensure_window();
  view::LayoutWindow* win = window();
  if (have_scene_ && ctx_.editor().windows.display_suspended()) return;  // `updatedisplay suspend`
  if (win == nullptr || needs_full_rebuild(*win)) {
    refresh_scene();
    return;
  }
  apply_redisplay(*win, win->take_redisplay());
}

void LayoutCanvas::apply_redisplay(view::LayoutWindow& win, const view::Redisplay& redisplay) {
  if (redisplay.empty()) return;
  if (redisplay.all) {
    refresh_scene();
    return;
  }
  editor::EditorState& ed = ctx_.editor();
  const db::CellDef& root = *win.root();
  for (const db::Rect& area : splice_areas(redisplay.areas)) {
    auto partial = [&] {
      const db::DatabaseLock::Shared reading(ed.database_lock);
      return display::build_scene(root, *ed.technology, *ed.styles, ed.fonts, scene_options(win, root, area));
    }();
    if (!partial || !display::splice_scene(scene_, std::move(partial).value(), area)) {
      refresh_scene();  // a cut the retained scene cannot take
      return;
    }
  }
  publish_scene(root, false);
  camera_changed(false);
}

void LayoutCanvas::fit_view() {
  const view::LayoutWindow* win = window();
  if (!synthetic_ && win != nullptr && win->root() != nullptr) {
    frame_root(*win->root(), camera_);
  } else {
    camera_.fit(bounds_);
  }
}

void LayoutCanvas::set_scene_overlay(const display::LayoutScene& scene, bool full) {
  overlay_state_.labels = scene.labels;
  overlay_state_.cells = scene.cells;
  overlay_state_.bounds = to_rect(bounds_);
  // The thumbnail walks every rectangle of the scene (about 0.5 s for the
  // 13.7 M rectangles of the 100 x 100 dfxtp_1 array in a Debug build): after
  // an edit of a large scene it is redone once the edits pause (idle()).
  if (!full && scene_rect_count(scene) > kMinimapSyncRects) {
    minimap_due_ = std::chrono::steady_clock::now() + kMinimapDelay;
    return;
  }
  minimap_due_.reset();
  // The canvas keeps the dark canvas color in both themes; so does the thumbnail.
  overlay_state_.minimap = minimap_thumbnail(scene, overlay_state_.bounds, theme_tokens(ThemeMode::dark).canvas,
                                             kMinimapWidth, kMinimapHeight);
}

bool LayoutCanvas::idle() {
  if (!minimap_due_ || std::chrono::steady_clock::now() < *minimap_due_) return minimap_due_.has_value();
  minimap_due_.reset();
  overlay_state_.minimap = minimap_thumbnail(scene_, overlay_state_.bounds, theme_tokens(ThemeMode::dark).canvas,
                                             kMinimapWidth, kMinimapHeight);
  painted_->invalidate();
  return false;
}

void LayoutCanvas::upload_polygons(render::PolygonScene polygons) {
  (void)renderer_->upload_polygons(polygons.vertices, polygons.base_x, polygons.base_y);
  polygons.vertices.clear();  // on the GPU now; the render thread needs only the batches
  auto shared = std::make_shared<const render::PolygonScene>(std::move(polygons));
  const std::lock_guard guard(view_mutex_);
  polygons_ = std::move(shared);
}

void LayoutCanvas::refresh_overlay() {
  painted_->invalidate();
}

void LayoutCanvas::set_drc_tag(std::optional<DrcCanvasTag> tag) {
  if (tag == overlay_state_.drc_tag) return;
  overlay_state_.drc_tag = std::move(tag);
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

std::string LayoutCanvas::zoom_text() const {
  render::Camera fitted = camera_;
  fitted.fit(bounds_);
  return zoom_percent_text(camera_.pixels_per_unit(), fitted.pixels_per_unit());
}

void LayoutCanvas::on_window_changed(view::WindowId id, view::Change change) {
  if (id != window_id_) {
    if (change == view::Change::opened && window() == nullptr) window_id_ = id;
    return;
  }
  switch (change) {
    case view::Change::view:
      // A core command (zoom, view, findbox, scroll, load) moved the view.
      // Build the scene first if the window has none yet, so its first fit
      // cannot replace the command's view later.
      if (const view::LayoutWindow* win = window()) {
        const view::ViewMapping mapping = win->mapping();  // the first fit below writes the mapping
        if (bounds_.empty()) refresh_scene();
        camera_from_mapping(mapping, camera_);
      }
      auto_fit_ = false;
      camera_changed(false);
      break;
    case view::Change::closed:
      window_id_ = -1;
      refresh_scene();
      break;
    case view::Change::grid:
    case view::Change::caption:
      refresh_overlay();
      break;
    default:
      // content, expansion, visibility, opened: the window holds the pending
      // redisplay; the frame after the command performs it (update_scene).
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
  // An Option-drag pans unless the tool binds Meta_ButtonN; the wheel, the
  // trackpad and a pinch move the camera directly.
  const bool pan_press = input.kind == InputKind::pointer_down && (input.modifiers & kModAlt) != 0 &&
                         !router_.has_button_macro(static_cast<int>(input.button) + 1, input.modifiers) &&
                         router_.mode() == EditMode::none;
  const bool gesture = input.kind == InputKind::wheel || input.kind == InputKind::pinch || pan_press ||
                       controller_.dragging();
  if (gesture) {
    const bool changed = controller_.handle(input);
    if (changed) {
      auto_fit_ = false;
      camera_changed(true);
    }
    return changed;
  }
  return route_input(input);
}

PointerContext LayoutCanvas::pointer(double x, double y) const {
  const view::LayoutWindow* win = window();
  return pointer_at(camera_, static_cast<double>(pixel_ratio_()), x, y, window_id_,
                    win != nullptr ? win->name() : std::string());
}

bool LayoutCanvas::route_input(const CanvasInput& input) {
  if (window() == nullptr || synthetic_) return false;
  bool used = false;
  switch (input.kind) {
    case InputKind::pointer_down:
    case InputKind::pointer_up: {
      const int button = input.button == PointerButton::left     ? 1
                         : input.button == PointerButton::middle ? 2
                         : input.button == PointerButton::right  ? 3
                                                                 : 0;
      used = router_.button(button, input.kind == InputKind::pointer_down, input.modifiers, pointer(input.x, input.y));
      break;
    }
    case InputKind::pointer_move: used = router_.motion(pointer(input.x, input.y)); break;
    case InputKind::key_down:
      if (const auto event = key_event_for(input.native_key, input.modifiers)) {
        std::optional<PointerContext> at;
        if (last_pointer_) at = pointer(last_pointer_->x, last_pointer_->y);
        used = router_.key(*event, at);
      }
      break;
    default: break;
  }
  if (used) {
    update_cursor();
    painted_->invalidate();  // the sweep preview
  }
  return used;
}

void LayoutCanvas::update_cursor() {
  const editor::EditorState& ed = ctx_.editor();
  const hefesto::ui::Tool* tool = ed.tools.current();
  const EditMode mode = tool == nullptr || tool->name == "box" ? router_.mode() : EditMode::none;
  viewport_->set_cursor(cursor_shape(hefesto::ui::state(ed).cursor_glyph, mode));
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
  last_pointer_ = input;
  const double ratio = static_cast<double>(pixel_ratio_());
  cursor_ = db::Point{static_cast<db::Coord>(std::lround(camera_.world_x(input.x * ratio))),
                      static_cast<db::Coord>(std::lround(camera_.world_y(input.y * ratio)))};
  if (view_changed_) view_changed_();
}

void LayoutCanvas::paint_overlay(ui::Painter& p, float w, float h) {
  overlay_state_.camera = camera_;
  overlay_state_.pixel_ratio = static_cast<double>(pixel_ratio_());
  overlay_state_.scale = unit_scale();
  const editor::EditorState& ed = ctx_.editor();
  overlay_state_.sweep = router_.sweep();
  const view::LayoutWindow* win = window();
  overlay_state_.edit_bbox.reset();
  if (ed.edit_cell != nullptr && win != nullptr && win->root() == ed.edit_cell && !ed.edit_cell->bbox().is_null()) {
    overlay_state_.edit_bbox = ed.edit_cell->bbox();
  }
  if (ed.styles) {
    const std::size_t styles[3] = {display::kStyleBox, display::kStyleMediumHighlights,
                                   display::kStyleOutlineHighlights};
    for (std::size_t i = 0; i < 3; ++i) {
      const display::ResolvedStyle r = display::resolve_style(*ed.styles, styles[i]);
      overlay_state_.highlight[i] = Rgba{r.r, r.g, r.b, 1.0F};
    }
  }
  overlay_state_.empty_hint = empty_hint_;
  render::Camera fitted = camera_;
  fitted.fit(bounds_);
  overlay_state_.fit_pixels_per_unit = fitted.pixels_per_unit();
  paint::canvas_overlay(p, w, h, overlay_state_, ctx_.mode);
}

void LayoutCanvas::draw(const ui::ViewportDraw& target) {
  Snapshot view;
  std::shared_ptr<const render::CanvasIndex> index;
  std::shared_ptr<const render::PolygonScene> polygons;
  {
    const std::lock_guard guard(view_mutex_);
    view = snapshot_;
    index = index_;
    polygons = polygons_;
  }
  if (index) {
    index->visible_ranges(view.window, view.units_per_pixel, ranges_);
  } else {
    ranges_.clear();
  }
  if (polygons && view.units_per_pixel > 0.0) {
    render::visible_polygon_batches(*polygons, 1.0 / view.units_per_pixel, polygon_ranges_);
  } else {
    polygon_ranges_.clear();
  }
  // The canvas keeps the design-system "canvas" color in both themes.
  const Rgba& bg = theme_tokens(ThemeMode::dark).canvas;
  render::DrawTarget draw_target{target.command_buffer, target.render_pass, target.x, target.y, target.width,
                                 target.height, false, true, {bg.r, bg.g, bg.b, 1.0F}};
  renderer_->record(draw_target, view.transform, ranges_, polygon_ranges_);
  {
    const std::lock_guard guard(view_mutex_);
    highlight_ranges_ = highlight_draws_;
    highlight_poly_ranges_ = highlight_poly_draws_;
  }
  // Recorded every frame (even empty), so the renderer retires replaced buffers.
  draw_target.clear = false;
  highlight_renderer_->record(draw_target, view.transform, highlight_ranges_, highlight_poly_ranges_);
}

void LayoutCanvas::refresh_highlights() {
  editor::EditorState& ed = ctx_.editor();
  view::LayoutWindow* win = window();
  display::HighlightScene scene;
  if (win != nullptr && win->root() != nullptr && !synthetic_) {
    // Everything, not only the visible surface: gestures move the camera
    // without a command, so the pass must cover what they may reveal.
    const db::Rect everywhere{-kHighlightReach, -kHighlightReach, kHighlightReach, kHighlightReach};
    scene = ed.highlights.build(ed, *win, everywhere);
    display::HighlightScene tiles = view::tile_overlay_scene(ed, *win, everywhere);
    for (display::HighlightItem& item : tiles.items) {
      display::resolve_highlight(item, ed.styles ? &*ed.styles : nullptr);
      scene.items.push_back(std::move(item));
    }
  }
  if (scene.items == highlights_) return;
  highlights_ = scene.items;
  render::LayoutBatches batches = render::highlight_batches(scene);
  highlight_renderer_->set_layer_styles(batches.styles);
  (void)highlight_renderer_->upload(batches.rects.instances());
  (void)highlight_renderer_->upload_polygons(batches.polygons.vertices, batches.polygons.base_x,
                                             batches.polygons.base_y);
  {
    const std::lock_guard guard(view_mutex_);
    highlight_draws_.assign(batches.rects.batches().begin(), batches.rects.batches().end());
    highlight_poly_draws_ = batches.polygons.batches;
  }
  // Text and point crosses are the overlay's (their size is in pixels).
  overlay_state_.marks.clear();
  for (const display::HighlightItem& item : highlights_) {
    if (item.shape == display::HighlightShape::text || item.shape == display::HighlightShape::cross) {
      overlay_state_.marks.push_back(item);
    }
  }
  viewport_->invalidate();
  painted_->invalidate();
}

std::pair<float, float> LayoutCanvas::popover_point(const db::Rect& rect) const {
  const ui::Rect area = overlay_.rect();
  const PixelRect r = world_to_pixels(camera_, rect, static_cast<double>(pixel_ratio_()));
  const double x = std::clamp(r.x + r.w + 8.0, 0.0, std::max(0.0, static_cast<double>(area.width) - 280.0));
  const double y = std::clamp(r.y, 0.0, std::max(0.0, static_cast<double>(area.height) - 120.0));
  return {area.x + static_cast<float>(x), area.y + static_cast<float>(y)};
}

}  // namespace hefesto::shell
