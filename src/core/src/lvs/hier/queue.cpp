// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hierarchy depth and the compare queue (lvs-hierarchy "Compare queue
// construction", "Hierarchy depth limit"; Q-LVS-36). Both walk the
// hierarchy level by level with explicit lists, never by recursion: a level
// holds each cell once, so a recursive hierarchy only grows the level count
// until the limit stops it.
//
// Owner: LVSHIER.

#include <algorithm>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/hier/codes.hpp"
#include "hefesto/lvs/hier/hierarchy.hpp"
#include "hefesto/lvs/net/names.hpp"

namespace hefesto::lvs {

bool is_hierarchical(const Cell& cell) noexcept {
  return !is_primitive(cell.device_class);
}

bool is_black_box(const Cell& cell) noexcept {
  return cell.device_class == DeviceClass::module || cell.flags.placeholder;
}

std::string_view base_name(std::string_view name) noexcept {
  if (!name.ends_with("]]")) {
    return name;
  }
  const auto open = name.rfind("[[");
  if (open == std::string_view::npos || open == 0) {
    return name;
  }
  const auto digits = name.substr(open + 2, name.size() - open - 4);
  const bool numeric =
      !digits.empty() && std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; });
  return numeric ? name.substr(0, open) : name;
}

namespace {

/// The hierarchical classes instanced by `cells`, each once, in order of
/// the cells and then of their instances (self-instances ignored).
std::vector<CellId> next_level(const NetlistDb& db, const std::vector<CellId>& cells) {
  std::vector<CellId> next;
  std::vector<bool> seen(db.cells.size(), false);
  for (const CellId id : cells) {
    for (const auto& instance : db.cells[id].instances) {
      const CellId child = instance.cell;
      if (child == id || child >= db.cells.size() || seen[child] || !is_hierarchical(db.cells[child])) {
        continue;
      }
      seen[child] = true;
      next.push_back(child);
    }
  }
  return next;
}

/// Every level below `top`, level 1 first; at most `limit` + 1 levels.
std::vector<std::vector<CellId>> levels_below(const NetlistDb& db, CellId top, std::size_t limit) {
  std::vector<std::vector<CellId>> levels;
  std::vector<CellId> current{top};
  while (levels.size() <= limit) {
    auto next = next_level(db, current);
    if (next.empty()) {
      break;
    }
    levels.push_back(next);
    current = std::move(next);
  }
  return levels;
}

}  // namespace

diag::Outcome<std::size_t> hierarchy_depth(const NetlistDb& db, CellId top, std::size_t limit) {
  const auto levels = levels_below(db, top, limit);
  if (levels.size() <= limit) {
    return levels.size();
  }
  const Cell& deepest = db.cells[levels.back().front()];
  return diag::make(hier_codes::kDepthLimit, "Hierarchy below cell " + db.cells[top].name + " is deeper than " +
                                                 std::to_string(limit) + " levels (or recursive) at cell " +
                                                 deepest.name + ".");
}

bool ClassIdentity::same(std::string_view name1, std::string_view name2) const {
  const auto a = base_name(name1);
  const auto b = base_name(name2);
  if (equated != nullptr) {
    for (const auto& [first, second] : *equated) {
      if (names_equal(first, a, case_insensitive) && names_equal(second, b, case_insensitive)) {
        return true;
      }
    }
  }
  return names_equal(a, b, case_insensitive);
}

namespace {

class QueueBuilder {
 public:
  explicit QueueBuilder(const QueueInput& input) : in_(input) {}

  std::vector<std::pair<CellId, CellId>> build() {
    const std::size_t depth = std::max(in_.depth1, in_.depth2);
    const auto levels1 = levels_below(*in_.db1, in_.top1, depth);
    const auto levels2 = levels_below(*in_.db2, in_.top2, depth);
    for (std::size_t level = depth; level >= 1; --level) {
      if (level <= levels1.size()) {
        visit(levels1[level - 1], true);
      }
      if (level <= levels2.size()) {
        visit(levels2[level - 1], false);
      }
    }
    in_.db1->cells[in_.top1].flags.matched = true;
    in_.db2->cells[in_.top2].flags.matched = true;
    queue_.emplace_back(in_.top1, in_.top2);
    return std::move(queue_);
  }

 private:
  void visit(const std::vector<CellId>& cells, bool first) {
    NetlistDb& own = first ? *in_.db1 : *in_.db2;
    for (const CellId id : cells) {
      if (own.cells[id].flags.matched) {
        continue;
      }
      const auto partner = find_partner(own.cells[id], first);
      if (!partner) {
        continue;  // unmatched: flattened when its parent is compared
      }
      own.cells[id].flags.matched = true;
      (first ? *in_.db2 : *in_.db1).cells[*partner].flags.matched = true;
      queue_.emplace_back(first ? id : *partner, first ? *partner : id);
    }
  }

  /// The equate-classes partner of `cell`, else the same-identity cell of
  /// the other netlist unless it is another cell's equate partner.
  std::optional<CellId> find_partner(const Cell& cell, bool first) const {
    const NetlistDb& other = first ? *in_.db2 : *in_.db1;
    const Cell& other_top = other.cells[first ? in_.top2 : in_.top1];
    if (const auto* entry = equate_entry(cell, first)) {
      return usable(other, other.find_cell(first ? entry->second : entry->first, other_top.file));
    }
    const auto candidate = usable(other, other.find_cell(base_name(cell.name), other_top.file));
    if (candidate && equated_elsewhere(other.cells[*candidate], cell, first)) {
      return std::nullopt;
    }
    return candidate;
  }

  /// The first `equate classes` pair that names `cell` on its side.
  [[nodiscard]] const std::pair<std::string, std::string>* equate_entry(const Cell& cell, bool first) const {
    if (in_.identity.equated == nullptr) {
      return nullptr;
    }
    for (const auto& entry : *in_.identity.equated) {
      if (names_equal(first ? entry.first : entry.second, base_name(cell.name), in_.identity.case_insensitive)) {
        return &entry;
      }
    }
    return nullptr;
  }

  static std::optional<CellId> usable(const NetlistDb& db, std::optional<CellId> id) {
    if (!id || !is_hierarchical(db.cells[*id]) || db.cells[*id].flags.matched) {
      return std::nullopt;
    }
    return id;
  }

  /// `candidate` (of the other netlist) is the equate partner of a cell
  /// other than `cell`.
  bool equated_elsewhere(const Cell& candidate, const Cell& cell, bool first) const {
    if (in_.identity.equated == nullptr) {
      return false;
    }
    const bool fold = in_.identity.case_insensitive;
    for (const auto& [name1, name2] : *in_.identity.equated) {
      const auto& own_name = first ? name1 : name2;
      const auto& other_name = first ? name2 : name1;
      if (names_equal(other_name, base_name(candidate.name), fold) &&
          !names_equal(own_name, base_name(cell.name), fold)) {
        return true;
      }
    }
    return false;
  }

  const QueueInput& in_;
  std::vector<std::pair<CellId, CellId>> queue_;
};

}  // namespace

std::vector<std::pair<CellId, CellId>> build_compare_queue(const QueueInput& input) {
  return QueueBuilder{input}.build();
}

}  // namespace hefesto::lvs
