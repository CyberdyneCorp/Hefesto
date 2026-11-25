// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The path-resistance priority queues (capability extresist-network "Path
// resistance and capacitance distribution"). MagicHeap is utils/heap.c's
// min-heap with integer keys, including the "built" state that makes the
// first net of a session insert unbuilt (Q-NETLIST-25, Q-NETLIST-46,
// magic-compat ON). StableQueue is the OFF mode's per-net queue with double
// keys and insertion-order ties.
// (Magic ref: utils/heap.c:114-460)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace hefesto::extresist::detail {

class MagicHeap {
 public:
  /// `built` is he_built as the session left it.
  explicit MagicHeap(std::int32_t built) : built_(built) { list_.resize(1); }

  void add(std::int32_t key, std::uint32_t id);  // HeapAddInt
  std::optional<std::uint32_t> remove_top();     // HeapRemoveTop
  [[nodiscard]] std::int32_t built() const noexcept { return built_; }

  /// The keys in array order (positions 1..used), for tests.
  [[nodiscard]] std::vector<std::int32_t> keys() const;

 private:
  struct Entry {
    std::int32_t key = 0;
    std::uint32_t id = 0;
  };
  void heapify(std::int32_t root);
  void swap(std::int32_t a, std::int32_t b) { std::swap(at(a), at(b)); }
  Entry& at(std::int32_t i) { return list_[static_cast<std::size_t>(i)]; }
  [[nodiscard]] const Entry& at(std::int32_t i) const { return list_[static_cast<std::size_t>(i)]; }

  std::vector<Entry> list_;  // 1-based; position 0 unused
  std::int32_t used_ = 0;
  std::int32_t built_ = 0;
};

class StableQueue {
 public:
  void add(double key, std::uint32_t id);
  std::optional<std::uint32_t> remove_top();

 private:
  struct Entry {
    double key = 0.0;
    std::uint64_t seq = 0;
    std::uint32_t id = 0;
  };
  std::vector<Entry> heap_;
  std::uint64_t seq_ = 0;
};

}  // namespace hefesto::extresist::detail
