// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The center of the Desktop-Layout screen: the Vulkan canvas (a Raven
// viewport in draw-callback mode) with the painted overlay on top of it
// (dot grid, rulers, subcell names, bitmap label text, highlight text and
// point crosses, the zoom control and the minimap). The box, the selection
// and the other highlights are a second GPU pass over the layout.
//
// The canvas shows the core's current layout window (design.md D5): it
// observes `editor.windows`, maps the window's ViewMapping to the camera,
// rebuilds the scene with display::build_scene and uploads
// render::to_batches when the window's content, expansion or visibility
// changes, and writes the camera back into the window's mapping after a
// gesture, so core commands see the view the user sees. Keys issue core
// Buttons, keys and pointer motion go through the core's macro tables
// (InputRouter); continuous gestures move the camera (CanvasController);
// the zoom control issues `zoom`.
// Raven-free: built from the raven_ui wrappers.

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "hefesto/display/scene.hpp"
#include "hefesto/render/camera.hpp"
#include "hefesto/render/canvas_index.hpp"
#include "hefesto/render/renderer.hpp"
#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/canvas_overlay.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/view/window.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"
#include "screen_paint.hpp"

namespace hefesto::shell {

class LayoutCanvas final : public view::ViewObserver {
 public:
  /// `pixel_ratio` returns the window's logical-to-pixel scale.
  LayoutCanvas(ScreenContext& ctx, std::mutex& queue_mutex, std::function<float()> pixel_ratio);
  ~LayoutCanvas() override;
  LayoutCanvas(const LayoutCanvas&) = delete;
  LayoutCanvas& operator=(const LayoutCanvas&) = delete;

  [[nodiscard]] ui::Widget& widget() noexcept { return overlay_; }
  /// Gives the canvas the keyboard focus (F6), so keys reach the macro tables.
  void focus() { viewport_->focus(); }

  /// Shows rectangles that are not a cell (the benchmark's synthetic layout).
  void show_rects(const std::vector<render::LayeredRect>& rects, const std::vector<render::LayerStyle>& styles);
  /// Rebuilds the whole scene of the current window from the editor session.
  void refresh_scene();
  /// Performs the window's pending redisplay ("Change-driven redisplay"):
  /// rebuilds only the changed areas of the retained scene, the whole scene
  /// when the root, technology or styles changed, nothing when nothing is
  /// pending or the display is suspended.
  void update_scene();
  /// Repaints the overlay (theme changed).
  void refresh_overlay();
  /// Rebuilds the highlight pass (box, selection, feedback, elements,
  /// crosshair, wiring preview, rulers, `*watch` tile overlay) when it
  /// changed. Called after every command.
  void refresh_highlights();
  void pan_pixels(double dx, double dy);
  void set_realtime(bool realtime);
  /// Work deferred until the edits pause (the minimap of a large scene).
  /// Call once per frame; true while some is still pending.
  bool idle();

  [[nodiscard]] std::size_t instance_count() const;
  /// True once the viewport has its size (after the first layout).
  [[nodiscard]] bool viewport_ready() const noexcept { return camera_.viewport_width() > 0; }
  /// The layout point under the pointer, in internal units.
  [[nodiscard]] const std::optional<db::Point>& cursor() const noexcept { return cursor_; }
  [[nodiscard]] UnitScale unit_scale() const;
  /// The zoom control's text ("400%": relative to the zoom that fits the cell).
  [[nodiscard]] std::string zoom_text() const;
  /// Called when the cursor moves or the view changes (status bar).
  void on_view_changed(std::function<void()> callback) { view_changed_ = std::move(callback); }
  /// The canvas's button, key and motion path through the macro tables.
  [[nodiscard]] InputRouter& router() noexcept { return router_; }
  /// Shows the pointer shape of the current tool and cursor glyph.
  void update_cursor();

  void on_window_changed(view::WindowId id, view::Change change) override;

  /// Adds a widget over the canvas (the job-completion toast).
  void add_overlay(const ui::Widget& child) { overlay_.add(child); }
  /// The window point (logical units) just right of the top-right corner of
  /// `rect` (root coordinates), clamped into the canvas: where the `drc why`
  /// popover opens.
  [[nodiscard]] std::pair<float, float> popover_point(const db::Rect& rect) const;

 private:
  struct Snapshot {
    render::ViewTransform transform;
    render::WorldWindow window;
    double units_per_pixel = 1.0;
  };

  [[nodiscard]] view::LayoutWindow* window() const;
  void ensure_window();
  void upload(const std::vector<render::LayeredRect>& rects, const std::vector<render::LayerStyle>& styles);
  [[nodiscard]] bool needs_full_rebuild(const view::LayoutWindow& win) const;
  void apply_redisplay(view::LayoutWindow& win, const view::Redisplay& redisplay);
  /// Uploads scene_ and refreshes the overlay's labels, cells and bounds.
  /// `full`: after a whole-scene build (the minimap is redone at once).
  void publish_scene(const db::CellDef& root, bool full);
  void set_scene_overlay(const display::LayoutScene& scene, bool full);
  /// Uploads the scene's triangles and glyphs (drawn merged with the rectangles by layer).
  void upload_polygons(render::PolygonScene polygons);
  /// Publishes the camera to the render thread; `to_core` also writes it
  /// into the window's mapping (after a gesture or a resize).
  void camera_changed(bool to_core);
  bool handle_input(const CanvasInput& input);
  bool route_input(const CanvasInput& input);
  [[nodiscard]] PointerContext pointer(double x, double y) const;
  bool handle_overlay_click(double x, double y);
  void track_cursor(const CanvasInput& input);
  void draw(const ui::ViewportDraw& target);
  void paint_overlay(ui::Painter& p, float w, float h);
  void fit_view();

  ScreenContext& ctx_;
  std::function<float()> pixel_ratio_;
  ui::Overlay overlay_ = ui::Overlay::create();
  std::unique_ptr<ui::Viewport> viewport_;
  std::unique_ptr<ui::Painted> painted_;

  std::unique_ptr<render::CanvasRenderer> renderer_;
  // The highlights: a second instance set drawn over the layout
  // (render::highlight_batches), rebuilt when a command changes them.
  std::unique_ptr<render::CanvasRenderer> highlight_renderer_;
  std::vector<display::HighlightItem> highlights_;  // what highlight_renderer_ holds
  render::Camera camera_;
  CanvasController controller_{camera_};
  InputRouter router_;
  std::optional<CanvasInput> last_pointer_;  // for keys: the pointer over the canvas
  render::Bounds bounds_;
  bool auto_fit_ = true;  // refit on resize until the user moves the view
  // The root the camera was last framed for: a new root (load, the first
  // cell) frames as the core does, not the previous cell's camera. Compared,
  // never dereferenced.
  const db::CellDef* framed_root_ = nullptr;
  bool synthetic_ = false;  // show_rects content stays until a cell is shown
  // The retained scene and what it was built from (compared, never dereferenced).
  display::LayoutScene scene_;
  bool have_scene_ = false;
  const db::CellDef* built_root_ = nullptr;
  const tech::Technology* built_tech_ = nullptr;
  bool built_styles_ = false;
  view::WindowId window_id_ = -1;
  std::optional<db::Point> cursor_;
  paint::OverlayState overlay_state_;
  std::string empty_hint_;
  std::optional<std::chrono::steady_clock::time_point> minimap_due_;
  std::function<void()> view_changed_;

  mutable std::mutex view_mutex_;  // guards snapshot_, index_ and polygons_ (the render thread reads them)
  Snapshot snapshot_;
  std::shared_ptr<const render::CanvasIndex> index_;
  std::shared_ptr<const render::PolygonScene> polygons_;  // batches only (vertices are on the GPU)
  std::vector<render::LayerBatch> ranges_;          // render thread only
  std::vector<render::LayerBatch> polygon_ranges_;  // render thread only
  std::vector<render::LayerBatch> highlight_draws_;       // guarded by view_mutex_
  std::vector<render::LayerBatch> highlight_poly_draws_;  // guarded by view_mutex_
  std::vector<render::LayerBatch> highlight_ranges_;       // render thread only
  std::vector<render::LayerBatch> highlight_poly_ranges_;  // render thread only
};

}  // namespace hefesto::shell
