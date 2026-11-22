// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Does `key` occur in `text`?" for many keys against one text, in time
// linear in the text plus the keys: a suffix automaton of the text (built
// once in O(n log 256), queried in O(|key| log 256)). Used by the `.subckt`
// header template, which checks every template key against the symbol's
// `extra` (Q-SCHEM-19); a find() per key costs O(keys x |extra|).
//
// Internal to schem/net. Owner: NETLIST.

#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::schem::detail {

class SubstringIndex {
 public:
  explicit SubstringIndex(std::string_view text) {
    states_.reserve(2 * text.size() + 1);
    states_.emplace_back();
    for (const char c : text) {
      extend(c);
    }
  }

  /// Whether `key` is a substring of the text (the empty key always is).
  [[nodiscard]] bool contains(std::string_view key) const {
    std::uint32_t state = 0;
    for (const char c : key) {
      const auto next = find(state, c);
      if (!next) {
        return false;
      }
      state = *next;
    }
    return true;
  }

 private:
  static constexpr std::uint32_t kNone = UINT32_MAX;

  struct State {
    std::uint32_t length = 0;
    std::uint32_t link = kNone;
    std::vector<std::pair<char, std::uint32_t>> next;  // sorted by character
  };

  [[nodiscard]] std::optional<std::uint32_t> find(std::uint32_t state, char c) const {
    const auto& next = states_[state].next;
    const auto it = std::lower_bound(next.begin(), next.end(), c, [](const auto& e, char k) { return e.first < k; });
    if (it == next.end() || it->first != c) {
      return std::nullopt;
    }
    return it->second;
  }

  void set(std::uint32_t state, char c, std::uint32_t target) {
    auto& next = states_[state].next;
    const auto it = std::lower_bound(next.begin(), next.end(), c, [](const auto& e, char k) { return e.first < k; });
    if (it != next.end() && it->first == c) {
      it->second = target;
    } else {
      next.insert(it, {c, target});
    }
  }

  /// The standard online construction: adds `c` to the end of the text.
  void extend(char c) {
    const auto current = static_cast<std::uint32_t>(states_.size());
    states_.push_back(State{states_[last_].length + 1, kNone, {}});
    std::uint32_t p = last_;
    while (p != kNone && !find(p, c)) {
      set(p, c, current);
      p = states_[p].link;
    }
    last_ = current;
    if (p == kNone) {
      states_[current].link = 0;
      return;
    }
    const std::uint32_t q = *find(p, c);
    if (states_[p].length + 1 == states_[q].length) {
      states_[current].link = q;
      return;
    }
    split(p, q, c, current);
  }

  /// Clones `q` for the shorter suffixes that reach it from `p` by `c`.
  void split(std::uint32_t p, std::uint32_t q, char c, std::uint32_t current) {
    const auto clone = static_cast<std::uint32_t>(states_.size());
    State copy{states_[p].length + 1, states_[q].link, states_[q].next};
    states_.push_back(std::move(copy));
    while (p != kNone && find(p, c) == q) {
      set(p, c, clone);
      p = states_[p].link;
    }
    states_[q].link = clone;
    states_[current].link = clone;
  }

  std::vector<State> states_;
  std::uint32_t last_ = 0;
};

}  // namespace hefesto::schem::detail
