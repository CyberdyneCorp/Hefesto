// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symmetry resolution and the verdict (lvs-comparison-algorithm "Symmetry
// resolution", "Deterministic arbitrary pairing"). Every stage visits the
// classes in class order and the vertices in vertex order, so the pairing is
// the same on every run and platform.
//
// Owner: LVSALG.

#include <algorithm>
#include <unordered_map>

#include "hefesto/lvs/net/names.hpp"
#include "lvs/cmp/state.hpp"

namespace hefesto::lvs {

namespace {

using Vertices = std::vector<std::uint32_t>;

std::pair<Vertices, Vertices> sides(const cmp::Partition& partition, std::size_t c) {
  std::pair<Vertices, Vertices> result;
  for (const auto v : partition.classes[c].members) {
    (v < partition.boundary ? result.first : result.second).push_back(v);
  }
  return result;
}

/// The unused vertices of `candidates` whose properties match `reference`
/// (always including `reference` itself), marked used.
Vertices take_matching(const Comparison::State& s, std::uint32_t reference, const Vertices& candidates,
                       std::vector<bool>& used) {
  Vertices group;
  for (std::size_t j = 0; j < candidates.size(); ++j) {
    if (!used[j] && (candidates[j] == reference || s.properties_match(reference, candidates[j]))) {
      used[j] = true;
      group.push_back(candidates[j]);
    }
  }
  return group;
}

/// Stage (a) for one symmetric device class: the devices whose properties
/// match the first unassigned circuit-1 device form a class of their own,
/// with equally many devices from each circuit.
bool resolve_class_by_property(Comparison::State& s, std::size_t c) {
  bool changed = false;
  const auto [one, two] = sides(s.devices, c);
  std::vector<bool> used1(one.size(), false);
  std::vector<bool> used2(two.size(), false);
  for (std::size_t i = 0; i < one.size(); ++i) {
    if (used1[i]) {
      continue;
    }
    const Vertices group1 = take_matching(s, one[i], one, used1);
    const Vertices group2 = take_matching(s, one[i], two, used2);
    const auto n = static_cast<std::ptrdiff_t>(std::min(group1.size(), group2.size()));
    Vertices moved(group1.begin(), group1.begin() + n);
    moved.insert(moved.end(), group2.begin(), group2.begin() + n);
    changed = s.devices.split_out(moved, s.rng.next()) || changed;
  }
  return changed;
}

bool resolve_by_property(Comparison::State& s) {
  bool changed = false;
  const std::size_t count = s.devices.classes.size();
  for (std::size_t c = 0; c < count; ++c) {
    if (s.devices.symmetric(c)) {
      changed = resolve_class_by_property(s, c) || changed;
    }
  }
  return changed;
}

/// Stages (b) and (c): a circuit-1 net and a circuit-2 net of one symmetric
/// class with equal names (and, for (b), at least one of them a port) form
/// a class of their own.
bool resolve_by_name(Comparison::State& s, bool ports_only) {
  bool changed = false;
  const std::size_t count = s.nets.classes.size();
  for (std::size_t c = 0; c < count; ++c) {
    if (!s.nets.symmetric(c)) {
      continue;
    }
    auto [one, two] = sides(s.nets, c);
    std::unordered_map<std::string, Vertices> by_name;
    for (const auto n : two) {
      by_name[name_key(s.net_name(n))].push_back(n);
    }
    std::vector<bool> used(s.graph.nets, false);
    for (const auto a : one) {
      const auto it = by_name.find(name_key(s.net_name(a)));
      if (it == by_name.end()) {
        continue;
      }
      for (const auto b : it->second) {
        const bool port = s.graph.net_port[a] || s.graph.net_port[b];
        if (used[b] || (ports_only && !port) || !names_equal(s.net_name(a), s.net_name(b), s.case_insensitive)) {
          continue;
        }
        used[b] = true;
        changed = s.nets.split_out({a, b}, s.rng.next()) || changed;
        break;
      }
    }
  }
  return changed;
}

/// Stage (d): the first symmetric class in class order (device classes
/// first) gives its last circuit-1 and last circuit-2 vertex a class of
/// their own.
bool pair_arbitrarily(Comparison::State& s) {
  for (std::size_t c = 0; c < s.devices.classes.size(); ++c) {
    if (s.devices.symmetric(c)) {
      auto [one, two] = sides(s.devices, c);
      return s.devices.split_out({one.back(), two.back()}, s.rng.next());
    }
  }
  for (std::size_t c = 0; c < s.nets.classes.size(); ++c) {
    if (s.nets.symmetric(c)) {
      auto [one, two] = sides(s.nets, c);
      return s.nets.split_out({one.back(), two.back()}, s.rng.next());
    }
  }
  return false;
}

/// After a stage: refine in exhaustive mode until convergence or a
/// mismatch, then verify again.
int settle(Comparison::State& s) {
  s.exhaustive = true;
  s.refine(true);
  return s.classify();
}

std::string verdict(int value, const Comparison::State& s) {
  const bool errors = s.property_state == PropertyState::errors;
  if (value == -1) {
    return "Netlists do not match.";
  }
  if (value == -2) {
    return errors ? "Netlists match uniquely with port and property errors." : "Netlists match uniquely with port errors.";
  }
  std::string text = value == 0 ? "Netlists match uniquely"
                                : "Netlists match with " + std::to_string(value) +
                                      (value == 1 ? " symmetry" : " symmetries");
  return text + (errors ? " with property errors." : ".");
}

}  // namespace

PairResult Comparison::State::resolve() {
  if (!initialized) {
    return make_result(-1);
  }
  converge();
  int value = cancelled ? -1 : classify();
  struct Stage {
    const char* message;
    bool (*run)(Comparison::State&);
  };
  static constexpr Stage kStages[] = {
      {"Resolving symmetries by property value.", [](Comparison::State& s) { return resolve_by_property(s); }},
      {"Resolving symmetries by pin name.", [](Comparison::State& s) { return resolve_by_name(s, true); }},
      {"Resolving symmetries by net name.", [](Comparison::State& s) { return resolve_by_name(s, false); }},
  };
  for (const auto& stage : kStages) {
    if (value <= 0 || cancelled) {
      break;
    }
    log.push_back(LogLine{stage.message, false});
    stage.run(*this);
    value = settle(*this);
  }
  while (value > 0 && !cancelled && pair_arbitrarily(*this)) {
    value = settle(*this);
  }
  if (cancelled) {
    return make_result(value);
  }
  log.push_back(LogLine{verdict(value, *this), false});
  if (property_state == PropertyState::errors && value != -1) {
    for (const auto& line : property_report) {
      log.push_back(LogLine{line, false});
    }
  }
  return make_result(value);
}

}  // namespace hefesto::lvs
