// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Typed cell properties (capability layout-properties; reserved Hefesto
// names per capability hefesto-mag-extensions).
// (Magic ref: database/database.h.in:730-795, database/DBprop.c)
//
// Owner: HIER (openspec/changes/m2-interactive-editing/design.md D2; MODEL in M1).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/hints/region.hpp"

namespace hefesto::db {

class CellDef;

struct IntegerList {
  std::vector<std::int32_t> values;
  friend bool operator==(const IntegerList&, const IntegerList&) = default;
};
/// Internal units; consecutive pairs are points that scale with the database.
struct DimensionList {
  std::vector<std::int32_t> values;
  friend bool operator==(const DimensionList&, const DimensionList&) = default;
};
/// 64-bit integers (file offsets), never scaled.
struct DoubleList {
  std::vector<std::int64_t> values;
  friend bool operator==(const DoubleList&, const DoubleList&) = default;
};
/// A plane property (MASKHINTS_* only): a region in Magic's maximal
/// horizontal strips, hints::HintRegion (mask-hints "Mask-hint property
/// data model"; openspec/changes/m3-drc design.md D4/D10).
using PlaneValue = hints::HintRegion;

using PropertyValue = std::variant<std::string, IntegerList, DimensionList, DoubleList, PlaneValue>;

/// Name -> typed value, iterated in Magic's string hash-table order (8
/// initial buckets, as DBPropPut creates it): the order of the `property`
/// listing (Q-MODEL-25). The `.mag` writer sorts names itself ("Writer
/// properties"). Side effects on cell flags: db::set_property().
class PropertyStore {
 public:
  PropertyStore();
  ~PropertyStore();
  PropertyStore(const PropertyStore&);
  PropertyStore& operator=(const PropertyStore&);
  PropertyStore(PropertyStore&&) noexcept;
  PropertyStore& operator=(PropertyStore&&) noexcept;

  /// Replaces any previous value of `name`.
  void put(std::string_view name, PropertyValue value);
  /// True if a property was removed.
  bool remove(std::string_view name);
  void clear();
  [[nodiscard]] const PropertyValue* find(std::string_view name) const;
  [[nodiscard]] std::size_t size() const noexcept;
  void for_each(const std::function<void(std::string_view, const PropertyValue&)>& visit) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// "Reserved-name type coercion": the value a reserved name is stored with
/// (FIXED_BBOX/OBS_BBOX: 4 dimensions; GDS_START/GDS_BEGIN/GDS_END: double;
/// GDS_FILE: string; MASKHINTS_*: plane of 4-value groups). Other names keep
/// `value`. A value that cannot be parsed falls back to its string form.
/// A plane property's region as Magic stores and enumerates it: the
/// rectangles painted into a tile plane (merged into maximal horizontal
/// strips), in the plane's area-enumeration order (DBSrPaintArea), which is
/// the order `.mag` files and the `property` command show them
/// (value.strips()).
[[nodiscard]] std::vector<Rect> plane_property_tiles(const PlaneValue& value);

[[nodiscard]] PropertyValue coerce_reserved_property(std::string_view name, const PropertyValue& value);

/// "Geometric properties follow the geometry": a dimension or plane value
/// rescaled by num/den (dimension values pairwise as points, a trailing odd
/// value unchanged) or translated by (dx, dy); other types are unchanged.
/// Scaling truncates like Magic's integer scaling (callers scale only by
/// factors that keep the grid exact).
[[nodiscard]] PropertyValue scale_property(const PropertyValue& value, std::int64_t num, std::int64_t den);
[[nodiscard]] PropertyValue translate_property(const PropertyValue& value, Coord dx, Coord dy);

/// Value text as the `property` command and the `.mag` writer print it
/// (lists space-separated, plane rectangles as groups of 4 values).
[[nodiscard]] std::string property_text(const PropertyValue& value);

/// Parses a typed value from command or file text: `type` is one of
/// "string", "integer", "dimension", "double" (DB-PROP-001 when the text
/// does not parse).
[[nodiscard]] diag::Outcome<PropertyValue> parse_property_value(std::string_view type, std::string_view text);

/// The recorded form of db::set_property (cell.hpp) used by commands: puts
/// (or with nullopt removes) the property and reports the change to the
/// library's db::ChangeRecorder (property_changed; design.md D4/D5). Does
/// nothing on a read-only cell or when the value is unchanged; returns
/// whether the property changed. File readers keep calling set_property,
/// which records nothing (reads are never undoable). Does not mark the cell
/// modified (the `property` command does).
bool change_property(CellDef& def, std::string_view name, std::optional<PropertyValue> value);

}  // namespace hefesto::db
