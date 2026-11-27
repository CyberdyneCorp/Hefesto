// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The series pass (lvs-device-reduction "Series combination"). Nets are
// visited once in increasing number; a merge reconnects the surviving
// device in place, so a chain reduces in one pass. Each net keeps a list of
// its pins that is filtered lazily (an entry is live while its device is
// alive and still on the net), so a pass is linear in pins plus merges.
//
// Owner: LVSRED.

#include <utility>

#include "lvs/reduce/reduce_internal.hpp"

namespace hefesto::lvs {

namespace {

using reduce_detail::CancelPoll;

struct PinRef {
  std::uint32_t instance = 0;
  std::uint32_t pin = 0;
};

class SeriesPass {
 public:
  SeriesPass(NetlistDb& db, CellId id, const ReduceOptions& options, ReductionSummary& summary)
      : db_(db), cell_(db.cells.at(id)), summary_(summary), poll_(options.cancel) {}

  std::uint64_t run() {
    fanout_ = reduce_detail::net_fanout(cell_);
    ports_ = reduce_detail::port_nets(cell_);
    pins_.assign(cell_.nets.size() + 1, {});
    for (std::uint32_t i = 0; i < cell_.instances.size(); ++i) {
      const auto& connections = cell_.instances[i].connections;
      for (std::uint32_t p = 0; p < connections.size(); ++p) {
        if (connections[p] != kNoNet) {
          pins_[connections[p]].push_back(PinRef{i, p});
        }
      }
    }
    dead_.assign(cell_.instances.size(), false);
    removed_.assign(cell_.nets.size(), false);
    std::uint64_t merged = 0;
    for (NetId net = 1; net <= cell_.nets.size(); ++net) {
      if (poll_()) {
        summary_.cancelled = true;
        break;
      }
      if (try_merge(net)) {
        ++merged;
      }
    }
    finish(merged);
    return merged;
  }

 private:
  [[nodiscard]] bool live(const PinRef& ref, NetId net) const {
    return !dead_[ref.instance] && cell_.instances[ref.instance].connections[ref.pin] == net;
  }

  /// The two live pins of `net` (fanout 2), in list order.
  [[nodiscard]] std::pair<PinRef, PinRef> two_pins(NetId net) const {
    std::pair<PinRef, PinRef> found;
    int count = 0;
    for (const auto& ref : pins_[net]) {
      if (!live(ref, net)) {
        continue;
      }
      (count == 0 ? found.first : found.second) = ref;
      if (++count == 2) {
        break;
      }
    }
    return found;
  }

  [[nodiscard]] bool combinable(const PinRef& a, const PinRef& b) const {
    if (a.instance == b.instance || a.pin > 1 || b.pin > 1) {
      return false;
    }
    const Instance& one = cell_.instances[a.instance];
    const Instance& two = cell_.instances[b.instance];
    if (one.cell != two.cell || !db_.cells.at(one.cell).flags.series_combinable) {
      return false;
    }
    if (one.connections.size() != two.connections.size() || one.connections.size() < 2) {
      return false;
    }
    for (std::size_t i = 2; i < one.connections.size(); ++i) {
      if (one.connections[i] != two.connections[i]) {
        return false;
      }
    }
    return true;
  }

  bool try_merge(NetId net) {
    if (fanout_[net] != 2 || ports_[net]) {
      return false;
    }
    auto [a, b] = two_pins(net);
    if (!combinable(a, b)) {
      return false;
    }
    if (b.instance < a.instance) {
      std::swap(a, b);  // the survivor is the first device in instance order
    }
    Instance& first = cell_.instances[a.instance];
    Instance& second = cell_.instances[b.instance];
    const NetId far = second.connections[1 - b.pin];
    first.connections[a.pin] = far;
    if (far != kNoNet) {
      pins_[far].push_back(a);
    }
    for (std::size_t i = 2; i < second.connections.size(); ++i) {
      if (second.connections[i] != kNoNet) {
        --fanout_[second.connections[i]];
      }
    }
    fanout_[net] = 0;
    removed_[net - 1] = true;
    reduce_detail::append_records(first, second, true);
    dead_[b.instance] = true;
    return true;
  }

  void finish(std::uint64_t merged) {
    reduce_detail::erase_dead(cell_, dead_);
    if (merged == 0) {
      return;
    }
    std::vector<bool> keep(removed_.size());
    for (std::size_t i = 0; i < removed_.size(); ++i) {
      keep[i] = !removed_[i];
    }
    cell_.compact_nets(keep);
    summary_.series_merged += merged;
    summary_.log.push_back("Class " + cell_.name + " (" + reduce_detail::file_label(cell_) + "):  Merged " +
                           std::to_string(merged) + " series devices.");
  }

  NetlistDb& db_;
  Cell& cell_;
  ReductionSummary& summary_;
  CancelPoll poll_;
  std::vector<std::uint32_t> fanout_;
  std::vector<bool> ports_;
  std::vector<std::vector<PinRef>> pins_;
  std::vector<bool> dead_;
  std::vector<bool> removed_;
};

}  // namespace

std::uint64_t series_pass(NetlistDb& db, CellId cell, const ReduceOptions& options, ReductionSummary& summary) {
  return SeriesPass{db, cell, options, summary}.run();
}

}  // namespace hefesto::lvs
