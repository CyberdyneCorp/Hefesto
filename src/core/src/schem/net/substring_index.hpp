// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Does `key` occur in `text`?" for many keys against one text. Used by the
// `.subckt` header template, which checks every template key against the
// symbol's `extra` (Q-SCHEM-19); a find() per key costs O(keys x |extra|).
//
// The first kDirectQueries queries use find(), which needs no memory; after
// them a suffix array of the text is built once (prefix doubling, O(n log^2
// n) time, 12 bytes per text byte while it is built and 4 kept) and each
// later query is a binary search, O(|key| log n). So few keys cost no
// memory, and many keys cost a bounded multiple of the text instead of
// quadratic time. A text over kMaxIndexedBytes (whose positions would not
// fit the 32-bit array) is never indexed.
//
// Internal to schem/net. Owner: NETLIST.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::schem::detail {

class SubstringIndex {
 public:
  /// Queries answered with find() before the suffix array is built.
  static constexpr std::size_t kDirectQueries = 64;
  /// The longest text indexed: positions and ranks are 32-bit.
  static constexpr std::size_t kMaxIndexedBytes = UINT32_MAX / 2;

  /// `text` must outlive the index.
  explicit SubstringIndex(std::string_view text) noexcept : text_{text} {}

  /// Whether `key` is a substring of the text (the empty key always is).
  [[nodiscard]] bool contains(std::string_view key) {
    if (key.empty()) {
      return true;
    }
    if (!indexed() && (++queries_ <= kDirectQueries || text_.size() > kMaxIndexedBytes)) {
      return text_.find(key) != std::string_view::npos;
    }
    if (!indexed()) {
      build();
    }
    const auto before = [this](std::uint32_t at, std::string_view k) { return text_.substr(at, k.size()) < k; };
    const auto it = std::lower_bound(suffixes_.begin(), suffixes_.end(), key, before);
    return it != suffixes_.end() && text_.substr(*it, key.size()) == key;
  }

  /// Whether the suffix array has been built (for tests).
  [[nodiscard]] bool indexed() const noexcept { return !suffixes_.empty() || text_.empty(); }

 private:
  /// Prefix doubling: sorts the suffixes by their first 2k bytes from their
  /// ranks by k bytes, until every rank is distinct.
  void build() {
    const auto n = static_cast<std::uint32_t>(text_.size());
    suffixes_.resize(n);
    std::iota(suffixes_.begin(), suffixes_.end(), std::uint32_t{0});
    std::vector<std::uint32_t> rank(n);
    std::vector<std::uint32_t> next(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      rank[i] = static_cast<unsigned char>(text_[i]);
    }
    for (std::uint32_t k = 1;; k *= 2) {
      const auto key = [&rank, n, k](std::uint32_t i) {
        return std::pair{rank[i], i + k < n ? rank[i + k] + 1 : std::uint32_t{0}};
      };
      const auto less = [&key](std::uint32_t a, std::uint32_t b) { return key(a) < key(b); };
      std::sort(suffixes_.begin(), suffixes_.end(), less);
      next[suffixes_[0]] = 0;
      for (std::uint32_t i = 1; i < n; ++i) {
        next[suffixes_[i]] = next[suffixes_[i - 1]] + (key(suffixes_[i - 1]) < key(suffixes_[i]) ? 1 : 0);
      }
      rank.swap(next);
      if (rank[suffixes_[n - 1]] == n - 1 || k >= n) {
        return;
      }
    }
  }

  std::string_view text_;
  std::size_t queries_ = 0;
  std::vector<std::uint32_t> suffixes_;  // text positions in suffix order
};

}  // namespace hefesto::schem::detail
