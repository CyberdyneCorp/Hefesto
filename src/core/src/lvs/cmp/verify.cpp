// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Match verification and the property checks of matched pairs
// (lvs-comparison-algorithm "Match verification"; lvs-property-matching
// "When properties are checked", "Property error results"; Q-LVS-10).
//
// Owner: LVSALG.

#include <algorithm>
#include <map>
#include <set>

#include "hefesto/lvs/net/names.hpp"
#include "lvs/cmp/state.hpp"

namespace hefesto::lvs {

namespace {

PropertySettings property_settings(const Comparison::State& state) {
  PropertySettings settings;
  settings.netgen_compat = state.options.netgen_compat;
  settings.strict_topology = state.options.strict_topology;
  settings.case_insensitive = state.case_insensitive;
  return settings;
}

/// The net class of each pin of device `d` (kNoPinKey for no net).
std::vector<std::uint32_t> pin_keys(const Comparison::State& state, std::uint32_t d) {
  std::vector<std::uint32_t> keys;
  for (std::uint32_t p = state.graph.dev_offset[d]; p < state.graph.dev_offset[d + 1]; ++p) {
    const auto n = state.graph.dev_net[p];
    keys.push_back(n == cmp::kNoVertex ? kNoPinKey : state.nets.class_of[n]);
  }
  return keys;
}

PairCheck check_pair(const Comparison::State& state, std::uint32_t a, std::uint32_t b) {
  const auto keys_a = pin_keys(state, a);
  const auto keys_b = pin_keys(state, b);
  const Instance& ia = state.instance_of(a);
  const Instance& ib = state.instance_of(b);
  const DeviceSide first{ia.name, &state.class_of_device(a), &ia, keys_a};
  const DeviceSide second{ib.name, &state.class_of_device(b), &ib, keys_b};
  return check_device_pair(first, second, property_settings(state));
}

/// Folds one pair's check into the comparison's property state and logs.
void record_check(Comparison::State& s, PairCheck check) {
  if (check.outcome == PropertyOutcome::failure) {
    // Q-LVS-10 (replicate): netgen's texts and codes are kept; the
    // structured result carries the detail property-check-failed.
    if (s.property_state == PropertyState::none) {
      s.property_state = PropertyState::failure;
    }
  } else if (check.outcome == PropertyOutcome::mismatch) {
    s.property_state = PropertyState::errors;
  }
  for (auto& line : check.report) {
    s.property_report.push_back(std::move(line));
  }
  for (auto& line : check.console) {
    s.log.push_back(LogLine{std::move(line), true});
  }
  for (auto& error : check.errors) {
    s.property_errors.push_back(std::move(error));
  }
}

}  // namespace

bool Comparison::State::properties_match(std::uint32_t a, std::uint32_t b) const {
  return check_pair(*this, a, b).outcome == PropertyOutcome::match;
}

void Comparison::State::check_properties() {
  for (std::size_t c = 0; c < devices.classes.size(); ++c) {
    if (!devices.matched_pair(c)) {
      continue;
    }
    const auto a = devices.classes[c].members[0];
    const auto b = devices.classes[c].members[1];
    const auto [it, added] = checked.try_emplace(a, b);
    if (!added && it->second == b) {
      continue;  // each pair is checked once
    }
    it->second = b;
    record_check(*this, check_pair(*this, a, b));
  }
}

std::size_t Comparison::State::symmetry_count() const {
  std::size_t k = 0;
  for (std::size_t c = 0; c < devices.classes.size(); ++c) {
    k += devices.symmetric(c) ? 1U : 0U;
  }
  for (std::size_t c = 0; c < nets.classes.size(); ++c) {
    k += nets.symmetric(c) ? 1U : 0U;
  }
  return k;
}

namespace {

std::size_t real_ports(const Cell& cell) {
  return static_cast<std::size_t>(
      std::count_if(cell.ports.begin(), cell.ports.end(), [](const Port& port) { return !port.proxy; }));
}

/// Ports of `cell` whose net reaches no device pin.
std::vector<std::string> disconnected_ports(const Cell& cell, const std::set<NetId>& vertex_nets) {
  std::vector<std::string> names;
  for (const auto& port : cell.ports) {
    if (!port.proxy && (port.net == kNoNet || vertex_nets.count(port.net) == 0)) {
      names.push_back(port.name);
    }
  }
  return names;
}

/// True when a net class holds unequally many ports of the two circuits.
bool port_counts_differ(const cmp::Partition& nets, const cmp::Graph& graph) {
  for (const cmp::Class& cls : nets.classes) {
    std::size_t ports1 = 0;
    std::size_t ports2 = 0;
    for (const auto n : cls.members) {
      if (graph.net_port[n]) {
        (graph.first_net(n) ? ports1 : ports2) += 1;
      }
    }
    if (ports1 != ports2) {
      return true;
    }
  }
  return false;
}

/// True when a disconnected port of circuit 1 has no same-named
/// disconnected port in circuit 2.
bool unpaired_disconnected_port(const Comparison::State& s) {
  std::set<NetId> nets1;
  std::set<NetId> nets2;
  for (std::uint32_t n = 0; n < s.graph.nets; ++n) {
    (s.graph.first_net(n) ? nets1 : nets2).insert(s.graph.net_id[n]);
  }
  const auto open2 = disconnected_ports(*s.cell2, nets2);
  const auto has_partner = [&](const std::string& name) {
    return std::any_of(open2.begin(), open2.end(),
                       [&](const std::string& other) { return names_equal(name, other, s.case_insensitive); });
  };
  const auto open1 = disconnected_ports(*s.cell1, nets1);
  return !std::all_of(open1.begin(), open1.end(), has_partner);
}

}  // namespace

bool Comparison::State::port_errors() const {
  return port_counts_differ(nets, graph) || real_ports(*cell1) != real_ports(*cell2) ||
         unpaired_disconnected_port(*this);
}

int Comparison::State::classify() {
  bool unequal = false;
  for (std::size_t c = 0; c < devices.classes.size() && !unequal; ++c) {
    unequal = !devices.legal(c);
  }
  for (std::size_t c = 0; c < nets.classes.size() && !unequal; ++c) {
    unequal = !nets.legal(c);
  }
  if (bad_match || unequal) {
    return -1;
  }
  check_properties();
  const std::size_t k = symmetry_count();
  if (k > 0) {
    return static_cast<int>(k);
  }
  return port_errors() ? -2 : 0;
}

Matching Comparison::State::matching() const {
  Matching result;
  for (std::size_t c = 0; c < devices.classes.size(); ++c) {
    if (devices.matched_pair(c)) {
      const auto& m = devices.classes[c].members;
      result.devices.emplace_back(graph.dev_instance[m[0]], graph.dev_instance[m[1]]);
    }
  }
  for (std::size_t c = 0; c < nets.classes.size(); ++c) {
    if (nets.matched_pair(c)) {
      const auto& m = nets.classes[c].members;
      result.nets.emplace_back(graph.net_id[m[0]], graph.net_id[m[1]]);
    }
  }
  std::sort(result.devices.begin(), result.devices.end());
  std::sort(result.nets.begin(), result.nets.end());
  return result;
}

PairResult Comparison::State::make_result(int value) const {
  PairResult result;
  result.cell1 = cell1->name;
  result.cell2 = cell2->name;
  result.verify = value;
  result.cancelled = cancelled;
  result.property_state = property_state;
  result.property_errors = property_errors;
  result.log = log;
  result.diagnostics = diagnostics;
  result.matching = matching();
  const bool errors = property_state == PropertyState::errors;
  if (!initialized || value == -1) {
    result.status = PairStatus::mismatch;
  } else if (value == -2) {
    result.status = errors ? PairStatus::match_port_property_errors : PairStatus::match_port_errors;
  } else if (value > 0) {
    result.status = PairStatus::symmetric;
  } else {
    result.status = errors ? PairStatus::match_property_errors : PairStatus::match;
  }
  return result;
}

// ------------------------------------------------------------ verify

bool symmetry_verdict_in_log(bool netgen_compat) noexcept {
  return !netgen_compat;
}

std::optional<VerifyOption> parse_verify_option(std::string_view word) {
  static constexpr std::pair<std::string_view, VerifyOption> kOptions[] = {
      {"nodes", VerifyOption::nodes},           {"elements", VerifyOption::elements},
      {"properties", VerifyOption::properties}, {"only", VerifyOption::only},
      {"all", VerifyOption::all},               {"equivalent", VerifyOption::equivalent},
      {"unique", VerifyOption::unique},
  };
  std::optional<VerifyOption> found;
  for (const auto& [name, option] : kOptions) {
    if (name == word) {
      return option;
    }
    if (!word.empty() && name.starts_with(word)) {
      if (found) {
        return std::nullopt;  // ambiguous
      }
      found = option;
    }
  }
  return found;
}

namespace {

bool returns_value(VerifyOption option) {
  return option == VerifyOption::equivalent || option == VerifyOption::unique;
}

bool prints_verdict(VerifyOption option) {
  return option == VerifyOption::only || option == VerifyOption::all || option == VerifyOption::properties;
}

void verify_mismatch(VerifyOption option, VerifyOutput& out) {
  const bool none = option == VerifyOption::none;
  out.net_report = none || option == VerifyOption::nodes || option == VerifyOption::all;
  out.device_report = none || option == VerifyOption::elements || option == VerifyOption::all;
  if (returns_value(option)) {
    out.value = 0;
    return;
  }
  if (option != VerifyOption::only && option != VerifyOption::properties) {
    return;
  }
  out.lines.push_back(LogLine{"Netlists do not match.", false});
  out.lines.push_back(LogLine{"Port matching may fail to disambiguate symmetries.", false});
}

void verify_port_errors(VerifyOption option, bool property_errors, VerifyOutput& out) {
  if (option == VerifyOption::equivalent) {
    out.value = 1;
  } else if (option == VerifyOption::unique) {
    out.value = property_errors ? -4 : -2;
  } else if (prints_verdict(option)) {
    out.lines.push_back(LogLine{"Circuits match uniquely with port errors.", false});
  }
}

void verify_symmetries(VerifyOption option, int k, bool netgen_compat, VerifyOutput& out) {
  if (option == VerifyOption::equivalent) {
    out.value = k;
  } else if (option == VerifyOption::unique) {
    out.value = 0;
  } else if (prints_verdict(option)) {
    const std::string text =
        "Circuits match with " + std::to_string(k) + (k == 1 ? " symmetry." : " symmetries.");
    out.lines.push_back(LogLine{text, !symmetry_verdict_in_log(netgen_compat)});
  }
}

void verify_unique(VerifyOption option, const Comparison::State& s, VerifyOutput& out) {
  const bool errors = s.property_state == PropertyState::errors;
  if (returns_value(option)) {
    out.value = errors ? -3 : 1;
    return;
  }
  if (!prints_verdict(option)) {
    return;
  }
  // Q-LVS-21 (replicate, both modes): the extra "." line after a unique
  // match without property errors.
  out.lines.push_back(LogLine{"Circuits match uniquely.", false});
  out.lines.push_back(LogLine{errors ? "Property errors were found." : ".", false});
  if (option == VerifyOption::properties && errors) {
    for (const auto& line : s.property_report) {
      out.lines.push_back(LogLine{line, false});
    }
  }
}

/// The names of the vertices of illegal classes (`-list`).
template <typename NameOf>
std::vector<std::string> bad_names(const cmp::Partition& partition, const NameOf& name_of) {
  std::vector<std::string> names;
  for (std::size_t c = 0; c < partition.classes.size(); ++c) {
    if (partition.legal(c) && !partition.classes[c].illegal) {
      continue;
    }
    for (const auto v : partition.classes[c].members) {
      names.push_back(name_of(v));
    }
  }
  return names;
}

void verify_uninitialized(VerifyOption option, const Comparison::State& s, VerifyOutput& out) {
  if (returns_value(option)) {
    out.value = -1;
  } else if (s.cell1 == nullptr) {
    out.lines.push_back(LogLine{"Verify:  no current cell to verify.", false});
  } else {
    out.lines.push_back(
        LogLine{"Verify:  cell " + s.cell1->name + " has no elements and/or nodes.  Not checked.", false});
  }
}

}  // namespace

VerifyOutput Comparison::verify(VerifyOption option, bool list) {
  VerifyOutput out;
  State& s = *state_;
  if (!s.initialized) {
    verify_uninitialized(option, s, out);
    return out;
  }
  const int value = s.classify();
  if (value == -1) {
    verify_mismatch(option, out);
  } else if (value == -2) {
    verify_port_errors(option, s.property_state == PropertyState::errors, out);
  } else if (value > 0) {
    verify_symmetries(option, value, s.options.netgen_compat, out);
  } else {
    verify_unique(option, s, out);
  }
  if (list) {
    out.bad_nets = bad_names(s.nets, [&s](std::uint32_t n) { return s.net_name(n); });
    out.bad_elements = bad_names(s.devices, [&s](std::uint32_t d) { return s.device_name(d); });
  }
  return out;
}

}  // namespace hefesto::lvs
