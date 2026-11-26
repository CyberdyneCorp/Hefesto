// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal model of property matching: typed values, the keys compared for
// one device pair, property records and the series/parallel reconciliation
// (lvs-property-matching "Value types and promotion", "Tolerance rules",
// "Series/parallel network reconciliation").
//
// Owner: LVSPROP.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/prop/properties.hpp"

namespace hefesto::lvs::prop {

/// A value after parsing and promotion. `present` is false for a missing
/// value. `real` and `value` types carry `number`; `integer` carries
/// `integer`; `string` and `expression` carry `text`.
struct Value {
  bool present = false;
  PropertyType type = PropertyType::string;
  double number = 0.0;
  std::int64_t integer = 0;
  std::string text;

  [[nodiscard]] bool numeric() const noexcept {
    return type == PropertyType::real || type == PropertyType::value || type == PropertyType::integer;
  }
  [[nodiscard]] double as_double() const noexcept {
    return type == PropertyType::integer ? static_cast<double>(integer) : number;
  }
};

/// One compared key of a pair: the target type and tolerance of "Value
/// types and promotion", the merge rules and the swap partner of a
/// pin-associated key.
struct Key {
  std::string name;
  PropertyType type = PropertyType::string;
  double tolerance = 0.0;
  std::uint16_t merge = 0;
  Value fallback;  // the definition's default (Q-LVS-18: the double default)
  bool multiplier = false;  // M
  bool series = false;      // S
  bool string_from_value = false;  // a string definition with no default
  std::string pin;          // associated pin, if any
  std::optional<std::size_t> swap_partner;  // key associated with the other pin of a permutable pair
};

struct Keys {
  std::vector<Key> list;  // M, S, then the properties of interest
  static constexpr std::size_t kM = 0;
  static constexpr std::size_t kS = 1;
};

/// A property record: its `_tag` series marker and one value per key.
struct Record {
  std::string tag;
  std::vector<Value> values;
};

using Network = std::vector<Record>;

/// Parses a raw instance value (number with SI suffix, expression reduced
/// through the record, or string).
[[nodiscard]] Value parse_value(const std::string& text, const PropertyRecord& record);

/// Promotes `value` toward `type` (the table of "Value types and
/// promotion"); a value that cannot be promoted keeps its type.
[[nodiscard]] Value promote(Value value, PropertyType type);

/// The keys of a pair: M and S first, then circuit 1's properties of
/// interest, then circuit 2's that circuit 1 lacks.
[[nodiscard]] Keys build_keys(const Cell& first, const Cell& second, bool case_insensitive);

/// The records of `instance` over `keys`, promoted to each key's type.
[[nodiscard]] Network build_network(const Instance& instance, const Keys& keys);

/// Fills missing values with each key's default (merging only).
void apply_defaults(Network& network, const Keys& keys);

/// The integer M or S of a record (missing counts 1).
[[nodiscard]] std::int64_t count_of(const Record& record, std::size_t key);
void set_count(Record& record, std::size_t key, std::int64_t count);

/// The type of a value with `value` read as `real`.
[[nodiscard]] PropertyType normal_type(const Value& value) noexcept;

/// Whether two values match under `key`'s tolerance (types must agree).
[[nodiscard]] bool values_match(const Key& key, const Value& a, const Value& b, const PropertySettings& settings);

/// The reconciliation rounds of "Series/parallel network reconciliation";
/// returns the number of rounds run.
std::size_t reconcile(Network& first, Network& second, const Keys& keys, const PropertySettings& settings);

/// Folds the multiplier (`series` false: M; true: S) into the additive
/// properties and collapses consecutive records ("Merging with comb").
void fold(Network& network, const Keys& keys, bool series, const PropertySettings& settings);

/// The text of a value for reports: %g, %d or the string.
[[nodiscard]] std::string value_text(const Value& value);

}  // namespace hefesto::lvs::prop
