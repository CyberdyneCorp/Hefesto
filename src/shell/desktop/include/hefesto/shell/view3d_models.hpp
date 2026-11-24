// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The view models of the desktop's 3D view (hefesto-3d-visualization
// "Interactive 3D inspection", hefesto-design-system "Screen patterns
// follow the reference mockups" (3D inspection); openspec/changes/
// m10-3d-view/design.md D7, D9, D10, D12; board
// docs/design/mockups/View3D.dc.html). Raven-free and GPU-free, so every
// control is tested headless:
//
// - the layer stack panel: one row per drawn layer, bottom to top, with its
//   µm range (LayerStack::physical_height_um, blank when unknown), its
//   visibility read from the window's per-mode visibility set (Q-MISC-71)
//   and its transparency (Hefesto state, outside Magic's);
// - the Tcl lines the controls run on the window's toolkit path (`see`,
//   `view` presets, `level`, `cif`, `cutbox`, `closewindow`), so each UI
//   action is a logged, replayable command and the panel and `see` share one
//   set both ways;
// - focus routing of typed lines to the 3D window's `wind3d` table;
// - the gesture mapping of mouse, trackpad and keys to view3d::Gesture
//   (hefesto/view3d/camera/input.hpp) and click-versus-drag;
// - picking through a replaceable resolver (view3d/pick: INSPECT), the
//   highlight set handed to the renderer and "Isolate selected net";
// - the DrawParams of a frame, the MeshWorker inputs and when a full build
//   is due, the mesh status, the mask-layer notice and the frame-time
//   readout.
//
// Owner: SHELL (design D3; tasks 9.1, 9.2, 9.6, 13.2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/render/mesh3d_renderer.hpp"
#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/ui/macros.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/client/session.hpp"
#include "hefesto/view3d/mesh/worker.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::view {
class LayoutWindow;
}

namespace hefesto::shell {

// --- layer stack panel -----------------------------------------------------------

/// One row of the layer stack panel: a drawn layer (a tile type in
/// layout-layer mode), with every stack entry that draws it (one per
/// display style, Q-MISC-77).
struct View3dLayerRow {
  std::string name;
  std::optional<tech::TileType> type;
  std::vector<view3d::LayerId> layers;  // stack entries, in stack order
  std::size_t style = 0;                // display style of the first entry (swatch)
  std::string range_um;                 // "1.38–1.73"; empty when physical_height_um is nullopt
  float bottom = 0.0F;                  // draw height of the first entry (ordering)
  bool visible = true;
  float alpha = 1.0F;

  friend bool operator==(const View3dLayerRow&, const View3dLayerRow&) = default;
};

/// "h–(h + t)" in µm with two decimals; empty for nullopt (no source).
[[nodiscard]] std::string micron_range(const std::optional<view3d::PhysicalHeight>& height);

/// The rows of `stack` bottom to top (ascending draw height, then stack
/// order); the panel lists them top first, as the board. `drawable` (per
/// stack entry, empty = all) leaves out entries whose style is neither
/// solid nor stippled (they are not drawn, design D8). Visibility comes
/// from the window's layout-layer set; `alpha` is per stack entry (empty =
/// opaque).
[[nodiscard]] std::vector<View3dLayerRow> layer_rows(const view3d::LayerStack& stack,
                                                     const view3d::WindowState& window,
                                                     std::span<const float> alpha = {},
                                                     const std::vector<bool>& drawable = {});

/// The hidden flags of `stack` for the renderer: the window's layout-layer
/// set in layout-layer mode; none in mask-layer mode (P1 draws the layout
/// mesh there and the mask set applies to the mask layers of P2).
[[nodiscard]] std::vector<bool> hidden_flags(const view3d::LayerStack& stack, const view3d::WindowState& window);

/// Whether the panel's visibility controls act: in layout-layer mode only
/// (in mask-layer mode `see` edits the mask set, whose layers P2 draws).
[[nodiscard]] bool visibility_editable(const view3d::WindowState& window) noexcept;

// --- the commands the controls run -----------------------------------------------

/// `command` sent to the 3D window: "<path> <command>" (the toolkit-path
/// command routes to the `wind3d` table, design D9).
[[nodiscard]] std::string window_command(std::string_view path, std::string_view command);
/// Show or hide a row: `see <layer>` / `see no <layer>`.
[[nodiscard]] std::string see_command(std::string_view path, std::string_view layer, bool show);
/// Isolate a row (hide all others): `see no *`, then `see <layer>`.
[[nodiscard]] std::vector<std::string> isolate_commands(std::string_view path, std::string_view layer);
/// Show every layer again: `see *`.
[[nodiscard]] std::string show_all_command(std::string_view path);
/// A camera preset: `view ax ay az` with view3d::preset's angles.
[[nodiscard]] std::string preset_command(std::string_view path, view3d::Preset preset);
/// The Level stepper: `level up` (step > 0) or `level down`.
[[nodiscard]] std::string level_command(std::string_view path, int step);
/// The Mask layers / Layout layers toggle: `cif` (switches the mode).
[[nodiscard]] std::string mode_command(std::string_view path);
/// The Cut box toggle: `cutbox box` / `cutbox none` (the wrapper's checkbutton).
[[nodiscard]] std::string cutbox_command(std::string_view path, bool on);
/// "Close 3D view": `closewindow`.
[[nodiscard]] std::string close_command(std::string_view path);

/// The commands a row's transparency button cycles through, as alpha
/// values: 1 (opaque), 0.5, 0.2, then 1 again.
[[nodiscard]] float next_alpha(float alpha) noexcept;

// --- focus routing ---------------------------------------------------------------

/// Whether `word` names a command of the 3D window's table.
using IsWindowCommand = std::function<bool(std::string_view word)>;

/// A line typed in the command bar: while the 3D view has the focus
/// (`focused_path` is its toolkit path), a line whose first word is a
/// `wind3d` command goes to the window ("<path> <line>"), as Magic sends a
/// command to the window that has the focus; every other line (Tcl, layout
/// commands, a line already naming a path) is unchanged.
[[nodiscard]] std::string route_typed_line(std::string_view line, const std::optional<std::string>& focused_path,
                                           const IsWindowCommand& is_window_command);

// --- view options (Hefesto state, outside Magic's) ------------------------------

struct View3dOptions {
  bool perspective = false;            // the Orthographic / Perspective toggle (D7)
  bool span_connected_layers = false;  // J1 "Span connected layers"
  bool show_frame_time = false;        // the frame-time readout (task 13.2)
  std::vector<float> alpha;            // per stack entry; empty = opaque

  friend bool operator==(const View3dOptions&, const View3dOptions&) = default;
};

/// Sets the transparency of every stack entry of `row`.
void set_row_alpha(View3dOptions& options, const View3dLayerRow& row, float alpha, std::size_t stack_size);

/// The frame's DrawParams: projection, level, stack, styles, hidden flags,
/// alpha, highlight, and the P1 mask-layer xy scale (Scene::xy_factor, as
/// DrawParams::xy_scale).
struct FrameInputs {
  const view3d::WindowState* window = nullptr;
  const view3d::LayerStack* stack = nullptr;
  const display::StyleSet* styles = nullptr;
  const view3d::HighlightSet* highlight = nullptr;
  const View3dOptions* options = nullptr;
};
[[nodiscard]] render::DrawParams draw_params(const FrameInputs& in);

/// The xy scale the P1 placeholder draws mask-layer mode with: the f applied
/// on entering the mode, 1 in layout-layer mode (view3d::Scene::xy_factor).
[[nodiscard]] float xy_factor(const view3d::WindowState& window) noexcept;

// --- mesh builds -----------------------------------------------------------------

/// What a full build reads; a change of any of these runs MeshWorker::build
/// (the stack, the clip area, the rendered cell, the J1 option).
struct BuildKey {
  const db::CellDef* cell = nullptr;  // compared, never dereferenced
  db::Rect clip;
  bool clipped = false;
  bool span_connected_layers = false;
  const void* technology = nullptr;   // compared, never dereferenced
  std::vector<view3d::StackLayer> layers;
  bool magic_compat = true;
  double microns_per_unit = 0.0;

  friend bool operator==(const BuildKey&, const BuildKey&) = default;
};

/// The rendered cell's clip area (Q-MISC-70): the cut box, or the cell's
/// bounding box grown by 1.
[[nodiscard]] view3d::ClipArea clip_area(const db::CellDef& cell, const view3d::WindowState& window);

[[nodiscard]] BuildKey build_key(const editor::EditorState& editor, const view3d::WindowState& window,
                                 const view3d::LayerStack& stack, bool span_connected_layers);

/// The MeshWorker inputs of `window` (null cell when it does not exist):
/// the session's database lock, the technology's residues for the J1
/// option, and the clip area's centre for the progressive order.
[[nodiscard]] view3d::BuildInputs build_inputs(editor::EditorState& editor, const view3d::WindowState& window,
                                               std::shared_ptr<const view3d::LayerStack> stack,
                                               bool span_connected_layers);

/// The change areas of a layout window for the 3D view (in root
/// coordinates): nullopt when the window does not show `rendered` as its
/// root or owes no redisplay; `all` when the whole window is to be redrawn.
struct ChangeAreas {
  bool all = false;
  std::vector<db::Rect> areas;
};
[[nodiscard]] std::optional<ChangeAreas> change_areas(const view::LayoutWindow& window, const db::CellDef* rendered);

/// The toolbar's mesh status: "Building mesh · 42 %" while a build runs,
/// "Mesh ready · 1,248 rectangles" after, "No mesh" before the first.
struct MeshStatus {
  bool building = false;
  double progress = 0.0;
  std::size_t rectangles = 0;
  bool have_model = false;
};
[[nodiscard]] std::string mesh_status_text(const MeshStatus& status);
[[nodiscard]] std::size_t rectangle_count(const view3d::MeshModel& mesh) noexcept;
/// 1248 -> "1,248".
[[nodiscard]] std::string grouped(std::size_t n);

// --- header, notice, level -------------------------------------------------------

/// "inverter · 3D" and "layout layers · level 1 · render3d opened this tab".
struct View3dHeader {
  std::string title;
  std::string subtitle;
};
[[nodiscard]] View3dHeader view3d_header(const view3d::WindowState& window);
/// "Mask layers arrive in phase 2" in mask-layer mode (design D1), else nullopt.
[[nodiscard]] std::optional<std::string> mask_mode_notice(const view3d::WindowState& window);

// --- gestures (hefesto/view3d/camera/input.hpp) ----------------------------------

/// A pointer drag of (dx, dy) logical pixels over a `width` x `height`
/// view: left orbits; middle, right, or left with Option or Shift pans.
[[nodiscard]] std::optional<view3d::Gesture> drag_gesture(PointerButton button, std::uint8_t modifiers, float dx,
                                                          float dy, int width, int height);
/// The wheel: a mouse wheel zooms (1.15 per line, Shift also scales z); a
/// trackpad's precise wheel pans by its pixels; Command or Control with a
/// precise wheel zooms instead.
[[nodiscard]] std::optional<view3d::Gesture> wheel_gesture(double wheel_x, double wheel_y, bool precise,
                                                           std::uint8_t modifiers, int width, int height);
/// A trackpad pinch: zoom by its scale (Shift also scales z).
[[nodiscard]] std::optional<view3d::Gesture> pinch_gesture(double scale, std::uint8_t modifiers);

/// The `wind3d` macro text of a key pressed in the 3D view (Magic's
/// W3Dcreate bindings: `L` level up, arrows scroll, `Z` zoom, ...), or
/// nullopt when the key has none.
[[nodiscard]] std::optional<std::string> key_macro(const ::hefesto::ui::MacroTables& macros, int native_key,
                                                   std::uint8_t modifiers);

/// Press, drag and release in the 3D view: a press that moves less than
/// kClickSlop logical pixels before its release is a click (a pick);
/// otherwise each move gives the drag's gesture.
class PointerTracker {
 public:
  static constexpr float kClickSlop = 3.0F;
  void press(float x, float y, PointerButton button, std::uint8_t modifiers);
  /// The gesture of a move while pressed (nullopt before the slop is passed).
  [[nodiscard]] std::optional<view3d::Gesture> move(float x, float y, int width, int height);
  /// The click point when the press stayed within the slop.
  [[nodiscard]] std::optional<std::pair<float, float>> release(float x, float y);
  [[nodiscard]] bool pressed() const noexcept { return pressed_; }

 private:
  bool pressed_ = false;
  bool dragging_ = false;
  PointerButton button_ = PointerButton::left;
  std::uint8_t modifiers_ = 0;
  float press_x_ = 0.0F;
  float press_y_ = 0.0F;
  float last_x_ = 0.0F;
  float last_y_ = 0.0F;
};

// --- picking (design D10; view3d/pick is INSPECT's) -------------------------------

/// The pick functions of hefesto/view3d/pick/pick.hpp, replaceable in tests.
/// With a scope set (the stack the model was built from, the rendered cell
/// and the compat flag), they call the overloads that map a layer back to
/// its tile type (PickContext, NetOptions); without one, the plain ones.
class PickResolver {
 public:
  PickResolver() = default;
  PickResolver(const PickResolver&) = delete;
  PickResolver& operator=(const PickResolver&) = delete;
  virtual ~PickResolver() = default;
  void set_scope(const view3d::PickContext& scope) noexcept { scope_ = scope; }
  [[nodiscard]] const view3d::PickContext& scope() const noexcept { return scope_; }
  [[nodiscard]] virtual std::optional<view3d::PickTarget> resolve(const view3d::MeshModel& mesh, view3d::PickId id);
  [[nodiscard]] virtual diag::Status select(editor::EditorState& editor, const view3d::PickTarget& target);
  [[nodiscard]] virtual view3d::HighlightSet from_selection(const view3d::MeshModel& mesh,
                                                            const editor::EditorState& editor);
  [[nodiscard]] virtual view3d::HighlightSet net(const view3d::MeshModel& mesh, const editor::EditorState& editor,
                                                 const view3d::PickTarget& seed);

 private:
  view3d::PickContext scope_;
};

/// The 3D view's picking state: the last picked target, the highlight of
/// the 2D selection, and "Isolate selected net".
class View3dPicking {
 public:
  explicit View3dPicking(PickResolver& resolver) noexcept : resolver_(resolver) {}

  /// A click read `id` from the pick image: resolves it and selects the
  /// target in 2D (3D -> 2D link). Returns the caption ("metal1 · picked ·
  /// selected in 2D"), or nullopt for the background or a stale id (the
  /// picked target is cleared).
  std::optional<std::string> click(const view3d::MeshModel* mesh, view3d::PickId id, editor::EditorState& editor,
                                   const view3d::LayerStack& stack);
  /// "Isolate selected net": the net of the picked target; false when
  /// nothing is picked. Calling it again while isolated clears it.
  bool toggle_isolate_net(const view3d::MeshModel* mesh, const editor::EditorState& editor);
  [[nodiscard]] bool net_isolated() const noexcept { return net_.has_value(); }
  /// Recomputes the highlight for a frame: the isolated net, else the 2D
  /// selection's instances (2D -> 3D link). Null when empty.
  const view3d::HighlightSet* highlight(const view3d::MeshModel* mesh, const editor::EditorState& editor);
  [[nodiscard]] const std::optional<view3d::PickTarget>& picked() const noexcept { return picked_; }
  /// A new model replaced the drawn one: its pick ids are other ordinals,
  /// so the isolated net is recomputed from the picked target against the
  /// model the next highlight() is given. The picked target (a tile, not
  /// an id) is kept.
  void model_changed() noexcept { net_stale_ = net_.has_value(); }
  /// Drops the picked target and the net (a closed window).
  void reset();

 private:
  PickResolver& resolver_;
  std::optional<view3d::PickTarget> picked_;
  std::optional<view3d::HighlightSet> net_;
  bool net_stale_ = false;
  view3d::HighlightSet shown_;
};

/// "metal1 · picked" (the layer name of the target's stack entry).
[[nodiscard]] std::string pick_caption(const view3d::PickTarget& target, const view3d::LayerStack& stack);

// --- frame time (task 13.2) ------------------------------------------------------

/// A rolling average of the last frames' draw times.
class FrameTimer {
 public:
  static constexpr std::size_t kWindow = 30;
  void add(double milliseconds);
  [[nodiscard]] double average_ms() const noexcept;
  [[nodiscard]] std::size_t samples() const noexcept { return samples_.size(); }
  /// "12.4 ms · 81 fps" (draw time of the 3D frame); "—" before a frame.
  [[nodiscard]] std::string text() const;

 private:
  std::vector<double> samples_;
  std::size_t next_ = 0;
};

}  // namespace hefesto::shell
