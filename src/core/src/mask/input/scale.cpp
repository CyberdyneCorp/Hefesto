// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The read engine's units ("Conversion of read units to internal units",
// "Mid-read subdivision of read units", "Grid limit"): CIFScaleCoord with
// the internal-grid subdivision it may make, the four-edge conversion of
// cifPaintCurrentFunc, CIFInputRescale with CIFScalePlanes (and
// calmaInputRescale's saved planes), and the read-unit subdivision of
// CIFParsePoint.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFrdutils.c
// (CIFScaleCoord, CIFParsePoint), cif/CIFrdcl.c (CIFScalePlanes,
// CIFInputRescale, cifPaintCurrentFunc) and calma/CalmaRdpt.c
// (calmaInputRescale), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Deviations (X-2 tier 1): results are computed in 64 bits and clamped to
// the usable plane instead of overflowing Magic's int.
//
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>

#include "hefesto/drc/rules/style.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/tech/loader.hpp"
#include "mask/input/engine_state.hpp"
#include "mask/input/reader.hpp"
#include "mask/style/text.hpp"

namespace hefesto::mask {

namespace {

/// The finest internal grid a subdivision may reach (DBLambda[1] is an int).
constexpr std::int64_t kMaxGridUnits = std::numeric_limits<std::int32_t>::max();

db::Coord clamp_coord(std::int64_t v) noexcept {
  return static_cast<db::Coord>(std::clamp<std::int64_t>(v, db::kPlaneMin, db::kPlaneMax));
}

/// C's truncating quotient of the half-up / half-down / any rounding.
std::int64_t snap_value(std::int64_t c, std::int64_t s, Snap snap) noexcept {
  switch (snap) {
    case Snap::half_up:
      return (c + (s >> 1)) / s;
    case Snap::half_down:
      return (c - (s >> 1)) / s;
    default:
      return c < 0 ? (c - (s >> 1)) / s : (c + ((s - 1) >> 1)) / s;
  }
}

}  // namespace

void ReadEngine::report_off_grid(std::int64_t remainder, std::int64_t denominator, bool snapped) {
  if (state_->snap_warned) {
    return;
  }
  state_->snap_warned = true;
  const std::string ratio = std::to_string(remainder) + "/" + std::to_string(denominator);
  state_->emit(snapped ? diag::make(input_detail::kSnappedToGrid, "Input off lambda grid by " + ratio + "; snapped to grid.")
                       : diag::make(input_detail::kGridRedefined, "Input off lambda grid by " + ratio + "; grid redefined."));
}

void ReadEngine::subdivide_grid(std::int64_t den) {
  // CIFScaleCoord: CIFTechInputScale(1, d, FALSE), CIFTechOutputScale(1, d),
  // DRCTechScale(1, d), DBScaleEverything(d, 1), DBLambda[1] *= d.
  (void)state_->style.technology_scale(1, den, false);
  const auto d32 = static_cast<std::int32_t>(den);
  const RescaleStep step = rescale_output_styles(editor_.mask.output_styles, 1, d32, compat_);
  for (const diag::Diagnostic& message : step.messages) {
    state_->emit(message);
  }
  drc::rescale_rules(editor_, 1, d32);
  (void)tech::scale_layout(editor_, den, 1);
  editor_.grid = tech::reduce(tech::GridScale{editor_.grid.internal * den, editor_.grid.lambda});
  state_->evaluation_valid = false;
}

db::Coord ReadEngine::scale_coord(std::int64_t c, Snap snap) {
  if (!state_->options.rescale_allowed) {
    snap = Snap::any;
  }
  std::int64_t s = std::max<std::int64_t>(state_->style.scale_factor, 1);
  if (c % s == 0) {
    return clamp_coord(c / s);
  }
  const std::int64_t g = style_text::gcf(std::abs(c), s);
  const std::int64_t remainder = std::abs(c % s) / g;
  std::int64_t den = s / g;
  if (state_->style.limit_scale(1, den) || editor_.grid.internal > kMaxGridUnits / den) {
    snap = Snap::any;  // the grid limit, or a grid Magic's int could not hold, refuses the subdivision
  }
  if (snap == Snap::exact) {
    report_off_grid(remainder, den, false);
    subdivide_grid(den);
    return clamp_coord(c / std::max<std::int64_t>(state_->style.scale_factor, 1));
  }
  if (snap == Snap::any) {
    report_off_grid(remainder, den, true);
    return clamp_coord(snap_value(c, s, snap));
  }
  if (den > 2) {
    report_off_grid(remainder, den, false);
    if ((den & 1) == 0) {
      den >>= 1;  // to the nearest half unit
    }
    subdivide_grid(den);
    s = std::max<std::int64_t>(state_->style.scale_factor, 1);
  }
  return clamp_coord(snap_value(c, s, snap));
}

db::Rect ReadEngine::scale_rect(const db::Rect& r, Snap snap, bool contact) {
  // xtop, ytop, xbot, ybot; a subdivision multiplies the edges done so far.
  const std::array<db::Coord, 4> in{r.x1, r.y1, r.x0, r.y0};
  std::array<std::int64_t, 4> out{};
  Snap mode = contact ? Snap::half_up : snap;
  for (std::size_t i = 0; i < in.size(); ++i) {
    const std::int64_t before = state_->style.scale_factor;
    out[i] = scale_coord(in[i], mode);
    const std::int64_t after = std::max<std::int64_t>(state_->style.scale_factor, 1);
    if (before != after) {
      for (std::size_t k = 0; k < i; ++k) {
        out[k] = std::clamp<std::int64_t>(out[k] * (before / after), db::kPlaneMin, db::kPlaneMax);
      }
    }
    if (contact && i == 1) {
      mode = Snap::half_down;
    }
  }
  return db::Rect{clamp_coord(out[2]), clamp_coord(out[3]), clamp_coord(out[0]), clamp_coord(out[1])};
}

void ReadEngine::rescale(std::int64_t n, std::int64_t d) {
  if (n <= 0 || d <= 0 || (n == 1 && d == 1)) {
    return;
  }
  state_->style.read_rescale(n, d);
  const auto scale_all = [&](std::vector<MaskPlane*>& planes) {
    for (MaskPlane* plane : planes) {
      if (plane != nullptr) {
        input_detail::scale_plane(*plane, n, d, compat_.magic_compat);
      }
    }
  };
  scale_all(state_->top);
  scale_all(state_->cell);
  for (auto& [cell, planes] : state_->saved) {
    scale_all(planes);
  }
  state_->evaluation_valid = false;
  state_->emit(diag::make(input_detail::kUnitsRescaled, "CIF style " + state_->style.name +
                                                            ": units rescaled by factor of " + std::to_string(n) +
                                                            " / " + std::to_string(d)));
}

namespace input_detail {

void paint_tile(MaskPlane& into, const db::TileView& tile, const db::Rect& area, bool magic_compat) {
  if (tile.split) {
    (void)into.paint_triangle(area, tile.diagonal, tile.right_side, magic_compat);
  } else {
    (void)into.paint(area);
  }
}

void scale_plane(MaskPlane& plane, std::int64_t n, std::int64_t d, bool magic_compat) {
  const std::vector<db::TileView> tiles = plane.solid_tiles();
  plane.clear();
  const auto scale = [n, d](db::Coord v) { return clamp_coord(tech::scale_value(v, n, d)); };
  for (const db::TileView& t : tiles) {
    const db::Rect r{scale(t.area.x0), scale(t.area.y0), scale(t.area.x1), scale(t.area.y1)};
    paint_tile(plane, t, r, magic_compat);
  }
}

}  // namespace input_detail

ReadUnitValue subdivide_read_unit(std::int64_t value, std::int64_t n, std::int64_t d, std::int64_t limit) noexcept {
  if (d <= 0 || value % d == 0) {
    return ReadUnitValue{d <= 0 ? value : value / d, 1, false};
  }
  const std::int64_t r = d / style_text::gcf(d, std::abs(value));
  const std::int64_t cap = (limit <= 0 || limit > 100) ? 100 : limit;
  if (n > cap / r || n * r > cap) {
    const std::int64_t biased = value < 0 ? value - ((d - 1) >> 1) : value + (d >> 1);
    return ReadUnitValue{biased / d, 1, true};
  }
  // value * r / d: r divides d / gcd, so (value / g) * (r * g / d) is exact; keep it in 64 bits.
  const std::int64_t g = style_text::gcf(d, std::abs(value));
  return ReadUnitValue{value / g, r, false};
}

}  // namespace hefesto::mask
