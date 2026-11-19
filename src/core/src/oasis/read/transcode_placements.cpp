// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PLACEMENT elements of the GDS-equivalent stream (capability oasis-read
// "Placement transforms", "Placement repetitions", "Instance properties").
//
// A placement is an SREF with STRANS (the flip bit), MAG and ANGLE as
// decoded; gds-read rounds the angle to a quarter turn with its error
// (Q-OASIS-12) and makes the magnified copies. A regular repetition whose
// vectors are axis-aligned in the child's coordinates (rules 1 and 2) is
// the AREF that gds-write writes for the same array: COLROW and the
// reference points P0, P0 + cols x the column vector, P0 + rows x the row
// vector. A dimension above COLROW's 32767 is written as 2 with the
// array-index property (attribute 99) giving the real index range, which
// gds-read applies after the array spacing. Any other repetition is one SREF
// per displacement (rule 3), each with the placement's properties, so
// coincident ones meet gds-read's duplicate-use rule (Q-OASIS-10).
// S_GDS_PROPERTY values become PROPATTR/PROPVALUE pairs in record order.
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <algorithm>
#include <array>
#include <limits>

#include "hefesto/gds/read/elements.hpp"
#include "transcoder.hpp"

namespace hefesto::oasis::read_detail {

namespace {

using gds::RecordType;

constexpr std::int16_t kArrayLimits = 99;  // CALMA_PROP_ARRAY_LIMITS
constexpr std::uint64_t kMaxColrow = 32767;

bool is_grid(std::uint8_t type) {
  return type == 1 || type == 2 || type == 3 || type == 8 || type == 9;
}

/// The attribute number of an S_GDS_PROPERTY as a GDS INT2: a number
/// outside INT2 becomes 32767, an attribute gds-read does not know, as the
/// original number is.
std::optional<std::int16_t> attribute(const PropertyValue& v) {
  if (v.kind == PropertyValue::Kind::unsigned_integer) {
    return int2_number(v.unsigned_value);
  }
  if (v.kind == PropertyValue::Kind::signed_integer) {
    const bool fits = v.signed_value >= std::numeric_limits<std::int16_t>::min() && v.signed_value <= kMaxGdsInt2;
    return static_cast<std::int16_t>(fits ? v.signed_value : kMaxGdsInt2);
  }
  return std::nullopt;
}

/// A point P0 + n * v, or nullopt outside 32 bits.
std::optional<std::array<std::int32_t, 2>> step(Point64 p0, std::uint64_t n, Point64 v) {
  std::int64_t dx = 0;
  std::int64_t dy = 0;
  if (__builtin_mul_overflow(static_cast<std::int64_t>(n), v.x, &dx) ||
      __builtin_mul_overflow(static_cast<std::int64_t>(n), v.y, &dy)) {
    return std::nullopt;
  }
  const auto x = coordinate(p0.x, dx);
  const auto y = coordinate(p0.y, dy);
  if (!x || !y) {
    return std::nullopt;
  }
  return std::array<std::int32_t, 2>{*x, *y};
}

/// The axes of an array use: the vector and count along the use's x and y.
struct Axes {
  Point64 x;
  std::uint64_t nx = 1;
  Point64 y;
  std::uint64_t ny = 1;
};

/// Rules 1 and 2 of "Placement repetitions": u = T^-1(a), v = T^-1(b).
std::optional<Axes> array_axes(const Element& e) {
  const Repetition& r = *e.repetition;
  const gds::ManhattanAngle angle = gds::manhattan_angle(e.angle);
  const db::Transform t = gds::strans_transform(e.flip ? 0x8000U : 0U, angle.degrees);
  // T is orthonormal: its inverse is its transpose.
  const auto inverse = [&t](Point64 p) { return Point64{t.a * p.x + t.d * p.y, t.b * p.x + t.e * p.y}; };
  const Point64 u = inverse(r.column_step);
  const Point64 v = inverse(r.row_step);
  const std::uint64_t na = r.columns;
  const std::uint64_t nb = r.rows;
  if ((na == 1 || u.y == 0) && (nb == 1 || v.x == 0)) {
    return Axes{r.column_step, na, r.row_step, nb};
  }
  if ((na == 1 || u.x == 0) && (nb == 1 || v.y == 0)) {
    return Axes{r.row_step, nb, r.column_step, na};
  }
  return std::nullopt;
}

}  // namespace

void Transcoder::placement_properties(const Property& p) {
  if (property_name(index_.names, p) != "S_GDS_PROPERTY" || p.values.size() != 2) {
    return;  // ignored, and processing goes on (rule 1)
  }
  const std::optional<std::int16_t> attr = attribute(p.values[0]);
  std::optional<std::string> value = string_value(index_.names, p.values[1]);
  if (!attr || !value) {
    return;
  }
  if (value->size() > kMaxString) {
    value->resize(kMaxString);
  }
  pending_->properties.emplace_back(*attr, std::move(*value));
}

std::string Transcoder::child_name(const Element& e) {
  std::string name = cell_name(index_.names, NameRef{e.name, e.reference});
  if (name.size() > kMaxString) {
    error("OASIS-READ-006", "cell name of " + std::to_string(name.size()) + " bytes cut to " +
                                std::to_string(kMaxString));
    name.resize(kMaxString);
  }
  return name;
}

void Transcoder::flush_placement() {
  if (!pending_) {
    return;
  }
  Placement p = std::move(*pending_);
  pending_.reset();
  const StreamPosition after = position_;
  position_ = p.position;
  p.child = child_name(p.element);
  out_.positions.emplace_back(gds_.size(), p.position);
  overflow_reported_ = false;
  if (!p.element.repetition) {
    if (room()) {
      reference(p, p.element.position);
    }
  } else if (!array(p)) {
    separate_uses(p);
  }
  position_ = after;
}

void Transcoder::reference_head(RecordType type, const Placement& p) {
  gds_.empty(type);
  gds_.string(RecordType::sname, p.child);
  const Element& e = p.element;
  if (!e.flip && e.magnification == 1.0 && e.angle == 0.0) {
    return;
  }
  gds_.int2(RecordType::strans, std::array<std::int16_t, 1>{static_cast<std::int16_t>(e.flip ? 0x8000 : 0)});
  if (e.magnification != 1.0) {
    gds_.real8(RecordType::mag, std::array<double, 1>{e.magnification});
  }
  if (e.angle != 0.0) {
    gds_.real8(RecordType::angle, std::array<double, 1>{e.angle});
  }
}

void Transcoder::reference_tail(const Placement& p, std::span<const std::int32_t> xy,
                                std::optional<std::string> limits) {
  gds_.int4(RecordType::xy, xy);
  if (limits) {
    gds_.int2(RecordType::propattr, std::array<std::int16_t, 1>{kArrayLimits});
    gds_.string(RecordType::propvalue, *limits);
  }
  for (const auto& [attr, value] : p.properties) {
    gds_.int2(RecordType::propattr, std::array<std::int16_t, 1>{attr});
    gds_.string(RecordType::propvalue, value);
  }
  gds_.empty(RecordType::endel);
}

void Transcoder::reference(const Placement& p, Point64 base) {
  const auto x = coordinate(base.x, 0);
  const auto y = coordinate(base.y, 0);
  if (!x || !y) {
    overflow();
    return;
  }
  reference_head(RecordType::sref, p);
  reference_tail(p, std::array<std::int32_t, 2>{*x, *y}, std::nullopt);
}

bool Transcoder::array(const Placement& p) {
  const Element& e = p.element;
  if (!is_grid(e.repetition->type)) {
    return false;
  }
  const std::optional<Axes> axes = array_axes(e);
  if (!axes) {
    return false;
  }
  if (!room()) {
    return true;
  }
  const std::uint64_t cx = axes->nx <= kMaxColrow ? axes->nx : 2;
  const std::uint64_t cy = axes->ny <= kMaxColrow ? axes->ny : 2;
  const auto p0 = step(e.position, 0, Point64{});
  const auto p1 = step(e.position, cx, axes->x);
  const auto p2 = step(e.position, cy, axes->y);
  constexpr auto kMaxIndex = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
  if (!p0 || !p1 || !p2 || axes->nx > kMaxIndex || axes->ny > kMaxIndex) {
    overflow();
    return true;
  }
  std::optional<std::string> limits;
  if (cx != axes->nx || cy != axes->ny) {
    limits = "0_" + std::to_string(axes->nx - 1) + "_0_" + std::to_string(axes->ny - 1);
  }
  reference_head(RecordType::aref, p);
  gds_.int2(RecordType::colrow,
            std::array<std::int16_t, 2>{static_cast<std::int16_t>(cx), static_cast<std::int16_t>(cy)});
  const std::array<std::int32_t, 6> xy{(*p0)[0], (*p0)[1], (*p1)[0], (*p1)[1], (*p2)[0], (*p2)[1]};
  reference_tail(p, xy, std::move(limits));
  return true;
}

void Transcoder::separate_uses(const Placement& p) {
  const Element& e = p.element;
  const std::uint64_t count = instance_count(*e.repetition);
  for (std::uint64_t i = 0; i < count; ++i) {
    if (!room()) {
      return;
    }
    if (const std::optional<Point64> base = instance_base(e, i)) {
      reference(p, *base);
    }
  }
}

}  // namespace hefesto::oasis::read_detail
