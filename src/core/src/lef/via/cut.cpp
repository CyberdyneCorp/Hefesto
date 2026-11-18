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

#include <cstdint>
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
  const auto half = static_cast<db::Coord>(size >> 1);
  db::Rect grown;
  grown.x0 = (cut.x0 + cut.x1) / 2 - half;  // C division truncates toward zero
  grown.y0 = (cut.y0 + cut.y1) / 2 - half;
  grown.x1 = grown.x0 + static_cast<db::Coord>(size);
  grown.y1 = grown.y0 + static_cast<db::Coord>(size);
  return grown;
}

}  // namespace hefesto::lef
