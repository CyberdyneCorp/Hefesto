// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The via geometry generator (LefGenViaGeometry, lef/lefRead.c:2545-2626;
// the LAYERS check of lef/defRead.c:2045-2083; capabilities lef-read
// "Generated via geometry", def-read "Generated DEF vias"; Q-LEFDEF-7, 61,
// 62). Magic is (c) the Regents of the University of California, BSD-style
// licence (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/lef/via/generate.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lef/read/units.hpp"
#include "hefesto/lef/via/cut.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

bool is_contact(const ViaContext& context, std::optional<tech::TileType> type) {
  return type && context.tech.is_contact(*type);
}

/// (int)roundf(v / s): binary32 division, rounded half away from zero;
/// nullopt where Magic's int cast is undefined (NaN or out of range).
std::optional<db::Coord> round_div(std::int64_t v, float s) {
  return to_internal(static_cast<float>(v), s);
}

/// a + b, or nullopt when it does not fit a coordinate (Magic's int
/// addition overflows there).
std::optional<db::Coord> add(db::Coord a, db::Coord b) {
  const std::int64_t sum = std::int64_t{a} + b;
  if (sum < std::numeric_limits<db::Coord>::min() || sum > std::numeric_limits<db::Coord>::max()) {
    return std::nullopt;
  }
  return static_cast<db::Coord>(sum);
}

/// The rectangle of an enclosure: (−round(W/u), −round(H/u), round(W/u),
/// round(H/u)) half units with W = cut array width + 2 enclosure; nullopt
/// when a side does not fit.
std::optional<db::Rect> enclosure_rect(const GeneratedViaParams& p, db::Point enclosure) {
  const std::int64_t w = std::int64_t{p.cut_size.x} * p.columns + std::int64_t{p.cut_spacing.x} * (p.columns - 1) +
                         2 * std::int64_t{enclosure.x};
  const std::int64_t h = std::int64_t{p.cut_size.y} * p.rows + std::int64_t{p.cut_spacing.y} * (p.rows - 1) +
                         2 * std::int64_t{enclosure.y};
  const auto x = round_div(w, p.scale);
  const auto y = round_div(h, p.scale);
  if (!x || !y || *x == std::numeric_limits<db::Coord>::min() || *y == std::numeric_limits<db::Coord>::min()) {
    return std::nullopt;
  }
  return db::Rect{-*x, -*y, *x, *y};
}

/// `r` moved by `by`, saturating at the coordinate range.
db::Rect shifted(const db::Rect& r, db::Point by) {
  const auto shift = [](db::Coord v, db::Coord d) {
    return static_cast<db::Coord>(std::clamp<std::int64_t>(std::int64_t{v} + d, std::numeric_limits<db::Coord>::min(),
                                                           std::numeric_limits<db::Coord>::max()));
  };
  return db::Rect{shift(r.x0, by.x), shift(r.y0, by.y), shift(r.x1, by.x), shift(r.y1, by.y)};
}

/// An ORIGIN or OFFSET in input units, as half units; a value that does
/// not fit is ignored (0), as the readers ignore such values.
db::Point half_units(db::Point p, float scale) {
  const float half = scale / 2;
  return db::Point{round_div(p.x, half).value_or(0), round_div(p.y, half).value_or(0)};
}

/// The cut at (x, y) input units with size (sx, sy) half units; nullopt
/// when a coordinate does not fit.
std::optional<db::Rect> cut_rect(std::int64_t x, std::int64_t y, db::Coord sx, db::Coord sy, float half) {
  const auto x0 = round_div(x, half);
  const auto y0 = round_div(y, half);
  if (!x0 || !y0) {
    return std::nullopt;
  }
  const auto x1 = add(*x0, sx);
  const auto y1 = add(*y0, sy);
  if (!x1 || !y1) {
    return std::nullopt;
  }
  return db::Rect{*x0, *y0, *x1, *y1};
}

/// The PATTERN of the cut array (magic-compat OFF); nullopt: every cut.
std::optional<CutPattern> pattern_of(const GeneratedViaParams& p, const ViaContext& context) {
  if (context.compat.magic_compat || p.pattern.empty()) {
    return std::nullopt;
  }
  auto parsed = cut_pattern(p.pattern, p.rows, p.columns);
  if (!parsed) {
    context.sink.emit(diag::make("LEF-VIA-008", "Cannot parse via PATTERN \"" + p.pattern + "\"; every cut is kept."));
  }
  return parsed;
}

/// The cuts, prepended to `out` in Magic's order (column-major, each
/// prepended, so the last cut is first; each grown). Collected in order
/// and reversed once: prepending each would cost the square of the count.
void add_cuts(const GeneratedViaParams& p, const ViaContext& context, std::vector<ViaShape>& out) {
  if (!p.layers.cut) {
    return;
  }
  const auto pattern = pattern_of(p, context);  // its message comes first, as before
  const float half = p.scale / 2;
  const auto sx = round_div(p.cut_size.x, half);
  const auto sy = round_div(p.cut_size.y, half);
  if (!sx || !sy) {
    return;  // a cut size that does not fit: no cuts
  }
  const tech::TileType cut = *p.layers.cut;
  const tech::TileType named = context.compat.magic_compat && p.layers.top ? *p.layers.top : cut;
  const std::int64_t step_x = std::int64_t{p.cut_size.x} + p.cut_spacing.x;
  const std::int64_t step_y = std::int64_t{p.cut_size.y} + p.cut_spacing.y;
  // C division truncates toward zero, as Magic's -wc / 2.
  const std::int64_t x0 = -(std::int64_t{p.cut_size.x} * p.columns + std::int64_t{p.cut_spacing.x} * (p.columns - 1)) / 2;
  const std::int64_t y0 = -(std::int64_t{p.cut_size.y} * p.rows + std::int64_t{p.cut_spacing.y} * (p.rows - 1)) / 2;
  std::vector<ViaShape> cuts;
  for (std::int32_t i = 0; i < p.columns; ++i) {
    for (std::int32_t j = 0; j < p.rows; ++j) {
      if (pattern && !pattern->at(j, i)) {
        continue;
      }
      if (const auto rect = cut_rect(x0 + i * step_x, y0 + j * step_y, *sx, *sy, half)) {
        cuts.push_back(ViaShape{cut, grow_cut(*rect, cut, named, context)});
      }
    }
  }
  out.insert(out.begin(), cuts.rbegin(), cuts.rend());
}

/// ORIGIN and OFFSET (magic-compat OFF, DEF 5.8): the top rectangle and the
/// bottom one move by their offsets, then everything by the origin.
void apply_offsets(ViaGeometry& g, bool has_bottom, const GeneratedViaParams& p) {
  g.area = shifted(g.area, half_units(p.top_offset, p.scale));
  if (has_bottom) {
    ViaShape& bottom = g.shapes.back();  // the list tail: generated first
    bottom.area = shifted(bottom.area, half_units(p.bottom_offset, p.scale));
  }
  const db::Point origin = half_units(p.origin, p.scale);
  g.area = shifted(g.area, origin);
  for (ViaShape& s : g.shapes) {
    s.area = shifted(s.area, origin);
  }
}

void warn_omitted(std::string_view via, std::string_view parameter, bool from_rule, const ViaContext& context) {
  std::string text = "Via \"" + std::string{via} + "\": " + std::string{parameter} + " not given; using " +
                     (from_rule ? "the VIARULE value." : "0.");
  context.sink.emit(diag::make_native("LEF-VIA-007", std::move(text)));
}

template <typename T>
T completed(const std::optional<T>& given, const T* rule_value, std::string_view via, std::string_view parameter,
            const ViaContext& context) {
  if (given) {
    return *given;
  }
  warn_omitted(via, parameter, rule_value != nullptr, context);
  return rule_value != nullptr ? *rule_value : T{};
}

}  // namespace

void order_layers(ViaLayers& layers, const ViaContext& context) {
  if (is_contact(context, layers.cut)) {
    return;
  }
  context.sink.emit(diag::make("LEF-VIA-005", "Improper layer order for VIARULE."));
  if (is_contact(context, layers.top)) {
    std::swap(layers.cut, layers.top);
  } else if (is_contact(context, layers.bottom)) {
    std::swap(layers.cut, layers.bottom);
  } else {
    context.sink.emit(diag::make("LEF-VIA-006", "No cut layer specified in VIARULE."));
  }
}

GeneratedViaParams complete_params(const ViaParamSet& given, const GeneratedViaParams* rule, std::string_view via_name,
                                   const ViaContext& context) {
  GeneratedViaParams p;
  p.layers = completed(given.layers, rule ? &rule->layers : nullptr, via_name, "LAYERS", context);
  p.cut_size = completed(given.cut_size, rule ? &rule->cut_size : nullptr, via_name, "CUTSIZE", context);
  p.cut_spacing = completed(given.cut_spacing, rule ? &rule->cut_spacing : nullptr, via_name, "CUTSPACING", context);
  // ENCLOSURE sets both enclosures or neither. No std::optional<std::pair>
  // here: GCC's -Wmaybe-uninitialized misreads one at -O2.
  if (given.bottom_enclosure && given.top_enclosure) {
    p.bottom_enclosure = *given.bottom_enclosure;
    p.top_enclosure = *given.top_enclosure;
  } else {
    warn_omitted(via_name, "ENCLOSURE", rule != nullptr, context);
    p.bottom_enclosure = rule != nullptr ? rule->bottom_enclosure : db::Point{};
    p.top_enclosure = rule != nullptr ? rule->top_enclosure : db::Point{};
  }
  p.rows = given.rows;
  p.columns = given.columns;
  p.scale = given.scale;
  p.origin = given.origin;
  p.bottom_offset = given.bottom_offset;
  p.top_offset = given.top_offset;
  p.pattern = given.pattern;
  return p;
}

ViaGeometry ViaGenerator::generated(const GeneratedViaParams& params, const ViaContext& context) {
  ViaGeometry g;
  g.type = params.layers.top;
  if (!(params.scale > 0.0F)) {
    return g;  // no unit: nothing to place (DEF read refuses such UNITS)
  }
  const auto top = enclosure_rect(params, params.top_enclosure);
  const auto bottom = enclosure_rect(params, params.bottom_enclosure);
  if (!top || (params.layers.bottom && !bottom)) {
    return g;  // a size that does not fit (Magic's int cast is undefined): no geometry
  }
  g.area = *top;
  if (params.layers.bottom) {
    g.shapes.push_back(ViaShape{*params.layers.bottom, *bottom});
  }
  add_cuts(params, context, g.shapes);
  if (!context.compat.magic_compat) {
    apply_offsets(g, params.layers.bottom.has_value(), params);
  }
  return g;
}

void apply_generated(ViaDefinition& via, const ViaGeometry& geometry) {
  via.area = geometry.area;
  via.type = geometry.type;
  via.shapes.insert(via.shapes.begin(), geometry.shapes.begin(), geometry.shapes.end());
  via.generated = true;
}

}  // namespace hefesto::lef
