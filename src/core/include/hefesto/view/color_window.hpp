// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `color` window client (capability ui-color-window): a window that
// edits one entry of the session color map with RGB and HSV bars, the color
// map editing state (the "unsaved color map" that `quit` and `load` protect
// through cmd::PromptHost), the RGB/HSV conversion, the button actions
// (press-only, Q-UI-37) and the client's commands `color`, `load`, `save`
// and `pushbutton`. `specialopen color [OCTAL]` opens one (VIEW-CLIENT-001
// stays for `netlist`).
//
// The commands are returned by color_client_commands() for the `color`
// client's table (DISPATCH, task 7.1): they are not in the global registry
// because `load`, `save` and `pushbutton` there are other commands. Until
// the client tables exist they act on the active color window (the one under
// the command point, else the most recently opened).
// (Magic ref: cmwind/CMWmain.c, cmwind/CMWcmmnds.c, cmwind/CMWrgbhsv.c,
// cmwind/CMWundo.c)
//
// Owner: VIEW (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::view {

/// The fixed area every color window shows (colorWindowRect).
inline constexpr db::Rect kColorWindowSurface{0, 1500, 24000, 17000};
/// The swatch of the edited color and its title (cmwCurrentColorArea / TextBox).
inline constexpr db::Rect kCurrentColorArea{6000, 12000, 18000, 15000};
inline constexpr db::Rect kCurrentColorTextBox{6000, 15500, 18000, 16500};
inline constexpr std::string_view kCurrentColorText = "Color Being Edited";

enum class ColorComponent : std::uint8_t { red, green, blue, hue, saturation, value };

struct ColorBar {
  ColorComponent component;
  std::string_view name;
  db::Rect bar;
  db::Rect name_rect;
  std::size_t style;  // red, green, blue, yellow, gray, brown-1
};

struct ColorPump {
  ColorComponent component;
  db::Rect rect;
  double amount;  // +-0.0078 for R, G, B; +-0.01 for H, S, V
};

/// The six bars and twelve pumps of "Color window creation and layout".
[[nodiscard]] std::span<const ColorBar> color_bars() noexcept;
[[nodiscard]] std::span<const ColorPump> color_pumps() noexcept;

// --- RGB/HSV conversion ("RGB/HSV conversion", exact) ---------------------

struct Rgb {
  double r = 0.0;
  double g = 0.0;
  double b = 0.0;
};
struct Hsv {
  double h = 0.0;
  double s = 0.0;
  double v = 0.0;
};
[[nodiscard]] Hsv rgb_to_hsv(const Rgb& rgb) noexcept;
[[nodiscard]] Rgb hsv_to_rgb(const Hsv& hsv) noexcept;

// --- state ----------------------------------------------------------------

/// One color window: its id (shared with layout windows), frame and color.
struct ColorWindow {
  WindowId id = 0;
  int color = 0;
  ScreenRect frame;
  std::string caption;  // `COLOR = 0<octal>`
};

/// editor::EditorState::color_map: the color windows and whether the color
/// map changed since it was last loaded or saved.
struct ColorMapState {
  std::vector<ColorWindow> windows;  // most recently opened last
  bool modified = false;
  bool exit_check_installed = false;

  [[nodiscard]] ColorWindow* find(WindowId id) noexcept;
  /// The window `pointer` names when it is a color window, else the most
  /// recently opened one; nullptr when none is open.
  [[nodiscard]] ColorWindow* active(std::optional<WindowId> pointer) noexcept;
};

/// `COLOR = 0<octal index>` (CMWloadWindow).
[[nodiscard]] std::string color_caption(int index);

/// `specialopen color [OCTAL]`: OCTAL read as octal and ANDed with 0o377
/// (0 when absent). Takes a window id from `editor.windows` (the 31-window
/// limit counts color windows) and installs the unsaved-map quit check.
[[nodiscard]] diag::Outcome<ColorWindow*> open_color_window(editor::EditorState& editor, std::string_view octal,
                                                            const ScreenRect& frame);
/// Closes a color window and frees its id; false when `id` is none.
bool close_color_window(editor::EditorState& editor, WindowId id);

/// The view mapping of a color window: its screen area (the frame minus a
/// border; CMWcreate removes the caption and scroll bars) showing
/// kColorWindowSurface (WindMove(window, &colorWindowRect)). `setpoint`
/// uses it to turn window pixels into surface coordinates.
[[nodiscard]] ViewMapping color_window_mapping(editor::EditorState& editor, const ColorWindow& window);

/// CMWloadWindow: the window edits `color` (caption updated).
void load_color(ColorWindow& window, int color);

/// The color map entries (the session's display styles; empty without them).
[[nodiscard]] std::size_t color_count(const editor::EditorState& editor) noexcept;

/// "Color component update" (cbUpdate): sets (`replace`) or adds `x` to
/// component `c` of the window's color, clamps, converts back from HSV,
/// stores ⌊v·255 + 0.5⌋, clears the entry's name, records an undo event and
/// marks the map modified.
void update_component(editor::EditorState& editor, const ColorWindow& window, ColorComponent c, double x,
                      bool replace);

/// The mouse buttons of a press.
enum class Button : std::uint8_t { left, middle, right };

/// "Color window buttons" (cmwButtonDown): a press at `point` (surface
/// coordinates of the color window). Returns false when it hit nothing.
bool press_button(editor::EditorState& editor, const ColorWindow& window, db::Point point, Button button);

/// "Unsaved color map protection" (CMWCheckWritten): true when the map is
/// not modified or the user answers `yes` to losing the changes.
[[nodiscard]] bool check_color_map_written(editor::EditorState& editor);

/// What a color window shows, in its surface coordinates: bar fills up to
/// (c + 0.5)/255 and the HSV derived from it, outlines and texts in the
/// bounding-box style (15), the pumps, and the swatch of the edited color.
[[nodiscard]] display::HighlightScene color_window_scene(const editor::EditorState& editor,
                                                         const ColorWindow& window);

/// The `color` client's commands `color`, `load`, `pushbutton`, `save`
/// (in strcmp order) for DISPATCH's client table.
[[nodiscard]] std::vector<cmd::CommandSpec> color_client_commands();

}  // namespace hefesto::view
