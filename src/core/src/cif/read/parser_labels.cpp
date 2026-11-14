// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF parser's label user extensions 94 and 95 (cifParseUser94,
// cifParseUser95, cif/CIFrdcl.c:1432-1680). The label corners stay in read
// units (the engine converts them with "any" conversion); the parser
// resolves the label type and stickiness (Q-MASK-327) and the 95 rescale
// (Q-MASK-314 fix). Owner: CIF.

#include <algorithm>

#include "hefesto/db/geometry.hpp"
#include "parser_impl.hpp"

namespace hefesto::cif::detail {

namespace {

/// The 95 rescale has no `cif limit` test in Magic; this cap only keeps N
/// and M from overflowing on a file of odd area labels.
constexpr std::int64_t kMaxUnitScale = std::int64_t{1} << 30;

bool in_plane(const ReadPoint& p) noexcept {
  return p.x >= db::kPlaneMin && p.x <= db::kPlaneMax && p.y >= db::kPlaneMin && p.y <= db::kPlaneMax;
}

}  // namespace

void Parser::user94() {
  CifLabel label;
  label.label.text = lex_.name();
  const auto location = point(1);
  if (!location) {
    error("94 command, but no location; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  label_layer(label, true);
  put_label(std::move(label), *location, *location);
}

std::optional<std::pair<ReadPoint, ReadPoint>> Parser::area_label_corners() {
  auto size = point(1);
  if (!size) {
    error("95 command, but no size; ignored.");
    return std::nullopt;
  }
  const std::int64_t saved = scale_n_;
  const auto center = point(2);  // doubled; halved below when exact
  if (!center) {
    error("95 command, but no location; ignored.");
    return std::nullopt;
  }
  if (saved != scale_n_) {
    *size = ReadPoint{mul(size->x, scale_n_ / saved), mul(size->y, scale_n_ / saved)};
  }
  ReadPoint ll{center->x - size->x, center->y - size->y};
  ReadPoint ur{center->x + size->x, center->y + size->y};
  const bool exact = ll.x % 2 == 0 && ll.y % 2 == 0;
  if (!exact && !range_error_ && mul(scale_n_, 2) <= kMaxUnitScale && mul(multiplier_, 2) <= kMaxUnitScale) {
    // Q-MASK-314 (fix, both modes): subdivide the read units by 2 and double
    // the symbol numerator, so later geometry of the symbol keeps its size.
    static_cast<void>(rescale(2));
    return std::pair{ll, ur};
  }
  if (!exact) {
    warning("CIF units at maximum scale; value is rounded");
  }
  return std::pair{ReadPoint{ll.x / 2, ll.y / 2}, ReadPoint{ur.x / 2, ur.y / 2}};
}

void Parser::user95() {
  CifLabel label;
  label.label.text = lex_.name();
  const auto corners = area_label_corners();
  if (!corners) {
    lex_.skip_to_semicolon();
    return;
  }
  label_layer(label, false);
  put_label(std::move(label), corners->first, corners->second);
}

void Parser::label_layer(CifLabel& label, bool point_label) {
  lex_.skip_blanks();
  if (lex_.peek() != ';') {
    const std::string name = lex_.name();
    const auto index = style_.find(name);
    if (!index) {
      error("label attached to unknown layer " + name + ".");
      label.type = 0;  // space, not sticky
      return;
    }
    label.label.cif_layer = index;
    label.type = style_.layers[*index].label_type;
    label.sticky = style_.layers[*index].sticky;
    return;
  }
  // No layer: 94 takes the current default label type, 95 space.
  label.type = point_label ? label_type_ : 0;
  label.label.cif_layer = layer_;
  label.sticky = default_label_sticky(label.type);
}

bool Parser::default_label_sticky(std::int32_t type) const {
  if (!options_.magic_compat) {
    // Q-MASK-327 OFF: the kind of the layer selected by the current L.
    return layer_ && style_.layers[*layer_].sticky;
  }
  // Q-MASK-327 ON, as Magic: the first CIF layer whose index is below the
  // number of layer rules and whose label type is the default type. An
  // index past the name table has type space and is never sticky.
  const std::size_t n = std::min(style_.rule_count, style_.layers.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (style_.layers[i].label_type == type) {
      return style_.layers[i].sticky;
    }
  }
  return false;
}

void Parser::put_label(CifLabel label, const ReadPoint& ll, const ReadPoint& ur) {
  if (label.type < 0 || (in_definition_ && !current_)) {
    return;
  }
  if (range_error_ || !in_plane(ll) || !in_plane(ur)) {
    outside_plane("label");
    return;
  }
  label.label.area = db::Rect{static_cast<db::Coord>(ll.x), static_cast<db::Coord>(ll.y),
                              static_cast<db::Coord>(ur.x), static_cast<db::Coord>(ur.y)};
  check(target_.cif_label(label));
}

}  // namespace hefesto::cif::detail
