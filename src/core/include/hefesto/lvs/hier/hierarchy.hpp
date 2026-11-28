// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The steps of hierarchical LVS that run_lvs strings together (capability
// lvs-hierarchy): the hierarchy depth and the compare queue, flattening,
// duplicate pins, global-to-port conversion and pin matching. Exposed so
// that each requirement is tested on its own. Every traversal is iterative
// (Requirement "Hierarchy depth limit").
//
// Owner: LVSHIER.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/report/model.hpp"

namespace hefesto::lvs {

/// A cell whose class is subcircuit or module: the cells that count for
/// the hierarchy and the queue (primitive devices do not).
[[nodiscard]] bool is_hierarchical(const Cell& cell) noexcept;

/// A black box: a module, or a placeholder made for an undefined
/// subcircuit ("Black boxes and flattening prohibitions").
[[nodiscard]] bool is_black_box(const Cell& cell) noexcept;

/// `name` without the `[[k]]` suffix of a renamed duplicate.
[[nodiscard]] std::string_view base_name(std::string_view name) noexcept;

/// The deepest level of subcircuit or module instances below `top`
/// (self-instances ignored). LVS-HIER-001, naming the deepest cell, when it
/// is above `limit` or the hierarchy is recursive.
[[nodiscard]] diag::Outcome<std::size_t> hierarchy_depth(const NetlistDb& db, CellId top, std::size_t limit);

/// Class-identity matching between the two circuits: equal base names
/// (case per `case_insensitive`), or a pair of `equated`.
struct ClassIdentity {
  bool case_insensitive = true;
  const std::vector<std::pair<std::string, std::string>>* equated = nullptr;

  [[nodiscard]] bool same(std::string_view name1, std::string_view name2) const;
};

/// "Compare queue construction": the pairs (circuit-1 cell, circuit-2
/// cell), bottom-up, the top pair last. Marks every queued cell matched.
/// `depth1` and `depth2` are hierarchy_depth() of the two tops.
struct QueueInput {
  NetlistDb* db1 = nullptr;
  CellId top1 = 0;
  std::size_t depth1 = 0;
  NetlistDb* db2 = nullptr;
  CellId top2 = 0;
  std::size_t depth2 = 0;
  ClassIdentity identity;
};
[[nodiscard]] std::vector<std::pair<CellId, CellId>> build_compare_queue(const QueueInput& input);

/// "Flattening semantics": replaces every direct instance of `klass` in
/// `parent` by its contents (names `<i>/<name>`), connects the port nets,
/// evaluates expression-valued properties with the instance's parameters
/// and copies the global nets. Does nothing for the cell itself, a
/// placeholder or a cell that is not a subcircuit, nor when
/// flatten_exceeds_limit() holds. Returns the instances flattened.
std::size_t flatten_instances(NetlistDb& db, CellId parent, CellId klass);

/// Whether flattening the direct instances of `klass` in `parent` would
/// leave more than `limit` (at most kMaxDevicesPerRun) instances in
/// `parent` (crafted-input limit, both netgen-compat modes; the run flow
/// ends with LVS-HIER-006).
[[nodiscard]] bool flatten_exceeds_limit(const NetlistDb& db, CellId parent, CellId klass,
                                         std::uint64_t limit = kMaxDevicesPerRun);

/// Flattens the instances of `klass` in every cell of `db`; returns the
/// instances flattened.
std::size_t flatten_class(NetlistDb& db, CellId klass);

/// "Duplicate and shorted pins", duplicate half: drops every port that
/// shares its net and its name with an earlier port, from the cell and from
/// every instance (merging the parent nets). Returns the console messages
/// `Duplicate pin <p> in cell <c> (<f>)`.
[[nodiscard]] std::vector<std::string> remove_duplicate_ports(NetlistDb& db, CellId cell);

/// lvs-setup-file `convert`: each global net of `cell` becomes a local net
/// with a new port at the end; every instance of `cell` connects it to the
/// parent's global net of that name (created when missing).
void convert_globals(NetlistDb& db, CellId cell);

/// Removes the ports of `cell` at the positions where `drop` is true, from
/// the cell and from every instance of it in `db`.
void remove_ports(NetlistDb& db, CellId cell, const std::vector<bool>& drop);

/// The inputs of "Pin matching" (`equate [-force] pins`).
struct PinMatchInput {
  /// The comparison's matched nets; nullptr when no partition exists.
  const Matching* matching = nullptr;
  bool forced = false;
  bool top = false;
  bool netgen_compat = true;  // Q-LVS-24 (placeholder_pin_equivalence)
  bool blackbox = false;      // automatic black-boxing
  bool case_insensitive = true;
};

struct PinMatchResult {
  int result = 1;  // 1 equivalent, 0 altered, 2 no pins (Q-LVS-17: never −1 or −2)
  std::vector<PinRow> rows;
  std::vector<OutputText> output;  // the table and the messages, in order
  std::vector<std::string> pins1;  // circuit 1's ports after matching
  std::vector<std::string> pins2;  // circuit 2's ports, in circuit 1's order
};

/// Matches the ports of the two cells and reorders circuit 2's ports and
/// the pins of its instances to circuit 1's order.
[[nodiscard]] PinMatchResult match_pins(NetlistDb& db1, CellId cell1, NetlistDb& db2, CellId cell2,
                                        const PinMatchInput& input, const ColumnWidths& widths);

/// Whether the pin names `a` and `b` are equal for pin matching: a
/// trailing `!` and a leading `\` are ignored, and with `array_equivalence`
/// a size-1 array `a[0]` equals `a`.
[[nodiscard]] bool pin_names_equal(std::string_view a, std::string_view b, bool case_insensitive,
                                   bool array_equivalence);

/// Anchored match of `name` against a `-noflatten` pattern: literal bytes,
/// `.`, bracket classes `[...]`/`[^...]`, `\` escapes and the postfix `*`,
/// `+`, `?`. Linear in name × pattern and never recursive (other regular
/// expression syntax is matched literally).
[[nodiscard]] bool pattern_matches(std::string_view pattern, std::string_view name);

}  // namespace hefesto::lvs
