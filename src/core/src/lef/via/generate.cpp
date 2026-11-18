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
#include <cmath>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lef/via/cut.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

bool is_contact(const ViaContext& context, std::optional<tech::TileType> type) {
  return type && context.tech.is_contact(*type);
}

/// (int)roundf(v / s): binary32 division, rounded half away from zero.
db::Coord round_div(std::int64_t v, float s) {
  return static_cast<db::Coord>(std::roundf(static_cast<float>(v) / s));
}

/// The rectangle of an enclosure: (−round(W/u), −round(H/u), round(W/u),
/// round(H/u)) half units with W = cut array width + 2 enclosure.
db::Rect enclosure_rect(const GeneratedViaParams& p, db::Point enclosure) {
  const std::int64_t w = std::int64_t{p.cut_size.x} * p.columns + std::int64_t{p.cut_spacing.x} * (p.columns - 1) +
                         2 * std::int64_t{enclosure.x};
  const std::int64_t h = std::int64_t{p.cut_size.y} * p.rows + std::int64_t{p.cut_spacing.y} * (p.rows - 1) +
                         2 * std::int64_t{enclosure.y};
  const db::Coord x = round_div(w, p.scale);
  const db::Coord y = round_div(h, p.scale);
  return db::Rect{-x, -y, x, y};
}

db::Rect shifted(const db::Rect& r, db::Point by) {
  return db::Rect{r.x0 + by.x, r.y0 + by.y, r.x1 + by.x, r.y1 + by.y};
}

/// An ORIGIN or OFFSET in input units, as half units.
db::Point half_units(db::Point p, float scale) {
  const float half = scale / 2;
  return db::Point{round_div(p.x, half), round_div(p.y, half)};
}

/// The pattern of the cut array: every cut, or (magic-compat OFF) the
/// PATTERN's cuts.
std::vector<std::vector<bool>> present_cuts(const GeneratedViaParams& p, const ViaContext& context) {
  const auto rows = static_cast<std::size_t>(std::max(p.rows, 0));
  const auto columns = static_cast<std::size_t>(std::max(p.columns, 0));
  std::vector<std::vector<bool>> all(rows, std::vector<bool>(columns, true));
  if (context.compat.magic_compat || p.pattern.empty()) {
    return all;
  }
  if (auto parsed = cut_pattern(p.pattern, p.rows, p.columns)) {
    return *std::move(parsed);
  }
  context.sink.emit(diag::make("LEF-VIA-008", "Cannot parse via PATTERN \"" + p.pattern + "\"; every cut is kept."));
  return all;
}

/// The cuts, prepended to `out` in Magic's order (column-major, each grown).
void add_cuts(const GeneratedViaParams& p, const ViaContext& context, std::vector<ViaShape>& out) {
  if (!p.layers.cut) {
    return;
  }
  const tech::TileType cut = *p.layers.cut;
  const tech::TileType named = context.compat.magic_compat && p.layers.top ? *p.layers.top : cut;
  const auto present = present_cuts(p, context);
  const float half = p.scale / 2;
  const std::int64_t wc = std::int64_t{p.cut_size.x} * p.columns + std::int64_t{p.cut_spacing.x} * (p.columns - 1);
  const std::int64_t hc = std::int64_t{p.cut_size.y} * p.rows + std::int64_t{p.cut_spacing.y} * (p.rows - 1);
  const db::Coord sx = round_div(p.cut_size.x, half);
  const db::Coord sy = round_div(p.cut_size.y, half);
  std::int64_t x = -wc / 2;  // C division truncates toward zero
  for (std::int32_t i = 0; i < p.columns; ++i) {
    std::int64_t y = -hc / 2;
    for (std::int32_t j = 0; j < p.rows; ++j) {
      if (present[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)]) {
        const db::Coord x0 = round_div(x, half);
        const db::Coord y0 = round_div(y, half);
        const db::Rect grown = grow_cut(db::Rect{x0, y0, x0 + sx, y0 + sy}, cut, named, context);
        out.insert(out.begin(), ViaShape{cut, grown});
      }
      y += std::int64_t{p.cut_size.y} + p.cut_spacing.y;
    }
    x += std::int64_t{p.cut_size.x} + p.cut_spacing.x;
  }
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
  const bool enclosure = given.bottom_enclosure && given.top_enclosure;
  const std::optional<std::pair<db::Point, db::Point>> pair =
      enclosure ? std::optional{std::pair{*given.bottom_enclosure, *given.top_enclosure}} : std::nullopt;
  const std::pair<db::Point, db::Point> rule_pair =
      rule ? std::pair{rule->bottom_enclosure, rule->top_enclosure} : std::pair<db::Point, db::Point>{};
  std::tie(p.bottom_enclosure, p.top_enclosure) =
      completed(pair, rule ? &rule_pair : nullptr, via_name, "ENCLOSURE", context);
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
  g.area = enclosure_rect(params, params.top_enclosure);
  if (params.layers.bottom) {
    g.shapes.push_back(ViaShape{*params.layers.bottom, enclosure_rect(params, params.bottom_enclosure)});
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
