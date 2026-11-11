// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask-hint operations (mask-hints; openspec/changes/m3-drc/design.md D10).
//
// Ported from Magic VLSI 8.3.694 database/DBio.c (dbReadProperties, the
// MASKHINTS_ value parser), drc/DRCbasic.c (drcTile, the exception inside
// test), database/DBcellcopy.c (dbCopyMaskHintPlaneFunc,
// dbCopyMaskHintsFunc, dbFlatCopyMaskHintsFunc, DBFlatCopyMaskHints),
// database/DBcellsrch.c (DBTreeSrCells, dbTreeCellSrFunc, dbCellSrFunc,
// dbMoveProp), cif/CIFgen.c (cifCopyPropPlaneFunc) and cif/CIFrdcl.c
// (CIFPaintCurrent), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: HINTS (openspec/changes/m3-drc/design.md D2).

#include "hefesto/hints/hints.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/property.hpp"

namespace hefesto::hints {

namespace {

constexpr tech::TileType kHintType = 1;

bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

// sscanf("%d") on a token: an optional sign and at least one digit; the
// value saturates at 64 bits and then wraps to 32 like the C conversion.
std::optional<std::int64_t> scan_int(std::string_view token) noexcept {
  std::size_t i = 0;
  const bool negative = !token.empty() && token[0] == '-';
  if (!token.empty() && (token[0] == '-' || token[0] == '+')) {
    i = 1;
  }
  if (i >= token.size() || !is_digit(token[i])) {
    return std::nullopt;
  }
  constexpr std::int64_t kLimit = std::int64_t{1} << 40;
  std::int64_t v = 0;
  for (; i < token.size() && is_digit(token[i]); ++i) {
    v = std::min(kLimit, v * 10 + (token[i] - '0'));
  }
  return negative ? -v : v;
}

// `ival *= scalen` (scalen > 1), then `ival /= scaled` (scaled > 1), in C int.
db::Coord scale_value(std::int64_t v, std::int64_t up, std::int64_t down) noexcept {
  auto i = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(v)));
  if (up > 1) {
    i = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(i * up)));
  }
  if (down > 1) {
    i = static_cast<std::int32_t>(i / down);
  }
  return i;
}

bool overlaps(const db::Rect& a, const db::Rect& b) noexcept {
  return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

// Magic's test: DBSrPaintArea over the zero-width segment finds a hint tile.
bool inside_compat(const db::Plane& plane, const Segment& s) {
  const db::Rect area = s.vertical ? db::Rect{s.at, s.lo, s.at, s.hi} : db::Rect{s.lo, s.at, s.hi, s.at};
  return !plane.enumerate(area, tech::TypeMask::of(kHintType), [](const db::TileView&) { return false; });
}

// The closed region holds a part of positive length of the segment.
bool inside_closed(const HintRegion& region, const Segment& s) {
  if (s.hi <= s.lo) {
    return false;
  }
  for (const db::Rect& r : region.strips()) {
    const db::Coord lo = s.vertical ? r.y0 : r.x0;
    const db::Coord hi = s.vertical ? r.y1 : r.x1;
    const db::Coord across0 = s.vertical ? r.x0 : r.y0;
    const db::Coord across1 = s.vertical ? r.x1 : r.y1;
    if (across0 <= s.at && s.at <= across1 && std::max(lo, s.lo) < std::min(hi, s.hi)) {
      return true;
    }
  }
  return false;
}

// One array element found by dbCellSrFunc: its transform to the root.
struct Element {
  const db::CellUse* use = nullptr;
  db::Transform to_root;
};

class FlatHintCopier {
 public:
  FlatHintCopier(const db::Rect& area, const db::Transform& to_destination, db::CellDef& destination,
                 db::Descend mode, bool magic_compat)
      : area_(area), to_destination_(to_destination), clip_(db::apply(to_destination, area)),
        destination_(destination), mode_(mode), magic_compat_(magic_compat) {}

  // DBCellSrArea over `def` (reached by `to_root`), dbTreeCellSrFunc on
  // every element found.
  void search(const db::CellDef& def, const db::Transform& to_root) {
    for (const db::CellUse* use : db::uses_in_search_order(def)) {
      if (use->def == nullptr) {
        continue;
      }
      for_each_overlapping_element(*use, to_root, [&](const Element& e) { visit(e); });
    }
  }

 private:
  template <typename F>
  void for_each_overlapping_element(const db::CellUse& use, const db::Transform& to_root, const F& f) const {
    const db::ArrayInfo& a = use.array;
    for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
      for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
        const db::Transform t = db::compose(to_root, db::element_transform(use, x, y));
        if (overlaps(db::apply(t, use.def->bbox()), area_)) {
          f(Element{&use, t});
        }
      }
    }
  }

  // Magic's dbTreeCellSrFunc copies a descended element only with
  // CU_DESCEND_ALL and always copies a kept one, so a restricting mode loses
  // the hints of the flattened levels. Hefesto copies every descended
  // element in both modes (Q-MISC-15, fix, X-2 tier 1) and the kept
  // element's own hints only under magic-compat (tier 2).
  void visit(const Element& e) {
    const bool descended = db::descends(*e.use, mode_);
    if (descended || magic_compat_) {
      copy_hints(*e.use->def, destination_, db::compose(to_destination_, e.to_root), clip_);
    }
    if (descended) {
      search(*e.use->def, e.to_root);
    }
  }

  db::Rect area_;
  db::Transform to_destination_;
  db::Rect clip_;
  db::CellDef& destination_;
  db::Descend mode_;
  bool magic_compat_;
};

}  // namespace

std::optional<std::string_view> hint_name(std::string_view key) noexcept {
  if (!key.starts_with(kPropertyPrefix)) {
    return std::nullopt;
  }
  return key.substr(kPropertyPrefix.size());
}

std::string property_key(std::string_view name) {
  std::string key{kPropertyPrefix};
  key += name;
  return key;
}

const HintRegion* find_hint(const db::CellDef& cell, std::string_view name) {
  const db::PropertyValue* value = cell.properties().find(property_key(name));
  return value != nullptr ? std::get_if<HintRegion>(value) : nullptr;
}

diag::Outcome<ParsedHint> parse_hint_value(std::string_view text, std::int64_t scale_up, std::int64_t scale_down) {
  std::vector<db::Rect> rects;
  db::Coord v[4] = {};
  std::size_t count = 0;
  std::size_t pos = 0;
  while (true) {
    while (pos < text.size() && is_blank(text[pos])) {
      ++pos;
    }
    if (pos >= text.size()) {
      break;
    }
    std::size_t end = pos;
    while (end < text.size() && !is_blank(text[end])) {
      ++end;
    }
    const auto value = scan_int(text.substr(pos, end - pos));
    if (!value) {
      return diag::make(db::codes::kPropertyValue,
                        "Mask-hint \"" + std::string{text.substr(pos)} + "\" has non-integer values!");
    }
    v[count++] = scale_value(*value, scale_up, scale_down);
    if (count == 4) {
      rects.push_back(db::Rect{v[0], v[1], v[2], v[3]});
      count = 0;
    }
    pos = end;
  }
  return ParsedHint{HintRegion{std::span<const db::Rect>{rects}}, count != 0};
}

std::string format_hint_value(const HintRegion& region) {
  std::string out;
  for (const db::Rect& r : region.strips()) {
    for (const db::Coord c : {r.x0, r.y0, r.x1, r.y1}) {
      if (!out.empty()) {
        out += ' ';
      }
      out += std::to_string(c);
    }
  }
  return out;
}

bool inside(const HintRegion& region, const Segment& segment, bool magic_compat) {
  if (region.plane() == nullptr) {
    return false;
  }
  return magic_compat ? inside_compat(*region.plane(), segment) : inside_closed(region, segment);
}

void copy_hints(const db::CellDef& source, db::CellDef& destination, const db::Transform& transform,
                const std::optional<db::Rect>& clip) {
  std::vector<std::pair<std::string, HintRegion>> copies;
  source.properties().for_each([&](std::string_view key, const db::PropertyValue& value) {
    const auto* region = std::get_if<HintRegion>(&value);
    if (region != nullptr && hint_name(key)) {
      HintRegion moved = region->transformed(transform);
      copies.emplace_back(std::string{key}, clip ? moved.clipped(*clip) : std::move(moved));
    }
  });
  for (auto& [key, region] : copies) {
    if (const auto* existing = std::get_if<HintRegion>(destination.properties().find(key))) {
      if (region.empty()) {
        continue;
      }
      region.paint(*existing);
    }
    db::set_property(destination, key, db::PropertyValue{std::move(region)});
  }
}

void flat_copy_hints(const db::CellDef& root, const db::Rect& area, const db::Transform& to_destination,
                     db::CellDef& destination, db::Descend mode, bool magic_compat) {
  FlatHintCopier copier{area, to_destination, destination, mode, magic_compat};
  copier.search(root, db::kIdentity);
}

void translate_hints(db::CellDef& cell, db::Coord dx, db::Coord dy) {
  std::vector<std::pair<std::string, HintRegion>> moved;
  cell.properties().for_each([&](std::string_view key, const db::PropertyValue& value) {
    if (const auto* region = std::get_if<HintRegion>(&value)) {
      moved.emplace_back(std::string{key}, region->translated(dx, dy));
    }
  });
  for (auto& [key, region] : moved) {
    db::set_property(cell, key, db::PropertyValue{std::move(region)});
  }
}

HintRegion output_region(const db::CellDef& cell, std::string_view name, std::int32_t scale) {
  const HintRegion* region = find_hint(cell, name);
  return region != nullptr ? region->scaled(scale, 1) : HintRegion{};
}

void add_input_region(db::CellDef& cell, std::string_view name, const HintRegion& region) {
  if (region.empty()) {
    return;
  }
  HintRegion merged = region;
  if (const HintRegion* existing = find_hint(cell, name)) {
    merged.paint(*existing);
  }
  db::set_property(cell, property_key(name), db::PropertyValue{std::move(merged)});
}

HintRegion gds_maskhints_region(const HintRegion& input, const HintRegion& generated, std::int32_t scale) {
  const HintRegion difference = input.minus(generated);
  std::vector<db::Rect> out;
  for (const db::Rect& r : difference.strips()) {
    out.push_back(db::Rect{r.x0 / scale, r.y0 / scale, r.x1 / scale, r.y1 / scale});
  }
  return HintRegion{std::span<const db::Rect>{out}};
}

}  // namespace hefesto::hints
