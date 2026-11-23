// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtStyle::micron_convert (the conversion half of extTechFinalStyle) and
// ExtStyle::rescale (ExtTechScale): capability extract-tech-section "Micron
// conversion" (Q-EXTRACT-3, Q-EXTRACT-19, Q-EXTRACT-21), "Scaling to
// internal units" (Q-EXTRACT-25), "Layer height and thickness defaults";
// openspec/changes/m7-extraction/design.md D7, tasks.md 4.6.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c extTechFinalStyle
// (:3858-3998) and ExtTechScale (:4022-4130), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Magic's arithmetic is kept: an int divided by a float in float, int
// lengths scaled with integer division, capacitances multiplied and then
// divided. The parameter list of a device line is shared by the line's
// records, and Magic's loops visit it once per record, so with magic-compat
// ON its offsets and limits are converted once per type of the line (an
// unreported Magic behaviour, kept as the standing rule asks); with it OFF
// once.
//
// Owner: EXTTECH.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "rule_context.hpp"

namespace hefesto::extract {

namespace {

constexpr double dbl(float f) noexcept { return static_cast<double>(f); }

// ExtTechScale's `(float)(scalen * scalen)`. The product is taken in float:
// Magic's int product overflows for a factor above 46340 (undefined
// behaviour, fixed in both modes).
constexpr float square(std::int32_t v) noexcept { return static_cast<float>(v) * static_cast<float>(v); }

// A float truncated to int as Magic's `int *= float` does, saturated to
// the int range and NaN as 0 instead of undefined (Q-EXTRACT-30).
std::int32_t truncate_int(double v) noexcept {
  return detail::saturate_int32(v);
}

// How many times Magic's per-record loops visit a pattern's shared list.
std::size_t visits(const DevicePattern& d, bool magic_compat) {
  return magic_compat ? std::max<std::size_t>(d.types.count(), 1) : 1;
}

bool area_param(const DeviceParam& p) {
  return !p.code.empty() && p.code.front() == 'a';
}

// `int /= float`: the int converted to float, divided, truncated.
double int_div_float(double value, float by) {
  return detail::saturate_int32(static_cast<double>(static_cast<float>(value) / by));
}

void convert_param(DeviceParam& p, float dscale, float dsq, bool magic_compat) {
  const float by = area_param(p) ? dsq : dscale;
  const auto div = [&](double v) { return magic_compat ? int_div_float(v, by) : v / dbl(by); };
  if (p.offset != 0.0) {
    p.offset = div(p.offset);
  }
  if (p.minimum > p.maximum) {
    return;
  }
  p.maximum = div(p.maximum);
  p.minimum = div(p.minimum);
}

void normalize_param(DeviceParam& p) {
  if (p.offset != 0.0) {
    p.offset = detail::saturate_int32(p.offset) / 1000;
  }
  if (p.minimum > p.maximum) {
    return;
  }
  p.maximum = detail::saturate_int32(p.maximum) / 1000;
  p.minimum = detail::saturate_int32(p.minimum) / 1000;
}

void convert_device(DevicePattern& d, float dscale, bool magic_compat) {
  const float dsq = dscale * dscale;
  const double scalefac = dbl(dscale);
  const double sqfac = scalefac * scalefac;
  // Q-EXTRACT-3: gscap like an area with magic-compat ON, a length OFF.
  d.perimeter_cap *= magic_compat ? sqfac : scalefac;
  d.area_cap *= sqfac;
  for (std::size_t i = 0; i < visits(d, magic_compat); ++i) {
    for (DeviceParam& p : d.params) {
      convert_param(p, dscale, dsq, magic_compat);
    }
  }
  for (DeviceResist& r : d.resist) {
    if (r.region == "area") {
      r.value = detail::saturate_int32(static_cast<double>(static_cast<float>(r.value) / dsq));
    } else if (r.region == "perimeter" || r.region == "linear") {
      r.value = detail::saturate_int32(static_cast<double>(static_cast<float>(r.value) / dscale));
    }
  }
}

void convert_caps(CapRules& caps, float dscale) {
  const double scalefac = dbl(dscale);
  const double sqfac = scalefac * scalefac;
  CapTables& t = caps.tables;
  for (double& v : t.area) {
    v *= sqfac;
  }
  for (auto& [key, v] : t.perimeter) {
    v *= scalefac;
  }
  for (auto& [key, v] : t.overlap) {
    v *= sqfac;
  }
  for (auto& [key, v] : t.fringe_mult) {
    v = static_cast<float>(dbl(v) * scalefac);
  }
  for (auto& [key, rules] : t.side_overlap) {
    for (EdgeRule& r : rules) {
      r.cap *= scalefac;
    }
  }
  for (auto& [key, rules] : t.side_couple) {
    for (EdgeRule& r : rules) {
      r.offset /= dscale;
    }
  }
  for (TypeCap& c : caps.area) {
    c.cap *= sqfac;
  }
  for (TypeCap& c : caps.perimeter) {
    c.cap *= scalefac;
  }
  for (OverlapRule& r : caps.overlap) {
    r.cap *= sqfac;
  }
  for (SideOverlapRule& r : caps.sideoverlap) {
    r.cap *= scalefac;
  }
  for (SidewallRule& r : caps.sidewall) {
    r.offset /= dscale;
  }
}

// doConvert (`units microns`): microns to lambda with dscale.
void convert_units(ExtStyle& s, float dscale) {
  const float dsq = dscale * dscale;
  for (DevicePattern& d : s.devices) {
    convert_device(d, dscale, s.magic_compat);
  }
  convert_caps(s.caps, dscale);
  for (TypeHeightThickness& h : s.heights) {
    h.thickness /= dscale;
    h.height /= dscale;
  }
  for (AntennaRule& a : s.antenna) {
    if (a.ratio_diff.size() > 1) {
      a.ratio_diff[1] = dbl(static_cast<float>(a.ratio_diff[1]) * dsq);
    }
  }
  // `(int)(((float)v / dscale) + 0.5)`: the quotient in float, the sum in double.
  s.side_halo = detail::saturate_int32(static_cast<double>(static_cast<float>(s.side_halo) / dscale) + 0.5);
  s.step_size = detail::saturate_int32(static_cast<double>(static_cast<float>(s.step_size) / dscale) + 0.5);
  s.side_halo_exact /= dbl(dscale);
  s.step_exact /= dbl(dscale);
}

// The zero-step reset and the /1000 normalisations (Q-EXTRACT-19,
// Q-EXTRACT-21), after the unit conversion.
void normalize(ExtStyle& s, diag::DiagnosticSink* sink) {
  if (!s.magic_compat) {
    s.step_size = detail::saturate_round(s.step_exact);
  }
  if (s.step_size <= 0) {
    if (sink != nullptr) {
      sink->emit(diag::make(detail::kCodeZeroStep, "Warning:  zero step size!  Resetting to default."));
    }
    s.step_size = 100;
    s.step_exact = 100.0;
  }
  if (!s.magic_compat) {
    s.side_halo = detail::saturate_round(s.side_halo_exact);
    return;
  }
  s.side_halo /= 1000;
  for (DevicePattern& d : s.devices) {
    for (std::size_t i = 0; i < visits(d, true); ++i) {
      for (DeviceParam& p : d.params) {
        normalize_param(p);
      }
    }
  }
}

ExtStyle convert(const ExtStyle& in, const mask::OutputStyle* style, diag::DiagnosticSink* sink) {
  ExtStyle out = in;
  if (out.converted) {
    return out;
  }
  out.converted = true;
  // With `noplaneordering` extTechFinalStyle returns before all of this.
  if (!out.plane_ordering) {
    return out;
  }
  if (out.units_microns) {
    convert_units(out, HeightView::microns_per_lambda(style));
  }
  normalize(out, sink);
  return out;
}

// ExtTechScale of one parameter (Q-EXTRACT-21: integer steps with
// magic-compat ON, milli-units kept exact with it OFF).
void scale_param(DeviceParam& p, std::int32_t n, std::int32_t d, bool magic_compat) {
  const bool area = area_param(p);
  const float sqn = square(n);
  const float sqd = square(d);
  const auto scale = [&](double v) -> double {
    if (!magic_compat) {
      return area ? v * dbl(sqd) / dbl(sqn) : v * d / n;
    }
    if (area) {
      const std::int32_t grown = truncate_int(dbl(static_cast<float>(v) * sqd));
      return truncate_int(dbl(static_cast<float>(grown) / sqn));
    }
    return truncate_int(static_cast<double>(static_cast<std::int64_t>(truncate_int(v)) * d / n));
  };
  if (p.offset != 0.0) {
    p.offset = scale(p.offset);
  }
  if (p.minimum > p.maximum) {
    return;
  }
  p.maximum = scale(p.maximum);
  p.minimum = scale(p.minimum);
}

void scale_device(DevicePattern& dev, std::int32_t n, std::int32_t d, bool magic_compat) {
  const float sqn = square(n);
  const float sqd = square(d);
  if (magic_compat) {
    dev.perimeter_cap *= dbl(sqn);
    dev.perimeter_cap /= dbl(sqd);
  } else {
    dev.perimeter_cap *= n;
    dev.perimeter_cap /= d;
  }
  dev.area_cap *= dbl(sqn);
  dev.area_cap /= dbl(sqd);
  for (std::size_t i = 0; i < visits(dev, magic_compat); ++i) {
    for (DeviceParam& p : dev.params) {
      scale_param(p, n, d, magic_compat);
    }
  }
}

void scale_caps(CapRules& caps, std::int32_t n, std::int32_t d) {
  const float sqn = square(n);
  const float sqd = square(d);
  const auto area = [&](double& v) {
    v *= dbl(sqn);
    v /= dbl(sqd);
  };
  const auto length = [&](double& v) {
    v *= n;
    v /= d;
  };
  CapTables& t = caps.tables;
  for (double& v : t.area) {
    area(v);
  }
  for (auto& [key, v] : t.perimeter) {
    length(v);
  }
  for (auto& [key, v] : t.overlap) {
    area(v);
  }
  for (auto& [key, v] : t.fringe_mult) {
    v *= static_cast<float>(n);
    v /= static_cast<float>(d);
  }
  for (auto& [key, rules] : t.side_couple) {
    for (EdgeRule& r : rules) {
      r.offset *= static_cast<float>(d);
      r.offset /= static_cast<float>(n);
    }
  }
  for (auto& [key, rules] : t.side_overlap) {
    for (EdgeRule& r : rules) {
      length(r.cap);
    }
  }
  for (TypeCap& c : caps.area) {
    area(c.cap);
  }
  for (TypeCap& c : caps.perimeter) {
    length(c.cap);
  }
  for (OverlapRule& r : caps.overlap) {
    area(r.cap);
  }
  for (SideOverlapRule& r : caps.sideoverlap) {
    length(r.cap);
  }
  for (SidewallRule& r : caps.sidewall) {
    r.offset *= static_cast<float>(d);
    r.offset /= static_cast<float>(n);
  }
}

// Step and halo: DBScaleValue with magic-compat ON; exact and rounded with
// it OFF (Q-EXTRACT-19).
void scale_lengths(ExtStyle& s, std::int32_t n, std::int32_t d, bool magic_compat) {
  if (magic_compat) {
    s.side_halo = scale_distance(s.side_halo, n, d);
    s.step_size = scale_distance(s.step_size, n, d);
    return;
  }
  if (s.step_exact == 0.0) {
    s.step_exact = s.step_size;
  }
  if (s.side_halo_exact == 0.0) {
    s.side_halo_exact = s.side_halo;
  }
  s.step_exact = s.step_exact * d / n;
  s.side_halo_exact = s.side_halo_exact * d / n;
  s.step_size = detail::saturate_round(s.step_exact);
  s.side_halo = detail::saturate_round(s.side_halo_exact);
}

}  // namespace

ExtStyle ExtStyle::micron_convert(const mask::OutputStyle* style) const {
  return convert(*this, style, nullptr);
}

ExtStyle ExtStyle::micron_convert(const mask::OutputStyle* style, diag::DiagnosticSink& sink) const {
  return convert(*this, style, &sink);
}

ExtStyle ExtStyle::rescale(std::int32_t n, std::int32_t d, const compat::CompatSettings& compat) const {
  ExtStyle out = *this;
  if (n <= 0 || d <= 0 || n == d) {
    return out;
  }
  const bool on = compat.magic_compat;
  const float sqn = square(n);
  const float sqd = square(d);
  out.units_per_lambda = out.units_per_lambda * static_cast<float>(n) / static_cast<float>(d);
  scale_lengths(out, n, d, on);
  for (DevicePattern& dev : out.devices) {
    scale_device(dev, n, d, on);
  }
  for (TypeHeightThickness& h : out.heights) {
    h.height *= static_cast<float>(d);
    h.height /= static_cast<float>(n);
    h.thickness *= static_cast<float>(d);
    h.thickness /= static_cast<float>(n);
  }
  for (AntennaRule& a : out.antenna) {
    if (a.ratio_diff.size() > 1) {
      auto v = static_cast<float>(a.ratio_diff[1]);
      v *= sqn;
      v /= sqd;
      a.ratio_diff[1] = dbl(v);
    }
  }
  scale_caps(out.caps, n, d);
  return out;
}

}  // namespace hefesto::extract
