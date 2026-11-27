// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The net driver check and the stderr report of a failed run (capability
// schematic-netlisting "Errors and warnings", paragraphs "Net driver check",
// "Report order of the driver check" and "Report text on stderr"; golden
// cases SCHEM-NL-09, NL-10, NL-11; Q-SCHEM-53).
//
// Owner: NETLIST.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/connectivity.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"
#include "netlist_run.hpp"

namespace hefesto::schem {

namespace detail {

namespace {

enum FindingKind : int { kUndriven = 0, kOpenNet = 1, kShortedOutput = 2 };

/// The pins that carry one net, by the `dir` of their symbol pin.
struct PinCounts {
  std::size_t out = 0;
  std::size_t inout = 0;
  std::size_t total = 0;  // in, out and inout pins

  void add(const PinCounts& other) noexcept {
    out += other.out;
    inout += other.inout;
    total += other.total;
  }
};

/// The net names of one level, in the order the level first gives them, with
/// the pins counted per name.
class NameCounts {
 public:
  void name(const std::string& net) {
    if (!net.empty() && counts_.try_emplace(net).second) order_.push_back(net);
  }

  void pin(const std::string& net, std::string_view dir) {
    if (net.empty()) return;
    name(net);
    PinCounts& counts = counts_[net];
    if (dir == "out") {
      ++counts.out;
    } else if (dir == "inout") {
      ++counts.inout;
    } else if (dir != "in") {
      return;  // any other or no `dir` counts nothing
    }
    ++counts.total;
  }

  [[nodiscard]] const std::vector<std::string>& order() const noexcept { return order_; }
  [[nodiscard]] const PinCounts& at(const std::string& net) const { return counts_.at(net); }

 private:
  std::vector<std::string> order_;
  std::unordered_map<std::string, PinCounts> counts_;
};

/// A Tcl boolean true value (`string is true`): `1` or another non-zero
/// number, or (any case) `true`, `yes`, `on` or an unambiguous prefix of
/// `true` or `yes`.
bool tcl_true(std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.remove_prefix(1);
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) value.remove_suffix(1);
  std::string lower{value};
  char* end = nullptr;
  const double number = std::strtod(lower.c_str(), &end);
  if (!lower.empty() && end == lower.c_str() + lower.size()) return number != 0;
  for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (lower.empty()) return false;
  return lower == "on" || std::string_view{"true"}.starts_with(lower) || std::string_view{"yes"}.starts_with(lower);
}

bool erc_setting(const NetlistEnvironment& env, std::string_view name, bool from_environment) {
  if (from_environment) return true;
  if (env.tcl == nullptr) return false;
  const std::optional<std::string> value = env.tcl->global_variable(name);
  return value && tcl_true(*value);
}

bool is_driver_code(std::string_view code) noexcept {
  return code == net_codes::kUndrivenNode || code == net_codes::kOpenNet || code == net_codes::kShortedOutput;
}

/// The pins of one instance by their `dir`; a label's pin counts nothing.
void count_pins(const Level& level, const Placed& p, NameCounts& names) {
  const auto& nets = level.naming.pin_nets[p.index];
  const bool label = p.type == "label";
  for (std::size_t k = 0; k < nets.size() && k < p.order.size(); ++k) {
    names.pin(nets[k], label ? std::string_view{} : std::string_view{p.symbol->pins[p.order[k]].direction});
  }
}

/// The names of `level` in the order it gives them (labels and ports,
/// wires, other pins), each with the pins that carry it; the name that lost
/// a short carries none.
NameCounts level_names(const Level& level) {
  NameCounts names;
  for (const Placed& p : level.instances) {
    if (p.ignored || !is_naming_type(p.type)) continue;
    for (const std::string& net : level.naming.pin_nets[p.index]) names.name(net);
  }
  for (const std::string& net : level.naming.wire_nets) names.name(net);
  for (const Placed& p : level.instances) {
    if (!p.ignored) count_pins(level, p, names);
  }
  for (const NetShort& lost : level.naming.shorts) names.name(lost.incoming);
  return names;
}

/// The bits the driver check reports on: each bit of each name, but the
/// global ones, with the counts of every name that carries it.
struct BitCounts {
  std::vector<std::string> order;
  std::unordered_map<std::string, PinCounts> counts;
};

BitCounts bit_counts(const NameCounts& names, const std::unordered_set<std::string>& globals, BusBudget& budget) {
  BitCounts bits;
  for (const std::string& net : names.order()) {
    for (std::string& bit : expand_bus(net, &budget)) {
      if (globals.count(bit) != 0) continue;
      const auto [it, inserted] = bits.counts.try_emplace(bit);
      it->second.add(names.at(net));
      if (inserted) bits.order.push_back(std::move(bit));
    }
  }
  return bits;
}

}  // namespace

/// A new level of the report; the level's shorts go after the last
/// separator.
void NetlistRun::open_report_level(const Level& level) {
  result_.report.push_back(DriverReportLevel{level.document->path, {}, {}});
  for (const diag::Diagnostic& d : level.naming.diagnostics) {
    if (d.code == net_codes::kShort) result_.report_shorts.push_back("Error: " + d.text);
  }
}

void NetlistRun::report_line(std::string line) {
  if (!result_.report.empty()) result_.report.back().lines.push_back(std::move(line));
}

void NetlistRun::report_before(std::string line) {
  if (!result_.report.empty()) result_.report.back().before.push_back(std::move(line));
}

/// The names a `global` label of `level` gives (the name `0` included,
/// which is never declared), and the run's declared global names.
std::unordered_set<std::string> NetlistRun::level_globals(const Level& level) const {
  std::unordered_set<std::string> names{globals_seen_.begin(), globals_seen_.end()};
  for (const Placed& p : level.instances) {
    if (p.ignored || p.type != "label") continue;
    auto global = p.instance.get("global", ValueForm::decoded);
    if (!global) global = p.symprops.get("global", ValueForm::decoded);
    if (!global || !(is_true(*global) || *global == "ground")) continue;
    for (const std::string& net : level.naming.pin_nets[p.index]) {
      for (std::string& bit : expand_bus(net)) names.insert(std::move(bit));
    }
  }
  return names;
}

/// "Net driver check" of one level, after its nets are named.
void NetlistRun::driver_check(const Level& level) {
  const NameCounts names = level_names(level);
  const BitCounts bits = bit_counts(names, level_globals(level), driver_budget_);
  std::vector<std::string> order = bits.order;
  if (compat_) {
    std::stable_sort(order.begin(), order.end(), [](const std::string& a, const std::string& b) {
      return driver_report_key(a) < driver_report_key(b);
    });
  } else {
    std::sort(order.begin(), order.end());
  }
  for (const std::string& net : order) {
    const PinCounts& counts = bits.counts.at(net);
    if (counts.out == 0 && counts.inout == 0) {
      driver_finding(net, kUndriven);
    } else if (counts.total == 1) {
      driver_finding(net, kOpenNet);
    } else if (counts.out >= 2) {
      driver_finding(net, kShortedOutput);
    }
  }
}

/// One finding: its xschem text in the report, and a diagnostic (with
/// xschem-compat ON a finding that stays a warning is in the report only).
void NetlistRun::driver_finding(const std::string& net, int kind) {
  static constexpr std::array<std::string_view, 3> kCodes{net_codes::kUndrivenNode, net_codes::kOpenNet,
                                                           net_codes::kShortedOutput};
  static constexpr std::array<std::string_view, 3> kTexts{"undriven node: ", "open net: ", "shorted output node: "};
  const auto index = static_cast<std::size_t>(kind);
  const bool is_error = kind == kUndriven || (kind == kOpenNet && open_net_is_error_) ||
                        (kind == kShortedOutput && shorted_output_is_error_);
  std::string text = std::string{kTexts[index]} + net;
  report_line((kind == kUndriven ? "Error: " : "Warning: ") + text);
  if (!is_error && compat_) return;
  diag::Diagnostic diagnostic = diag::make(kCodes[index], std::move(text));
  diagnostic.severity = is_error ? diag::Severity::error : diag::Severity::warning;
  result_.failed = result_.failed || is_error;
  result_.diagnostics.push_back(std::move(diagnostic));
}

/// The xschemrc settings, read once per run.
void NetlistRun::init_driver_check() {
  open_net_is_error_ = erc_setting(env_, "erc_open_net_is_error", env_.erc_open_net_is_error);
  shorted_output_is_error_ = erc_setting(env_, "erc_shorted_output_is_error", env_.erc_shorted_output_is_error);
}

/// Q-SCHEM-53: whether every error of a failed run is a driver finding.
void NetlistRun::finish_driver_check() {
  result_.driver_check_only =
      result_.failed && std::all_of(result_.diagnostics.begin(), result_.diagnostics.end(), [](const auto& d) {
        return d.severity != diag::Severity::error || is_driver_code(d.code);
      });
}

}  // namespace detail

std::uint32_t driver_report_key(std::string_view name) noexcept {
  std::uint32_t h = 5381;
  for (const char c : name) {
    h = h * 33U + static_cast<unsigned char>(c);
  }
  return h % 31627U;
}

std::string driver_report_text(const NetlistResult& result) {
  if (result.report.empty()) return {};
  const auto separator = [](const DriverReportLevel& level) {
    return "-----------" + level.schematic.generic_string() + "\n";
  };
  std::string out;
  for (const DriverReportLevel& level : result.report) {
    for (const std::string& line : level.before) out += line + "\n";
    out += separator(level);
    for (const std::string& line : level.lines) out += line + "\n";
  }
  out += separator(result.report.front());
  for (const std::string& line : result.report_shorts) out += line + "\n";
  return out;
}

}  // namespace hefesto::schem
