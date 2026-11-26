// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop's 3D view (the editor-view tab "3D"; openspec/changes/
// m10-3d-view/design.md D8, D9, D10, D12, tasks 9.1, 9.2, 9.6, 13.2,
// 16.6-16.8, 16.10; Q-MISC-84 board look, Q-MISC-85 ghosted layers; board
// docs/design/mockups/View3D.dc.html; round 5: floating toolbar, P2
// Cross-section and Export glTF, task 11.5): a second Raven viewport, like
// LayoutCanvas, in texture mode, showing the render::VulkanOffscreenTarget
// that render::VulkanMesh3dRenderer draws into, with the layer stack panel
// and the view, parity and inspect controls beside it.
//
// - It is the session's view3d::WindowDisplay (Session::display): the client
//   calls redraw() when the window opens and after every 3D command, and
//   closed() on `closewindow`; both only record what to do, and on_frame()
//   (UI thread, outside commands) does it.
// - One view3d::MeshWorker per window builds the mesh in the background
//   (BuildInputs::lock = the session's database lock); a change of the
//   stack, clip area, cell or J1 option runs a full build, and the layout
//   windows' change areas for the rendered cell feed changed(). Worker
//   callbacks are posted to the UI loop; the UI thread never waits on a
//   build, and the previous model stays visible until the next is done.
// - Mouse, trackpad and keys: gestures (view3d/camera/input.hpp) move the
//   window's ViewState as `view`/`scroll`/`zoom` would; keys run the
//   `wind3d` macro table on the window's toolkit path; a click picks.
// - Commands typed while the 3D tab is shown go to the window's path when
//   their first word is a `wind3d` command (route_typed_line).
// Raven-free: built from the raven_ui wrappers.
//
// Owner: SHELL (design D3).

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/render/mesh3d_target.hpp"
#include "hefesto/render/mesh3d_vulkan_renderer.hpp"
#include "hefesto/shell/view3d_models.hpp"
#include "hefesto/view/window.hpp"
#include "hefesto/view3d/client/session.hpp"
#include "hefesto/view3d/mesh/worker.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

class View3dView final : public view3d::WindowDisplay, public view::ViewObserver {
 public:
  /// `queue_mutex` is the lock shared with Raven around the Vulkan queue;
  /// `pixel_ratio` the window's logical-to-pixel scale. Installs itself as
  /// the session's WindowDisplay.
  View3dView(ScreenContext& ctx, std::mutex& queue_mutex, std::function<float()> pixel_ratio);
  ~View3dView() override;
  View3dView(const View3dView&) = delete;
  View3dView& operator=(const View3dView&) = delete;

  [[nodiscard]] ui::Widget& widget() noexcept { return root_; }
  /// Called with true when the 3D window opens, false when it closes.
  void on_open_changed(std::function<void(bool)> callback) { open_changed_ = std::move(callback); }
  /// Called when a pick changed the 2D selection (the main window refreshes).
  void on_selection_changed(std::function<void()> callback) { selection_changed_ = std::move(callback); }
  /// The 3D tab is shown (true) or the Layout tab (false).
  void set_active(bool active);
  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] bool window_open() const;
  void focus() { viewport_->focus(); }
  /// A typed line, routed to the 3D window while its tab is shown.
  [[nodiscard]] std::string route_typed(const std::string& line) const;
  /// After every command: the 2D selection may have changed (highlight).
  void refresh();
  /// Once per frame on the UI thread: syncs with the window and draws.
  void on_frame();

  void redraw(const editor::EditorState& editor, const view3d::WindowState& window) override;
  void closed(const view3d::WindowState& window) override;
  void on_window_changed(view::WindowId id, view::Change change) override;

 private:
  struct Row;
  struct Shared;  // what worker callbacks reach through posted tasks
  struct Control {
    std::unique_ptr<ui::Painted> widget;
    View3dControl id = View3dControl::isometric;
  };
  /// One group of the floating toolbar (flowed into rows by its width).
  struct ToolbarGroup {
    const ui::Widget* widget = nullptr;
    float width = 0.0F;
  };

  void build_stage();
  void build_side();
  void build_actions(ui::Box& box);
  void build_layer_section(ui::Box& box);
  void build_view_section(ui::Box& box);
  void build_inspect_section(ui::Box& box);
  void build_section_panel(ui::Box& box);
  /// The view toolbar floating over the bottom of the 3D view (board, tasks
  /// 16.10, 9.6): Mask / Layout layers, the Level stepper, Cut box and the
  /// mesh status, flowed into as many rows as the view's width needs.
  void build_toolbar();
  /// Re-flows the toolbar's groups for a view `width` logical units wide.
  void layout_toolbar(float width);
  /// Keeps the status group as wide as its text and the toolbar flowed to
  /// the view's width (once per frame).
  void fit_toolbar();
  [[nodiscard]] std::string status_line() const;
  void paint_status(ui::Painter& p, float w, float h);
  void paint_level(ui::Painter& p, float w, float h);
  void paint_gizmo(ui::Painter& p, float h);
  void paint_section(ui::Painter& p, float w, float h);
  /// The height the floating toolbar covers at the bottom of the view.
  [[nodiscard]] float toolbar_cover() const;
  /// Runs `which`'s angles framed to the cell (`view`, `scroll`, `zoom`;
  /// Q-MISC-84), with `scale_z` when given; only the angles while no model
  /// or size is known.
  void apply_camera(view3d::Preset which, std::optional<float> scale_z = std::nullopt);
  /// The isometric default once per window the desktop opened (Q-MISC-84,
  /// default_camera_commands).
  void apply_default_camera();
  /// The camera frame of the drawn mesh (CameraFrame).
  [[nodiscard]] CameraFrame camera_frame() const;
  [[nodiscard]] View3dControlState control_state() const;
  /// A painted button (a toggle when control_view gives a state) whose
  /// label, screen-reader name and enabled state follow control_view.
  ui::Painted& add_button(ui::Box& box, View3dControl id, std::function<void()> click, float width = -1.0F);
  /// Two buttons side by side in the side panel's two-column grid.
  void add_pair(ui::Box& box, std::string_view group_name, std::pair<View3dControl, std::function<void()>> left,
                std::pair<View3dControl, std::function<void()>> right);
  void add_section(ui::Box& box, std::string_view title);
  Row& row_widgets(std::size_t index);

  // P2 actions (task 11.5).
  void toggle_section();
  /// "Export glTF…": the format menu under the button (round 6), then
  /// export_as.
  void export_model();
  /// The save dialog for `format` and `export3d <file> -format …`.
  void export_as(View3dExportFormat format);
  /// The painted widget of control `id` (nullptr when it is not built).
  [[nodiscard]] ui::Painted* control_widget(View3dControl id) const;

  // "Open in separate window" (round 6): the 3D view in a Raven window of
  // its own, a second texture-mode viewport showing the same target; the
  // tab's viewport shows nothing meanwhile. Closing that window (or the
  // toggle, or closing the 3D window) returns the view to the tab.
  void toggle_separate_window();
  void open_separate_window();
  void close_separate_window();
  /// The viewport that shows the frames: the separate window's while it is
  /// open, else the tab's.
  [[nodiscard]] ui::Viewport& shown_viewport() const;
  /// Logical-to-pixel scale of the window that shows the frames.
  [[nodiscard]] float shown_ratio() const;
  /// Wires a viewport's size and input to this view.
  void connect_viewport(ui::Viewport& viewport, bool separate);
  [[nodiscard]] bool export_registered() const;
  /// Recomputes the cross-section of the drawn model along section_line_
  /// (hidden layers left out); `report` sends its diagnostics to the console.
  void update_section(bool report);
  /// A click in the 3D view while the cross-section is on: A, then A'
  /// (on the z = 0 plane; needs view3d::ground_point).
  bool section_click(float x, float y);

  [[nodiscard]] const view3d::WindowState* window() const;
  [[nodiscard]] std::string path() const;
  [[nodiscard]] bool magic_compat() const;
  void run(const std::string& line);
  void ensure_window_hook();
  void ensure_path_command();
  void sync();
  void start_build(const BuildKey& key, view3d::BuildInputs inputs);
  /// Mesh units per root unit of the drawn model (MeshModel::units_per_root; 1 without one).
  [[nodiscard]] std::int32_t mesh_units() const noexcept;
  void take_model(const view3d::MeshHandle& model);
  void refresh_rows();
  /// Rebuilds rows_ and row_colors_ from the drawn (or building) stack.
  void rebuild_rows(const view3d::WindowState& window, const view3d::LayerStack& stack);
  /// The screen-reader names and enabled state of one panel row.
  static void update_row(Row& row, const View3dLayerRow& r, bool editable);
  void repaint();
  bool ensure_renderer();
  void draw_frame();
  void reset_window_state();

  bool handle_input(const CanvasInput& input);
  bool handle_pointer(const CanvasInput& input);
  void apply_gesture(const view3d::Gesture& gesture);
  void pick_at(float x, float y);
  /// Points the resolver at `stack` (the drawn model's), the rendered cell and the compat flag.
  void update_pick_scope(const view3d::LayerStack* stack);

  void paint_stage(ui::Painter& p, float w, float h);
  void paint_row(ui::Painter& p, float w, float h, std::size_t index, bool hovered, bool focused);

  void toggle_row(std::size_t index);
  void isolate_row(std::size_t index);
  void cycle_alpha(std::size_t index);

  ScreenContext& ctx_;
  std::mutex& queue_mutex_;
  std::function<float()> pixel_ratio_;
  ui::Box root_ = ui::Box::horizontal();
  ui::Overlay stage_ = ui::Overlay::create();
  ui::Box toolbar_ = ui::Box::vertical();
  std::vector<ui::Box> toolbar_rows_;
  std::vector<ToolbarGroup> toolbar_groups_;
  float toolbar_width_ = -1.0F;  // the view width the toolbar was flowed for
  std::unique_ptr<ui::Painted> status_paint_;
  std::unique_ptr<ui::Painted> level_paint_;
  std::unique_ptr<ui::Painted> section_paint_;
  std::unique_ptr<ui::Viewport> viewport_;
  std::unique_ptr<ui::Painted> stage_paint_;
  ui::Panel side_panel_ = ui::Panel::create(Rgba{});
  ui::Box side_column_ = ui::Box::vertical();  // actions, the scrolled layer stack, View and Inspect
  ui::ScrollBox side_scroll_ = ui::ScrollBox::create();
  ui::Box side_ = ui::Box::vertical();     // the layer stack (scrolled)
  ui::Box side_bottom_ = ui::Box::vertical();  // View, Inspect, the section panel (always shown)
  ui::Box rows_box_ = ui::Box::vertical();
  std::vector<Control> controls_;
  std::vector<std::unique_ptr<ui::Painted>> painted_;  // section labels, notes, separators
  std::vector<std::unique_ptr<Row>> rows_widgets_;
  std::deque<ui::Box> boxes_;  // stable addresses: groups are referenced after more are added

  std::function<void(bool)> open_changed_;
  std::function<void()> selection_changed_;

  std::unique_ptr<render::VulkanOffscreenTarget> target_;
  std::unique_ptr<render::VulkanMesh3dRenderer> renderer_;
  std::string renderer_error_;
  std::unique_ptr<view3d::MeshWorker> worker_;
  std::shared_ptr<Shared> shared_;
  // Technologies a build may still read (kept alive until the worker is idle).
  std::vector<std::shared_ptr<const tech::Technology>> held_tech_;

  // The window's state as last synced (UI thread only).
  bool was_open_ = false;
  bool active_ = false;
  bool sync_pending_ = false;
  bool dirty_ = false;
  bool highlight_dirty_ = true;
  BuildKey key_;
  bool have_key_ = false;
  std::shared_ptr<const view3d::LayerStack> build_stack_;  // of the build in progress or done last
  std::uint64_t build_id_ = 0;  // BuildInputs::build_id of the last start_build
  std::shared_ptr<const view3d::LayerStack> drawn_stack_;  // of model_
  view3d::MeshHandle model_;
  view3d::MeshHandle uploaded_;                               // the model the renderer draws (pick ids decode against it)
  std::shared_ptr<const view3d::LayerStack> uploaded_stack_;  // of uploaded_
  View3dOptions options_;
  std::vector<View3dLayerRow> rows_;  // display order: top layer first
  std::vector<Rgba> row_colors_;      // swatches, per row
  std::vector<bool> drawable_;          // per stack entry of the drawn stack (or of the first build)
  std::vector<bool> pending_drawable_;  // of build_stack_, until its model is drawn
  GhostDefaults ghost_;                 // Q-MISC-85, per stack entry like drawable_
  DefaultCameraLatch camera_latch_;
  PickResolver resolver_;
  View3dPicking picking_{resolver_};
  const view3d::HighlightSet* highlight_ = nullptr;
  std::string pick_caption_;
  std::optional<view3d::Segment> section_line_;  // the cut line while the cross-section is on
  std::optional<db::Point> section_first_;       // the first of two clicks
  view3d::CrossSection section_;
  bool substrate_on_ = true;
  PointerTracker pointer_;
  FrameTimer timer_;
  std::pair<int, int> pixels_{0, 0};
  ui::PopupMenu export_menu_;
  std::unique_ptr<ui::Window> separate_window_;
  std::unique_ptr<ui::Viewport> separate_viewport_;
  bool separate_close_requested_ = false;  // set in the window's close callback, acted on in on_frame
};

}  // namespace hefesto::shell
