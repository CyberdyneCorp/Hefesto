// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MagicHeap (utils/heap.c, smallest on top, integer keys) and StableQueue.
// (Magic ref: utils/heap.c:114-460)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <algorithm>

#include "extresist/network/heap.hpp"

namespace hefesto::extresist::detail {

void MagicHeap::heapify(std::int32_t root) {
  while (true) {
    std::int32_t x = used_ < (root << 1) ? 0 : root + root;
    if (x == 0) {
      break;
    }
    const std::int32_t r = used_ < ((root << 1) + 1) ? 0 : root + root + 1;
    if (r != 0 && at(x).key > at(r).key) {
      x = r;
    }
    if (at(x).key >= at(root).key) {
      return;
    }
    swap(root, x);
    root = x;
  }
}

void MagicHeap::add(std::int32_t key, std::uint32_t id) {
  std::int32_t i = ++used_;
  if (static_cast<std::size_t>(i) >= list_.size()) {
    list_.resize(static_cast<std::size_t>(i) + 1);
  }
  at(i) = Entry{key, id};
  if (built_ == 0) {
    return;
  }
  while (true) {
    std::int32_t cmp = i;
    if ((i & 1) != 0 && i != 1 && at(i).key >= at(i - 1).key) {
      cmp = i - 1;
    }
    if ((i >>= 1) == 0) {
      return;
    }
    if (at(cmp).key >= at(i).key) {
      return;
    }
    swap(cmp, i);
    heapify(cmp);
  }
}

std::optional<std::uint32_t> MagicHeap::remove_top() {
  if (used_ == 0) {
    return std::nullopt;
  }
  if (built_ == 0) {
    for (std::int32_t i = used_; i > 0; --i) {
      heapify(i);
    }
  }
  built_ = used_;
  const std::uint32_t top = at(1).id;
  at(1) = at(used_);
  --used_;
  heapify(1);
  return top;
}

std::vector<std::int32_t> MagicHeap::keys() const {
  std::vector<std::int32_t> out;
  for (std::int32_t i = 1; i <= used_; ++i) {
    out.push_back(at(i).key);
  }
  return out;
}

namespace {

bool later(const auto& a, const auto& b) {
  return a.key > b.key || (a.key == b.key && a.seq > b.seq);
}

}  // namespace

void StableQueue::add(double key, std::uint32_t id) {
  heap_.push_back(Entry{key, seq_++, id});
  std::push_heap(heap_.begin(), heap_.end(), [](const Entry& a, const Entry& b) { return later(a, b); });
}

std::optional<std::uint32_t> StableQueue::remove_top() {
  if (heap_.empty()) {
    return std::nullopt;
  }
  std::pop_heap(heap_.begin(), heap_.end(), [](const Entry& a, const Entry& b) { return later(a, b); });
  const std::uint32_t id = heap_.back().id;
  heap_.pop_back();
  return id;
}

}  // namespace hefesto::extresist::detail
