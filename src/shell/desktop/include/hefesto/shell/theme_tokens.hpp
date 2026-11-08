// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The forge color tokens of hefesto-design-system ("Color tokens and themes"),
// as data. The Raven theme is built from these values in the shell's Raven
// layer; keeping them here, toolkit-free, lets tests check the values and
// their contrast.

#pragma once

#include <cstdint>
#include <string_view>

namespace hefesto::shell {

struct Rgba {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
  float a = 1.0F;

  /// From 0xRRGGBB.
  static constexpr Rgba hex(std::uint32_t rgb) noexcept {
    return Rgba{static_cast<float>((rgb >> 16U) & 0xFFU) / 255.0F, static_cast<float>((rgb >> 8U) & 0xFFU) / 255.0F,
                static_cast<float>(rgb & 0xFFU) / 255.0F, 1.0F};
  }
};

enum class ThemeMode : std::uint8_t { dark, light };

struct ThemeTokens {
  Rgba surface0;
  Rgba surface1;
  Rgba surface2;
  Rgba border;
  Rgba text_primary;
  Rgba text_secondary;
  Rgba accent;
  Rgba canvas;
  Rgba success;
  Rgba warning;
  Rgba danger;
  Rgba info;
};

[[nodiscard]] const ThemeTokens& theme_tokens(ThemeMode mode) noexcept;

/// WCAG 2.x contrast ratio between two opaque colors (1 to 21).
[[nodiscard]] double contrast_ratio(const Rgba& a, const Rgba& b) noexcept;

[[nodiscard]] std::string_view to_string(ThemeMode mode) noexcept;

}  // namespace hefesto::shell
