// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The parallel pass (lvs-device-reduction "Parallel combination",
// "Permutable pins in matching"; Q-LVS-9, Q-LVS-26). Each device gets a
// canonical connection signature; sorting the devices by (class, signature,
// instance order) puts every parallel group together, so a pass is
// O(n log n) and the survivor of a group is its first device in instance
// order.
//
// Owner: LVSRED.

#include <algorithm>
#include <numeric>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>

#include "hefesto/lvs/net/permute.hpp"
#include "hefesto/lvs/reduce/codes.hpp"
#include "lvs/reduce/reduce_internal.hpp"

namespace hefesto::lvs {

namespace {

using reduce_detail::CancelPoll;

/// A no-connect net in a signature ("every no-connect net SHALL count as one
/// and the same net").
constexpr NetId kOpenNet = 0xFFFF'FFFFU;

/// How a class canonicalizes the pins of its devices.
struct ClassRule {
  bool allowed = false;
  /// Q-LVS-9 ON: the first declared pair; the signature is the smaller of
  /// the plain and the exchanged order.
  std::optional<std::pair<std::uint32_t, std::uint32_t>> first_pair;
  /// Q-LVS-9 OFF: the permutation groups; the nets inside each group are
  /// sorted, so every permutation-equivalent order gives one signature.
  std::vector<std::vector<std::uint32_t>> groups;
};

/// Q-LVS-9 (Resolution: compat): the one branch point of the alternate
/// parallel signature. ON keeps netgen's first-pair-only rule; OFF uses
/// every permutation group.
ClassRule class_rule(const Cell& klass, bool netgen_compat) {
  ClassRule rule;
  rule.allowed = !klass.flags.no_parallel;
  const auto pairs = permute_pairs(klass);
  if (pairs.pairs.empty()) {
    return rule;
  }
  if (netgen_compat) {
    rule.first_pair = pairs.pairs.front();
    return rule;
  }
  const auto roots = permute_groups(klass);
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> members;
  for (std::uint32_t i = 0; i < roots.size(); ++i) {
    members[roots[i]].push_back(i);
  }
  for (std::uint32_t i = 0; i < roots.size(); ++i) {
    if (roots[i] == i && members[i].size() > 1) {
      rule.groups.push_back(members[i]);
    }
  }
  return rule;
}

void canonicalize(std::span<NetId> sig, const ClassRule& rule) {
  if (rule.first_pair) {
    const auto [a, b] = *rule.first_pair;
    if (a < sig.size() && b < sig.size()) {
      std::vector<NetId> swapped(sig.begin(), sig.end());
      std::swap(swapped[a], swapped[b]);
      if (std::lexicographical_compare(swapped.begin(), swapped.end(), sig.begin(), sig.end())) {
        std::copy(swapped.begin(), swapped.end(), sig.begin());
      }
    }
    return;
  }
  for (const auto& group : rule.groups) {
    std::vector<NetId> nets;
    for (const auto pin : group) {
      if (pin < sig.size()) {
        nets.push_back(sig[pin]);
      }
    }
    std::sort(nets.begin(), nets.end());
    for (std::size_t k = 0; k < nets.size(); ++k) {
      sig[group[k]] = nets[k];
    }
  }
}

class ParallelPass {
 public:
  ParallelPass(NetlistDb& db, CellId id, const ReduceOptions& options, ReductionSummary& summary)
      : db_(db), cell_(db.cells.at(id)), options_(options), summary_(summary), poll_(options.cancel) {}

  std::uint64_t run() {
    build_signatures();
    if (poll_.cancelled()) {
      summary_.cancelled = true;
      return 0;
    }
    std::sort(order_.begin(), order_.end(), [this](std::uint32_t a, std::uint32_t b) { return less(a, b); });
    std::uint64_t merged = 0;
    std::vector<bool> dead(cell_.instances.size(), false);
    for (std::size_t begin = 0; begin < order_.size();) {
      std::size_t end = begin + 1;
      while (end < order_.size() && same(order_[begin], order_[end])) {
        ++end;
      }
      if (end - begin > 1) {
        merge_group(std::span<const std::uint32_t>{order_}.subspan(begin, end - begin), dead);
        merged += end - begin - 1;
      }
      begin = end;
    }
    reduce_detail::erase_dead(cell_, dead);
    if (merged > 0) {
      summary_.parallel_merged += merged;
      summary_.log.push_back("Class " + cell_.name + " (" + reduce_detail::file_label(cell_) + "):  Merged " +
                             std::to_string(merged) + " parallel devices.");
    }
    return merged;
  }

 private:
  const ClassRule& rule_for(CellId klass) {
    auto [it, added] = rules_.try_emplace(klass);
    if (added) {
      const Cell& class_cell = db_.cells.at(klass);
      it->second = class_rule(class_cell, options_.netgen_compat);
      const auto missing = permute_pairs(class_cell).missing;
      if (missing) {
        const std::string text = "Class " + class_cell.name + " does not have pin " + *missing + ".";
        summary_.log.push_back(text);
        summary_.diagnostics.push_back(diag::make(reduce_codes::kMissingPin, text));
      }
    }
    return it->second;
  }

  void build_signatures() {
    fanout_ = reduce_detail::net_fanout(cell_);
    const auto ports = reduce_detail::port_nets(cell_);
    // Q-LVS-26 (replicate): no-connects are one net in "open" mode, outside
    // top cells; `property parallel connected` turns it off. A port's net is
    // never a no-connect (Hefesto choice where the spec is silent).
    const bool open = options_.parallel_open && !cell_.flags.top;
    offsets_.assign(cell_.instances.size() + 1, 0);
    for (std::uint32_t i = 0; i < cell_.instances.size(); ++i) {
      const Instance& instance = cell_.instances[i];
      offsets_[i + 1] = offsets_[i];
      if (poll_() || !rule_for(instance.cell).allowed) {
        continue;
      }
      const auto start = signatures_.size();
      for (const NetId net : instance.connections) {
        const bool no_connect = open && net != kNoNet && fanout_[net] == 1 && !ports[net];
        signatures_.push_back(no_connect ? kOpenNet : net);
      }
      canonicalize(std::span<NetId>{signatures_}.subspan(start), rule_for(instance.cell));
      offsets_[i + 1] = static_cast<std::uint32_t>(signatures_.size());
      order_.push_back(i);
    }
  }

  [[nodiscard]] std::span<const NetId> signature(std::uint32_t i) const {
    return std::span<const NetId>{signatures_}.subspan(offsets_[i], offsets_[i + 1] - offsets_[i]);
  }

  [[nodiscard]] bool less(std::uint32_t a, std::uint32_t b) const {
    const CellId ca = cell_.instances[a].cell;
    const CellId cb = cell_.instances[b].cell;
    if (ca != cb) {
      return ca < cb;
    }
    const auto sa = signature(a);
    const auto sb = signature(b);
    if (!std::equal(sa.begin(), sa.end(), sb.begin(), sb.end())) {
      return std::lexicographical_compare(sa.begin(), sa.end(), sb.begin(), sb.end());
    }
    return a < b;
  }

  [[nodiscard]] bool same(std::uint32_t a, std::uint32_t b) const {
    const auto sa = signature(a);
    const auto sb = signature(b);
    return cell_.instances[a].cell == cell_.instances[b].cell && std::equal(sa.begin(), sa.end(), sb.begin(), sb.end());
  }

  void merge_group(std::span<const std::uint32_t> group, std::vector<bool>& dead) {
    Instance& survivor = cell_.instances[group.front()];
    const auto sig = signature(group.front());
    survivor.open_pins.resize(survivor.connections.size(), OpenPin::none);
    for (std::size_t pin = 0; pin < survivor.connections.size(); ++pin) {
      const NetId net = survivor.connections[pin];
      const bool open = pin < sig.size() && sig[pin] == kOpenNet;
      if (open) {
        survivor.open_pins[pin] = OpenPin::no_connects;
      } else if (net != kNoNet && fanout_[net] == group.size()) {
        survivor.open_pins[pin] = OpenPin::tied_together;
      }
    }
    for (const auto index : group.subspan(1)) {
      reduce_detail::append_records(survivor, cell_.instances[index], false);
      dead[index] = true;
    }
  }

  NetlistDb& db_;
  Cell& cell_;
  const ReduceOptions& options_;
  ReductionSummary& summary_;
  CancelPoll poll_;
  std::unordered_map<CellId, ClassRule> rules_;
  std::vector<std::uint32_t> fanout_;
  std::vector<NetId> signatures_;
  std::vector<std::uint32_t> offsets_;
  std::vector<std::uint32_t> order_;
};

}  // namespace

std::uint64_t parallel_pass(NetlistDb& db, CellId cell, const ReduceOptions& options, ReductionSummary& summary) {
  return ParallelPass{db, cell, options, summary}.run();
}

}  // namespace hefesto::lvs
