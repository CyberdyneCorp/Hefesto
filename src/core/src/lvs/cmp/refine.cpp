// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Initial partition, legality and iterative refinement
// (lvs-comparison-algorithm "Initial partition", "Partition legality",
// "Iterative refinement", "Long-running comparisons"; Q-LVS-4 fix).
//
// A class is split by the new keys of its vertices; within each group of
// equal keys the structural signatures (the sorted (pin label, neighbour
// class) pairs the key was hashed from) are compared, and a group whose
// signatures differ is split by signature. So a key collision never joins
// two vertices whose neighbourhoods differ (Q-LVS-4), in both modes.
//
// Owner: LVSALG.

#include <algorithm>
#include <numeric>
#include <utility>

#include "hefesto/lvs/cmp/codes.hpp"
#include "lvs/cmp/state.hpp"

namespace hefesto::lvs {

namespace cmp {

std::pair<std::size_t, std::size_t> Partition::counts(std::size_t c) const {
  const auto& members = classes[c].members;
  const auto second = static_cast<std::size_t>(
      std::count_if(members.begin(), members.end(), [this](std::uint32_t v) { return v >= boundary; }));
  return {members.size() - second, second};
}

bool Partition::legal(std::size_t c) const {
  const auto [a, b] = counts(c);
  return a == b;
}

bool Partition::matched_pair(std::size_t c) const {
  const auto [a, b] = counts(c);
  return a == 1 && b == 1;
}

bool Partition::symmetric(std::size_t c) const {
  const auto [a, b] = counts(c);
  return a == b && a > 1;
}

void Partition::rebuild_class_of() {
  for (std::uint32_t c = 0; c < classes.size(); ++c) {
    for (const auto v : classes[c].members) {
      class_of[v] = c;
    }
  }
}

bool Partition::split_out(const std::vector<std::uint32_t>& vertices, std::uint64_t label) {
  if (vertices.empty()) {
    return false;
  }
  const std::uint32_t c = class_of[vertices.front()];
  auto& members = classes[c].members;
  if (vertices.size() >= members.size()) {
    return false;
  }
  std::vector<std::uint32_t> moved = vertices;
  std::sort(moved.begin(), moved.end());
  std::vector<std::uint32_t> rest;
  std::set_difference(members.begin(), members.end(), moved.begin(), moved.end(), std::back_inserter(rest));
  members = std::move(rest);
  const auto index = static_cast<std::uint32_t>(classes.size());
  for (const auto v : moved) {
    class_of[v] = index;
  }
  classes.push_back(Class{std::move(moved), label, false});
  return true;
}

bool Partition::combine_illegal() {
  std::vector<std::uint32_t> merged;
  std::vector<Class> kept;
  for (std::size_t c = 0; c < classes.size(); ++c) {
    if (legal(c)) {
      kept.push_back(std::move(classes[c]));
      kept.back().illegal = false;
    } else {
      merged.insert(merged.end(), classes[c].members.begin(), classes[c].members.end());
    }
  }
  if (merged.empty()) {
    classes = std::move(kept);
    return false;
  }
  std::sort(merged.begin(), merged.end());
  classes.clear();
  classes.push_back(Class{std::move(merged), 0, true});
  std::move(kept.begin(), kept.end(), std::back_inserter(classes));
  rebuild_class_of();
  return true;
}

}  // namespace cmp

namespace {

using cmp::Partition;
using Signature = std::vector<std::pair<std::uint64_t, std::uint32_t>>;

/// Groups `vertices` (in vertex order) into classes by `group_key`, classes
/// ordered by their first vertex.
void build_classes(Partition& partition, const std::vector<std::uint64_t>& group_key, std::uint32_t count) {
  std::vector<std::uint32_t> order(count);
  std::iota(order.begin(), order.end(), 0U);
  std::stable_sort(order.begin(), order.end(),
                   [&group_key](std::uint32_t a, std::uint32_t b) { return group_key[a] < group_key[b]; });
  partition.classes.clear();
  for (std::size_t i = 0; i < order.size(); ++i) {
    if (i == 0 || group_key[order[i]] != group_key[order[i - 1]]) {
      partition.classes.emplace_back();
    }
    partition.classes.back().members.push_back(order[i]);
  }
  std::sort(partition.classes.begin(), partition.classes.end(),
            [](const cmp::Class& a, const cmp::Class& b) { return a.members.front() < b.members.front(); });
  partition.class_of.assign(count, 0);
  partition.rebuild_class_of();
}

/// Splits a run of equal keys by structural signature (Q-LVS-4); appends
/// the resulting groups.
template <typename SignatureOf>
void split_run(std::vector<std::uint32_t> run, const SignatureOf& signature_of,
               std::vector<std::vector<std::uint32_t>>& groups) {
  if (run.size() < 2) {
    groups.push_back(std::move(run));
    return;
  }
  std::vector<std::pair<Signature, std::uint32_t>> signed_run;
  signed_run.reserve(run.size());
  for (const auto v : run) {
    signed_run.emplace_back(signature_of(v), v);
  }
  const bool collision = std::any_of(signed_run.begin() + 1, signed_run.end(),
                                     [&](const auto& entry) { return entry.first != signed_run.front().first; });
  if (!collision) {
    groups.push_back(std::move(run));
    return;
  }
  std::sort(signed_run.begin(), signed_run.end());
  for (std::size_t i = 0; i < signed_run.size(); ++i) {
    if (i == 0 || signed_run[i].first != signed_run[i - 1].first) {
      groups.emplace_back();
    }
    groups.back().push_back(signed_run[i].second);
  }
}

template <typename SignatureOf>
void split_class(Partition& partition, std::size_t c, const std::vector<std::uint64_t>& key,
                 const SignatureOf& signature_of, cmp::SplitMix64& rng) {
  std::vector<std::uint32_t> members = partition.classes[c].members;
  std::stable_sort(members.begin(), members.end(), [&key](std::uint32_t a, std::uint32_t b) { return key[a] < key[b]; });
  std::vector<std::vector<std::uint32_t>> groups;
  std::size_t begin = 0;
  for (std::size_t i = 1; i <= members.size(); ++i) {
    if (i == members.size() || key[members[i]] != key[members[begin]]) {
      split_run({members.begin() + static_cast<std::ptrdiff_t>(begin), members.begin() + static_cast<std::ptrdiff_t>(i)},
                signature_of, groups);
      begin = i;
    }
  }
  if (groups.size() < 2) {
    return;
  }
  for (auto& group : groups) {
    std::sort(group.begin(), group.end());
  }
  std::sort(groups.begin(), groups.end(), [](const auto& a, const auto& b) { return a.front() < b.front(); });
  partition.classes[c].members = std::move(groups.front());
  for (std::size_t g = 1; g < groups.size(); ++g) {
    const auto index = static_cast<std::uint32_t>(partition.classes.size());
    for (const auto v : groups[g]) {
      partition.class_of[v] = index;
    }
    partition.classes.push_back(cmp::Class{std::move(groups[g]), rng.next(), false});
  }
}

template <typename SignatureOf>
void split_partition(Partition& partition, std::vector<std::uint64_t> key, const SignatureOf& signature_of,
                     bool exhaustive, cmp::SplitMix64& rng) {
  const std::size_t count = partition.classes.size();
  for (std::size_t c = 0; c < count; ++c) {
    if (partition.classes[c].members.size() > 1 && (exhaustive || !partition.matched_pair(c))) {
      split_class(partition, c, key, signature_of, rng);
    }
  }
  partition.key = std::move(key);
}

}  // namespace

void Comparison::State::initial_partition() {
  devices.boundary = graph.devices1;
  nets.boundary = graph.nets1;
  devices.key = graph.dev_identity;
  std::vector<std::uint64_t> kinds(graph.dev_kind.begin(), graph.dev_kind.end());
  build_classes(devices, kinds, graph.devices);
  nets.key.assign(graph.nets, 0);
  for (std::uint32_t n = 0; n < graph.nets; ++n) {
    nets.key[n] = graph.net_offset[n + 1] - graph.net_offset[n];  // fanout
  }
  build_classes(nets, nets.key, graph.nets);
  bad_match = false;
  const bool illegal_devices = devices.combine_illegal();
  const bool illegal_nets = nets.combine_illegal();
  bad_match = illegal_devices || illegal_nets;
}

namespace {

/// "Device keys": previous key XOR the sum of (pin label XOR net-class label).
std::vector<std::uint64_t> device_keys(const cmp::Graph& graph, const Partition& devices, const Partition& nets,
                                       std::uint64_t mask) {
  std::vector<std::uint64_t> keys(graph.devices);
  for (std::uint32_t d = 0; d < graph.devices; ++d) {
    std::uint64_t sum = 0;
    for (std::uint32_t p = graph.dev_offset[d]; p < graph.dev_offset[d + 1]; ++p) {
      if (graph.dev_net[p] != cmp::kNoVertex) {
        sum += graph.dev_label[p] ^ nets.classes[nets.class_of[graph.dev_net[p]]].label;
      }
    }
    keys[d] = (devices.key[d] ^ sum) & mask;
  }
  return keys;
}

/// "Net keys": the sum of (pin label XOR new device key XOR device-class label).
std::vector<std::uint64_t> net_keys(const cmp::Graph& graph, const Partition& devices, std::uint64_t mask) {
  std::vector<std::uint64_t> keys(graph.nets, 0);
  for (std::uint32_t n = 0; n < graph.nets; ++n) {
    for (std::uint32_t p = graph.net_offset[n]; p < graph.net_offset[n + 1]; ++p) {
      const auto d = graph.net_dev[p];
      keys[n] += graph.net_label[p] ^ devices.key[d] ^ devices.classes[devices.class_of[d]].label;
    }
    keys[n] &= mask;
  }
  return keys;
}

Signature device_signature(const cmp::Graph& graph, const Partition& nets, std::uint32_t d) {
  Signature signature;
  for (std::uint32_t p = graph.dev_offset[d]; p < graph.dev_offset[d + 1]; ++p) {
    if (graph.dev_net[p] != cmp::kNoVertex) {
      signature.emplace_back(graph.dev_label[p], nets.class_of[graph.dev_net[p]]);
    }
  }
  std::sort(signature.begin(), signature.end());
  return signature;
}

Signature net_signature(const cmp::Graph& graph, const Partition& devices, std::uint32_t n) {
  Signature signature;
  for (std::uint32_t p = graph.net_offset[n]; p < graph.net_offset[n + 1]; ++p) {
    signature.emplace_back(graph.net_label[p], devices.class_of[graph.net_dev[p]]);
  }
  std::sort(signature.begin(), signature.end());
  return signature;
}

void relabel(Partition& partition, cmp::SplitMix64& rng) {
  for (auto& c : partition.classes) {
    c.label = rng.next();
  }
}

}  // namespace

bool Comparison::State::round() {
  const auto before = std::make_pair(devices.classes.size(), nets.classes.size());
  relabel(devices, rng);
  relabel(nets, rng);
  split_partition(
      devices, device_keys(graph, devices, nets, options.key_mask),
      [this](std::uint32_t d) { return device_signature(graph, nets, d); }, exhaustive, rng);
  split_partition(
      nets, net_keys(graph, devices, options.key_mask),
      [this](std::uint32_t n) { return net_signature(graph, devices, n); }, exhaustive, rng);
  const bool illegal_devices = devices.combine_illegal();
  const bool illegal_nets = nets.combine_illegal();
  bad_match = bad_match || illegal_devices || illegal_nets;
  return std::make_pair(devices.classes.size(), nets.classes.size()) == before;
}

bool Comparison::State::refine(bool stop_on_mismatch) {
  if (!initialized) {
    return false;
  }
  // A guard against a partition that cycles: every non-converging round
  // that does not cycle adds a class, so 2 x vertices rounds are plenty.
  const std::size_t limit = 2U * (static_cast<std::size_t>(graph.devices) + graph.nets) + 16U;
  for (std::size_t rounds = 0;; ++rounds) {
    if (options.cancel.requested()) {
      cancelled = true;
      diagnostics.push_back(diag::make(cmp_codes::kCancelled, "LVS comparison cancelled between refinement rounds"));
      return false;
    }
    if (round() || (stop_on_mismatch && bad_match)) {
      return true;
    }
    if (rounds >= limit) {
      if (!round_limit_reported) {
        round_limit_reported = true;
        diagnostics.push_back(diag::make(cmp_codes::kRoundLimit, "refinement stopped after " +
                                                                    std::to_string(rounds + 1) + " rounds"));
      }
      return true;
    }
  }
}

void Comparison::State::converge() {
  exhaustive = false;
  if (!refine(false)) {
    return;
  }
  exhaustive = true;
  refine(false);
}

}  // namespace hefesto::lvs
