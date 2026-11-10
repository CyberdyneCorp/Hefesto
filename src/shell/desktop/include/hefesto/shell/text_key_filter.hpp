// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Text-field keyboard fixes on our side of Raven 0.2.2 (m2-interactive-editing
// task 10.1; docs/decisions/raven-go-no-go.md "Raven issues"). Raven's Cocoa
// backend passes `[NSEvent characters]` as text, so the arrow, Home/End,
// Page and function keys insert AppKit's private-use characters
// U+F700-U+F8FF in text fields, and it emits no text for an auto-repeated
// key, so a held key types once. The window's event hook runs every event
// through this filter before the widgets: it drops the private-use text and,
// where the platform does not repeat text itself, retypes the held key's
// text on each repeat. Raven-free and testable.
//
// Owner: SHELL (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstdint>
#include <string>

namespace hefesto::shell {

/// AppKit's function-key range (NSUpArrowFunctionKey .. the end of the
/// private-use block it reserves): never text.
[[nodiscard]] constexpr bool is_function_key_text(std::uint32_t codepoint) noexcept {
  return codepoint >= 0xF700U && codepoint <= 0xF8FFU;
}

class TextKeyFilter {
 public:
  /// `repeat_text`: true where the platform sends no text for a repeated
  /// key (Raven's Cocoa backend); X11 and Wayland repeat the text themselves.
  explicit TextKeyFilter(bool repeat_text) noexcept : repeat_text_(repeat_text) {}

  /// A key press or repeat. Returns the UTF-8 text to type for a repeat
  /// of the held key (empty: nothing), when `text_focus` (a text field has
  /// the keyboard focus) and the platform does not repeat text itself.
  [[nodiscard]] std::string key_down(int key, bool repeat, bool text_focus);
  void key_up(int key) noexcept;
  /// A text event: false drops it (a function key's private-use character).
  /// Kept text is remembered as the held key's text for its repeats.
  [[nodiscard]] bool text(std::uint32_t codepoint);

 private:
  bool repeat_text_;
  int held_key_ = 0;  // 0: none
  std::string held_text_;
  bool recording_ = false;  // between a press and its next key event: its text
};

}  // namespace hefesto::shell
