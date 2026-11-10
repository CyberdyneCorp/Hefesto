// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Typed cell properties (layout-properties "Typed property store",
// "Reserved-name type coercion"). The store is Magic's string hash table
// created with 8 buckets (database/DBprop.c DBPropPut); removal unlinks the
// entry (HashRemove) and clearing recreates the table (DBPropClearAll).

#include "hefesto/db/property.hpp"

#include <charconv>
#include <cstdint>
#include <utility>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/change_recorder.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::db {

namespace {

constexpr std::size_t kPropertyBuckets = 8;

template <typename Int>
bool parse_int_list(std::string_view text, std::vector<Int>& out) {
  std::size_t pos = 0;
  while (pos < text.size()) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
      ++pos;
    }
    if (pos >= text.size()) {
      break;
    }
    Int v{};
    const char* first = text.data() + pos;
    const char* last = text.data() + text.size();
    if (*first == '+') {
      ++first;
    }
    const auto [ptr, ec] = std::from_chars(first, last, v);
    if (ec != std::errc{} || (ptr != last && *ptr != ' ' && *ptr != '\t')) {
      return false;
    }
    out.push_back(v);
    pos = static_cast<std::size_t>(ptr - text.data());
  }
  return true;
}

template <typename Int>
std::string join(const std::vector<Int>& values) {
  std::string out;
  for (const Int v : values) {
    if (!out.empty()) {
      out += ' ';
    }
    out += std::to_string(v);
  }
  return out;
}

bool starts_with(std::string_view s, std::string_view prefix) {
  return s.substr(0, prefix.size()) == prefix;
}

PropertyValue coerce_dimensions4(const PropertyValue& value, const std::string& text) {
  if (const auto* d = std::get_if<DimensionList>(&value); d != nullptr && d->values.size() == 4) {
    return value;
  }
  std::vector<std::int32_t> v;
  if (parse_int_list(text, v) && v.size() == 4) {
    return DimensionList{std::move(v)};
  }
  return text;
}

PropertyValue coerce_double(const PropertyValue& value, const std::string& text) {
  if (std::holds_alternative<DoubleList>(value)) {
    return value;
  }
  std::vector<std::int64_t> v;
  if (parse_int_list(text, v) && v.size() == 1) {
    return DoubleList{std::move(v)};
  }
  return text;
}

PropertyValue coerce_plane(const PropertyValue& value, const std::string& text) {
  if (std::holds_alternative<PlaneValue>(value)) {
    return value;
  }
  std::vector<std::int32_t> v;
  if (!parse_int_list(text, v) || v.empty() || v.size() % 4 != 0) {
    return text;
  }
  PlaneValue plane;
  for (std::size_t i = 0; i < v.size(); i += 4) {
    plane.rects.push_back(canonical(Rect{v[i], v[i + 1], v[i + 2], v[i + 3]}));
  }
  return plane;
}

}  // namespace

struct PropertyStore::Impl {
  util::MagicHashTable<PropertyValue> table{kPropertyBuckets};
};

PropertyStore::PropertyStore() : impl_(std::make_unique<Impl>()) {}
PropertyStore::~PropertyStore() = default;
PropertyStore::PropertyStore(const PropertyStore& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}
PropertyStore& PropertyStore::operator=(const PropertyStore& other) {
  if (this != &other) {
    impl_ = std::make_unique<Impl>(*other.impl_);
  }
  return *this;
}
PropertyStore::PropertyStore(PropertyStore&&) noexcept = default;
PropertyStore& PropertyStore::operator=(PropertyStore&&) noexcept = default;

void PropertyStore::put(std::string_view name, PropertyValue value) {
  impl_->table.insert(std::string{name}, std::move(value));
}

bool PropertyStore::remove(std::string_view name) {
  return impl_->table.remove(std::string{name});
}

void PropertyStore::clear() {
  impl_ = std::make_unique<Impl>();
}

const PropertyValue* PropertyStore::find(std::string_view name) const {
  const auto* entry = impl_->table.find(std::string{name});
  return entry != nullptr && entry->value ? &*entry->value : nullptr;
}

std::size_t PropertyStore::size() const noexcept {
  return impl_->table.size();
}

void PropertyStore::for_each(const std::function<void(std::string_view, const PropertyValue&)>& visit) const {
  impl_->table.for_each([&](const auto& entry) {
    if (entry.value) {
      visit(entry.key, *entry.value);
    }
  });
}

std::string property_text(const PropertyValue& value) {
  struct Visitor {
    std::string operator()(const std::string& s) const { return s; }
    std::string operator()(const IntegerList& l) const { return join(l.values); }
    std::string operator()(const DimensionList& l) const { return join(l.values); }
    std::string operator()(const DoubleList& l) const { return join(l.values); }
    std::string operator()(const PlaneValue& p) const {
      std::vector<std::int32_t> v;
      for (const Rect& r : p.rects) {
        v.insert(v.end(), {r.x0, r.y0, r.x1, r.y1});
      }
      return join(v);
    }
  };
  return std::visit(Visitor{}, value);
}

namespace {

template <typename F>
PropertyValue map_points(const PropertyValue& value, F f) {
  if (const auto* d = std::get_if<DimensionList>(&value)) {
    DimensionList out = *d;
    for (std::size_t i = 0; i + 1 < out.values.size(); i += 2) {
      const Point p = f(Point{out.values[i], out.values[i + 1]});
      out.values[i] = p.x;
      out.values[i + 1] = p.y;
    }
    return out;
  }
  if (const auto* pl = std::get_if<PlaneValue>(&value)) {
    PlaneValue out;
    for (const Rect& r : pl->rects) {
      const Point a = f(Point{r.x0, r.y0});
      const Point b = f(Point{r.x1, r.y1});
      out.rects.push_back(canonical(Rect{a.x, a.y, b.x, b.y}));
    }
    return out;
  }
  return value;
}

}  // namespace

PropertyValue scale_property(const PropertyValue& value, std::int64_t num, std::int64_t den) {
  return map_points(value, [&](Point p) {
    return Point{static_cast<Coord>(p.x * num / den), static_cast<Coord>(p.y * num / den)};
  });
}

PropertyValue translate_property(const PropertyValue& value, Coord dx, Coord dy) {
  return map_points(value, [&](Point p) { return Point{p.x + dx, p.y + dy}; });
}

PropertyValue coerce_reserved_property(std::string_view name, const PropertyValue& value) {
  const std::string text = property_text(value);
  if (name == "FIXED_BBOX" || name == "OBS_BBOX") {
    return coerce_dimensions4(value, text);
  }
  if (name == "GDS_START" || name == "GDS_BEGIN" || name == "GDS_END") {
    return coerce_double(value, text);
  }
  if (name == "GDS_FILE") {
    return text;
  }
  if (starts_with(name, "MASKHINTS_")) {
    return coerce_plane(value, text);
  }
  return value;
}

diag::Outcome<PropertyValue> parse_property_value(std::string_view type, std::string_view text) {
  const auto bad = [&] {
    return diag::make(codes::kPropertyValue,
                      "cannot parse \"" + std::string{text} + "\" as a " + std::string{type} + " property");
  };
  if (type == "string") {
    return PropertyValue{std::string{text}};
  }
  if (type == "integer" || type == "dimension") {
    std::vector<std::int32_t> v;
    if (!parse_int_list(text, v)) {
      return bad();
    }
    return type == "integer" ? PropertyValue{IntegerList{std::move(v)}} : PropertyValue{DimensionList{std::move(v)}};
  }
  if (type == "double") {
    std::vector<std::int64_t> v;
    if (!parse_int_list(text, v)) {
      return bad();
    }
    return PropertyValue{DoubleList{std::move(v)}};
  }
  return bad();
}

std::vector<Rect> plane_property_tiles(const PlaneValue& value) {
  Plane plane;
  PaintRow row{};
  row.fill(1);
  for (const Rect& r : value.rects) {
    if (r.x0 < r.x1 && r.y0 < r.y1) {
      (void)plane.paint(r, row);
    }
  }
  std::vector<Rect> out;
  plane.enumerate(whole_plane(), tech::TypeMask::of(1), [&](const TileView& t) {
    out.push_back(t.area);
    return true;
  });
  return out;
}

bool change_property(CellDef& def, std::string_view name, std::optional<PropertyValue> value) {
  if (def.flags().no_edit) {
    return false;  // DBPropPut ignores read-only cells
  }
  const PropertyValue* found = def.properties().find(name);
  std::optional<PropertyValue> old_value = found != nullptr ? std::optional<PropertyValue>{*found} : std::nullopt;
  if (old_value == value) {
    return false;
  }
  if (ChangeRecorder* r = def.recorder()) {
    r->property_changed(def, name, old_value, value);
  }
  set_property(def, name, std::move(value));
  return true;
}

}  // namespace hefesto::db
