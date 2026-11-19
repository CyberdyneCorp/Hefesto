// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cut growing (LefGrowVia, lef/lefRead.c:2473-2523; CIFGetContactSize,
// cif/CIFtech.c:2327-2382; capability lef-read "Cut growing", Q-LEFDEF-17).
// Magic is (c) the Regents of the University of California, BSD-style
// licence (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/lef/via/cut.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

/// ceil(v / f) for v >= 0 and f > 0, as LefGrowVia computes it.
std::int64_t ceil_div(std::int64_t v, std::int64_t f) {
  return v % f == 0 ? v / f : v / f + 1;
}

/// v clamped to the coordinate range: Magic's int arithmetic overflows
/// there, which is undefined; Hefesto saturates in both modes.
db::Coord saturated(std::int64_t v) {
  return static_cast<db::Coord>(
      std::clamp<std::int64_t>(v, std::numeric_limits<db::Coord>::min(), std::numeric_limits<db::Coord>::max()));
}

void warn_mismatch(const db::Rect& cut, std::int64_t edge, tech::TileType named_type, const ViaContext& context) {
  const std::string name{context.tech.type_name(named_type)};
  const std::string e = std::to_string(edge);
  context.sink.emit(diag::make("LEF-VIA-003", "Cut size for magic type \"" + name + "\" (" + e + " x " + e +
                                                  ") does not match LEF/DEF"));
  context.sink.emit(diag::make("LEF-VIA-004", "Via cut size (" + std::to_string(cut.width()) + " x " +
                                                  std::to_string(cut.height()) +
                                                  ").  Magic layer cut size will be used!"));
}

}  // namespace

db::Rect grow_cut(const db::Rect& cut, tech::TileType type, tech::TileType named_type, const ViaContext& context) {
  if (context.style == nullptr || !context.tech.is_contact(type)) {
    return cut;
  }
  const auto found = context.style->contact_cut(type);
  const std::int64_t factor = context.style->scale.scale_factor;
  if (!found || factor <= 0) {
    return cut;  // CIFGetContactSize answers 0: nothing is grown
  }
  const std::int64_t size = ceil_div(2 * found->size(), factor);
  const std::int64_t edge = ceil_div(2 * std::int64_t{found->edge}, factor);
  if (size <= 0 || edge <= 0) {
    return cut;
  }
  if (cut.width() != edge || cut.height() != edge) {
    warn_mismatch(cut, edge, named_type, context);
  }
  const std::int64_t half = size >> 1;
  // 64-bit sums: a cut can reach the int range, where Magic's int sums
  // overflow. C division truncates toward zero.
  const std::int64_t x0 = (std::int64_t{cut.x0} + cut.x1) / 2 - half;
  const std::int64_t y0 = (std::int64_t{cut.y0} + cut.y1) / 2 - half;
  return db::Rect{saturated(x0), saturated(y0), saturated(x0 + size), saturated(y0 + size)};
}

}  // namespace hefesto::lef
