// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/text_key_filter.hpp"

namespace hefesto::shell {

namespace {

void append_utf8(std::string& out, std::uint32_t cp) {
  const auto byte = [&out](std::uint32_t v) { out.push_back(static_cast<char>(static_cast<unsigned char>(v))); };
  if (cp < 0x80U) {
    byte(cp);
  } else if (cp < 0x800U) {
    byte(0xC0U | (cp >> 6U));
    byte(0x80U | (cp & 0x3FU));
  } else if (cp < 0x10000U) {
    byte(0xE0U | (cp >> 12U));
    byte(0x80U | ((cp >> 6U) & 0x3FU));
    byte(0x80U | (cp & 0x3FU));
  } else {
    byte(0xF0U | (cp >> 18U));
    byte(0x80U | ((cp >> 12U) & 0x3FU));
    byte(0x80U | ((cp >> 6U) & 0x3FU));
    byte(0x80U | (cp & 0x3FU));
  }
}

}  // namespace

std::string TextKeyFilter::key_down(int key, bool repeat, bool text_focus) {
  if (!repeat) {
    // A new press: its text (if any) follows as a text event.
    held_key_ = key;
    held_text_.clear();
    recording_ = true;
    return {};
  }
  recording_ = false;  // the repeat's own text (ours, or the platform's) is not the press's
  if (!repeat_text_ || !text_focus || key != held_key_) return {};
  return held_text_;
}

void TextKeyFilter::key_up(int key) noexcept {
  if (key != held_key_) return;
  held_key_ = 0;
  held_text_.clear();
  recording_ = false;
}

bool TextKeyFilter::text(std::uint32_t codepoint) {
  if (is_function_key_text(codepoint)) return false;
  // Control characters are not text either (Raven's text fields drop them).
  if (recording_ && codepoint >= 0x20U && codepoint != 0x7FU) append_utf8(held_text_, codepoint);
  return true;
}

}  // namespace hefesto::shell
