// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Helpers shared by the reduction passes.
//
// Owner: LVSRED.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/reduce/reduce.hpp"

namespace hefesto::lvs::reduce_detail {

/// The cancel poll interval of "Reduction cost".
inline constexpr std::size_t kCancelInterval = 65536;

/// The record a device without records contributes to a merge: {M = 1}.
[[nodiscard]] PropertyRecord unit_record();

/// Appends `from`'s records (or the unit record) and names to `into`.
void append_records(Instance& into, Instance& from, bool series);

/// Device pins per net (index NetId), counting every connection.
[[nodiscard]] std::vector<std::uint32_t> net_fanout(const Cell& cell);

/// Whether each net (index NetId) is a port's net.
[[nodiscard]] std::vector<bool> port_nets(const Cell& cell);

/// Erases the instances flagged dead, keeping the order of the rest.
void erase_dead(Cell& cell, const std::vector<bool>& dead);

/// The file number as printed in the messages.
[[nodiscard]] std::string file_label(const Cell& cell);

/// Polls `cancel` every kCancelInterval calls; true once cancelled.
class CancelPoll {
 public:
  explicit CancelPoll(const CancelToken& cancel) : cancel_(cancel) {}
  bool operator()() {
    if (++count_ % kCancelInterval == 0 && cancel_.requested()) {
      cancelled_ = true;
    }
    return cancelled_;
  }
  [[nodiscard]] bool cancelled() const noexcept { return cancelled_; }

 private:
  const CancelToken& cancel_;
  std::size_t count_ = 0;
  bool cancelled_ = false;
};

}  // namespace hefesto::lvs::reduce_detail
