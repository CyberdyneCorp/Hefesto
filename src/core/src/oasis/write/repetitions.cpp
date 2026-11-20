// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shape repetition detection (oasis-write "Shape repetition detection",
// Q-OASIS-67): the deterministic grouping of the spec, over the origins of
// one key (rows of equal pitch, then columns of equal rows), and over the
// candidates of one mask layer (groups by key in order of first
// occurrence, written sorted by origin and group).
// Owner: OASW.

#include "hefesto/oasis/write/repetitions.hpp"

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>

#include "internal.hpp"

namespace hefesto::oasis {

namespace {

/// A run (p, d, k): k origins from p, d apart along x.
struct Run {
  Point64 origin;
  std::int64_t step = 0;
  std::uint64_t count = 1;
};

using YX = std::pair<std::int64_t, std::int64_t>;

/// The origins still to place, sorted by (y, x), with their multiplicity.
class Pending {
 public:
  explicit Pending(std::span<const Point64> origins) {
    for (const Point64& p : origins) {
      ++left_[YX{p.y, p.x}];
    }
  }

  [[nodiscard]] bool empty() const noexcept { return left_.empty(); }

  Point64 take_first() {
    const YX first = left_.begin()->first;
    remove(first);
    return Point64{first.second, first.first};
  }

  /// The next origin, if it is on row `y`.
  [[nodiscard]] std::optional<Point64> next_on_row(std::int64_t y) const {
    if (left_.empty() || left_.begin()->first.first != y) {
      return std::nullopt;
    }
    return Point64{left_.begin()->first.second, y};
  }

  /// Removes `p` when it is pending.
  bool take(Point64 p) {
    const auto it = left_.find(YX{p.y, p.x});
    if (it == left_.end()) {
      return false;
    }
    remove(it->first);
    return true;
  }

 private:
  void remove(const YX& key) {
    const auto it = left_.find(key);
    if (--it->second == 0) {
      left_.erase(it);
    }
  }

  std::map<YX, std::uint64_t> left_;
};

std::vector<Run> row_runs(std::span<const Point64> origins) {
  Pending pending(origins);
  std::vector<Run> runs;
  while (!pending.empty()) {
    Run run;
    run.origin = pending.take_first();
    if (const std::optional<Point64> q = pending.next_on_row(run.origin.y)) {
      run.step = q->x - run.origin.x;
      while (pending.take(Point64{run.origin.x + static_cast<std::int64_t>(run.count) * run.step, run.origin.y})) {
        ++run.count;
      }
    }
    runs.push_back(run);
  }
  std::ranges::sort(runs, [](const Run& a, const Run& b) {
    return std::tuple{a.origin.x, a.step, a.count, a.origin.y} < std::tuple{b.origin.x, b.step, b.count, b.origin.y};
  });
  return runs;
}

bool same_columns(const Run& a, const Run& b) {
  return a.origin.x == b.origin.x && a.step == b.step && a.count == b.count;
}

std::optional<Repetition> repetition(const Run& run, std::uint64_t rows, std::int64_t row_step) {
  if (run.count < 2 && rows < 2) {
    return std::nullopt;
  }
  Repetition r;
  r.columns = run.count;
  r.rows = rows;
  if (run.count >= 2) {
    r.column_step = {run.step, 0};
  }
  if (rows >= 2) {
    r.row_step = {0, row_step};
  }
  r.type = run.count >= 2 ? (rows >= 2 ? 1 : 2) : 3;
  return r;
}

}  // namespace

std::vector<RepetitionGroup> detect_repetitions(std::span<const Point64> origins) {
  const std::vector<Run> runs = row_runs(origins);
  std::vector<RepetitionGroup> out;
  std::size_t i = 0;
  while (i < runs.size()) {
    const Run& run = runs[i];
    std::uint64_t m = 1;
    std::int64_t e = 0;
    if (i + 1 < runs.size() && same_columns(run, runs[i + 1])) {
      e = runs[i + 1].origin.y - run.origin.y;
      m = 2;
      while (i + m < runs.size() && same_columns(run, runs[i + m]) &&
             runs[i + m].origin.y == run.origin.y + static_cast<std::int64_t>(m) * e) {
        ++m;
      }
    }
    out.push_back(RepetitionGroup{run.origin, repetition(run, m, e)});
    i += m;
  }
  return out;
}

namespace detail {

namespace {

/// The key of a candidate: kind, RECTANGLE size, POLYGON point list.
using Key = std::tuple<int, std::int64_t, std::int64_t, std::vector<YX>>;

Key key_of(const Element& e) {
  std::vector<YX> points;
  points.reserve(e.points.size());
  for (const Point64& p : e.points) {
    points.emplace_back(p.x, p.y);
  }
  return {static_cast<int>(e.kind), e.size.x, e.size.y, std::move(points)};
}

struct Placed {
  Element element;
  std::size_t group = 0;
};

}  // namespace

std::vector<Element> group_shapes(std::span<const Element> candidates) {
  std::map<Key, std::size_t> index;
  std::vector<std::size_t> first;  // a candidate of each group, in order of first occurrence
  std::vector<std::vector<Point64>> origins;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    const auto [it, fresh] = index.try_emplace(key_of(candidates[i]), first.size());
    if (fresh) {
      first.push_back(i);
      origins.emplace_back();
    }
    origins[it->second].push_back(candidates[i].position);
  }
  std::vector<Placed> placed;
  for (std::size_t g = 0; g < first.size(); ++g) {
    for (RepetitionGroup& group : detect_repetitions(origins[g])) {
      Element e = candidates[first[g]];
      e.position = group.origin;
      e.repetition = std::move(group.repetition);
      placed.push_back(Placed{std::move(e), g});
    }
  }
  std::ranges::stable_sort(placed, [](const Placed& a, const Placed& b) {
    return std::tuple{a.element.position.y, a.element.position.x, a.group} <
           std::tuple{b.element.position.y, b.element.position.x, b.group};
  });
  std::vector<Element> out;
  out.reserve(placed.size());
  for (Placed& p : placed) {
    out.push_back(std::move(p.element));
  }
  return out;
}

}  // namespace detail

}  // namespace hefesto::oasis
