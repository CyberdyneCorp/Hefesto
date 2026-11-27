// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Values, keys and records of property matching (see records.hpp).
//
// Owner: LVSPROP.

#include "lvs/prop/records.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <utility>

#include "hefesto/lvs/net/expression.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/net/permute.hpp"
#include "hefesto/lvs/net/si_value.hpp"

namespace hefesto::lvs {

std::string format_g(double value, int precision) {
  std::array<char, 64> buffer{};
  const auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general, precision);
  return {buffer.data(), result.ptr};
}

double relative_delta(double a, double b, bool netgen_compat) noexcept {
  if (netgen_compat) {
    return 2.0 * std::fabs(a - b) / (a + b);
  }
  const double scale = std::fmax(std::fabs(a), std::fabs(b));
  return scale == 0.0 ? 0.0 : std::fabs(a - b) / scale;
}

bool within_relative_tolerance(double a, double b, double t, bool netgen_compat) noexcept {
  const double ratio = relative_delta(a, b, netgen_compat);
  if (netgen_compat) {
    return !(ratio > t);  // a NaN ratio (0/0) and a negative sum match
  }
  return ratio <= t;
}

namespace prop {

namespace {

Value number_value(double number) {
  Value value;
  value.present = true;
  value.type = PropertyType::real;
  value.number = number;
  return value;
}

Value text_value(std::string text, PropertyType type) {
  Value value;
  value.present = true;
  value.type = type;
  value.text = std::move(text);
  return value;
}

bool integral(double number) noexcept {
  return std::isfinite(number) && std::trunc(number) == number && std::fabs(number) < 9.0e18;
}

bool is_double_type(PropertyType type) noexcept {
  return type == PropertyType::real || type == PropertyType::value;
}

/// The target definition of two sides ("Target type"): double from circuit
/// 1, double from circuit 2, integer from 1, integer from 2, string from 1,
/// string from 2, else circuit 1's.
const PropertyDef* target_definition(const PropertyDef* first, const PropertyDef* second) {
  if (first == nullptr || second == nullptr) {
    return first != nullptr ? first : second;
  }
  if (first->type == second->type) {
    return first;
  }
  const std::array<std::pair<const PropertyDef*, bool>, 6> order{{
      {first, is_double_type(first->type)},
      {second, is_double_type(second->type)},
      {first, first->type == PropertyType::integer},
      {second, second->type == PropertyType::integer},
      {first, first->type == PropertyType::string},
      {second, second->type == PropertyType::string},
  }};
  for (const auto& [definition, wanted] : order) {
    if (wanted) {
      return definition;
    }
  }
  return first;
}

Key count_key(std::string name, bool series) {
  Key key;
  key.name = std::move(name);
  key.type = PropertyType::integer;
  key.multiplier = !series;
  key.series = series;
  key.fallback.present = true;
  key.fallback.type = PropertyType::integer;
  key.fallback.integer = 1;
  return key;
}

Key key_from(const PropertyDef& definition) {
  Key key;
  key.name = definition.key;
  key.type = definition.type;
  key.tolerance = definition.tolerance;
  key.merge = definition.merge;
  key.pin = definition.pin;
  key.string_from_value = definition.type == PropertyType::string && definition.default_value.empty();
  if (!definition.default_value.empty()) {
    // Q-LVS-18 fix (both modes): a missing double takes the definition's
    // default, not an unset value.
    key.fallback = promote(parse_value(definition.default_value, {}), definition.type);
  }
  return key;
}

/// The pin position of each key (nullopt: no pin, or a pin `klass` lacks).
using PinPositions = std::vector<std::optional<std::uint32_t>>;

/// The last key associated with pin position `pin`, if any.
std::optional<std::size_t> last_key_at(const PinPositions& positions, std::uint32_t pin) {
  for (std::size_t j = positions.size(); j-- > 0;) {
    if (positions[j] == pin) {
      return j;
    }
  }
  return std::nullopt;
}

/// The key on the other pin of the last permutable pair holding `pin` that
/// has one, if any.
template <typename Pairs>
std::optional<std::size_t> swap_partner_of(std::uint32_t pin, const Pairs& pairs, const PinPositions& positions) {
  std::optional<std::size_t> found;
  for (const auto& [a, b] : pairs) {
    if (pin != a && pin != b) {
      continue;
    }
    if (const auto partner = last_key_at(positions, pin == a ? b : a)) {
      found = partner;
    }
  }
  return found;
}

/// Links each pin-associated key to the key associated with the other pin
/// of its permutable pair ("Pin-associated properties").
void link_swap_partners(Keys& keys, const Cell& klass) {
  const auto pairs = permute_pairs(klass).pairs;
  PinPositions positions;
  positions.reserve(keys.list.size());
  for (const auto& key : keys.list) {
    positions.push_back(key.pin.empty() ? std::nullopt : port_position(klass, key.pin));
  }
  for (std::size_t k = 0; k < keys.list.size(); ++k) {
    const auto partner = positions[k] ? swap_partner_of(*positions[k], pairs, positions) : std::nullopt;
    if (partner) {
      keys.list[k].swap_partner = *partner;
    }
  }
}

}  // namespace

Value parse_value(const std::string& text, const PropertyRecord& record) {
  if (is_expression_text(text)) {
    const ParameterScope scope{&record, nullptr, nullptr};
    const auto resolved = resolve_value(text, scope);
    if (resolved.type == PropertyType::value) {
      return number_value(resolved.number);
    }
    return text_value(resolved.text, resolved.type);
  }
  if (is_si_number(text)) {
    if (const auto number = parse_si_value(text)) {
      return number_value(*number);
    }
  }
  return text_value(text, PropertyType::string);
}

Value promote(Value value, PropertyType type) {
  if (!value.present || value.type == PropertyType::expression) {
    return value;
  }
  switch (type) {
    case PropertyType::integer:
      if (value.type != PropertyType::integer && value.numeric() && integral(value.number)) {
        value.integer = static_cast<std::int64_t>(value.number);
        value.type = PropertyType::integer;
      }
      return value;
    case PropertyType::real:
    case PropertyType::value:
      if (value.type == PropertyType::integer) {
        value.number = static_cast<double>(value.integer);
      }
      if (value.numeric()) {
        value.type = type;
      }
      return value;
    case PropertyType::string:
      if (value.numeric()) {
        value.text = value_text(value);
        value.type = PropertyType::string;
      }
      return value;
    case PropertyType::expression:
      return value;
  }
  return value;
}

std::string value_text(const Value& value) {
  if (value.type == PropertyType::integer) {
    return std::to_string(value.integer);
  }
  if (value.numeric()) {
    return format_g(value.number);
  }
  return value.text;
}

Keys build_keys(const Cell& first, const Cell& second, bool case_insensitive) {
  Keys keys;
  keys.list.push_back(count_key("M", false));
  keys.list.push_back(count_key("S", true));
  const auto is_count = [](std::string_view name) {
    return names_equal(name, "M", true) || names_equal(name, "S", true);
  };
  const auto find_in = [case_insensitive](const Cell& cell, std::string_view name) -> const PropertyDef* {
    for (const auto& definition : cell.properties) {
      if (names_equal(definition.key, name, case_insensitive)) {
        return &definition;
      }
    }
    return nullptr;
  };
  for (const auto& definition : first.properties) {
    if (!is_count(definition.key)) {
      keys.list.push_back(key_from(*target_definition(&definition, find_in(second, definition.key))));
      keys.list.back().name = definition.key;
    }
  }
  for (const auto& definition : second.properties) {
    if (!is_count(definition.key) && find_in(first, definition.key) == nullptr) {
      keys.list.push_back(key_from(definition));
    }
  }
  link_swap_partners(keys, first);
  return keys;
}

Network build_network(const Instance& instance, const Keys& keys) {
  Network network;
  network.reserve(instance.properties.size());
  for (const auto& raw : instance.properties) {
    Record record;
    if (const auto* tag = find_property(raw, "_tag")) {
      record.tag = tag->value;
    }
    record.values.resize(keys.list.size());
    for (std::size_t i = 0; i < keys.list.size(); ++i) {
      const Key& key = keys.list[i];
      if (const auto* property = find_property(raw, key.name)) {
        Value value = parse_value(property->value, raw);
        record.values[i] = key.string_from_value ? std::move(value) : promote(std::move(value), key.type);
      }
    }
    network.push_back(std::move(record));
  }
  return network;
}

void apply_defaults(Network& network, const Keys& keys) {
  for (auto& record : network) {
    for (std::size_t i = 0; i < keys.list.size(); ++i) {
      if (!record.values[i].present && keys.list[i].fallback.present) {
        record.values[i] = keys.list[i].fallback;
      }
    }
  }
}

std::int64_t count_of(const Record& record, std::size_t key) {
  const Value& value = record.values[key];
  if (!value.present) {
    return 1;
  }
  if (value.type == PropertyType::integer) {
    return value.integer;
  }
  return value.numeric() && integral(value.number) ? static_cast<std::int64_t>(value.number) : 1;
}

void set_count(Record& record, std::size_t key, std::int64_t count) {
  Value value;
  value.present = true;
  value.type = PropertyType::integer;
  value.integer = count;
  record.values[key] = value;
}

namespace {

bool strings_match(const Key& key, const std::string& a, const std::string& b, bool case_insensitive) {
  const long prefix = std::lround(key.tolerance);
  if (prefix <= 0) {
    return names_equal(a, b, case_insensitive) || (a.empty() && b.empty());
  }
  const auto n = static_cast<std::size_t>(prefix);
  if (std::min(a.size(), n) != std::min(b.size(), n)) {
    return false;
  }
  for (std::size_t i = 0; i < std::min(a.size(), n); ++i) {
    if (fold_name_byte(static_cast<unsigned char>(a[i])) != fold_name_byte(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

PropertyType normal_type(const Value& value) noexcept {
  return value.type == PropertyType::value ? PropertyType::real : value.type;
}

bool values_match(const Key& key, const Value& a, const Value& b, const PropertySettings& settings) {
  if (a.type == PropertyType::expression || b.type == PropertyType::expression || normal_type(a) != normal_type(b)) {
    return false;
  }
  switch (normal_type(a)) {
    case PropertyType::integer:
      return std::fabs(static_cast<double>(a.integer) - static_cast<double>(b.integer)) <= key.tolerance;
    case PropertyType::real:
      return within_relative_tolerance(a.number, b.number, key.tolerance, settings.netgen_compat);
    case PropertyType::string:
      return strings_match(key, a.text, b.text, settings.case_insensitive);
    default:
      return false;
  }
}

}  // namespace prop

}  // namespace hefesto::lvs
