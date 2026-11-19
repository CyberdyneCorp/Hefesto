// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS point lists and repetitions (capability oasis-format "Point lists",
// "Repetitions"; design D11, security review M6-S3):
// - point lists of types 0-5 decode to their vertices; a count of 0 drops
//   the element, a type above 5 stops (its length is unknown);
// - repetitions keep their parameters and expand lazily (instance_count,
//   offset_at); the decoder checks that the farthest offset fits in 64 bits
//   and that every step fits in 32 bits, else the element is dropped;
// - the encoders write a given type, or report that it cannot represent
//   the values.
//
// Owner: OASFMT (design D2).

#include <cstdlib>
#include <limits>

#include "hefesto/oasis/codec/elements.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

namespace detail {

namespace {

__extension__ typedef __int128 Wide;  // NOLINT(modernize-use-using): __extension__ needs the typedef form

constexpr Wide kInt64Max = std::numeric_limits<std::int64_t>::max();

bool fits64(Wide value) noexcept {
  return value <= kInt64Max && value >= -kInt64Max - 1;
}

DeltaForm list_form(std::uint8_t type, std::uint64_t index) {
  switch (type) {
    case 0:
    case 1:
      return horizontal_delta(index, type == 1) ? DeltaForm::one_horizontal : DeltaForm::one_vertical;
    case 2:
      return DeltaForm::two;
    case 3:
      return DeltaForm::three;
    default:
      return DeltaForm::g;
  }
}

/// The repetition dimension n' + 2 (OASIS-CODEC-005 past 2^64 - 1).
std::uint64_t dimension(Input& in) {
  const std::uint64_t stored = read_unsigned(in);
  if (stored > std::numeric_limits<std::uint64_t>::max() - 2) {
    fail("OASIS-CODEC-005", "integer overflow in a repetition dimension");
  }
  return stored + 2;
}

std::int64_t spacing(Input& in, Drop& drop) {
  const std::int64_t value = to_length(read_unsigned(in), drop);
  check_delta({value, 0}, drop);
  return value;
}

std::uint64_t grid(Input& in) {
  const std::uint64_t value = read_unsigned(in);
  if (value == 0) {
    fail("OASIS-CODEC-010", "zero repetition grid");
  }
  return value;
}

/// The farthest grid offset must fit in 64 bits.
void check_grid(const Repetition& r, Drop& drop) {
  const Wide cx = static_cast<Wide>(r.columns - 1) * r.column_step.x;
  const Wide cy = static_cast<Wide>(r.columns - 1) * r.column_step.y;
  const Wide rx = static_cast<Wide>(r.rows - 1) * r.row_step.x;
  const Wide ry = static_cast<Wide>(r.rows - 1) * r.row_step.y;
  const auto magnitude = [](Wide v) { return v < 0 ? -v : v; };
  if (!fits64(magnitude(cx) + magnitude(rx)) || !fits64(magnitude(cy) + magnitude(ry))) {
    drop.set("OASIS-CODEC-008", "coordinate overflow in a repetition");
  }
}

Repetition grid_repetition(Input& in, std::uint8_t type, Drop& drop) {
  Repetition r;
  r.type = type;
  switch (type) {
    case 1:
      r.columns = dimension(in);
      r.rows = dimension(in);
      r.column_step = {spacing(in, drop), 0};
      r.row_step = {0, spacing(in, drop)};
      break;
    case 2:
      r.columns = dimension(in);
      r.column_step = {spacing(in, drop), 0};
      break;
    case 3:
      r.rows = dimension(in);
      r.row_step = {0, spacing(in, drop)};
      break;
    case 8:
      r.columns = dimension(in);
      r.rows = dimension(in);
      r.column_step = read_delta(in, DeltaForm::g);
      r.row_step = read_delta(in, DeltaForm::g);
      break;
    default:  // 9
      r.columns = dimension(in);
      r.column_step = read_delta(in, DeltaForm::g);
      break;
  }
  check_delta(r.column_step, drop);
  check_delta(r.row_step, drop);
  check_grid(r, drop);
  return r;
}

/// One step of a list repetition, multiplied by the grid.
Point64 list_step(Input& in, std::uint8_t type, std::uint64_t factor, Drop& drop) {
  Point64 step;
  if (type >= 10) {
    step = read_delta(in, DeltaForm::g);
  } else {
    const std::int64_t s = to_length(read_unsigned(in), drop);
    step = type <= 5 ? Point64{s, 0} : Point64{0, s};
  }
  const Wide x = static_cast<Wide>(step.x) * factor;
  const Wide y = static_cast<Wide>(step.y) * factor;
  if (!fits64(x) || !fits64(y)) {
    drop.set("OASIS-CODEC-008", "coordinate overflow in a repetition");
    return {};
  }
  step = {static_cast<std::int64_t>(x), static_cast<std::int64_t>(y)};
  check_delta(step, drop);
  return step;
}

Repetition list_repetition(Input& in, std::uint8_t type, Drop& drop) {
  Repetition r;
  r.type = type;
  const std::uint64_t stored = read_unsigned(in);  // n' (n = n' + 2, so n' + 1 steps)
  const bool gridded = type == 5 || type == 7 || type == 11;
  r.grid = gridded ? grid(in) : 0;
  Point64 position;
  for (std::uint64_t i = 0;; ++i) {  // i = 0 .. n', without computing n' + 1
    position = add(position, list_step(in, type, gridded ? r.grid : 1, drop), drop);
    r.offsets.push_back(position);
    if (i == stored) {
      break;
    }
  }
  return r;
}

bool is_grid_type(std::uint8_t type) noexcept {
  return type <= 3 || type == 8 || type == 9;
}

Point64 axis_step(Point64 previous, Point64 current) {
  return {current.x - previous.x, current.y - previous.y};
}

bool octangular(Point64 d) noexcept {
  return d.x == 0 || d.y == 0 || std::llabs(d.x) == std::llabs(d.y);
}

/// Whether the edges p[0]->p[1] ... alternate horizontal and vertical,
/// starting horizontal unless `vertical_first`, with nonzero length when
/// `nonzero`.
bool alternating(std::span<const Point64> p, bool vertical_first, bool nonzero) noexcept {
  for (std::size_t i = 1; i < p.size(); ++i) {
    const Point64 d = axis_step(p[i - 1], p[i]);
    const bool horizontal = horizontal_delta(i - 1, vertical_first);
    const std::int64_t along = horizontal ? d.x : d.y;
    const std::int64_t across = horizontal ? d.y : d.x;
    if (across != 0 || (nonzero && along == 0)) {
      return false;
    }
  }
  return true;
}

/// The vertex chain P0 = (0, 0), points..., closed back to P0 for a polygon.
std::vector<Point64> chain(std::span<const Point64> points, bool closed) {
  std::vector<Point64> p{Point64{}};
  p.insert(p.end(), points.begin(), points.end());
  if (closed) {
    p.push_back(Point64{});
  }
  return p;
}

bool every_step(std::span<const Point64> p, bool (*test)(Point64) noexcept) noexcept {
  for (std::size_t i = 1; i < p.size(); ++i) {
    if (!test(axis_step(p[i - 1], p[i]))) {
      return false;
    }
  }
  return true;
}

bool manhattan(Point64 d) noexcept {
  return d.x == 0 || d.y == 0;
}

/// Writes the 1-deltas of a type 0/1 list; the polygon's synthesized vertex
/// (the last point) must match the rule.
bool write_alternating(std::vector<std::byte>& out, std::uint8_t type, std::span<const Point64> points, bool polygon) {
  const std::span<const Point64> listed = polygon ? points.first(points.size() - 1) : points;
  const std::vector<Point64> p = chain(listed, false);
  if (listed.empty() || !alternating(p, type == 1, false)) {
    return false;
  }
  const bool last_horizontal = horizontal_delta(listed.size() - 1, type == 1);
  if (polygon && synthesized_vertex(listed.back(), last_horizontal) != points.back()) {
    return false;
  }
  write_unsigned(out, type);
  write_unsigned(out, listed.size());
  for (std::size_t i = 1; i < p.size(); ++i) {
    write_delta(out, axis_step(p[i - 1], p[i]), list_form(type, i - 1));
  }
  return true;
}

bool write_steps(std::vector<std::byte>& out, std::uint8_t type, std::span<const Point64> points) {
  const std::vector<Point64> p = chain(points, false);
  if ((type == 2 && !every_step(p, manhattan)) || (type == 3 && !every_step(p, octangular))) {
    return false;
  }
  write_unsigned(out, type);
  write_unsigned(out, points.size());
  Point64 previous_step;
  for (std::size_t i = 1; i < p.size(); ++i) {
    const Point64 step = axis_step(p[i - 1], p[i]);
    const Point64 written = type == 5 ? axis_step(previous_step, step) : step;
    write_delta(out, written, list_form(type, i - 1));
    previous_step = step;
  }
  return true;
}

bool grid_representable(const Repetition& r) noexcept {
  const Point64 c = r.column_step;
  const Point64 w = r.row_step;
  const bool x_axis = c.y == 0 && c.x >= 0;
  const bool y_axis = w.x == 0 && w.y >= 0;
  switch (r.type) {
    case 1:
      return r.columns >= 2 && r.rows >= 2 && x_axis && y_axis;
    case 2:
      return r.columns >= 2 && r.rows == 1 && x_axis && w == Point64{};
    case 3:
      return r.rows >= 2 && r.columns == 1 && y_axis && c == Point64{};
    case 8:
      return r.columns >= 2 && r.rows >= 2;
    default:  // 9
      return r.columns >= 2 && r.rows == 1 && w == Point64{};
  }
}

bool write_grid(std::vector<std::byte>& out, const Repetition& r) {
  if (!grid_representable(r)) {
    return false;
  }
  write_unsigned(out, r.type);
  write_unsigned(out, (r.type == 3 ? r.rows : r.columns) - 2);
  if (r.type == 1 || r.type == 8) {
    write_unsigned(out, r.rows - 2);
  }
  switch (r.type) {
    case 1:
      write_unsigned(out, static_cast<std::uint64_t>(r.column_step.x));
      write_unsigned(out, static_cast<std::uint64_t>(r.row_step.y));
      break;
    case 2:
      write_unsigned(out, static_cast<std::uint64_t>(r.column_step.x));
      break;
    case 3:
      write_unsigned(out, static_cast<std::uint64_t>(r.row_step.y));
      break;
    case 8:
      write_delta(out, r.column_step, DeltaForm::g);
      write_delta(out, r.row_step, DeltaForm::g);
      break;
    default:  // 9
      write_delta(out, r.column_step, DeltaForm::g);
      break;
  }
  return true;
}

/// One step of a list repetition in the units the type writes, or nullopt.
std::optional<Point64> list_unit(std::uint8_t type, Point64 step, std::uint64_t grid) {
  const bool along_x = type == 4 || type == 5;
  const bool along_y = type == 6 || type == 7;
  if ((along_x && (step.y != 0 || step.x < 0)) || (along_y && (step.x != 0 || step.y < 0))) {
    return std::nullopt;
  }
  if (grid == 0) {
    return step;
  }
  const auto g = static_cast<std::int64_t>(grid);
  if (step.x % g != 0 || step.y % g != 0) {
    return std::nullopt;
  }
  return Point64{step.x / g, step.y / g};
}

bool write_list(std::vector<std::byte>& out, const Repetition& r) {
  const bool gridded = r.type == 5 || r.type == 7 || r.type == 11;
  if (r.offsets.empty() || (gridded != (r.grid != 0)) || r.grid > std::numeric_limits<std::int64_t>::max()) {
    return false;
  }
  std::vector<std::byte> body;
  Point64 previous;
  for (const Point64 offset : r.offsets) {
    const auto unit = list_unit(r.type, axis_step(previous, offset), r.grid);
    if (!unit) {
      return false;
    }
    if (r.type >= 10) {
      write_delta(body, *unit, DeltaForm::g);
    } else {
      write_unsigned(body, static_cast<std::uint64_t>(r.type <= 5 ? unit->x : unit->y));
    }
    previous = offset;
  }
  write_unsigned(out, r.type);
  write_unsigned(out, r.offsets.size() - 1);
  if (gridded) {
    write_unsigned(out, r.grid);
  }
  out.insert(out.end(), body.begin(), body.end());
  return true;
}

}  // namespace

Point64 synthesized_vertex(Point64 last, bool horizontal) noexcept {
  return horizontal ? Point64{last.x, 0} : Point64{0, last.y};
}

PointList read_point_list(Input& in, Drop& drop) {
  const std::uint32_t type = read_u32(in, "point-list type");
  if (type > 5) {
    fail("OASIS-CODEC-009", "invalid point list type " + std::to_string(type));
  }
  PointList list;
  list.type = static_cast<std::uint8_t>(type);
  const std::uint64_t count = read_unsigned(in);
  if (count == 0) {
    drop.set("OASIS-CODEC-009", "invalid point list (count 0)");
  }
  Point64 point;
  Point64 difference;
  for (std::uint64_t i = 0; i < count; ++i) {
    const Point64 delta = read_delta(in, list_form(list.type, i));
    check_delta(delta, drop);
    if (type == 5) {
      difference = add(difference, delta, drop);
      point = add(point, difference, drop);
    } else {
      point = add(point, delta, drop);
    }
    list.vertices.push_back(point);
  }
  return list;
}

Repetition read_repetition(Input& in, const std::optional<Repetition>& modal, Drop& drop) {
  const std::uint32_t type = read_u32(in, "repetition type");
  if (type == 0) {
    if (!modal) {
      fail("OASIS-CODEC-020", "modal variable repetition undefined");
    }
    return *modal;
  }
  if (type > 11) {
    fail("OASIS-CODEC-010", "invalid repetition type " + std::to_string(type));
  }
  const auto t = static_cast<std::uint8_t>(type);
  return is_grid_type(t) ? grid_repetition(in, t, drop) : list_repetition(in, t, drop);
}

bool write_repetition(std::vector<std::byte>& out, const Repetition& repetition) {
  if (repetition.type == 0 || repetition.type > 11) {
    return false;
  }
  return is_grid_type(repetition.type) ? write_grid(out, repetition) : write_list(out, repetition);
}

bool write_point_list(std::vector<std::byte>& out, std::uint8_t type, std::span<const Point64> points, bool polygon) {
  if (points.empty() || type > 5) {
    return false;
  }
  return type <= 1 ? write_alternating(out, type, points, polygon) : write_steps(out, type, points);
}

std::uint8_t narrowest_point_list(std::span<const Point64> points, bool polygon) noexcept {
  const std::vector<Point64> p = chain(points, polygon);
  const std::size_t edges = p.size() - 1;
  if (polygon ? edges >= 4 && edges % 2 == 0 : edges >= 1) {
    for (const std::uint8_t type : {std::uint8_t{0}, std::uint8_t{1}}) {
      if (alternating(p, type == 1, true)) {
        return type;
      }
    }
  }
  if (every_step(p, manhattan)) {
    return 2;
  }
  return every_step(p, octangular) ? 3 : 4;
}

}  // namespace detail

std::uint64_t instance_count(const Repetition& repetition) noexcept {
  if (repetition.type == 0) {
    return 1;
  }
  if (!detail::is_grid_type(repetition.type)) {
    return repetition.offsets.size() + 1;
  }
  std::uint64_t count = 0;
  if (__builtin_mul_overflow(repetition.columns, repetition.rows, &count)) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return count;
}

Point64 offset_at(const Repetition& repetition, std::uint64_t index) noexcept {
  if (index == 0 || repetition.type == 0) {
    return {};
  }
  if (!detail::is_grid_type(repetition.type)) {
    return index <= repetition.offsets.size() ? repetition.offsets[index - 1] : Point64{};
  }
  const std::uint64_t columns = repetition.columns == 0 ? 1 : repetition.columns;
  const auto i = static_cast<detail::Wide>(index % columns);
  const auto j = static_cast<detail::Wide>(index / columns);
  const detail::Wide x = i * repetition.column_step.x + j * repetition.row_step.x;
  const detail::Wide y = i * repetition.column_step.y + j * repetition.row_step.y;
  return {static_cast<std::int64_t>(x), static_cast<std::int64_t>(y)};
}

std::vector<Point64> expand(const Repetition& repetition) {
  const std::uint64_t count = instance_count(repetition);
  std::vector<Point64> offsets;
  for (std::uint64_t index = 0; index < count; ++index) {
    offsets.push_back(offset_at(repetition, index));
  }
  return offsets;
}

}  // namespace hefesto::oasis
