// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs::Comparison and compare_pair (lvs-comparison-algorithm "Forced
// equivalences", "Reset and stepping commands", "Single-threaded comparison
// per pair"). The state lives in the object: no global is read or written.
//
// Owner: LVSALG.

#include "hefesto/lvs/cmp/compare.hpp"

#include <cstdint>
#include <utility>

#include "hefesto/lvs/net/names.hpp"
#include "lvs/cmp/state.hpp"

namespace hefesto::lvs {

Comparison::Comparison(const NetlistDb& db1, CellId cell1, const NetlistDb& db2, CellId cell2,
                       CompareOptions options)
    : state_(std::make_unique<State>()) {
  State& s = *state_;
  s.db1 = &db1;
  s.db2 = &db2;
  s.cell1 = &db1.cells.at(cell1);
  s.cell2 = &db2.cells.at(cell2);
  s.rng = cmp::SplitMix64{options.seed};
  s.options = std::move(options);
  s.build();
}

Comparison::~Comparison() = default;
Comparison::Comparison(Comparison&&) noexcept = default;
Comparison& Comparison::operator=(Comparison&&) noexcept = default;

bool Comparison::initialized() const noexcept {
  return state_->initialized;
}

const std::vector<diag::Diagnostic>& Comparison::diagnostics() const noexcept {
  return state_->diagnostics;
}

void Comparison::converge() {
  state_->converge();
}

PairResult Comparison::resolve() {
  return state_->resolve();
}

PairResult Comparison::result() {
  return state_->make_result(state_->initialized ? state_->classify() : -1);
}

bool Comparison::iterate() {
  return state_->initialized && state_->round();
}

std::optional<std::string> Comparison::restart() {
  if (!state_->initialized) {
    return std::string{"Need to initialize data structures first!"};
  }
  State& s = *state_;
  s.initial_partition();
  s.checked.clear();
  s.property_state = PropertyState::none;
  s.property_errors.clear();
  s.property_report.clear();
  return std::nullopt;
}

std::string Comparison::exhaustive(std::optional<bool> on) {
  if (on) {
    state_->exhaustive = *on;
  }
  return state_->exhaustive ? "Exhaustive subdivision ENABLED." : "Exhaustive subdivision DISABLED.";
}

namespace {

/// Appends the members of every symmetric class of `partition`, each named
/// `(1)<name>` or `(2)<name>` by its circuit.
template <typename IsFirst, typename NameOf>
void append_symmetric(const cmp::Partition& partition, const IsFirst& first, const NameOf& name_of,
                      std::vector<std::vector<std::string>>& out) {
  for (std::size_t c = 0; c < partition.classes.size(); ++c) {
    if (!partition.symmetric(c)) {
      continue;
    }
    auto& names = out.emplace_back();
    for (const auto v : partition.classes[c].members) {
      names.push_back((first(v) ? "(1)" : "(2)") + name_of(v));
    }
  }
}

}  // namespace

std::vector<std::vector<std::string>> Comparison::symmetries() const {
  const State& s = *state_;
  std::vector<std::vector<std::string>> result;
  append_symmetric(
      s.devices, [&](std::uint32_t d) { return s.graph.first_device(d); },
      [&](std::uint32_t d) { return s.device_name(d); }, result);
  append_symmetric(
      s.nets, [&](std::uint32_t n) { return s.graph.first_net(n); }, [&](std::uint32_t n) { return s.net_name(n); },
      result);
  return result;
}

std::string Comparison::symmetry_command() {
  return "Symmetry breaking method has been deprecated.";
}

PartitionSummary Comparison::summary() const {
  const State& s = *state_;
  PartitionSummary summary;
  summary.device_vertices = s.graph.devices;
  summary.net_vertices = s.graph.nets;
  summary.device_classes = s.devices.classes.size();
  summary.net_classes = s.nets.classes.size();
  for (std::size_t c = 0; c < s.devices.classes.size(); ++c) {
    summary.illegal_device_classes += (!s.devices.legal(c) || s.devices.classes[c].illegal) ? 1U : 0U;
  }
  for (std::size_t c = 0; c < s.nets.classes.size(); ++c) {
    summary.illegal_net_classes += (!s.nets.legal(c) || s.nets.classes[c].illegal) ? 1U : 0U;
  }
  return summary;
}

Matching Comparison::matching() const {
  return state_->matching();
}

namespace {

/// The vertex of `name` among [begin, end), or kNoVertex.
template <typename NameOf>
std::uint32_t find_vertex(std::uint32_t begin, std::uint32_t end, std::string_view name, bool case_insensitive,
                          const NameOf& name_of) {
  for (std::uint32_t v = begin; v < end; ++v) {
    if (names_equal(name_of(v), name, case_insensitive)) {
      return v;
    }
  }
  return cmp::kNoVertex;
}

EquateOutput equate(cmp::Partition& partition, cmp::SplitMix64& rng, std::uint32_t a, std::uint32_t b,
                    const std::string& what, std::string_view name1, std::string_view name2) {
  EquateOutput out;
  const std::string names = std::string{name1} + " and " + std::string{name2};
  if (a == cmp::kNoVertex || b == cmp::kNoVertex || partition.class_of[a] != partition.class_of[b]) {
    out.message = "Unable to equate " + what + " " + names + ".";
    return out;
  }
  partition.split_out({a, b}, rng.next());
  out.value = 1;
  out.message = (what == "nodes" ? "Nodes " : "Elements ") + names + " are equivalent.";
  return out;
}

}  // namespace

EquateOutput Comparison::equate_nodes(std::string_view net1, std::string_view net2) {
  State& s = *state_;
  if (!s.initialized) {
    return EquateOutput{0, "Circuits not being compared, must specify netlists."};
  }
  const auto name_of = [&s](std::uint32_t n) { return s.net_name(n); };
  const auto a = find_vertex(0, s.graph.nets1, net1, s.case_insensitive, name_of);
  const auto b = find_vertex(s.graph.nets1, s.graph.nets, net2, s.case_insensitive, name_of);
  return equate(s.nets, s.rng, a, b, "nodes", net1, net2);
}

EquateOutput Comparison::equate_elements(std::string_view device1, std::string_view device2) {
  State& s = *state_;
  if (!s.initialized) {
    return EquateOutput{0, "Circuits not being compared, must specify netlists."};
  }
  const auto name_of = [&s](std::uint32_t d) { return s.device_name(d); };
  const auto a = find_vertex(0, s.graph.devices1, device1, s.case_insensitive, name_of);
  const auto b = find_vertex(s.graph.devices1, s.graph.devices, device2, s.case_insensitive, name_of);
  return equate(s.devices, s.rng, a, b, "elements", device1, device2);
}

PairResult compare_pair(const NetlistDb& db1, CellId cell1, const NetlistDb& db2, CellId cell2,
                        const CompareOptions& options) {
  Comparison comparison{db1, cell1, db2, cell2, options};
  return comparison.resolve();
}

}  // namespace hefesto::lvs
