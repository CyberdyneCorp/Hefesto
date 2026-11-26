// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Series/parallel network reconciliation (lvs-property-matching
// "Series/parallel network reconciliation", "Strict and relaxed topology").
// Sorting uses a three-way comparator on exact values and std::stable_sort,
// so the order is one total preorder on every platform (Q-LVS-8 fix).
//
// Owner: LVSPROP.

#include <algorithm>
#include <cstddef>

#include "lvs/prop/records.hpp"

namespace hefesto::lvs::prop {

namespace {

struct Direction {
  std::size_t count = Keys::kM;  // the multiplier summed by a merge
  std::size_t other = Keys::kS;  // the multiplier strict topology guards
  std::uint16_t add = merge_rule::kParallelAdd;
  std::uint16_t par = merge_rule::kParallelPar;
  std::uint16_t critical = merge_rule::kParallelCritical | merge_rule::kLegacyParallelCritical;
};

Direction direction(bool series) {
  if (!series) {
    return {};
  }
  return Direction{Keys::kS, Keys::kM, merge_rule::kSeriesAdd, merge_rule::kSeriesPar,
                   merge_rule::kSeriesCritical | merge_rule::kLegacySeriesCritical};
}

bool is_count(std::size_t key) noexcept {
  return key == Keys::kM || key == Keys::kS;
}

/// The maximal run of records joined in parallel (each record after the
/// first has no marker) or in series (each has one); the first longest.
std::pair<std::size_t, std::size_t> longest_group(const Network& network, bool series) {
  std::pair<std::size_t, std::size_t> best{0, 0};
  std::size_t begin = 0;
  for (std::size_t i = 1; i <= network.size(); ++i) {
    const bool joins = i < network.size() && (network[i].tag.empty() != series);
    if (joins) {
      continue;
    }
    if (i - begin > best.second - best.first) {
      best = {begin, i};
    }
    begin = i;
  }
  return best;
}

int compare_numbers(double a, double b) noexcept {
  return a < b ? -1 : (b < a ? 1 : 0);
}

/// Three-way order of two values: missing first, then numbers, then text.
int compare_values(const Value& a, const Value& b) {
  if (a.present != b.present) {
    return a.present ? 1 : -1;
  }
  if (a.numeric() != b.numeric()) {
    return a.numeric() ? -1 : 1;
  }
  if (a.numeric()) {
    return compare_numbers(a.as_double(), b.as_double());
  }
  return a.text.compare(b.text) < 0 ? -1 : (a.text == b.text ? 0 : 1);
}

/// The sort keys of "Sort rule" for one record.
struct SortKeys {
  Value primary;
  Value secondary;
};

std::optional<std::size_t> first_key_with(const Keys& keys, std::uint16_t bits) {
  for (std::size_t i = 2; i < keys.list.size(); ++i) {
    if ((keys.list[i].merge & bits) != 0) {
      return i;
    }
  }
  return std::nullopt;
}

Value scaled(const Value& value, std::int64_t multiplier, bool divide) {
  if (!value.numeric()) {
    return value;
  }
  Value result = value;
  result.type = PropertyType::real;
  const auto m = static_cast<double>(multiplier);
  result.number = divide ? value.as_double() / m : value.as_double() * m;
  return result;
}

Value count_value(std::int64_t count) {
  Value value;
  value.present = true;
  value.type = PropertyType::integer;
  value.integer = count;
  return value;
}

SortKeys sort_keys(const Record& record, const Keys& keys, const Direction& dir) {
  const auto critical = first_key_with(keys, dir.critical);
  const auto additive = first_key_with(keys, static_cast<std::uint16_t>(dir.add | dir.par));
  const std::int64_t m = count_of(record, dir.count);
  if (critical) {
    if (!additive) {
      return {record.values[*critical], count_value(m)};
    }
    const bool par = (keys.list[*additive].merge & dir.par) != 0;
    return {record.values[*critical], scaled(record.values[*additive], m, par)};
  }
  if (additive) {
    return {record.values[*additive], count_value(m)};
  }
  Value other;
  if (keys.list.size() > 2) {
    other = record.values[2];
  }
  return {count_value(m), other};
}

void sort_group(Network& network, std::pair<std::size_t, std::size_t> group, const Keys& keys,
                const Direction& dir) {
  const auto begin = network.begin() + static_cast<std::ptrdiff_t>(group.first);
  const auto end = network.begin() + static_cast<std::ptrdiff_t>(group.second);
  std::stable_sort(begin, end, [&](const Record& a, const Record& b) {
    const SortKeys ka = sort_keys(a, keys, dir);
    const SortKeys kb = sort_keys(b, keys, dir);
    const int primary = compare_values(ka.primary, kb.primary);
    return primary != 0 ? primary < 0 : compare_values(ka.secondary, kb.secondary) < 0;
  });
}

bool same_value(const Key& key, const Value& a, const Value& b, const PropertySettings& settings) {
  if (!a.present || !b.present) {
    return a.present == b.present;
  }
  return values_match(key, a, b, settings);
}

/// Whether b can merge into a by summing the direction's multiplier.
bool mergeable(const Record& a, const Record& b, const Keys& keys, const Direction& dir,
               const PropertySettings& settings) {
  if (settings.strict_topology && (count_of(a, dir.other) > 1 || count_of(b, dir.other) > 1)) {
    return false;  // the merge would need M > 1 and S > 1 together
  }
  for (std::size_t i = 0; i < keys.list.size(); ++i) {
    if (i != dir.count && !same_value(keys.list[i], a.values[i], b.values[i], settings)) {
      return false;
    }
  }
  return true;
}

bool merge_step(Network& network, const Keys& keys, bool series, const PropertySettings& settings) {
  const auto group = longest_group(network, series);
  if (group.second - group.first < 2) {
    return false;
  }
  const Direction dir = direction(series);
  sort_group(network, group, keys, dir);
  bool changed = false;
  std::size_t keep = group.first;
  std::vector<bool> removed(network.size(), false);
  for (std::size_t j = group.first + 1; j < group.second; ++j) {
    if (mergeable(network[keep], network[j], keys, dir, settings)) {
      set_count(network[keep], dir.count, count_of(network[keep], dir.count) + count_of(network[j], dir.count));
      removed[j] = true;
      changed = true;
    } else {
      keep = j;
    }
  }
  // Compact without moving a record onto itself (a self-move empties it).
  std::size_t out = 0;
  for (std::size_t i = 0; i < network.size(); ++i) {
    if (removed[i]) {
      continue;
    }
    if (out != i) {
      network[out] = std::move(network[i]);
    }
    ++out;
  }
  network.resize(out);
  return changed;
}

bool foldable(const Key& key, const Direction& dir) {
  return (key.merge & (dir.add | dir.par)) != 0 && (key.merge & dir.critical) == 0;
}

Value combine(const Value& a, const Value& b, bool par) {
  if (!a.numeric() || !b.numeric()) {
    return a;
  }
  Value result = a;
  result.type = PropertyType::real;
  const double x = a.as_double();
  const double y = b.as_double();
  result.number = par ? x * y / (x + y) : x + y;
  return result;
}

bool collapsible(const Record& a, const Record& b, const Keys& keys, const Direction& dir,
                 const PropertySettings& settings) {
  for (std::size_t i = 0; i < keys.list.size(); ++i) {
    if (!is_count(i) && !foldable(keys.list[i], dir) &&
        !same_value(keys.list[i], a.values[i], b.values[i], settings)) {
      return false;
    }
  }
  return true;
}

}  // namespace

void fold(Network& network, const Keys& keys, bool series, const PropertySettings& settings) {
  const Direction dir = direction(series);
  for (auto& record : network) {
    // A record with no additive value to absorb the multiplier keeps it,
    // so an M (or S) difference stays a difference (Hefesto choice where
    // the spec is silent).
    bool absorbed = false;
    const std::int64_t m = count_of(record, dir.count);
    for (std::size_t i = 2; i < keys.list.size(); ++i) {
      if (foldable(keys.list[i], dir) && record.values[i].numeric()) {
        record.values[i] = scaled(record.values[i], m, (keys.list[i].merge & dir.par) != 0);
        absorbed = true;
      }
    }
    if (absorbed) {
      set_count(record, dir.count, 1);
    }
  }
  Network out;
  for (auto& record : network) {
    if (out.empty() || !collapsible(out.back(), record, keys, dir, settings)) {
      out.push_back(std::move(record));
      continue;
    }
    for (std::size_t i = 2; i < keys.list.size(); ++i) {
      if (foldable(keys.list[i], dir)) {
        out.back().values[i] = combine(out.back().values[i], record.values[i], (keys.list[i].merge & dir.par) != 0);
      }
    }
  }
  network = std::move(out);
}

std::size_t reconcile(Network& first, Network& second, const Keys& keys, const PropertySettings& settings) {
  apply_defaults(first, keys);
  apply_defaults(second, keys);
  std::size_t rounds = 0;
  for (;;) {
    ++rounds;
    // A leading marker encloses nothing.
    for (Network* network : {&first, &second}) {
      if (!network->empty()) {
        network->front().tag.clear();
      }
    }
    if (first.size() < 2 && second.size() < 2) {
      break;
    }
    const bool parallel1 = merge_step(first, keys, false, settings);
    const bool parallel2 = merge_step(second, keys, false, settings);
    if (parallel1 || parallel2) {
      continue;
    }
    const bool series1 = merge_step(first, keys, true, settings);
    const bool series2 = merge_step(second, keys, true, settings);
    if (!series1 && !series2) {
      break;
    }
  }
  return rounds;
}

}  // namespace hefesto::lvs::prop
