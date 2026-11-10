// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Key normalization, interactive keystrokes, box tool buttons and the touch
// gesture mapping (ui-command-dispatch "Interactive keystroke handling";
// ui-tools-and-input "Key event normalization", "Box tool buttons", "Touch
// and pointer gesture mapping"). Owner: DISPATCH.
// (Magic ref: graphics/grTOGL1.c:505-610, textio/txCommands.c:1199-1363,
// dbwind/DBWbuttons.c:355-478, dbwind/DBWtools.c:373-406)

#include "hefesto/ui/input.hpp"

#include <array>
#include <cmath>
#include <cstdio>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::ui {

namespace {

constexpr std::uint32_t kReturn = 0xFF0DU;
constexpr std::uint8_t kModifierMask = kShift | kLock | kControl | kMeta;

std::uint8_t event_modifiers(std::uint32_t state, bool macos) {
  auto mods = static_cast<std::uint8_t>(state & (kShift | kLock | kControl));
  const std::uint32_t meta_bits = macos ? (kMeta | kMod2 | kMod3 | kMod4 | kMod5) : std::uint32_t{kMeta};
  if ((state & meta_bits) != 0) {
    mods = static_cast<std::uint8_t>(mods | kMeta);
  }
  return mods;
}

bool is_keypad(std::uint32_t keysym) {
  return keysym_name(keysym & 0xFFFFU).starts_with("KP_");
}

// "Remove single modifier (Control or Shift)" of an ASCII-valued key.
std::uint8_t drop_single_modifier(std::uint8_t mods) {
  if ((mods & (kLock | kMeta)) != 0) {
    return mods;
  }
  if ((mods & kControl) == 0) {
    return static_cast<std::uint8_t>(mods & ~kShift);
  }
  if ((mods & kShift) == 0) {
    return static_cast<std::uint8_t>(mods & ~kControl);
  }
  return mods;
}

Corner nearest_corner(const db::Rect& box, db::Point p) {
  const bool left = p.x < (box.x0 + box.x1) / 2;
  const bool bottom = p.y < (box.y0 + box.y1) / 2;
  if (left) {
    return bottom ? Corner::bl : Corner::tl;
  }
  return bottom ? Corner::br : Corner::tr;
}

std::uint8_t bit(Button b) {
  return static_cast<std::uint8_t>(b);
}

std::vector<std::string> box_command(Button released, Corner corner) {
  const bool move = released == Button::left;
  return {"box", move ? "move" : "corner", std::string{corner_name(corner)}, "cursor"};
}

}  // namespace

// --- Key event normalization ------------------------------------------------

bool is_modifier_keysym(std::uint32_t keysym) noexcept {
  // X11 IsModifierKey: Shift_L..Hyper_R, Mode_switch, Num_Lock and the ISO
  // lock/shift keys.
  return (keysym >= 0xFFE1U && keysym <= 0xFFEEU) || keysym == 0xFF7EU || keysym == 0xFF7FU ||
         (keysym >= 0xFE01U && keysym <= 0xFE13U);
}

bool is_interrupt_key(const KeyEvent& event) noexcept {
  return event.text.size() == 1 && event.text.front() == '\x03';
}

std::optional<Key> normalize_key_event(const KeyEvent& event, bool macos) {
  if (is_modifier_keysym(event.keysym)) {
    return std::nullopt;
  }
  std::uint8_t mods = event_modifiers(event.state, macos);
  if (event.text.empty()) {
    return Key{event.keysym & 0xFFFFU, mods};
  }
  if (is_keypad(event.keysym)) {
    return Key{event.keysym & 0xFFFFU, static_cast<std::uint8_t>(mods & ~kShift)};
  }
  mods = drop_single_modifier(mods);
  auto ch = static_cast<unsigned char>(event.text.front());
  if ((mods & kControl) != 0 && ch < 32U) {
    ch = static_cast<unsigned char>(ch + 'A' - 1);
  }
  return Key{ch, static_cast<std::uint8_t>(mods & kModifierMask)};
}

// --- Interactive keystroke handling ------------------------------------------

std::string macro_table_for(const editor::EditorState& editor, std::string_view client) {
  if (client != "layout") {
    return std::string{client};
  }
  const Tool* tool = editor.tools.current();
  return tool == nullptr ? std::string{"box"} : tool->name;
}

KeyAction handle_key(const editor::EditorState& editor, const Key& key, std::string_view client) {
  const std::string table = macro_table_for(editor, client);
  if (const Macro* macro = editor.macros.find(table, key)) {
    if (!macro->interactive) {
      return KeyAction{KeyActionKind::run, macro->text, std::nullopt};
    }
    std::string_view text = macro->text;
    if (text.starts_with(':')) {
      text.remove_prefix(1);
    }
    return KeyAction{KeyActionKind::prompt, std::string{text}, std::nullopt};
  }
  if (key.modifiers == 0 && (key.code == ':' || key.code == ';')) {
    return KeyAction{KeyActionKind::prompt, {}, std::nullopt};
  }
  if (key.modifiers == 0 && (key.code == kReturn || key.code == '\r')) {
    return {};
  }
  return KeyAction{KeyActionKind::unknown, {},
                   diag::make(codes::kUnknownKey, "Unknown macro or short command: '" + key_display_name(key) + "'")};
}

void remember_command_line(editor::EditorState& editor, std::string_view line) {
  if (line.empty()) {
    return;
  }
  editor.macros.set(macro_table_for(editor, "layout"), Key{'.', 0}, Macro{std::string{line}, {}, false});
}

// --- Box tool buttons --------------------------------------------------------

int corner_glyph(Corner corner, bool move) noexcept {
  static constexpr std::array<int, 4> kMove = {6, 7, 8, 9};    // LL LR UL UR
  static constexpr std::array<int, 4> kResize = {2, 3, 4, 5};  // LL LR UL UR
  const auto index = static_cast<std::size_t>(corner);
  return move ? kMove.at(index) : kResize.at(index);
}

std::string_view corner_name(Corner corner) noexcept {
  static constexpr std::array<std::string_view, 4> kNames = {"bl", "br", "tl", "tr"};
  return kNames.at(static_cast<std::size_t>(corner));
}

std::optional<std::vector<std::string>> box_tool_button(editor::EditorState& editor, Button button, bool down,
                                                        std::optional<db::Rect> box, db::Point point) {
  UiState& ui = state(editor);
  BoxToolState& tool = ui.box_tool;
  if (button == Button::middle) {
    tool.held = static_cast<std::uint8_t>(down ? tool.held | bit(button) : tool.held & ~bit(button));
    return down ? std::optional<std::vector<std::string>>{{"paint", "cursor"}} : std::nullopt;
  }
  const Button other = button == Button::left ? Button::right : Button::left;
  if (down) {
    tool.held = static_cast<std::uint8_t>(tool.held | bit(button));
    const bool both = (tool.held & bit(other)) != 0;
    if (both) {
      tool.corner = box ? nearest_corner(*box, point) : Corner::bl;
    } else {
      tool.corner = button == Button::left ? Corner::bl : Corner::tr;
    }
    // With both down, the first button pressed decides move or resize.
    ui.cursor_glyph = corner_glyph(tool.corner, (both ? other : button) == Button::left);
    return std::nullopt;
  }
  tool.held = static_cast<std::uint8_t>(tool.held & ~bit(button));
  if ((tool.held & (bit(Button::left) | bit(Button::right))) != 0) {
    ui.cursor_glyph = corner_glyph(tool.corner, other == Button::left);
    return std::nullopt;
  }
  ui.cursor_glyph = 0;
  return box_command(button, tool.corner);
}

// --- Touch and pointer gesture mapping ----------------------------------------

std::span<const GestureBinding> gesture_table() noexcept {
  static constexpr std::array kTable = {
      GestureBinding{Gesture::tap, "the current tool's primary button (Button1)"},
      GestureBinding{Gesture::drag, "the current tool's primary button (Button1) pressed, moved and released"},
      GestureBinding{Gesture::pencil, "the current tool's primary button (Button1)"},
      GestureBinding{Gesture::long_press, "a context menu of the tool's Button2 and Button3 actions"},
      GestureBinding{Gesture::two_finger_pan, "scroll by the dragged distance"},
      GestureBinding{Gesture::pinch, "zoom about the pinch center"},
  };
  return kTable;
}

Key gesture_button(const TouchModifiers& mods) noexcept {
  const int button = mods.button == 2 || mods.button == 3 ? mods.button : 1;
  std::uint8_t modifiers = 0;
  if (mods.shift) {
    modifiers = static_cast<std::uint8_t>(modifiers | kShift);
  }
  if (mods.control) {
    modifiers = static_cast<std::uint8_t>(modifiers | kControl);
  }
  return button_key(button, modifiers);
}

std::vector<std::pair<std::string, std::string>> long_press_actions(const editor::EditorState& editor) {
  std::vector<std::pair<std::string, std::string>> out;
  const std::string table = macro_table_for(editor, "layout");
  for (const int button : {2, 3}) {
    for (const std::uint8_t mods : {std::uint8_t{0}, std::uint8_t{kShift}, std::uint8_t{kControl}}) {
      const Key key = button_key(button, mods);
      if (const Macro* macro = editor.macros.find(table, key)) {
        out.emplace_back(key_display_name(key), macro->text);
      }
    }
  }
  return out;
}

std::string pan_command(db::Coord dx, db::Coord dy) {
  // The fingers drag the layout: the view moves the other way.
  std::string out;
  const auto add = [&](std::string_view dir, db::Coord amount) {
    if (amount == 0) {
      return;
    }
    if (!out.empty()) {
      out += " ; ";
    }
    out += "scroll " + std::string{dir} + " " + std::to_string(amount < 0 ? -amount : amount) + "i";
  };
  add(dx > 0 ? "l" : "r", dx);
  add(dy > 0 ? "d" : "u", dy);
  return out;
}

std::string pinch_command(double scale) {
  if (!(scale > 0.0)) {
    return {};
  }
  std::array<char, 32> buf{};
  std::snprintf(buf.data(), buf.size(), "zoom %g", 1.0 / scale);
  return buf.data();
}

}  // namespace hefesto::ui
