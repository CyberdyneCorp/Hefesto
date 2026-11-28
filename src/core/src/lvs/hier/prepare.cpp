// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Preparing a cell pair and pre-match flattening (lvs-hierarchy "Preparing
// a cell pair", "Pre-match flattening", "Duplicate and shorted pins";
// Q-LVS-11: the pre-match heuristic is matched to netgen's observable
// decisions by the NG-HIER golden cases). Each round flattens at most one
// class, so the decisions follow one deterministic order: circuit 1's
// classes in order of first instance, then circuit 2's other classes.
//
// Owner: LVSHIER.

#include <algorithm>
#include <map>
#include <optional>

#include "hefesto/lvs/hier/codes.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/reduce/reduce.hpp"
#include "lvs/hier/run_state.hpp"

namespace hefesto::lvs::hier_detail {

bool may_flatten(const RunState& state, bool first, CellId klass) {
  const Cell& cell = state.db(first).cells[klass];
  const auto& prohibited = first ? state.prohibited1 : state.prohibited2;
  return is_flattenable(cell.device_class) && !cell.flags.placeholder &&
         !(klass < prohibited.size() && prohibited[klass]);
}

namespace {

std::string instances_text(std::size_t n) {
  return "(" + std::to_string(n) + (n == 1 ? " instance)" : " instances)");
}

/// The first direct instance class of `cell` that is unmatched and may be
/// flattened.
std::optional<CellId> unmatched_class(const RunState& state, bool first, CellId cell) {
  const NetlistDb& db = state.db(first);
  for (const auto& instance : db.cells[cell].instances) {
    if (instance.cell != cell && !db.cells[instance.cell].flags.matched && may_flatten(state, first, instance.cell)) {
      return instance.cell;
    }
  }
  return std::nullopt;
}

}  // namespace

std::optional<std::size_t> flatten_bounded(RunState& state, bool first, CellId parent, CellId klass) {
  NetlistDb& db = state.db(first);
  const std::uint64_t limit = std::min(state.options.device_limit, kMaxDevicesPerRun);
  if (flatten_exceeds_limit(db, parent, klass, limit)) {
    state.stop = diag::make(hier_codes::kDeviceLimit, "Flattening " + db.cells[klass].name + " in " +
                                                          db.cells[parent].name + " would exceed the limit of " +
                                                          std::to_string(limit) + " devices in a cell.");
    return std::nullopt;
  }
  return flatten_instances(db, parent, klass);
}

void flatten_unmatched(RunState& state, bool first, CellId cell) {
  NetlistDb& db = state.db(first);
  // Each class is flattened once per round; flattening exposes the next
  // level, so the rounds are bounded by the hierarchy depth.
  for (std::size_t round = 0; round <= state.options.hierarchy_depth * 4; ++round) {
    const auto klass = state.halted() ? std::nullopt : unmatched_class(state, first, cell);
    if (!klass) {
      return;
    }
    const auto flattened = flatten_bounded(state, first, cell, *klass);
    if (!flattened) {
      return;
    }
    const std::size_t n = *flattened;
    state.log("Flattening unmatched subcell " + db.cells[*klass].name + " in circuit " + db.cells[cell].name + " (" +
              std::to_string(db.cells[cell].file) + ")" + instances_text(n));
  }
  state.result.diagnostics.push_back(diag::make(
      hier_codes::kFlattenRounds, "Flattening of unmatched subcells of " + db.cells[cell].name + " stopped."));
}

namespace {

/// Instance counts of a cell's classes by class identity key, with the
/// classes in order of first instance.
struct Tally {
  std::map<std::string, std::uint64_t> counts;
  std::vector<std::pair<std::string, CellId>> order;
};

class PreMatch {
 public:
  PreMatch(RunState& state, CellId cell1, CellId cell2) : state_(state), cell1_(cell1), cell2_(cell2) {}

  /// One round of heuristics 1 to 3; true when it flattened something.
  bool round() {
    const Tally t1 = tally(true, cell1_);
    const Tally t2 = tally(false, cell2_);
    for (const auto* order : {&t1.order, &t2.order}) {
      for (const auto& [key, unused] : *order) {
        if (order == &t2.order && t1.counts.count(key) != 0) {
          continue;
        }
        if (try_class(key, t1, t2)) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  [[nodiscard]] std::string key_of(const std::string& name, bool first) const {
    std::string_view base = base_name(name);
    if (!first) {
      for (const auto& [name1, name2] : state_.equated) {
        if (names_equal(name2, base, state_.case_insensitive)) {
          base = base_name(name1);
          break;
        }
      }
    }
    return state_.case_insensitive ? name_key(base) : std::string{base};
  }

  [[nodiscard]] Tally tally(bool first, CellId cell) const {
    Tally t;
    const NetlistDb& db = state_.db(first);
    for (const auto& instance : db.cells[cell].instances) {
      const auto key = key_of(db.cells[instance.cell].name, first);
      if (t.counts[key]++ == 0) {
        t.order.emplace_back(key, instance.cell);
      }
    }
    return t;
  }

  static std::optional<CellId> class_for(const Tally& t, const std::string& key) {
    for (const auto& [k, id] : t.order) {
      if (k == key) {
        return id;
      }
    }
    return std::nullopt;
  }

  static std::uint64_t count(const std::map<std::string, std::uint64_t>& m, const std::string& key) {
    const auto it = m.find(key);
    return it == m.end() ? 0 : it->second;
  }

  /// The counts of `t` after flattening the `n` instances of class `key`,
  /// whose own tally is `child`.
  static std::map<std::string, std::uint64_t> after(const Tally& t, const std::string& key, const Tally& child) {
    auto counts = t.counts;
    const std::uint64_t n = count(counts, key);
    counts.erase(key);
    for (const auto& [k, c] : child.counts) {
      counts[k] += n * c;
    }
    return counts;
  }

  static std::uint64_t distance(std::uint64_t a, std::uint64_t b) { return a > b ? a - b : b - a; }

  bool try_class(const std::string& key, const Tally& t1, const Tally& t2) {
    const std::uint64_t n1 = count(t1.counts, key);
    const std::uint64_t n2 = count(t2.counts, key);
    if (n1 == n2) {
      return false;
    }
    const auto x1 = class_for(t1, key);
    const auto x2 = class_for(t2, key);
    const bool flat1 = x1 && *x1 != cell1_ && may_flatten(state_, true, *x1);
    const bool flat2 = x2 && *x2 != cell2_ && may_flatten(state_, false, *x2);
    if (flat1 && flat2 && both_improve(key, t1, t2, *x1, *x2)) {
      flatten(true, *x1);
      flatten(false, *x2);
      return true;
    }
    if (flat2 && (n1 == 0 || n2 > n1) && one_improves(key, t1, t2, *x2, false)) {
      flatten(false, *x2);
      return true;
    }
    if (flat1 && (n2 == 0 || n1 > n2) && one_improves(key, t1, t2, *x1, true)) {
      flatten(true, *x1);
      return true;
    }
    return false;
  }

  /// Heuristic 1: every child class present in both circuits moves toward
  /// agreement (none moves away, at least one gets closer).
  bool both_improve(const std::string& key, const Tally& t1, const Tally& t2, CellId x1, CellId x2) const {
    const Tally child1 = tally(true, x1);
    const Tally child2 = tally(false, x2);
    const auto new1 = after(t1, key, child1);
    const auto new2 = after(t2, key, child2);
    bool closer = false;
    for (const auto* child : {&child1, &child2}) {
      for (const auto& [k, unused] : child->counts) {
        if (count(t1.counts, k) == 0 || count(t2.counts, k) == 0) {
          continue;
        }
        const auto before = distance(count(t1.counts, k), count(t2.counts, k));
        const auto now = distance(count(new1, k), count(new2, k));
        if (now > before) {
          return false;
        }
        closer = closer || now < before;
      }
    }
    return closer;
  }

  /// Heuristics 2 and 3: the summed disagreement over the class and its
  /// children decreases.
  bool one_improves(const std::string& key, const Tally& t1, const Tally& t2, CellId x, bool first) const {
    const Tally child = tally(first, x);
    const auto new1 = first ? after(t1, key, child) : t1.counts;
    const auto new2 = first ? t2.counts : after(t2, key, child);
    std::uint64_t before = distance(count(t1.counts, key), count(t2.counts, key));
    std::uint64_t now = distance(count(new1, key), count(new2, key));
    for (const auto& [k, unused] : child.counts) {
      before += distance(count(t1.counts, k), count(t2.counts, k));
      now += distance(count(new1, k), count(new2, k));
    }
    return now < before;
  }

  void flatten(bool first, CellId klass) {
    const CellId parent = first ? cell1_ : cell2_;
    NetlistDb& db = state_.db(first);
    if (!flatten_bounded(state_, first, parent, klass)) {
      return;
    }
    state_.log("Flattening instances of " + db.cells[klass].name + " in cell " + db.cells[parent].name + " (" +
               std::to_string(db.cells[parent].file) + ") makes a better match");
  }

  RunState& state_;
  CellId cell1_;
  CellId cell2_;
};

void log_lines(RunState& state, const std::vector<std::string>& lines, bool console_only) {
  for (const auto& line : lines) {
    console_only ? state.console(line) : state.log(line);
  }
}

/// Step 3: true when the black-box check skips steps 4 and 5.
bool black_boxes(RunState& state, CellId cell1, CellId cell2) {
  const Cell& a = state.db1.cells[cell1];
  const Cell& b = state.db2.cells[cell2];
  const bool empty1 = is_black_box(a) && a.instances.empty();
  const bool empty2 = is_black_box(b) && b.instances.empty();
  if (empty1 && empty2) {
    state.log("Circuit 1 cell " + a.name + " and Circuit 2 cell " + b.name + " are black boxes.");
    return true;
  }
  if (empty2 && !a.instances.empty()) {
    state.log("Circuit 2 cell " + b.name + " is a black box; will not flatten Circuit 1");
    return true;
  }
  if (empty1 && !b.instances.empty()) {
    state.log("Circuit 1 cell " + a.name + " is a black box; will not flatten Circuit 2");
    return true;
  }
  return false;
}

/// Step 5, with heuristic 4 (zero-valued devices); true when it changed
/// anything.
bool prematch_round(RunState& state, CellId cell1, CellId cell2, bool top) {
  const bool flattened = PreMatch{state, cell1, cell2}.round();
  const auto zero = remove_zero_valued(state.db1, cell1, state.db2, cell2, top);
  log_lines(state, zero.log, false);
  return flattened || zero.zero_valued_removed > 0;
}

}  // namespace

void prepare_pair(RunState& state, CellId cell1, CellId cell2, bool top) {
  log_lines(state, remove_duplicate_ports(state.db1, cell1), true);
  log_lines(state, remove_duplicate_ports(state.db2, cell2), true);
  log_lines(state, declare_shorted_ports(state.db1, cell1), true);
  log_lines(state, declare_shorted_ports(state.db2, cell2), true);
  convert_globals(state.db1, cell1);
  convert_globals(state.db2, cell2);
  if (black_boxes(state, cell1, cell2)) {
    return;
  }
  flatten_unmatched(state, true, cell1);
  flatten_unmatched(state, false, cell2);
  for (std::size_t round = 0; round <= state.options.hierarchy_depth * 4; ++round) {
    if (state.halted() || !prematch_round(state, cell1, cell2, top)) {
      return;
    }
    state.log("Making another compare attempt.");
    state.console("Flattened mismatched instances and attempting compare again.");
    flatten_unmatched(state, true, cell1);
    flatten_unmatched(state, false, cell2);
  }
  state.result.diagnostics.push_back(diag::make(
      hier_codes::kFlattenRounds, "Pre-match flattening of " + state.db1.cells[cell1].name + " stopped."));
}

}  // namespace hefesto::lvs::hier_detail
