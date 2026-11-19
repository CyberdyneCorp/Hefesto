// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS encoder: element records with modal field omission (capability
// oasis-format element records; oasis-write "Modal field omission",
// "Placements", "Text translation", "Read-only OASIS vendor cells"; design
// D11). A field is omitted exactly when its modal variable is defined and
// equal to the value; coordinates are always absolute; the modal state is
// updated as the decoder will update it, so decoding the output gives the
// same elements back. The RECTANGLE square form and TRAPEZOID records 24
// and 25 are never written.
//
// Owner: OASFMT (design D2).

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "encoder_impl.hpp"
#include "hefesto/oasis/codec/primitives.hpp"

namespace hefesto::oasis {

namespace {

/// Coordinates, lengths and steps are written up to 2^58 in magnitude, so
/// every delta between two of them fits the delta encodings.
constexpr std::int64_t kMaxMagnitude = std::int64_t{1} << 58;

[[noreturn]] void reject(std::string text) {
  detail::fail("OASIS-CODEC-026", std::move(text));
}

void put(std::vector<std::byte>& out, std::uint8_t byte) {
  out.push_back(std::byte{byte});
}

bool small(std::int64_t v) noexcept {
  return v > -kMaxMagnitude && v < kMaxMagnitude;
}

bool small(Point64 p) noexcept {
  return small(p.x) && small(p.y);
}

bool small_repetition(const Repetition& r) noexcept {
  return small(r.column_step) && small(r.row_step) && std::ranges::all_of(r.offsets, [](Point64 p) { return small(p); });
}

/// The ranges every element must respect.
void check_ranges(const Element& e) {
  const bool lengths = e.size.x >= 0 && e.size.y >= 0 && e.half_width >= 0 && e.radius >= 0;
  const bool magnitudes = small(e.position) && small(e.size) && small(e.half_width) && small(e.start_extension) &&
                          small(e.end_extension) && small(e.delta_a) && small(e.delta_b) && small(e.radius) &&
                          std::ranges::all_of(e.points, [](Point64 p) { return small(p); }) &&
                          (!e.repetition || small_repetition(*e.repetition));
  const bool transform = e.magnification > 0.0 && std::isfinite(e.magnification) && std::isfinite(e.angle);
  if (!lengths || !magnitudes || !transform || e.layer > 0xffffffffU || e.datatype > 0xffffffffU ||
      e.ctrapezoid_type > 25) {
    reject("OASIS encoder: element value out of range");
  }
}

bool quarter_turn(double angle) noexcept {
  return angle == 0.0 || angle == 90.0 || angle == 180.0 || angle == 270.0;
}

Point64 extent(const std::vector<Point64>& vertices) {
  if (vertices.empty()) {
    return {};
  }
  const auto [xmin, xmax] = std::ranges::minmax(vertices, {}, &Point64::x);
  const auto [ymin, ymax] = std::ranges::minmax(vertices, {}, &Point64::y);
  return {xmax.x - xmin.x, ymax.y - ymin.y};
}

/// The extension-scheme code of one extension value (0 when modal).
std::uint64_t extension_code(std::int64_t value, const std::optional<std::int64_t>& modal, std::int64_t half_width) {
  if (modal == value) {
    return 0;
  }
  if (value == 0) {
    return 1;
  }
  return value == half_width ? 2 : 3;
}

/// Sets `bit` of an info byte when `on` (no integer promotion on the byte).
void set_bit(std::uint8_t& info, bool on, std::uint8_t bit) {
  if (on) {
    info = static_cast<std::uint8_t>(info | bit);
  }
}

}  // namespace

std::uint8_t Encoder::Impl::layer_bits(std::vector<std::byte>& fields, const Element& e) {
  std::uint8_t bits = 0;
  if (modal.layer != e.layer) {
    bits |= 0x01U;
    write_unsigned(fields, e.layer);
    modal.layer = e.layer;
  }
  if (modal.datatype != e.datatype) {
    bits |= 0x02U;
    write_unsigned(fields, e.datatype);
    modal.datatype = e.datatype;
  }
  return bits;
}

std::uint8_t Encoder::Impl::position_bits(std::vector<std::byte>& fields, const Element& e,
                                          std::optional<std::int64_t>& mx, std::optional<std::int64_t>& my,
                                          std::uint8_t bit_x, std::uint8_t bit_y, std::uint8_t bit_r) {
  std::uint8_t bits = 0;
  if (mx != e.position.x) {
    bits |= bit_x;
    write_signed(fields, e.position.x);
    mx = e.position.x;
  }
  if (my != e.position.y) {
    bits |= bit_y;
    write_signed(fields, e.position.y);
    my = e.position.y;
  }
  if (!e.repetition) {
    return bits;
  }
  bits |= bit_r;
  const Repetition& r = *e.repetition;
  if (r.type == 0 || modal.repetition == r) {
    if (!modal.repetition) {
      reject("OASIS encoder: repetition type 0 with no modal repetition");
    }
    put(fields, 0);
    return bits;
  }
  if (!detail::write_repetition(fields, r)) {
    reject("OASIS encoder: repetition parameters do not fit repetition type " + std::to_string(r.type));
  }
  modal.repetition = r;
  return bits;
}

bool Encoder::Impl::length(std::vector<std::byte>& fields, std::int64_t value, std::optional<std::int64_t>& slot) {
  if (slot == value) {
    return false;
  }
  write_unsigned(fields, static_cast<std::uint64_t>(value));
  slot = value;
  return true;
}

void Encoder::Impl::element(const Element& e) {
  if (phase != Phase::cells) {
    reject("OASIS encoder: element() outside a cell");
  }
  check_ranges(e);
  using Writer = void (Impl::*)(std::vector<std::byte>&, const Element&);
  static constexpr std::array<Writer, 10> kWriters = {
      &Impl::placement, &Impl::text,   &Impl::rectangle, &Impl::polygon,  &Impl::path,
      &Impl::trapezoid, &Impl::ctrapezoid, &Impl::circle, &Impl::xgeometry, &Impl::xelement,
  };
  std::vector<std::byte> out;
  const ModalState saved = modal;
  try {
    (this->*kWriters[static_cast<std::size_t>(e.kind)])(out, e);
  } catch (const detail::CodecError&) {
    modal = saved;  // nothing was written: keep the modal state in step with the output
    throw;
  }
  emit(out);
}

std::uint8_t Encoder::Impl::placement_cell(std::vector<std::byte>& fields, const std::string& name) {
  if (options.strict ? modal.placement_cell_reference == cellnames.ref(name) : modal.placement_cell == name) {
    return 0;
  }
  name_field(fields, name, cellnames, StringKind::n_string);
  modal.placement_cell = options.strict ? std::nullopt : std::optional<std::string>{name};
  modal.placement_cell_reference = options.strict ? std::optional<std::uint64_t>{cellnames.ref(name)} : std::nullopt;
  return static_cast<std::uint8_t>(options.strict ? 0xc0U : 0x80U);
}

void Encoder::Impl::placement(std::vector<std::byte>& out, const Element& e) {
  const bool simple = e.magnification == 1.0 && quarter_turn(e.angle);
  std::vector<std::byte> fields;
  if (e.name.empty()) {
    reject("OASIS encoder: PLACEMENT without a cell name");
  }
  std::uint8_t info = placement_cell(fields, e.name);
  if (simple) {
    info |= static_cast<std::uint8_t>(static_cast<unsigned>(e.angle / 90.0) << 1U);
  } else {
    set_bit(info, e.magnification != 1.0, 0x04);
    set_bit(info, e.angle != 0.0, 0x02);
    if (e.magnification != 1.0) {
      write_real(fields, e.magnification);
    }
    if (e.angle != 0.0) {
      write_real(fields, e.angle);
    }
  }
  set_bit(info, e.flip, 0x01);
  info |= position_bits(fields, e, modal.placement_x, modal.placement_y, 0x20, 0x10, 0x08);
  put(out, simple ? 17 : 18);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

void Encoder::Impl::text(std::vector<std::byte>& out, const Element& e) {
  std::vector<std::byte> fields;
  std::uint8_t info = 0;
  if (options.strict ? modal.text_string_reference != textstrings.ref(e.name) : modal.text_string != e.name) {
    info |= options.strict ? 0x60U : 0x40U;
    name_field(fields, e.name, textstrings, StringKind::a_string);
    modal.text_string = options.strict ? std::nullopt : std::optional<std::string>{e.name};
    modal.text_string_reference = options.strict ? std::optional<std::uint64_t>{textstrings.ref(e.name)} : std::nullopt;
  }
  if (modal.textlayer != e.layer) {
    info |= 0x01U;
    write_unsigned(fields, e.layer);
    modal.textlayer = e.layer;
  }
  if (modal.texttype != e.datatype) {
    info |= 0x02U;
    write_unsigned(fields, e.datatype);
    modal.texttype = e.datatype;
  }
  info |= position_bits(fields, e, modal.text_x, modal.text_y, 0x10, 0x08, 0x04);
  put(out, 19);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

void Encoder::Impl::rectangle(std::vector<std::byte>& out, const Element& e) {
  std::vector<std::byte> fields;
  std::uint8_t info = layer_bits(fields, e);
  set_bit(info, length(fields, e.size.x, modal.geometry_w), 0x40);
  set_bit(info, length(fields, e.size.y, modal.geometry_h), 0x20);
  info |= position_bits(fields, e, modal.geometry_x, modal.geometry_y, 0x10, 0x08, 0x04);
  put(out, 20);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

void Encoder::Impl::polygon(std::vector<std::byte>& out, const Element& e) {
  std::vector<std::byte> fields;
  std::uint8_t info = layer_bits(fields, e);
  const std::uint8_t type = e.point_list_type.value_or(detail::narrowest_point_list(e.points, true));
  if (modal.polygon_points != e.points || modal.polygon_point_list_type != type) {
    if (!detail::write_point_list(fields, type, e.points, true)) {
      reject("OASIS encoder: polygon vertices do not fit point-list type " + std::to_string(type));
    }
    info |= 0x20U;
    modal.polygon_points = e.points;
    modal.polygon_point_list_type = type;
  }
  info |= position_bits(fields, e, modal.geometry_x, modal.geometry_y, 0x10, 0x08, 0x04);
  put(out, 21);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

void Encoder::Impl::path(std::vector<std::byte>& out, const Element& e) {
  std::vector<std::byte> fields;
  std::uint8_t info = layer_bits(fields, e);
  set_bit(info, length(fields, e.half_width, modal.path_half_width), 0x40);
  const std::uint64_t start = extension_code(e.start_extension, modal.path_start_extension, e.half_width);
  const std::uint64_t end = extension_code(e.end_extension, modal.path_end_extension, e.half_width);
  if (start != 0 || end != 0) {
    info |= 0x80U;
    write_unsigned(fields, (start << 2U) | end);
    if (start == 3) {
      write_signed(fields, e.start_extension);
    }
    if (end == 3) {
      write_signed(fields, e.end_extension);
    }
    modal.path_start_extension = e.start_extension;
    modal.path_end_extension = e.end_extension;
  }
  const std::uint8_t type = e.point_list_type.value_or(detail::narrowest_point_list(e.points, false));
  if (modal.path_points != e.points || modal.path_point_list_type != type) {
    if (!detail::write_point_list(fields, type, e.points, false)) {
      reject("OASIS encoder: path centerline does not fit point-list type " + std::to_string(type));
    }
    info |= 0x20U;
    modal.path_points = e.points;
    modal.path_point_list_type = type;
  }
  info |= position_bits(fields, e, modal.geometry_x, modal.geometry_y, 0x10, 0x08, 0x04);
  put(out, 22);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

void Encoder::Impl::trapezoid(std::vector<std::byte>& out, const Element& e) {
  std::vector<std::byte> fields;
  std::uint8_t info = layer_bits(fields, e);
  set_bit(info, e.vertical, 0x80);
  set_bit(info, length(fields, e.size.x, modal.geometry_w), 0x40);
  set_bit(info, length(fields, e.size.y, modal.geometry_h), 0x20);
  write_signed(fields, e.delta_a);
  write_signed(fields, e.delta_b);
  info |= position_bits(fields, e, modal.geometry_x, modal.geometry_y, 0x10, 0x08, 0x04);
  put(out, 23);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

void Encoder::Impl::ctrapezoid(std::vector<std::byte>& out, const Element& e) {
  std::vector<std::byte> fields;
  std::uint8_t info = layer_bits(fields, e);
  if (modal.ctrapezoid_type != e.ctrapezoid_type) {
    info |= 0x80U;
    write_unsigned(fields, e.ctrapezoid_type);
    modal.ctrapezoid_type = e.ctrapezoid_type;
  }
  if (ctrapezoid_uses_width(e.ctrapezoid_type)) {
    set_bit(info, length(fields, e.size.x, modal.geometry_w), 0x40);
  }
  if (ctrapezoid_uses_height(e.ctrapezoid_type)) {
    set_bit(info, length(fields, e.size.y, modal.geometry_h), 0x20);
  }
  info |= position_bits(fields, e, modal.geometry_x, modal.geometry_y, 0x10, 0x08, 0x04);
  const Point64 box = extent(outline(e));
  modal.geometry_w = box.x;  // as the decoder sets them (Q-OASIS-4)
  modal.geometry_h = box.y;
  put(out, 26);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

void Encoder::Impl::circle(std::vector<std::byte>& out, const Element& e) {
  std::vector<std::byte> fields;
  std::uint8_t info = layer_bits(fields, e);
  set_bit(info, length(fields, e.radius, modal.circle_radius), 0x20);
  info |= position_bits(fields, e, modal.geometry_x, modal.geometry_y, 0x10, 0x08, 0x04);
  put(out, 27);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

void Encoder::Impl::xelement(std::vector<std::byte>& out, const Element& e) {
  put(out, 32);
  write_unsigned(out, e.attribute);
  write_string(out, e.payload);
}

void Encoder::Impl::xgeometry(std::vector<std::byte>& out, const Element& e) {
  std::vector<std::byte> fields;
  write_unsigned(fields, e.attribute);
  std::uint8_t info = layer_bits(fields, e);
  write_string(fields, e.payload);
  info |= position_bits(fields, e, modal.geometry_x, modal.geometry_y, 0x10, 0x08, 0x04);
  put(out, 33);
  put(out, info);
  out.insert(out.end(), fields.begin(), fields.end());
}

}  // namespace hefesto::oasis
