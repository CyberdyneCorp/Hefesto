// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/input_router.hpp"

#include <array>
#include <cmath>
#include <utility>

#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/state.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::shell {

namespace {

// X11 state bits (ShiftMask, ControlMask, Mod1Mask, Mod4Mask).
constexpr std::uint32_t kX11Shift = 1U;
constexpr std::uint32_t kX11Control = 4U;
constexpr std::uint32_t kX11Mod1 = 8U;

struct SpecialKey {
  int native;
  std::uint32_t keysym;
  char text;  // XLookupString's text, '\0' for none
};

// Raven key -> X11 keysym (keysymdef.h) for the keys that are not ASCII.
constexpr std::array kSpecial = {
    SpecialKey{raven_key::kEscape, 0xFF1BU, '\x1b'},     SpecialKey{raven_key::kEnter, 0xFF0DU, '\r'},
    SpecialKey{raven_key::kTab, 0xFF09U, '\t'},          SpecialKey{raven_key::kBackspace, 0xFF08U, '\b'},
    SpecialKey{raven_key::kInsert, 0xFF63U, '\0'},       SpecialKey{raven_key::kDelete, 0xFFFFU, '\x7f'},
    SpecialKey{raven_key::kRight, 0xFF53U, '\0'},        SpecialKey{raven_key::kLeft, 0xFF51U, '\0'},
    SpecialKey{raven_key::kDown, 0xFF54U, '\0'},         SpecialKey{raven_key::kUp, 0xFF52U, '\0'},
    SpecialKey{raven_key::kPageUp, 0xFF55U, '\0'},       SpecialKey{raven_key::kPageDown, 0xFF56U, '\0'},
    SpecialKey{raven_key::kHome, 0xFF50U, '\0'},         SpecialKey{raven_key::kEnd, 0xFF57U, '\0'},
    SpecialKey{raven_key::kCapsLock, 0xFFE5U, '\0'},     SpecialKey{raven_key::kPrintScreen, 0xFF61U, '\0'},
    SpecialKey{raven_key::kPause, 0xFF13U, '\0'},        SpecialKey{raven_key::kKpDecimal, 0xFFAEU, '.'},
    SpecialKey{raven_key::kKpDivide, 0xFFAFU, '/'},      SpecialKey{raven_key::kKpMultiply, 0xFFAAU, '*'},
    SpecialKey{raven_key::kKpSubtract, 0xFFADU, '-'},    SpecialKey{raven_key::kKpAdd, 0xFFABU, '+'},
    SpecialKey{raven_key::kKpEnter, 0xFF8DU, '\r'},      SpecialKey{raven_key::kLeftShift, 0xFFE1U, '\0'},
    SpecialKey{raven_key::kLeftCtrl, 0xFFE3U, '\0'},     SpecialKey{raven_key::kLeftAlt, 0xFFE9U, '\0'},
    SpecialKey{raven_key::kLeftSuper, 0xFFEBU, '\0'},    SpecialKey{raven_key::kRightShift, 0xFFE2U, '\0'},
    SpecialKey{raven_key::kRightCtrl, 0xFFE4U, '\0'},    SpecialKey{raven_key::kRightAlt, 0xFFEAU, '\0'},
    SpecialKey{raven_key::kRightSuper, 0xFFECU, '\0'},   SpecialKey{raven_key::kMenu, 0xFF67U, '\0'},
};

// The US layout's shifted characters.
constexpr std::string_view kUnshifted = "1234567890-=[]\\;',./`";
constexpr std::string_view kShifted = "!@#$%^&*()_+{}|:\"<>?~";

std::uint32_t x11_bits(std::uint8_t modifiers) {
  std::uint32_t state = 0;
  if ((modifiers & kModShift) != 0) state |= kX11Shift;
  if ((modifiers & kModCtrl) != 0) state |= kX11Control;
  if ((modifiers & kModAlt) != 0) state |= kX11Mod1;
  if ((modifiers & kModSuper) != 0) state |= ui::kMod4;
  return state;
}

std::optional<ui::KeyEvent> special_key(int native, std::uint32_t state) {
  for (const SpecialKey& k : kSpecial) {
    if (k.native == native) {
      return ui::KeyEvent{k.keysym, state, k.text == '\0' ? std::string() : std::string(1, k.text)};
    }
  }
  if (native >= raven_key::kF1 && native <= raven_key::kF12) {
    return ui::KeyEvent{0xFFBEU + static_cast<std::uint32_t>(native - raven_key::kF1), state, {}};
  }
  if (native >= raven_key::kKp0 && native <= raven_key::kKp9) {
    const int digit = native - raven_key::kKp0;
    return ui::KeyEvent{0xFFB0U + static_cast<std::uint32_t>(digit), state, std::string(1, static_cast<char>('0' + digit))};
  }
  return std::nullopt;
}

/// The character a printable Raven key types on a US layout.
char printable_char(int native, bool shift) {
  const auto c = static_cast<char>(native);
  if (c >= 'A' && c <= 'Z') return shift ? c : static_cast<char>(c - 'A' + 'a');
  const std::size_t i = kUnshifted.find(c);
  if (i != std::string_view::npos && shift) return kShifted[i];
  return c;
}

std::string replace_all(std::string text, std::string_view from, const std::string& to) {
  for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
    text.replace(at, from.size(), to);
  }
  return text;
}

std::string_view button_name(int button) {
  switch (button) {
    case 1: return "left";
    case 2: return "middle";
    default: return "right";
  }
}

std::string coord(db::Coord v) {
  return std::to_string(v) + "i";
}

}  // namespace

std::optional<ui::KeyEvent> key_event_for(int native_key, std::uint8_t modifiers) {
  const std::uint32_t state = x11_bits(modifiers);
  if (native_key >= raven_key::kEscape) return special_key(native_key, state);
  if (native_key < raven_key::kSpace || native_key > 126) return std::nullopt;
  const bool shift = (modifiers & kModShift) != 0;
  const bool control = (modifiers & kModCtrl) != 0;
  if (native_key == raven_key::kSpace) {
    // Shift- and Control-space keep their keysym (Shift_XK_space, Control_XK_space).
    return ui::KeyEvent{0x20U, state, shift || control ? std::string() : std::string(" ")};
  }
  const char c = printable_char(native_key, shift);
  std::string text(1, c);
  if (control && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
    text = std::string(1, static_cast<char>(c & 0x1F));  // XLookupString's control character
  }
  return ui::KeyEvent{static_cast<std::uint32_t>(static_cast<unsigned char>(c)), state, std::move(text)};
}

PointerContext pointer_at(const render::Camera& camera, double pixel_ratio, double x, double y, std::int32_t window,
                          std::string window_name) {
  const double ratio = pixel_ratio > 0.0 ? pixel_ratio : 1.0;
  const double fx = x * ratio;
  const double fy = y * ratio;
  PointerContext at;
  at.window_name = std::move(window_name);
  at.state.window = window;
  at.state.point = db::Point{static_cast<db::Coord>(std::lround(camera.world_x(fx))),
                             static_cast<db::Coord>(std::lround(camera.world_y(fy)))};
  // The root area under the screen pixel (point selection tolerance).
  const double x0 = camera.world_x(std::floor(fx));
  const double x1 = camera.world_x(std::floor(fx) + 1.0);
  const double y1 = camera.world_y(std::floor(fy));
  const double y0 = camera.world_y(std::floor(fy) + 1.0);
  at.state.pixel_area = db::Rect{static_cast<db::Coord>(std::floor(x0)), static_cast<db::Coord>(std::floor(y0)),
                                 static_cast<db::Coord>(std::ceil(x1)) - 1, static_cast<db::Coord>(std::ceil(y1)) - 1};
  if (at.state.pixel_area.x1 < at.state.pixel_area.x0) at.state.pixel_area.x1 = at.state.pixel_area.x0;
  if (at.state.pixel_area.y1 < at.state.pixel_area.y0) at.state.pixel_area.y1 = at.state.pixel_area.y0;
  // Magic's window pixels count up from the bottom edge.
  at.pixel = db::Point{static_cast<db::Coord>(std::lround(fx)),
                       static_cast<db::Coord>(camera.viewport_height() - 1 - std::lround(fy))};
  return at;
}

CursorShape cursor_shape(int glyph, EditMode mode) noexcept {
  switch (glyph) {
    case 2: case 5: return CursorShape::resize_nesw;  // resize by the lower-left / upper-right corner
    case 3: case 4: return CursorShape::resize_nwse;
    case 6: case 7: case 8: case 9: case 21: return CursorShape::move;
    case 18: case 19: return CursorShape::crosshair;
    default: break;
  }
  switch (mode) {
    case EditMode::paint:
    case EditMode::measure: return CursorShape::crosshair;
    case EditMode::label: return CursorShape::ibeam;
    case EditMode::none: break;
  }
  return CursorShape::arrow;
}

InputRouter::InputRouter(cmd::Dispatcher& dispatcher, editor::EditorState& editor, bool macos)
    : dispatcher_(dispatcher), editor_(editor), macos_(macos) {}

void InputRouter::set_mode(EditMode mode) {
  mode_ = mode;
  sweep_from_.reset();
}

std::uint8_t InputRouter::x11_state(std::uint8_t modifiers) const {
  return static_cast<std::uint8_t>(x11_bits(modifiers));
}

bool InputRouter::box_tool() const {
  const ui::Tool* tool = editor_.tools.current();
  return tool == nullptr || tool->name == "box";
}

void InputRouter::set_point(const PointerContext& at) {
  editor_.command_point = at.state;
  ui::UiState& ui = ui::state(editor_);
  ui.pointer_pixel = at.pixel;
  ui.point_forced = false;
  dispatcher_.set_client("layout");
}

bool InputRouter::run(const std::string& line, const PointerContext* at) {
  if (at != nullptr) set_point(*at);
  const std::string text = at != nullptr ? ui::substitute_window(line, at->window_name) : line;
  issued_.push_back(text);
  const cmd::ParsedScript parsed = cmd::parse_script(text, "key");
  if (parsed.error) {
    dispatcher_.report(*parsed.error);
    return false;
  }
  for (const cmd::ParsedCommand& c : parsed.commands) {
    const cmd::Invocation invocation{c.words.front(), {c.words.begin() + 1, c.words.end()}, cmd::Origin::key,
                                     std::nullopt};
    if (!dispatcher_.execute(invocation).ok()) return false;
  }
  return true;
}

bool InputRouter::has_button_macro(int button, std::uint8_t modifiers) const {
  ui::KeyEvent event{ui::kPointerButton1 + static_cast<std::uint32_t>(button - 1), x11_state(modifiers), {}};
  const auto key = ui::normalize_key_event(event, macos_);
  return key && editor_.macros.find(ui::macro_table_for(editor_, "layout"), *key) != nullptr;
}

bool InputRouter::button(int button, bool down, std::uint8_t modifiers, const PointerContext& at) {
  if (button < 1 || button > 3) return false;
  if (mode_ != EditMode::none && button == 1 && box_tool()) return mode_button(down, modifiers, at);
  const auto bit = static_cast<std::uint8_t>(1U << button);
  if (!down) {
    if ((fallback_buttons_ & bit) == 0) return false;
    fallback_buttons_ = static_cast<std::uint8_t>(fallback_buttons_ & ~bit);
    run("pushbutton " + std::string(button_name(button)) + " up", &at);
    return true;
  }
  ui::KeyEvent event{ui::kPointerButton1 + static_cast<std::uint32_t>(button - 1), x11_state(modifiers), {}};
  const auto key = ui::normalize_key_event(event, macos_);
  const ui::Macro* macro = key ? editor_.macros.find(ui::macro_table_for(editor_, "layout"), *key) : nullptr;
  if (macro != nullptr) {
    run(macro->text, &at);
    return true;
  }
  // No macro: the tool's own button handler (Magic's DBWbuttons path).
  fallback_buttons_ = static_cast<std::uint8_t>(fallback_buttons_ | bit);
  run("pushbutton " + std::string(button_name(button)) + " down", &at);
  return true;
}

bool InputRouter::mode_button(bool down, std::uint8_t modifiers, const PointerContext& at) {
  if (down) {
    sweep_from_ = snapped(at.state.point);
    sweep_to_ = *sweep_from_;
    return true;
  }
  if (!sweep_from_) return false;
  sweep_to_ = snapped(at.state.point);
  set_point(at);
  finish_sweep((modifiers & kModAlt) != 0);
  return true;
}

void InputRouter::finish_sweep(bool erase) {
  const db::Rect r = *sweep();
  sweep_from_.reset();
  const std::string box =
      "box values " + coord(r.x0) + " " + coord(r.y0) + " " + coord(r.x1) + " " + coord(r.y1);
  switch (mode_) {
    case EditMode::paint:
      if (paint_layer_.empty()) {
        run(box, nullptr);
      } else {
        run(box + " ; " + (erase ? "erase " : "paint ") + paint_layer_, nullptr);
      }
      break;
    case EditMode::label:
      if (run(box, nullptr) && prompt_) prompt_("label ");
      break;
    case EditMode::measure: run(box + " ; measure", nullptr); break;
    case EditMode::none: break;
  }
}

std::optional<db::Rect> InputRouter::sweep() const {
  if (!sweep_from_) return std::nullopt;
  return db::Rect{std::min(sweep_from_->x, sweep_to_.x), std::min(sweep_from_->y, sweep_to_.y),
                  std::max(sweep_from_->x, sweep_to_.x), std::max(sweep_from_->y, sweep_to_.y)};
}

bool InputRouter::key(const ui::KeyEvent& event, const std::optional<PointerContext>& at) {
  if (event.keysym == 0xFF1BU && sweep_from_) {  // Escape cancels a sweep
    sweep_from_.reset();
    return true;
  }
  if (ui::is_interrupt_key(event)) {
    (void)ui::request_interrupt(editor_);
    return true;
  }
  const auto key = ui::normalize_key_event(event, macos_);
  if (!key) return false;  // a modifier alone
  if (at) set_point(*at);
  const ui::KeyAction action = ui::handle_key(editor_, *key);
  switch (action.kind) {
    case ui::KeyActionKind::run: run(action.text, at ? &*at : nullptr); return true;
    case ui::KeyActionKind::prompt:
      if (prompt_) prompt_(action.text);
      return true;
    case ui::KeyActionKind::unknown:
      if (action.error) dispatcher_.report(*action.error);
      return true;
    case ui::KeyActionKind::none: return true;
  }
  return false;
}

bool InputRouter::motion(const PointerContext& at) {
  if (sweep_from_) {
    sweep_to_ = snapped(at.state.point);
    return true;
  }
  const ui::UiState& ui = ui::state(editor_);
  if (!ui.motion.empty()) {
    const db::Point p = snapped(at.state.point);
    std::string line = replace_all(ui.motion, "%X", std::to_string(p.x - ui.drag_origin.x));
    line = replace_all(std::move(line), "%Y", std::to_string(p.y - ui.drag_origin.y));
    return run(line, &at);
  }
  if (editor_.wire.tracking) return run("*bypass wire show", &at);
  return false;
}

db::Point InputRouter::snapped(db::Point p) const {
  // ui-tools-and-input "Pointer snapping" (the core's `cursor` uses the same grids).
  cmd::SnapGrid grid;
  if (editor_.snap == editor::SnapMode::internal) return p;
  if (editor_.snap == editor::SnapMode::lambda) {
    const std::int64_t pitch = editor_.grid.lambda > 0 ? editor_.grid.internal / editor_.grid.lambda : 1;
    grid.pitch_x = grid.pitch_y = static_cast<db::Coord>(pitch > 0 ? pitch : 1);
  } else if (const view::LayoutWindow* w = editor_.windows.current()) {
    const db::Rect s = w->grid().spacing;
    grid.origin = db::Point{s.x0, s.y0};
    grid.pitch_x = s.x1 - s.x0 > 0 ? s.x1 - s.x0 : 1;
    grid.pitch_y = s.y1 - s.y0 > 0 ? s.y1 - s.y0 : 1;
  }
  return cmd::snap_point(p, grid);
}

}  // namespace hefesto::shell
