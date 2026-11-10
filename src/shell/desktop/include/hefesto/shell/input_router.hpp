// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Routes the layout canvas's pointer buttons, keys and pointer motion
// through the core's macro tables, as Magic's Tk event handler does
// (capability hefesto-input-and-touch "Desktop input parity with Magic";
// ui-tools-and-input "Key event normalization", "Box tool buttons", "Other
// tools' buttons"; ui-command-dispatch "Interactive keystroke handling";
// design.md D7 "Input path").
//
// Before each button, key or motion command the router sets the command
// point (cmd::PointerState in EditorState::command_point: window, root point,
// pixel area) and the window pixel, then:
//   - a button press looks up `ButtonN` with its modifiers in the current
//     tool's table (ui::handle_key) and runs the macro (`%W` replaced by the
//     window's name). Without a macro the press and the release go to the
//     current tool's handler as `pushbutton BUTTON down|up`.
//   - a key press is normalized (ui::normalize_key_event, macOS: any of
//     Mod1-Mod5 is Meta) and handled by ui::handle_key: a macro runs, an
//     interactive macro (or `:`) opens the command bar pre-filled, an
//     unbound key reports UI-KEY-001 through the Dispatcher's sinks.
//   - pointer motion runs `*bypass wire show` while the wiring tool is
//     tracking (Magic rebinds <Motion> for it) and the pick tool's drag
//     command (ui::UiState::motion, `%X`/`%Y` = the snapped pointer minus the
//     drag origin, tools.tcl magic::startselect) while it drags.
// Every command goes through the Dispatcher with origin `key`, so it is
// logged and replayable. Continuous camera gestures (wheel, pinch, trackpad
// scroll, Option-drag) stay direct in CanvasController (M0 deviation D6).
//
// The Paint, Label and Measure rail tools are Hefesto modes on top of the box
// tool (DESIGN board M2-Tool-States, decisions pending confirmation): the left
// button sweeps a rectangle, previewed by the canvas, and its release issues
// `box values` followed by `paint LAYER` (Option: `erase LAYER`), opens the
// command bar with `label ` (as Magic's `l` key), or runs `measure`.
//
// Toolkit-independent: the canvas translates Raven events; unit-tested with
// injected events (tests/unit/shell/input_router_test.cpp).
//
// Owner: SHELL (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/pointer.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/render/camera.hpp"
#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/ui/input.hpp"

namespace hefesto::shell {

/// The X11 key event of a native key (Raven's RKey codes; see raven_key
/// below) with `modifiers` (NativeModifier bits), for a US keyboard layout:
/// the keysym, the X11 state (Shift, Control, Mod1 = Alt/Option, Mod4 =
/// Super/Command) and the text XLookupString would give (control
/// characters with Control, none for function keys). std::nullopt for a
/// code without a keysym.
[[nodiscard]] std::optional<ui::KeyEvent> key_event_for(int native_key, std::uint8_t modifiers);

/// Raven's RKey values (raven_window.h); raven_ui.cpp checks them at compile time.
namespace raven_key {
inline constexpr int kSpace = 32, kEscape = 256, kEnter = 257, kTab = 258, kBackspace = 259, kInsert = 260,
                     kDelete = 261, kRight = 262, kLeft = 263, kDown = 264, kUp = 265, kPageUp = 266,
                     kPageDown = 267, kHome = 268, kEnd = 269, kCapsLock = 270, kPrintScreen = 271, kPause = 272,
                     kF1 = 273, kF12 = 284, kKp0 = 285, kKp9 = 294, kKpDecimal = 295, kKpDivide = 296,
                     kKpMultiply = 297, kKpSubtract = 298, kKpAdd = 299, kKpEnter = 300, kLeftShift = 301,
                     kLeftCtrl = 302, kLeftAlt = 303, kLeftSuper = 304, kRightShift = 305, kRightCtrl = 306,
                     kRightAlt = 307, kRightSuper = 308, kMenu = 309;
}  // namespace raven_key

/// Where the pointer is, for one event: the command point plus the window
/// pixel (Magic's window coordinates, y up) and the window's name (`%W`).
struct PointerContext {
  cmd::PointerState state;
  db::Point pixel;
  std::string window_name;
};

/// The pointer context of a canvas point (`x`, `y` in logical pixels, y down)
/// shown by `camera` (framebuffer pixels = logical × `pixel_ratio`).
[[nodiscard]] PointerContext pointer_at(const render::Camera& camera, double pixel_ratio, double x, double y,
                                        std::int32_t window, std::string window_name);

/// The rail modes the shell adds on top of the box tool.
enum class EditMode : std::uint8_t { none, paint, label, measure };

/// The system pointer shapes the canvas uses for Magic's cursor glyphs.
enum class CursorShape : std::uint8_t { arrow, crosshair, ibeam, move, resize_nesw, resize_nwse };

/// The pointer shape for cursor glyph `glyph` (ui::UiState::cursor_glyph:
/// box corners 2-9, nettool 18, wiring 19, drag 21, pick 22) in rail mode
/// `mode`: a corner glyph shows a diagonal resize (resize) or move (move)
/// pointer, the wiring and netlist tools a crosshair; with the plain box
/// tool, the Paint and Measure modes show a crosshair and Label an I-beam.
[[nodiscard]] CursorShape cursor_shape(int glyph, EditMode mode) noexcept;

class InputRouter {
 public:
  InputRouter(cmd::Dispatcher& dispatcher, editor::EditorState& editor, bool macos = true);

  /// Called with the text to put in the command bar (interactive macros, `:`).
  void on_prompt(std::function<void(const std::string&)> callback) { prompt_ = std::move(callback); }

  /// The rail mode; any mode other than `none` applies with the box tool only.
  void set_mode(EditMode mode);
  [[nodiscard]] EditMode mode() const noexcept { return mode_; }
  /// The layer the Paint mode paints (the layer palette's active layer).
  void set_paint_layer(std::string layer) { paint_layer_ = std::move(layer); }
  [[nodiscard]] const std::string& paint_layer() const noexcept { return paint_layer_; }

  /// `button` 1 left, 2 middle, 3 right; `modifiers` NativeModifier bits.
  /// Returns true when the event was used.
  bool button(int button, bool down, std::uint8_t modifiers, const PointerContext& at);
  /// A key press in the canvas; `at` is the pointer when it is over the canvas.
  bool key(const ui::KeyEvent& event, const std::optional<PointerContext>& at);
  /// Pointer motion over the canvas (no command when nothing tracks it).
  bool motion(const PointerContext& at);

  /// True when the current tool's table binds `ButtonN` with `modifiers`
  /// (an Option-drag pans only when Meta_ButtonN has no macro).
  [[nodiscard]] bool has_button_macro(int button, std::uint8_t modifiers) const;

  /// The rectangle a Paint/Label/Measure sweep covers now (root, snapped),
  /// for the canvas preview; std::nullopt when no sweep is in progress.
  [[nodiscard]] std::optional<db::Rect> sweep() const;

  /// Every command line the router ran, in order (tests, the Console).
  [[nodiscard]] const std::vector<std::string>& issued() const noexcept { return issued_; }

 private:
  void set_point(const PointerContext& at);
  bool run(const std::string& line, const PointerContext* at);
  [[nodiscard]] std::uint8_t x11_state(std::uint8_t modifiers) const;
  [[nodiscard]] db::Point snapped(db::Point p) const;
  [[nodiscard]] bool box_tool() const;
  bool mode_button(bool down, std::uint8_t modifiers, const PointerContext& at);
  void finish_sweep(bool erase);

  cmd::Dispatcher& dispatcher_;
  editor::EditorState& editor_;
  bool macos_ = true;
  std::function<void(const std::string&)> prompt_;
  EditMode mode_ = EditMode::none;
  std::string paint_layer_;
  std::uint8_t fallback_buttons_ = 0;  // buttons whose press went to `pushbutton`
  std::optional<db::Point> sweep_from_;
  db::Point sweep_to_;
  std::vector<std::string> issued_;
};

}  // namespace hefesto::shell
