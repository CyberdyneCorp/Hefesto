// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/theme_tokens.hpp"

#include <algorithm>
#include <cmath>

namespace hefesto::shell {

namespace {

// Values from openspec/specs/hefesto-design-system/spec.md.
constexpr ThemeTokens kDark{
    Rgba::hex(0x121418), Rgba::hex(0x181B20), Rgba::hex(0x20242B), Rgba::hex(0x2F343D),
    Rgba::hex(0xE8EAED), Rgba::hex(0xA4ABB6), Rgba::hex(0xFF8A3D), Rgba::hex(0x0B0C0E),
    Rgba::hex(0x3FB27F), Rgba::hex(0xE5B13A), Rgba::hex(0xF0605D), Rgba::hex(0x5AA9F0),
    // mockup-derived
    Rgba::hex(0x15181C), Rgba::hex(0x0F1114), Rgba::hex(0x2A2F37), Rgba::hex(0x3D434E),
    Rgba::hex(0x8A919D), Rgba::hex(0xC9CED6), Rgba::hex(0x1A0F06), Rgba::hex(0xFFB27A),
    Rgba::hex(0x5FD3A0), Rgba::hex(0xF0C661), Rgba::hex(0xFF8784), Rgba::hex(0x82BEF5),
    Rgba::hex(0xFFA866), Rgba::hex(0xE5762E),
};

constexpr ThemeTokens kLight{
    Rgba::hex(0xF6F7F9), Rgba::hex(0xFFFFFF), Rgba::hex(0xEEF0F3), Rgba::hex(0xD9DDE3),
    Rgba::hex(0x16191D), Rgba::hex(0x4E5663), Rgba::hex(0xC24E0C), Rgba::hex(0x0B0C0E),
    Rgba::hex(0x3FB27F), Rgba::hex(0xE5B13A), Rgba::hex(0xF0605D), Rgba::hex(0x5AA9F0),
    // derived light counterparts of the mockup shades
    Rgba::hex(0xF0F2F5), Rgba::hex(0xFFFFFF), Rgba::hex(0xE2E5EA), Rgba::hex(0xC9CED6),
    Rgba::hex(0x5E6673), Rgba::hex(0x3A414C), Rgba::hex(0xFFFFFF), Rgba::hex(0xC24E0C),
    // status text: >= 4.5:1 on the 14 % chip tint over the bar and on row_selected
    Rgba::hex(0x12693F), Rgba::hex(0x7A5400), Rgba::hex(0xB2302D), Rgba::hex(0x1A5E9E),
    Rgba::hex(0xD65A12), Rgba::hex(0xA8430A),
};

double linear(float channel) noexcept {
  const double c = static_cast<double>(channel);
  return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double luminance(const Rgba& c) noexcept {
  return 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b);
}

}  // namespace

const ThemeTokens& theme_tokens(ThemeMode mode) noexcept {
  return mode == ThemeMode::light ? kLight : kDark;
}

const ThemeTokens& canvas_overlay_tokens(ThemeMode /*mode*/) noexcept {
  return kDark;
}

double contrast_ratio(const Rgba& a, const Rgba& b) noexcept {
  const double la = luminance(a);
  const double lb = luminance(b);
  return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

std::string_view to_string(ThemeMode mode) noexcept {
  return mode == ThemeMode::light ? "light" : "dark";
}

}  // namespace hefesto::shell
