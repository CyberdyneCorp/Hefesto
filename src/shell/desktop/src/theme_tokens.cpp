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
};

constexpr ThemeTokens kLight{
    Rgba::hex(0xF6F7F9), Rgba::hex(0xFFFFFF), Rgba::hex(0xEEF0F3), Rgba::hex(0xD9DDE3),
    Rgba::hex(0x16191D), Rgba::hex(0x4E5663), Rgba::hex(0xC24E0C), Rgba::hex(0x0B0C0E),
    Rgba::hex(0x3FB27F), Rgba::hex(0xE5B13A), Rgba::hex(0xF0605D), Rgba::hex(0x5AA9F0),
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

double contrast_ratio(const Rgba& a, const Rgba& b) noexcept {
  const double la = luminance(a);
  const double lb = luminance(b);
  return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

std::string_view to_string(ThemeMode mode) noexcept {
  return mode == ThemeMode::light ? "light" : "dark";
}

}  // namespace hefesto::shell
