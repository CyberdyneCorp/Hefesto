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
//   is due, the mesh status and the frame-time
//   readout;
// - the board look of the desktop tab in both magic-compat modes
//   (Q-MISC-84, approved 2026-11-24): the theme's canvas token as the
//   background, the renderer's board lighting, the isometric camera framed
//   to the cell when the desktop opens the window (render3d's path; as
//   `view`/`scroll`/`zoom` commands, once per window) and the x/y/z axis
//   gizmo;
// - the default transparency of ghosted layers (Q-MISC-85: wells, implants
//   and other non-conducting area layers at 20 %, view3d::ghosted_by_default),
//   which a user's per-row choice overrides for the life of the window.
//
// - the controls as a screen reader reads them (control_view), the layout
//   that fits a 1280-px window (flow_rows, grid_cell_width), and the P2
//   actions: Export glTF (export_request, export_command) and the
//   cross-section panel (box_cut_line, section_panel).
//
// Owner: SHELL (design D3; tasks 9.1, 9.2, 9.6, 11.5, 13.2, 16.6-16.8, 16.10).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
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
#include "hefesto/shell/theme_tokens.hpp"
#include "hefesto/shell/view_models.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/ui/macros.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/client/session.hpp"
#include "hefesto/view3d/mesh/worker.hpp"
#include "hefesto/platform/file_picker.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "hefesto/view3d/section/section.hpp"
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
  bool ghosted = false;                 // ghosted by default (Q-MISC-85)

  friend bool operator==(const View3dLayerRow&, const View3dLayerRow&) = default;
};

/// "h–(h + t)" in µm with two decimals; empty for nullopt (no source).
[[nodiscard]] std::string micron_range(const std::optional<view3d::PhysicalHeight>& height);

/// The rows of `stack` bottom to top (ascending draw height, then stack
/// order); the panel lists them top first, as the board. `drawable` (per
/// stack entry, empty = all) leaves out entries whose style is neither
/// solid nor stippled (they are not drawn, design D8). Visibility comes
/// from the window's layout-layer set; `alpha` is per stack entry (empty =
/// opaque); `ghosted` per stack entry (empty = none; a row is ghosted when
/// its first entry is).
[[nodiscard]] std::vector<View3dLayerRow> layer_rows(const view3d::LayerStack& stack,
                                                     const view3d::WindowState& window,
                                                     std::span<const float> alpha = {},
                                                     const std::vector<bool>& drawable = {},
                                                     const std::vector<bool>& ghosted = {});

/// The screen-reader name of a row's label: "nwell, 0.00–1.00 µm, shown"
/// and, for a translucent row, ", ghosted, 20 percent opaque" (a ghosted
/// row) or ", 50 percent opaque".
[[nodiscard]] std::string row_accessible_name(const View3dLayerRow& row);
/// The screen-reader name of a row's transparency button: "Transparency of
/// nwell, 20 percent" (", ghosted by default" for a ghosted row).
[[nodiscard]] std::string alpha_button_name(const View3dLayerRow& row);

/// The hidden flags of `stack` for the renderer: the window's layout-layer
/// set in layout-layer mode; none in mask-layer mode (the view takes the
/// mask set there from view3d::hidden_layers, FrameInputs::hidden).
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
/// The commands that put the window on `camera` (Q-MISC-84): `view ax ay
/// az`, `scroll tx ty tz` and `zoom s_xy s_z`, all absolute, so `view`,
/// `scroll` and `zoom` then report what the user sees. The presets and the
/// default camera run them with a view framed to the cell
/// (view3d::frame_view). prescale_z is not a command value and is kept.
[[nodiscard]] std::vector<std::string> camera_commands(std::string_view path, const view3d::ViewState& camera);
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
  bool show_frame_time = false;        // the frame-time readout (task 13.2)
  std::vector<float> alpha;            // per stack entry; empty = opaque
  /// The user's per-row transparency, by layer name: it overrides the
  /// ghosted default and survives rebuilds of the window's stack; cleared
  /// when the window closes.
  std::map<std::string, float, std::less<>> alpha_override;

  friend bool operator==(const View3dOptions&, const View3dOptions&) = default;
};

/// The default transparency of a ghosted layer (Q-MISC-85): the 20 % step
/// of the row's transparency control.
inline constexpr float kGhostAlpha = 0.2F;

/// Sets the transparency of every stack entry of `row` and records it as
/// the user's choice for that layer name (alpha_override).
void set_row_alpha(View3dOptions& options, const View3dLayerRow& row, float alpha, std::size_t stack_size);

/// Recomputes `options.alpha` for `stack`: per entry the user's choice for
/// its name when there is one, else kGhostAlpha when `ghosted` flags it
/// (view3d::ghosted_by_default), else opaque.
void apply_default_alpha(View3dOptions& options, const view3d::LayerStack& stack, const std::vector<bool>& ghosted);

/// The ghosted flags and the per-entry alpha of the stack that is drawn
/// (Q-MISC-85). The view builds a new stack's mesh while it still draws the
/// previous one, whose layer ids differ (a magic-compat toggle, a styles or
/// technology reload): a new stack's flags are held until its model is
/// drawn, so the frames and rows in between keep the drawn stack's alpha.
class GhostDefaults {
 public:
  /// A build of `stack` started with its `ghosted` flags. While no stack is
  /// drawn yet (`drawing` false) the flags and alpha apply at once.
  void building(const view3d::LayerStack& stack, std::vector<bool> ghosted, bool drawing, View3dOptions& options);
  /// The model of the stack last passed to building() is now drawn as `stack`.
  void drawn(const view3d::LayerStack& stack, View3dOptions& options);
  /// The window closed: no flags.
  void clear() noexcept;
  /// Per entry of the drawn stack (or of the first build before a model).
  [[nodiscard]] const std::vector<bool>& ghosted() const noexcept { return ghosted_; }

 private:
  std::vector<bool> pending_;
  std::vector<bool> ghosted_;
};

// --- the board look (Q-MISC-84) -------------------------------------------------

/// The 3D view's background in `mode`: the theme's canvas token (the 2D
/// canvas's background), as display values 0..1, in both magic-compat modes.
[[nodiscard]] std::array<float, 3> view3d_background(ThemeMode mode) noexcept;

/// True once per opened window: the first call with `open` after the window
/// opened; a call with !open (the window closed) re-arms it. The view asks
/// for the default camera when it answers true, so a user's later camera is
/// kept.
class DefaultCameraLatch {
 public:
  [[nodiscard]] bool take(bool open) noexcept;

 private:
  bool applied_ = false;
};

/// The box the camera frames (view3d::frame_view): `area` in x and y, and
/// in z the lowest h and the highest h + t of the layers of `stack` drawn
/// (`drawable` per stack entry, empty = all), in draw units.
[[nodiscard]] view3d::Extent3 stack_extent(const db::Rect& area, const view3d::LayerStack& stack,
                                           const std::vector<bool>& drawable = {});

/// Per stack entry, whether `mesh` draws any face of it (the layers the
/// camera frames: a stack lists every layer of the technology).
[[nodiscard]] std::vector<bool> layers_present(const view3d::MeshModel& mesh, std::size_t stack_size);

/// The z scale the desktop tab opens with in layout-layer mode (Q-MISC-84
/// board look): heights in proportion to the widths, instead of Magic's
/// default 25x exaggeration, which spreads the layers far apart. Set
/// through `zoom`, so `zoom` reports it; `defaults` returns to Magic's
/// values. board_scale_z gives the value of a window in either mode.
inline constexpr float kBoardScaleZ = 1.0F;

/// What a framed camera reads: the window, the mesh drawn and its stack,
/// the drawable flag per stack entry (empty = all) and the size of the
/// drawable in pixels.
struct CameraFrame {
  const view3d::WindowState* window = nullptr;
  const view3d::MeshModel* mesh = nullptr;
  const view3d::LayerStack* stack = nullptr;
  std::vector<bool> drawable;
  int width_px = 0;
  int height_px = 0;
};

/// True when `frame` has a window, a mesh, its stack and a non-empty size.
[[nodiscard]] bool frame_ready(const CameraFrame& frame) noexcept;

/// The commands that apply the camera preset `which` framed to the layers
/// the mesh draws (view3d::frame_view, in the mesh's units: mask units in
/// mask-layer mode), with `scale_z` when given, else the window's: `view`,
/// `scroll` and `zoom` (camera_commands). A frame that is not ready gives
/// the preset's plain `view` (preset_command); no window gives nothing.
[[nodiscard]] std::vector<std::string> preset_camera_commands(const CameraFrame& frame, view3d::Preset which,
                                                              std::optional<float> scale_z = std::nullopt);

/// True when the desktop opened `window` itself: render3d's toolkit path
/// (the 3D tab and Options > 3D Display run `magic::render3d`). A window on
/// any other path (`specialopen wind3d`, a script's own path) keeps Magic's
/// top-down default view (Q-MISC-84, view-3d "Default view").
[[nodiscard]] bool desktop_opened(const view3d::WindowState& window) noexcept;

/// The board z scale of `window`: kBoardScaleZ in layout-layer mode and
/// kBoardScaleZ x f in mask-layer mode (f = WindowState::applied_factor):
/// Magic's rescale multiplies s_z by f there, and the mask-layer mesh is in
/// mask units (x and y times f), so the heights keep their proportion to
/// the widths in both modes.
[[nodiscard]] float board_scale_z(const view3d::WindowState& window) noexcept;
/// The board z scale of `window` drawing `stack` (task 17.2, approved
/// 2026-11-26): board_scale_z(window), and in mask-layer mode with
/// magic-compat ON (the stack's flag) additionally divided by the stack's µm
/// per internal unit (LayerStack::microns_per_unit). With it ON the
/// mask-layer heights are the `render` values as stored (µm, Q-MISC-72)
/// while x and y are in mask units, so this draws the stack in the same
/// proportion as with it OFF, where the heights are already converted. A
/// µm per unit that is zero, negative or not finite (or a result that is
/// not finite) keeps board_scale_z(window). Desktop tab only: Magic's s_z
/// (`defaults`, script windows) is untouched; the tab sets it with `zoom`.
[[nodiscard]] float board_scale_z(const view3d::WindowState& window, const view3d::LayerStack& stack) noexcept;

/// The desktop tab's opening camera (Q-MISC-84): the isometric preset
/// framed to the drawn layers at board_scale_z (of the window and the
/// frame's stack) when the desktop opened the
/// window (desktop_opened) and the frame is ready; nothing otherwise (a
/// window opened elsewhere keeps the top-down default view).
[[nodiscard]] std::vector<std::string> default_camera_commands(const CameraFrame& frame);

/// One arrow of the axis gizmo: its screen direction (x right, y down) as
/// the view's rotation projects the unit axis; the z arrow points toward the
/// higher layers (the displayed scene's +z: the model mirrored in z).
struct AxisArrow {
  char name = 'x';
  float dx = 0.0F;
  float dy = 0.0F;
};
/// The x, y and z arrows of `view`'s rotation (scale and translation ignored).
[[nodiscard]] std::array<AxisArrow, 3> axis_gizmo(const view3d::ViewState& view);

/// The frame's DrawParams: projection, level, stack, styles, hidden flags,
/// alpha, highlight (xy scale 1: the mesh is in its own units in both
/// modes, view3d::Scene::xy_factor), the background and the board lighting in both
/// magic-compat modes (Q-MISC-84: Magic's GL lighting is never used by the
/// desktop tab).
struct FrameInputs {
  const view3d::WindowState* window = nullptr;
  const view3d::LayerStack* stack = nullptr;
  const display::StyleSet* styles = nullptr;
  const view3d::HighlightSet* highlight = nullptr;
  const View3dOptions* options = nullptr;
  /// The background (view3d_background); nullopt leaves the renderer's.
  std::optional<std::array<float, 3>> background = std::nullopt;
  /// Per stack layer hidden flags (view3d::hidden_layers, both modes);
  /// null takes hidden_flags (the layout visibility set).
  const std::vector<bool>* hidden = nullptr;
};
[[nodiscard]] render::DrawParams draw_params(const FrameInputs& in);

// --- mesh builds -----------------------------------------------------------------

/// What a full build reads; a change of any of these runs MeshWorker::build
/// (the stack and its mode, the clip area, the rendered cell, the output
/// style of a mask-layer build, the J1 option).
struct BuildKey {
  const db::CellDef* cell = nullptr;  // compared, never dereferenced
  db::Rect clip;
  bool clipped = false;
  bool span_connected_layers = false;
  const void* technology = nullptr;   // compared, never dereferenced
  /// MaskSource::style, compared by address and held: a reloaded style can
  /// then never reuse the address of the one this key was built from.
  std::shared_ptr<const void> mask_style;
  view3d::Mode mode = view3d::Mode::layout_layer;
  std::vector<view3d::StackLayer> layers;
  bool magic_compat = true;
  double microns_per_unit = 0.0;

  friend bool operator==(const BuildKey&, const BuildKey&) = default;
};

/// The key of the window's build inputs (view3d::window_build: the stack
/// of its mode, the clip area of Q-MISC-70 or mask_clip, the mask source).
[[nodiscard]] BuildKey build_key(const view3d::BuildInputs& window_inputs, bool span_connected_layers);

/// The "Span connected layers" toggle (J1; task 17.3, decision B17,
/// approved 2026-11-26): flips the open window's
/// view3d::WindowState::span_connected_layers on `path`
/// (view3d::set_span_connected_layers), so `export3d` and every build of
/// the window read it; the tab's build key follows it (a full build).
/// Returns the new value, or nullopt when no window is open on `path`.
[[nodiscard]] std::optional<bool> toggle_span_connected_layers(view3d::Session& session, std::string_view path);

/// Whether a model the worker delivered is of the last build the view
/// started (BuildInputs::build_id); an older one is ignored.
[[nodiscard]] bool model_is_current(const view3d::MeshModel& model, std::uint64_t build_id) noexcept;

/// The MeshWorker inputs: view3d::window_build's inputs of the window
/// (null cell when it does not exist; the layout-layer or mask-layer
/// source, the clip area and its centre) with the session's database lock
/// and the J1 option.
[[nodiscard]] view3d::BuildInputs build_inputs(editor::EditorState& editor, view3d::BuildInputs window_inputs,
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
  /// Selects `target` in 2D (3D -> 2D link); false when nothing was
  /// selected (a mask-layer target has no tile type, or the source tile is
  /// gone), and the selection is then unchanged.
  [[nodiscard]] virtual bool select(editor::EditorState& editor, const view3d::PickTarget& target);
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
  /// picked target is cleared). The caption says "selected in 2D" only
  /// when the 2D selection was changed (selected_in_2d()).
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
  /// The last click changed the 2D selection.
  [[nodiscard]] bool selected_in_2d() const noexcept { return selected_in_2d_; }
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
  bool selected_in_2d_ = false;
  std::optional<view3d::HighlightSet> net_;
  bool net_stale_ = false;
  view3d::HighlightSet shown_;
};

/// The layout-net overlay of a frame (task 10.4; design D10;
/// hefesto-3d-visualization "Net highlighted over mask-layer geometry"):
/// in mask-layer mode the nets of the 2D selection's paint
/// (view3d::net_overlay of the selection, hierarchical search, the session's
/// compat flag), against `mesh`, the model the renderer draws (its pick ids
/// and units); empty in layout-layer mode, which keeps the instance
/// highlight, and without a mesh.
[[nodiscard]] view3d::NetOverlay mask_net_overlay(const view3d::MeshModel* mesh, const editor::EditorState& editor,
                                                  const view3d::WindowState& window, bool magic_compat);

/// The overlay's prisms in the renderer's type (render::OverlayPrism, the
/// same fields), copied field by field.
template <typename Prism>
[[nodiscard]] std::vector<Prism> overlay_prisms(const view3d::NetOverlay& overlay) {
  std::vector<Prism> out;
  out.reserve(overlay.prisms.size());
  for (const view3d::NetOverlayPrism& p : overlay.prisms) {
    Prism q{};
    q.x0 = p.x0;
    q.y0 = p.y0;
    q.x1 = p.x1;
    q.y1 = p.y1;
    q.z_bottom = p.z_bottom;
    q.z_top = p.z_top;
    out.push_back(q);
  }
  return out;
}

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

// --- controls and their screen-reader names (task 9.6) ---------------------------

/// Every control of the 3D tab outside the layer rows: the P2 actions, the
/// View, Inspect and toolbar controls (board docs/design/mockups/View3D.dc.html).
enum class View3dControl : std::uint8_t {
  cross_section,
  export_model,
  orthographic,
  perspective,
  isometric,
  top,
  front,
  side,
  substrate,
  isolate_net,
  span_connected_layers,
  frame_time,
  close_view,
  show_all_layers,
  mask_layers,
  layout_layers,
  level_down,
  level_up,
  cut_box,
  separate_window,  // round 6: the tab menu's "Open in separate window"
};
inline constexpr std::size_t kView3dControlCount = 20;

/// What the controls read: the window, the view options and the picking state.
struct View3dControlState {
  bool window_open = false;
  bool mask_mode = false;
  bool cut_box = false;
  bool perspective = false;
  bool span_connected_layers = false;
  bool frame_time = false;
  bool picked = false;
  bool net_isolated = false;
  bool section_on = false;
  bool substrate_on = true;
  bool substrate_available = false;  // the renderer takes a substrate slab
  bool export_available = false;     // `export3d` is a registered command
  bool visibility_editable = true;
  bool separate_window = false;            // the view is shown in its own window
  bool separate_window_available = false;  // the host can open a second window
};

/// One control as shown and as a screen reader reads it.
struct View3dControlView {
  std::string label;            // painted text
  std::string accessible_name;  // a toggle's ends with ", on" or ", off"
  std::optional<bool> on;       // a toggle's state; nullopt for a plain button
  bool enabled = true;
  std::string tooltip;

  friend bool operator==(const View3dControlView&, const View3dControlView&) = default;
};

/// "<label>, on" / "<label>, off": a painted toggle's state in its name (a
/// Raven painted widget exposes no checked state to the platform's screen
/// reader, so the state is spoken with the name).
[[nodiscard]] std::string view3d_toggle_name(std::string_view label, bool on);
[[nodiscard]] View3dControlView control_view(View3dControl control, const View3dControlState& state);
/// The Level stepper's value, a text widget: "Level 1".
[[nodiscard]] std::string level_text(int level);

// --- layout that fits the window (1280 x 800 and up) ----------------------------

/// The side panel's width and inner padding.
inline constexpr float kView3dSideWidth = 272.0F;
inline constexpr float kView3dSidePad = 12.0F;
/// Distance of the floating toolbar from the view's edges (the board's 12 px).
inline constexpr float kView3dToolbarMargin = 12.0F;
/// Space between the toolbar's groups and between its rows.
inline constexpr float kView3dToolbarSpacing = 8.0F;

/// The width the side panel's rows have: the panel less its padding.
[[nodiscard]] constexpr float side_content_width() noexcept {
  return kView3dSideWidth - 2.0F * kView3dSidePad;
}
/// The width of each cell of a `columns`-column button grid across `width`,
/// `spacing` apart (the board's two-column camera grid).
[[nodiscard]] float grid_cell_width(float width, int columns, float spacing) noexcept;

/// A layer row: its three buttons (Hide, Solo, transparency), the room
/// left for the scroll bar, and where the row's text starts (after the swatch).
inline constexpr float kView3dRowButtonWidth = 44.0F;
inline constexpr float kView3dRowSpacing = 2.0F;
inline constexpr float kView3dScrollBarRoom = 8.0F;
inline constexpr float kView3dRowTextX = 22.0F;
/// The width a row's name and µm range have before the row's buttons (the
/// side panel's width is fixed, so it is the same at 1280 x 800 and up).
[[nodiscard]] constexpr float row_text_width() noexcept {
  return side_content_width() - kView3dScrollBarRoom - 3.0F * (kView3dRowButtonWidth + kView3dRowSpacing) -
         kView3dRowTextX - 4.0F;
}
/// The painted row cuts its name and range to row_text_width() with
/// shell::ellipsize (view_models.hpp); the row's tooltip and screen-reader
/// name keep the full text (row_tooltip, row_accessible_name).
/// A row's tooltip: "mrdlcontact, 6.63–7.26 µm" (the full name and range,
/// which the painted row may ellipsize).
[[nodiscard]] std::string row_tooltip(const View3dLayerRow& row);

/// A flow layout: items of `widths` left to right, `spacing` apart, starting
/// a new row when the next item would pass `available`. Returns the row of
/// each item; an item wider than `available` takes a row of its own.
[[nodiscard]] std::vector<std::size_t> flow_rows(std::span<const float> widths, float available, float spacing);
/// The width of the widest row of a flow_rows result.
[[nodiscard]] float flow_width(std::span<const float> widths, const std::vector<std::size_t>& rows, float spacing);

// --- export (task 11.5; design D14) ----------------------------------------------

/// The save dialog of "Export glTF…": "<cell>.gltf" proposed, glTF, glTF
/// binary and STL filters (export3d takes the format from the extension).
[[nodiscard]] platform::FilePickerRequest export_request(const view3d::WindowState& window);
/// `export3d <file>`, run on the window's path when export3d is a command
/// of the 3D window's table (`on_window_table`), else as a plain command;
/// the file is quoted as one Tcl word.
[[nodiscard]] std::string export_command(std::string_view path, std::string_view file, bool on_window_table);

/// The formats the Export button's menu offers (round 6): `export3d`'s
/// `-format gltf|glb|stl`, and STL as one merged file (`-merged`) or one
/// file per layer (export3d's default for STL).
enum class View3dExportFormat : std::uint8_t { gltf, glb, stl_per_layer, stl_merged };
inline constexpr std::array<View3dExportFormat, 4> kView3dExportFormats{
    View3dExportFormat::gltf, View3dExportFormat::glb, View3dExportFormat::stl_per_layer,
    View3dExportFormat::stl_merged};
/// The menu item of `format`: "glTF (.gltf)…", "glTF binary (.glb)…",
/// "STL, one file per layer…", "STL, merged into one file…".
[[nodiscard]] std::string export_format_label(View3dExportFormat format);
/// The save dialog for `format`: its one filter and "<cell>.<ext>" proposed.
[[nodiscard]] platform::FilePickerRequest export_request(const view3d::WindowState& window,
                                                         View3dExportFormat format);
/// `export3d <file> -format <gltf|glb|stl> [-merged]` (the format is given,
/// so a file named with another extension is still written in the chosen
/// format), on the window's path as export_command.
[[nodiscard]] std::string export_command(std::string_view path, std::string_view file, bool on_window_table,
                                         View3dExportFormat format);

// --- cross-section (task 11.5; design D10) ----------------------------------------

/// The cut line of `area`: its long-axis centre line (axis-aligned), from the
/// left (or bottom) edge to the right (or top) edge; a square cuts along x.
[[nodiscard]] view3d::Segment box_cut_line(const db::Rect& area);
/// Two points (two clicks on the ground plane) made axis-aligned along the
/// longer of the two directions from `a`.
[[nodiscard]] view3d::Segment axis_aligned_line(db::Point a, db::Point b);
/// The rectangle the "Cross-section" button cuts: the layout box when it is
/// in the rendered cell, else the window's cut box, else the cell's bounding
/// box; nullopt when the cell does not exist.
[[nodiscard]] std::optional<db::Rect> section_area(const editor::EditorState& editor,
                                                   const view3d::WindowState& window);
/// The 3D view's clipping plane for `line` (render::SectionPlane): the
/// vertical plane through the line, keeping the side away from the
/// isometric viewer (y >= c for a line along x, x >= c for one along y),
/// so the cut faces the user.
[[nodiscard]] render::SectionPlane section_clip_plane(const view3d::Segment& line) noexcept;
/// A view3d::ground_point result back in root coordinates: it is in the
/// mesh's units, x and y times `xy_factor` (MeshModel::units_per_root:
/// the output style's scale factor in mask-layer mode).
[[nodiscard]] db::Point ground_to_root(db::Point ground, float xy_factor) noexcept;
/// A cut line in root coordinates in the mesh's units (times
/// MeshModel::units_per_root), as view3d::section and the clipping plane
/// take it.
[[nodiscard]] view3d::Segment segment_in_mesh(const view3d::Segment& line, std::int32_t units_per_root) noexcept;

/// One bar of the section panel, in panel units (x right, y down).
struct SectionBar {
  view3d::LayerId layer = 0;
  float x = 0.0F;
  float y = 0.0F;
  float w = 0.0F;
  float h = 0.0F;

  friend bool operator==(const SectionBar&, const SectionBar&) = default;
};
/// The "Cross-section A–A′" panel: each interval from bottom_um to top_um
/// over its span along the line, scaled to `width` x `height` (x 0 = A, the
/// panel's bottom = 0 µm, its top = the highest top), the layers that have
/// no height named, and the screen-reader text. Hidden layers (`hidden`,
/// per stack entry) are left out.
struct SectionPanel {
  std::vector<SectionBar> bars;
  std::vector<std::string> no_height;
  double length_um = 0.0;
  double top_um = 0.0;
  std::string summary;  // "A–A′ 3.40 µm · 4 layers"
  std::string accessible_name;
};
[[nodiscard]] SectionPanel section_panel(const view3d::CrossSection& section, const view3d::LayerStack& stack,
                                         double length_um, float width, float height,
                                         const std::vector<bool>& hidden = {});

}  // namespace hefesto::shell
