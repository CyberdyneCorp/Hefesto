// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// esSIvalue and the printf formats of the SPICE writer (capability
// netlist-spice-options "SI value formatting").
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:3485-3656
// (nDecimals, morphNumericString, esSIvalue), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include "netlist/spice/numbers.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <system_error>

#include "hefesto/netlist/spice/settings.hpp"

namespace hefesto::netlist::spice {

namespace detail {

namespace {

std::string chars(double value, std::chars_format format, int precision) {
  std::array<char, 400> buffer{};
  const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, format, precision);
  return ec == std::errc{} ? std::string(buffer.data(), end) : std::string{};
}

// nDecimals: `%*.*f` with the width Magic computes (whole digits, sign,
// point and n decimals).
std::string n_decimals(double d, int n) {
  double d2 = d >= 0 ? d : -d;
  std::size_t width = d >= 0 ? 0 : 1;
  if (d2 < 1) {
    ++width;
  }
  while (d2 >= 1) {
    d2 /= 10.0;
    ++width;
  }
  width += 1 + static_cast<std::size_t>(n);
  std::string text = chars(d, std::chars_format::fixed, n);
  if (text.size() < width) {
    text.insert(0, width - text.size(), ' ');
  }
  return text;
}

// morphNumericString on the text n_decimals printed: trailing zeros and
// then a trailing point are removed.
void strip_zeros(std::string& text) {
  if (text.find('.') == std::string::npos) {
    return;
  }
  while (!text.empty() && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
}

struct Scaled {
  float value;
  char suffix;
};

// The suffix choice in esSIvalue's order; the products are stored in a
// float, as Magic's `value *= 1.0E15` on a float parameter does.
Scaled choose_suffix(float value) {
  const double a = dbl(std::fabs(value));
  if (a < 1.0E-18) {
    return {value, '\0'};
  }
  if (a < 0.9999E-13) {
    return {static_cast<float>(dbl(value) * 1.0E15), 'f'};
  }
  if (a < 1.0001E-10) {
    return {static_cast<float>(dbl(value) * 1.0E12), 'p'};
  }
  if (a < 1.0001E-7) {
    return {static_cast<float>(dbl(value) * 1.0E9), 'n'};
  }
  if (a < 1.0001E-4) {
    return {static_cast<float>(dbl(value) * 1.0E6), 'u'};
  }
  if (a < 1.0001E-2) {
    return {static_cast<float>(dbl(value) * 1.0E3), 'm'};
  }
  if (a > 0.9999E9) {
    return {static_cast<float>(dbl(value) / 1.0E9), 'G'};
  }
  if (a > 0.9999E3) {
    return {static_cast<float>(dbl(value) / 1.0E3), 'k'};
  }
  return {value, '\0'};
}

}  // namespace

std::string format_g(double value) {
  if (std::isnan(value)) {
    return std::signbit(value) ? "-nan" : "nan";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-inf" : "inf";
  }
  return chars(value, std::chars_format::general, 6);
}

std::string format_f(double value) {
  if (!std::isfinite(value)) {
    return format_g(value);
  }
  return chars(value, std::chars_format::fixed, 6);
}

std::string si(float value) {
  if (!std::isfinite(value)) {
    return format_g(dbl(value));
  }
  const Scaled scaled = choose_suffix(value);
  std::string text = n_decimals(dbl(scaled.value), 5);
  strip_zeros(text);
  if (scaled.suffix != '\0') {
    text += scaled.suffix;
  }
  return text;
}

}  // namespace detail

std::string format_si(double value) {
  return detail::si(static_cast<float>(value));
}

}  // namespace hefesto::netlist::spice
