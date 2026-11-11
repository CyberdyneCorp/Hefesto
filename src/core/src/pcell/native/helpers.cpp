// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK helper procedures (Q-SCRIPT-40), ported from the pinned PDKs:
// open_pdks sky130A.tcl:282-353 and gf180mcuD.tcl:233-304 (identical), IHP
// ihp-sg13g2-util.tcl:691-787 (u2i rounds, i2u formats with %.3f).

#include "hefesto/pcell/native/helpers.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <utility>
#include <vector>

#include "pcell/native/tcl_number.hpp"

namespace hefesto::pcell {

namespace {

using Map = std::vector<std::pair<std::string_view, std::string_view>>;

// Tcl's `string map`: at each position the first key that matches is
// replaced and the scan continues after it.
std::string string_map(std::string_view text, const Map& map) {
  std::string out;
  std::size_t i = 0;
  while (i < text.size()) {
    bool replaced = false;
    for (const auto& [key, value] : map) {
      if (text.substr(i, key.size()) == key) {
        out += value;
        i += key.size();
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      out += text[i++];
    }
  }
  return out;
}

std::string lower(std::string_view text) {
  std::string out{text};
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

// (round([magic::cif scale output] * 10000)) / 10000.0
std::optional<tcl::Number> scale_factor(std::string_view cif_scale) {
  const tcl::Result scale = tcl::operand(cif_scale);
  if (scale.status != tcl::Status::ok) {
    return std::nullopt;
  }
  const tcl::Result times = tcl::apply('*', scale.value, tcl::Number::of(std::int64_t{10000}));
  const tcl::Result rounded = times.status == tcl::Status::ok ? tcl::round(times.value) : times;
  if (rounded.status != tcl::Status::ok) {
    return std::nullopt;
  }
  const tcl::Result factor = tcl::apply('/', rounded.value, tcl::Number::of(10000.0));
  return factor.status == tcl::Status::ok ? std::optional{factor.value} : std::nullopt;
}

std::optional<tcl::Number> ok_value(const tcl::Result& r) {
  return r.status == tcl::Status::ok ? std::optional{r.value} : std::nullopt;
}

}  // namespace

std::optional<SpiceFloat> PdkHelpers::spice2float(std::string_view value, std::string_view faultval) const {
  static const Map kUnits = {{"um", "u"}, {"nm", "n"}, {"uF", "n"}, {"nF", "n"}, {"pF", "p"}, {"aF", "a"}};
  static const Map kSuffixes = {{"meg", "* 1.0e6"},    {"k", "* 1.0e3"},    {"m", "* 1.0e-3"},
                                {"u", "* 1.0e-6"},     {"n", "* 1.0 e-9"},  {"p", "* 1.0e-12"},
                                {"f", "* 1.0e-15"},    {"a", "* 1.0e-15"}};
  const std::string expression = string_map(string_map(lower(value), kUnits), kSuffixes);
  const tcl::Result r = tcl::evaluate(expression);
  if (r.status == tcl::Status::unsupported) {
    return std::nullopt;
  }
  if (r.status == tcl::Status::error) {
    return SpiceFloat{std::string{faultval}, true};
  }
  return SpiceFloat{tcl::format(r.value), false};
}

std::optional<std::string> PdkHelpers::u2i(std::string_view microns) const {
  const auto factor = scale_factor(cif_scale_output);
  const auto v = ok_value(tcl::operand(microns));
  if (!factor || !v) {
    return std::nullopt;
  }
  const auto quotient = ok_value(tcl::apply('/', *v, *factor));
  if (!quotient) {
    return std::nullopt;
  }
  if (family != HelperFamily::sg13g2) {
    return tcl::format(*quotient);
  }
  const auto rounded = ok_value(tcl::round(*quotient));
  return rounded ? std::optional{tcl::format(*rounded)} : std::nullopt;
}

std::optional<std::string> PdkHelpers::i2u(std::string_view internal) const {
  const auto factor = scale_factor(cif_scale_output);
  const auto v = ok_value(tcl::operand(internal));
  if (!factor || !v) {
    return std::nullopt;
  }
  const auto product = ok_value(tcl::apply('*', *factor, *v));
  if (!product) {
    return std::nullopt;
  }
  if (family != HelperFamily::sg13g2) {
    return tcl::format(*product);
  }
  std::array<char, 400> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.3f", product->as_double());
  return std::string{buffer.data()};
}

std::optional<std::string> PdkHelpers::three_digit_past_decimal(std::string_view value) const {
  // expr int([expr $value * 1000 + 0.5 ]) / 1000.0
  const auto inner = ok_value(tcl::evaluate(std::string{value} + " * 1000 + 0.5"));
  if (!inner) {
    return std::nullopt;
  }
  const auto reparsed = ok_value(tcl::operand(tcl::format(*inner)));
  const auto truncated = reparsed ? ok_value(tcl::truncate(*reparsed)) : std::nullopt;
  if (!truncated) {
    return std::nullopt;
  }
  const auto result = ok_value(tcl::apply('/', *truncated, tcl::Number::of(1000.0)));
  return result ? std::optional{tcl::format(*result)} : std::nullopt;
}

}  // namespace hefesto::pcell
