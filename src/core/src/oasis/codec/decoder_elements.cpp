// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS decoder: element records (capability oasis-format "PLACEMENT
// records", "TEXT record", "RECTANGLE record", "POLYGON record", "PATH
// record", "TRAPEZOID records", "CTRAPEZOID record", "CIRCLE record",
// "Extension records", "Modal variables and reset rules"; design D11,
// Q-OASIS-3, 4, 11). Each handler reads every field its info byte
// announces, takes the omitted ones from the modal variables, and stores
// the resolved values back, so the modal state after a record is a
// function of the resolved element (the encoder relies on the same rule).
// An element that is invalid but whose fields could be read is dropped
// with a diagnostic and decoding continues.
//
// Owner: OASFMT (design D2).

#include <cmath>
#include <utility>

#include "decoder_impl.hpp"

namespace hefesto::oasis {

namespace {

constexpr std::uint8_t kBitX = 0x10;
constexpr std::uint8_t kBitY = 0x08;
constexpr std::uint8_t kBitR = 0x04;
constexpr std::uint8_t kBitD = 0x02;
constexpr std::uint8_t kBitL = 0x01;

bool has(std::uint8_t info, std::uint8_t bit) {
  return (info & bit) != 0;
}

Element element_of(ElementKind kind) {
  Element e;
  e.kind = kind;
  return e;
}

template <typename T>
const T& modal_value(const std::optional<T>& slot, std::string_view name) {
  if (!slot) {
    detail::fail("OASIS-CODEC-020", "modal variable " + std::string{name} + " undefined");
  }
  return *slot;
}

/// Width and height of the bounding box of `vertices`.
Point64 extent(const std::vector<Point64>& vertices) {
  if (vertices.empty()) {
    return {};
  }
  Point64 lo = vertices.front();
  Point64 hi = lo;
  for (const Point64 v : vertices) {
    lo = {std::min(lo.x, v.x), std::min(lo.y, v.y)};
    hi = {std::max(hi.x, v.x), std::max(hi.y, v.y)};
  }
  return {hi.x - lo.x, hi.y - lo.y};
}

}  // namespace

std::uint64_t Decoder::Impl::u32_field(bool present, std::optional<std::uint64_t>& slot, std::string_view name) {
  if (present) {
    slot = detail::read_u32(in, name);
  }
  return modal_value(slot, name);
}

std::int64_t Decoder::Impl::length_field(bool present, std::optional<std::int64_t>& slot, std::string_view name) {
  if (present) {
    slot = detail::to_length(detail::read_unsigned(in), drop);
  }
  return modal_value(slot, name);
}

std::int64_t Decoder::Impl::coordinate(bool present, std::optional<std::int64_t>& slot) {
  if (present) {
    const std::int64_t value = detail::read_signed(in);
    slot = modal.xy_absolute ? value : detail::add(slot.value_or(0), value, drop);
  }
  return slot.value_or(0);
}

std::optional<Repetition> Decoder::Impl::repetition(bool present) {
  if (!present) {
    return std::nullopt;
  }
  Repetition r = detail::read_repetition(in, modal.repetition, drop);
  modal.repetition = r;
  return r;
}

void Decoder::Impl::layer_fields(Element& e, std::uint8_t info) {
  e.layer = u32_field(has(info, kBitL), modal.layer, "layer");
  e.datatype = u32_field(has(info, kBitD), modal.datatype, "datatype");
}

void Decoder::Impl::position_fields(Element& e, std::uint8_t info) {
  e.position.x = coordinate(has(info, kBitX), modal.geometry_x);
  e.position.y = coordinate(has(info, kBitY), modal.geometry_y);
  e.repetition = repetition(has(info, kBitR));
}

void Decoder::Impl::finish_element(Element e) {
  target = PropertyTarget::element;
  record.element = std::move(e);
}

void Decoder::Impl::drop_unless_outline(const Element& e) {
  const std::vector<Point64> vertices = outline(e);
  if (!drop.dropped() && vertices.size() < 3) {
    drop.set("OASIS-CODEC-022", "degenerate trapezoid with fewer than 3 distinct vertices");
  }
}

void Decoder::Impl::placement() {
  const std::uint8_t info = in.get();
  Element e = element_of(ElementKind::placement);
  if (has(info, 0x80)) {
    if (has(info, 0x40)) {
      modal.placement_cell_reference = detail::read_unsigned(in);
      modal.placement_cell.reset();
      used[0].insert(*modal.placement_cell_reference);
    } else {
      modal.placement_cell = detail::read_string(in);
      modal.placement_cell_reference.reset();
      check(StringKind::n_string, *modal.placement_cell);
      strict.inline_use(detail::InlineUse::placement_by_name);
    }
  } else if (!modal.placement_cell && !modal.placement_cell_reference) {
    detail::fail("OASIS-CODEC-020", "modal variable placement-cell undefined");
  }
  e.name = modal.placement_cell.value_or("");
  e.reference = modal.placement_cell_reference;
  if (record.id == RecordId::placement_transform) {
    e.magnification = has(info, 0x04) ? detail::read_real(in) : 1.0;
    e.angle = has(info, 0x02) ? detail::read_real(in) : 0.0;
    if (!std::isfinite(e.magnification) || !std::isfinite(e.angle)) {
      drop.set("OASIS-CODEC-021", "invalid magnification or angle in a PLACEMENT");
    } else if (e.magnification <= 0.0) {
      // D3(b) of the user decisions of 2026-11-19: dropped with a warning.
      drop.set("OASIS-CODEC-029", "invalid magnification (0 or less) in a PLACEMENT; the placement is dropped");
    }
  } else {
    e.angle = 90.0 * ((info >> 1U) & 3U);
  }
  e.flip = has(info, 0x01);
  e.position.x = coordinate(has(info, 0x20), modal.placement_x);
  e.position.y = coordinate(has(info, 0x10), modal.placement_y);
  e.repetition = repetition(has(info, 0x08));
  finish_element(std::move(e));
}

void Decoder::Impl::text() {
  const std::uint8_t info = in.get();
  if (has(info, 0x80)) {
    warn("OASIS-CODEC-022", "TEXT info byte with bit 7 set");
  }
  Element e = element_of(ElementKind::text);
  if (has(info, 0x40)) {
    if (has(info, 0x20)) {
      modal.text_string_reference = detail::read_unsigned(in);
      modal.text_string.reset();
      used[1].insert(*modal.text_string_reference);
    } else {
      modal.text_string = detail::read_string(in);
      modal.text_string_reference.reset();
      check(StringKind::a_string, *modal.text_string);
      strict.inline_use(detail::InlineUse::text_string);
    }
  } else if (!modal.text_string && !modal.text_string_reference) {
    detail::fail("OASIS-CODEC-020", "modal variable text-string undefined");
  }
  e.name = modal.text_string.value_or("");
  e.reference = modal.text_string_reference;
  e.layer = u32_field(has(info, 0x01), modal.textlayer, "textlayer");
  e.datatype = u32_field(has(info, 0x02), modal.texttype, "texttype");
  e.position.x = coordinate(has(info, 0x10), modal.text_x);
  e.position.y = coordinate(has(info, 0x08), modal.text_y);
  e.repetition = repetition(has(info, 0x04));
  finish_element(std::move(e));
}

void Decoder::Impl::rectangle() {
  const std::uint8_t info = in.get();
  const bool square = has(info, 0x80);
  Element e = element_of(ElementKind::rectangle);
  layer_fields(e, info);
  e.size.x = length_field(has(info, 0x40), modal.geometry_w, "geometry-w");
  if (square && has(info, 0x20)) {
    warn("OASIS-CODEC-022", "RECTANGLE with both S and H set; the height is ignored");
    (void)detail::read_unsigned(in);
  }
  if (square) {
    modal.geometry_h = e.size.x;  // Q-OASIS-3
  }
  e.size.y = square ? e.size.x : length_field(has(info, 0x20), modal.geometry_h, "geometry-h");
  position_fields(e, info);
  finish_element(std::move(e));
}

void Decoder::Impl::polygon() {
  const std::uint8_t info = in.get();
  Element e = element_of(ElementKind::polygon);
  layer_fields(e, info);
  if (has(info, 0x20)) {
    PointList list = detail::read_point_list(in, drop);
    const std::size_t n = list.vertices.size();
    if (list.type <= 1 && n > 0) {
      if (n % 2 != 0) {
        warn("OASIS-CODEC-022", "type 0/1 point list with odd count");
      }
      const bool last_horizontal = detail::horizontal_delta(n - 1, list.type == 1);
      list.vertices.push_back(detail::synthesized_vertex(list.vertices.back(), last_horizontal));
    }
    modal.polygon_points = std::move(list.vertices);
    modal.polygon_point_list_type = list.type;
  }
  e.points = modal_value(modal.polygon_points, "polygon-point-list");
  e.point_list_type = modal.polygon_point_list_type;
  position_fields(e, info);
  if (e.points.size() + 1 < 3) {
    drop.set("OASIS-CODEC-021", "polygon with fewer than 3 vertices");
  }
  finish_element(std::move(e));
}

void Decoder::Impl::path() {
  const std::uint8_t info = in.get();
  Element e = element_of(ElementKind::path);
  layer_fields(e, info);
  e.half_width = length_field(has(info, 0x40), modal.path_half_width, "path-halfwidth");
  if (has(info, 0x80)) {
    const std::uint64_t scheme = detail::read_unsigned(in);
    const auto extension = [&](std::uint64_t code, std::optional<std::int64_t>& slot) {
      if (code == 1) {
        slot = 0;
      } else if (code == 2) {
        slot = e.half_width;  // Q-OASIS-11: the numeric value
      } else if (code == 3) {
        slot = detail::read_signed(in);
      }
    };
    extension((scheme >> 2U) & 3U, modal.path_start_extension);
    extension(scheme & 3U, modal.path_end_extension);
  }
  e.start_extension = modal_value(modal.path_start_extension, "path-start-extension");
  e.end_extension = modal_value(modal.path_end_extension, "path-end-extension");
  if (has(info, 0x20)) {
    PointList list = detail::read_point_list(in, drop);
    modal.path_points = std::move(list.vertices);
    modal.path_point_list_type = list.type;
  }
  e.points = modal_value(modal.path_points, "path-point-list");
  e.point_list_type = modal.path_point_list_type;
  position_fields(e, info);
  if (e.points.empty()) {
    drop.set("OASIS-CODEC-021", "path with fewer than 2 vertices");
  }
  finish_element(std::move(e));
}

void Decoder::Impl::trapezoid() {
  const std::uint8_t info = in.get();
  Element e = element_of(ElementKind::trapezoid);
  e.vertical = has(info, 0x80);
  layer_fields(e, info);
  e.size.x = length_field(has(info, 0x40), modal.geometry_w, "geometry-w");
  e.size.y = length_field(has(info, 0x20), modal.geometry_h, "geometry-h");
  e.delta_a = record.id != RecordId::trapezoid_b ? detail::read_signed(in) : 0;
  e.delta_b = record.id != RecordId::trapezoid_a ? detail::read_signed(in) : 0;
  position_fields(e, info);
  drop_unless_outline(e);
  finish_element(std::move(e));
}

void Decoder::Impl::ctrapezoid() {
  const std::uint8_t info = in.get();
  Element e = element_of(ElementKind::ctrapezoid);
  layer_fields(e, info);
  bool valid = true;
  if (has(info, 0x80)) {
    const std::uint32_t read = detail::read_u32(in, "ctrapezoid-type");
    valid = read <= 25;
    if (valid) {
      modal.ctrapezoid_type = static_cast<std::uint8_t>(read);
    } else {
      drop.set("OASIS-CODEC-021", "invalid CTRAPEZOID type " + std::to_string(read));
    }
  }
  const std::uint8_t type = valid ? modal_value(modal.ctrapezoid_type, "ctrapezoid-type") : 0;
  e.ctrapezoid_type = type;
  const bool width = has(info, 0x40) || (valid && ctrapezoid_uses_width(type));
  const bool height = has(info, 0x20) || (valid && ctrapezoid_uses_height(type));
  const std::int64_t w = width ? length_field(has(info, 0x40), modal.geometry_w, "geometry-w") : 0;
  const std::int64_t h = height ? length_field(has(info, 0x20), modal.geometry_h, "geometry-h") : 0;
  e.size = {ctrapezoid_uses_width(type) ? w : 0, ctrapezoid_uses_height(type) ? h : 0};
  position_fields(e, info);
  if (valid) {
    const Point64 box = extent(outline(e));
    modal.geometry_w = box.x;  // Q-OASIS-4: the decoded bounding box
    modal.geometry_h = box.y;
    drop_unless_outline(e);
  }
  finish_element(std::move(e));
}

void Decoder::Impl::circle() {
  const std::uint8_t info = in.get();
  Element e = element_of(ElementKind::circle);
  layer_fields(e, info);
  e.radius = length_field(has(info, 0x20), modal.circle_radius, "circle-radius");
  position_fields(e, info);
  finish_element(std::move(e));
}

void Decoder::Impl::xelement() {
  Element e = element_of(ElementKind::xelement);
  e.attribute = detail::read_unsigned(in);
  e.payload = detail::read_string(in);
  finish_element(std::move(e));
}

void Decoder::Impl::xgeometry() {
  const std::uint8_t info = in.get();
  Element e = element_of(ElementKind::xgeometry);
  e.attribute = detail::read_unsigned(in);
  layer_fields(e, info);
  e.payload = detail::read_string(in);
  position_fields(e, info);
  finish_element(std::move(e));
}

}  // namespace hefesto::oasis
