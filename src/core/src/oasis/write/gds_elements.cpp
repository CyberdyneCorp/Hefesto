// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The GDS element stream as structures and elements (see internal.hpp).
// Every record is bounded by GDSII's 16-bit record length, so an element
// holds at most one XY record's 8191 points per XY record read (the last
// XY record of an element wins).
// Owner: OASW.

#include <utility>

#include "internal.hpp"

namespace hefesto::oasis::detail {

namespace {

using gds::RecordType;

std::int32_t unsigned16(std::span<const std::byte> data) {
  const std::optional<std::int16_t> value = gds::i2_at(data, 0);
  return value ? static_cast<std::int32_t>(static_cast<std::uint16_t>(*value)) : 0;
}

std::int32_t signed16(std::span<const std::byte> data) {
  return gds::i2_at(data, 0).value_or(0);
}

std::vector<Point64> xy_points(std::span<const std::byte> data) {
  const std::vector<std::int32_t> values = gds::i4_values(data);
  std::vector<Point64> points;
  points.reserve(values.size() / 2);
  for (std::size_t i = 0; i + 1 < values.size(); i += 2) {
    points.push_back(Point64{values[i], values[i + 1]});
  }
  return points;
}

std::uint16_t bits16(std::span<const std::byte> data) {
  if (data.size() < 2) {
    return 0;
  }
  return static_cast<std::uint16_t>((std::to_integer<unsigned>(data[0]) << 8U) | std::to_integer<unsigned>(data[1]));
}

void set_property_value(GdsElement& e, std::span<const std::byte> data) {
  if (!e.properties.empty()) {
    e.properties.back().value = payload_text(data);
  }
}

// The records of an element other than the transform and path ones.
void apply_common(GdsElement& e, const gds::Record& r) {
  const std::span<const std::byte> data{r.data};
  switch (static_cast<RecordType>(r.type)) {
    case RecordType::layer:
      e.layer = unsigned16(data);
      break;
    case RecordType::datatype:
    case RecordType::texttype:
    case RecordType::boxtype:
    case RecordType::nodetype:
      e.datatype = unsigned16(data);
      break;
    case RecordType::xy:
      e.points = xy_points(data);
      break;
    case RecordType::string:
      e.string = payload_text(data);
      break;
    case RecordType::presentation:
      e.presentation = true;
      break;
    case RecordType::propattr:
      e.properties.push_back(GdsProperty{signed16(data), {}});
      break;
    case RecordType::propvalue:
      set_property_value(e, data);
      break;
    default:
      break;
  }
}

void apply(GdsElement& e, const gds::Record& r) {
  const std::span<const std::byte> data{r.data};
  switch (static_cast<RecordType>(r.type)) {
    case RecordType::sname:
      e.sname = payload_text(data);
      break;
    case RecordType::strans:
      e.strans = bits16(data);
      break;
    case RecordType::mag:
      e.magnification = gds::r8_at(data, 0);
      break;
    case RecordType::angle:
      e.angle = gds::r8_at(data, 0);
      break;
    case RecordType::colrow:
      e.columns = signed16(data);
      e.rows = gds::i2_at(data, 1).value_or(0);
      break;
    case RecordType::width:
      e.width = gds::i4_at(data, 0).value_or(0);
      break;
    case RecordType::pathtype:
      e.pathtype = signed16(data);
      break;
    case RecordType::bgnextn:
      e.begin_extension = gds::i4_at(data, 0).value_or(0);
      break;
    case RecordType::endextn:
      e.end_extension = gds::i4_at(data, 0).value_or(0);
      break;
    default:
      apply_common(e, r);
      break;
  }
}

std::optional<GdsKind> element_kind(std::uint8_t type) {
  switch (static_cast<RecordType>(type)) {
    case RecordType::boundary:
      return GdsKind::boundary;
    case RecordType::path:
      return GdsKind::path;
    case RecordType::sref:
      return GdsKind::sref;
    case RecordType::aref:
      return GdsKind::aref;
    case RecordType::text:
      return GdsKind::text;
    case RecordType::node:
      return GdsKind::node;
    case RecordType::box:
      return GdsKind::box;
    default:
      return std::nullopt;
  }
}

diag::Diagnostic truncated_stream() {
  return diag::make(codes::kGdsStream, "OASIS write: the GDS element stream ends inside an element or structure.");
}

}  // namespace

std::string payload_text(std::span<const std::byte> data) {
  return std::string{reinterpret_cast<const char*>(data.data()), data.size()};
}

diag::Outcome<GdsElement> GdsItems::element(GdsKind kind) {
  GdsElement e;
  e.kind = kind;
  for (;;) {
    diag::Outcome<std::optional<gds::Record>> next = reader_.next();
    if (!next.ok()) {
      return next.error();
    }
    if (!next.value()) {
      return truncated_stream();
    }
    if (static_cast<RecordType>(next.value()->type) == RecordType::endel) {
      return e;
    }
    apply(e, *next.value());
  }
}

// BGNSTR, then its STRNAME (an old-style vendor copy may carry it later).
diag::Outcome<GdsItem> GdsItems::structure() {
  GdsItem item;
  item.kind = GdsItem::Kind::structure;
  diag::Outcome<std::optional<gds::Record>> peeked = reader_.peek();
  if (!peeked.ok()) {
    return peeked.error();
  }
  if (peeked.value() && static_cast<RecordType>(peeked.value()->type) == RecordType::strname) {
    item.name = payload_text(peeked.value()->data);
    (void)reader_.next();
  }
  return item;
}

diag::Outcome<std::optional<GdsItem>> GdsItems::next() {
  for (;;) {
    diag::Outcome<std::optional<gds::Record>> next = reader_.next();
    if (!next.ok()) {
      return next.error();
    }
    if (!next.value()) {
      return std::optional<GdsItem>{};
    }
    const gds::Record& r = *next.value();
    GdsItem item;
    if (const std::optional<GdsKind> kind = element_kind(r.type)) {
      diag::Outcome<GdsElement> element = this->element(*kind);
      if (!element.ok()) {
        return element.error();
      }
      item.kind = GdsItem::Kind::element;
      item.element = std::move(element.value());
      return std::optional<GdsItem>{std::move(item)};
    }
    switch (static_cast<RecordType>(r.type)) {
      case RecordType::libname:
        item.kind = GdsItem::Kind::library_name;
        item.name = payload_text(r.data);
        return std::optional<GdsItem>{std::move(item)};
      case RecordType::bgnstr: {
        diag::Outcome<GdsItem> s = structure();
        if (!s.ok()) {
          return s.error();
        }
        return std::optional<GdsItem>{std::move(s.value())};
      }
      case RecordType::endstr:
        item.kind = GdsItem::Kind::structure_end;
        return std::optional<GdsItem>{std::move(item)};
      case RecordType::endlib:
        item.kind = GdsItem::Kind::library_end;
        return std::optional<GdsItem>{std::move(item)};
      default:
        break;  // HEADER, BGNLIB, UNITS and records outside elements
    }
  }
}

}  // namespace hefesto::oasis::detail
