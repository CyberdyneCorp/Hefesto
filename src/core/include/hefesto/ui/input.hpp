// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pointer and key input in the core (capabilities ui-command-dispatch
// "Interactive keystroke handling", ui-tools-and-input "Key event
// normalization", "Box tool buttons", "Touch and pointer gesture mapping";
// design.md D7). Front ends turn native events into keysyms and modifiers;
// these functions turn them into macro keys, macro lookups and commands, so
// every front end (desktop, iPad in M14) behaves the same.
// (Magic ref: graphics/grTOGL1.c:505-610, textio/txCommands.c:1199-1363,
// dbwind/DBWbuttons.c:355-478)
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/ui/macros.hpp"
#include "hefesto/ui/state.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::ui {

// --- Key event normalization ------------------------------------------------

/// X11 modifier state bits beyond Modifier (Mod2..Mod5).
inline constexpr std::uint32_t kMod2 = 16U, kMod3 = 32U, kMod4 = 64U, kMod5 = 128U;

/// A native key press or pointer button press, translated by the front end.
struct KeyEvent {
  std::uint32_t keysym = 0;  // X11 keysym (XK_Pointer_ButtonN for buttons)
  std::uint32_t state = 0;   // X11 modifier state (Shift, Lock, Control, Mod1..Mod5)
  std::string text;          // the ASCII text of the key, empty if none
};

/// True for modifier-only keysyms (Shift_L ... Hyper_R, Caps_Lock, ...).
[[nodiscard]] bool is_modifier_keysym(std::uint32_t keysym) noexcept;

/// "Key event normalization": the macro key of an event, std::nullopt for a
/// modifier-only press. `macos`: any of Mod1-Mod5 counts as Meta.
[[nodiscard]] std::optional<Key> normalize_key_event(const KeyEvent& event, bool macos);

/// True when the event is Ctrl-C (the interrupt key).
[[nodiscard]] bool is_interrupt_key(const KeyEvent& event) noexcept;

// --- Interactive keystroke handling ------------------------------------------

/// The macro table for keys and buttons in a window of `client`: the
/// current tool's for `layout`, the client name otherwise.
[[nodiscard]] std::string macro_table_for(const editor::EditorState& editor, std::string_view client);

enum class KeyActionKind : std::uint8_t {
  none,     // nothing to do (Return without macro: reprint the prompt)
  run,      // run `text` as a command line
  prompt,   // open the command prompt pre-filled with `text`
  unknown,  // report `error` (no macro)
};

struct KeyAction {
  KeyActionKind kind = KeyActionKind::none;
  std::string text;
  std::optional<diag::Diagnostic> error;
};

/// What a key pressed in a window of `client` does (outside a pending text
/// entry): a macro runs, an interactive macro opens the prompt (a leading
/// `:` removed), `:`/`;` without macro open an empty prompt, Return
/// without macro does nothing, any other key reports UI-KEY-001.
[[nodiscard]] KeyAction handle_key(const editor::EditorState& editor, const Key& key,
                                   std::string_view client = "layout");

/// A line entered at the command prompt: a non-empty line is stored as the
/// `.` macro of the layout client's current table (the `.` repeat).
void remember_command_line(editor::EditorState& editor, std::string_view line);

// --- Box tool buttons --------------------------------------------------------

/// The box tool's handling of a button event at the command point (the
/// state lives in ui::state(editor).box_tool; the cursor glyph is updated).
/// Returns the command to run, if any: `paint cursor` on a middle press,
/// `box move <corner> cursor` or `box corner <corner> cursor` when the last
/// button comes up. `box` is the current box in root coordinates (corner
/// choice when both buttons are down).
[[nodiscard]] std::optional<std::vector<std::string>> box_tool_button(editor::EditorState& editor, Button button,
                                                                      bool down, std::optional<db::Rect> box,
                                                                      db::Point point);

/// ui-tools-and-input "Pointer snapping" (ToolSnapToGrid): `p` snapped to
/// the `snap` grid (internal: unchanged; lambda: the lambda pitch; user: the
/// current window's grid). The `cursor` command and front ends computing
/// `%X`/`%Y` or sweep corners use the same rule.
[[nodiscard]] db::Point snap_to_tool_grid(const editor::EditorState& editor, db::Point p);

/// The cursor glyphs of "Box tool buttons" (move: 6-9, resize: 2-5).
[[nodiscard]] int corner_glyph(Corner corner, bool move) noexcept;

/// The position name of a corner as `box` takes it (`bl`, `br`, `tl`, `tr`).
[[nodiscard]] std::string_view corner_name(Corner corner) noexcept;

// --- Touch and pointer gesture mapping ----------------------------------------

enum class Gesture : std::uint8_t { tap, drag, pencil, long_press, two_finger_pan, pinch };

/// The floating modifier key of the touch UI (hefesto-input-and-touch).
struct TouchModifiers {
  /// 0: primary (Button1), 2: middle (Button2), 3: right (Button3).
  int button = 0;
  bool shift = false;
  bool control = false;
};

/// One entry of the gesture table (documentation and the shell's help).
struct GestureBinding {
  Gesture gesture;
  std::string_view meaning;
};
[[nodiscard]] std::span<const GestureBinding> gesture_table() noexcept;

/// The button key a tap, drag or Pencil stroke stands for under `mods`.
[[nodiscard]] Key gesture_button(const TouchModifiers& mods) noexcept;

/// The long-press context menu: the macros of Button2 and Button3 (and
/// their Shift/Control variants) in the current tool's table, as
/// (key display name, command text) pairs.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> long_press_actions(const editor::EditorState& editor);

/// The command a two-finger pan by (dx, dy) layout units runs: `scroll`
/// by the dragged distance in internal units (empty when zero).
[[nodiscard]] std::string pan_command(db::Coord dx, db::Coord dy);
/// The command a pinch by `scale` (> 1: fingers apart) runs: `zoom 1/scale`.
[[nodiscard]] std::string pinch_command(double scale);

}  // namespace hefesto::ui
