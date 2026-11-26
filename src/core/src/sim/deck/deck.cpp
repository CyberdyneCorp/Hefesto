// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Deck preparation (task 13.1): the `spice` and `raw` sources, testbench
// composition and stimuli. The layout and schematic sources (task 13.4)
// and ModelDescriptor::load (task 13.2) fail with CORE-NOTIMPL-009.
//
// Owner: SIMDECK.

#include "hefesto/sim/deck/deck.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <tuple>
#include <utility>

#include "hefesto/pdk/sha256.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/deck/spice_text.hpp"
#include "hefesto/sim/not_implemented.hpp"
#include "sim/deck/deck_text.hpp"

namespace hefesto::sim {

namespace {

// --- stimuli ---------------------------------------------------------------------

std::string numbers(const std::vector<double>& values) {
  std::string out;
  for (const double v : values) {
    out += (out.empty() ? "" : " ") + format_g12(v);
  }
  return out;
}

diag::Diagnostic bad_source(const Stimulus& s, const std::string& why) {
  return diag::make("SIM-DECK-010", "Source " + s.name + ": " + why);
}

std::optional<diag::Diagnostic> check_counts(const Stimulus& s) {
  const std::size_t n = s.values.size();
  switch (s.kind) {
    case Stimulus::Kind::dc: return n == 1 ? std::nullopt : std::optional{bad_source(s, "DC needs one value")};
    case Stimulus::Kind::pulse: return n == 7 ? std::nullopt : std::optional{bad_source(s, "PULSE needs 7 values")};
    case Stimulus::Kind::sin:
      return n >= 3 && n <= 6 ? std::nullopt : std::optional{bad_source(s, "SIN needs 3 to 6 values")};
    case Stimulus::Kind::pwl:
      return n >= 2 && n % 2 == 0 ? std::nullopt : std::optional{bad_source(s, "PWL needs (t, v) pairs")};
  }
  return std::nullopt;
}

std::optional<diag::Diagnostic> check_values(const Stimulus& s) {
  if (s.kind == Stimulus::Kind::pulse) {
    static constexpr std::array<std::pair<std::size_t, const char*>, 3> kTimes = {
        {{3, "tr"}, {4, "tf"}, {5, "pw"}}};
    for (const auto& [index, name] : kTimes) {
      if (s.values[index] < 0.0) {
        return bad_source(s, std::string{"PULSE "} + name + " must not be negative");
      }
    }
  }
  if (s.kind == Stimulus::Kind::pwl) {
    for (std::size_t i = 2; i < s.values.size(); i += 2) {
      if (s.values[i] < s.values[i - 2]) {
        return bad_source(s, "PWL time values must be non-decreasing");
      }
    }
  }
  return std::nullopt;
}

std::string waveform(const Stimulus& s) {
  switch (s.kind) {
    case Stimulus::Kind::dc: return format_g12(s.values.front());
    case Stimulus::Kind::pulse: return "PULSE(" + numbers(s.values) + ")";
    case Stimulus::Kind::sin: return "SIN(" + numbers(s.values) + ")";
    case Stimulus::Kind::pwl: return "PWL(" + numbers(s.values) + ")";
  }
  return {};
}

diag::Status check_unique(const std::vector<Stimulus>& stimuli) {
  std::vector<std::string> seen;
  for (const Stimulus& s : stimuli) {
    std::string name = ascii_lower(s.name);
    if (std::find(seen.begin(), seen.end(), name) != seen.end()) {
      return bad_source(s, "the name is used twice");
    }
    seen.push_back(std::move(name));
  }
  return diag::Done{};
}

// --- composition -------------------------------------------------------------------

/// The ports of `.subckt <name>` in `lines` (case-insensitive name).
std::optional<std::vector<std::string>> subckt_ports(const std::vector<std::string>& lines, const std::string& name) {
  for (const deck::LogicalLine& line : deck::logical_lines(lines, 0)) {
    if (deck::card_of(line.text) != ".subckt") {
      continue;
    }
    std::vector<std::string> tokens = spice_tokens(line.text);
    if (tokens.size() < 2 || ascii_lower(tokens[1]) != ascii_lower(name)) {
      continue;
    }
    std::vector<std::string> ports;
    for (std::size_t i = 2; i < tokens.size() && ascii_lower(tokens[i]) != "params:"; ++i) {
      if (line.text.find(tokens[i] + "=") == std::string::npos) {
        ports.push_back(tokens[i]);
      }
    }
    return ports;
  }
  return std::nullopt;
}

struct Composer {
  const Testbench& tb;
  PreparedDeck& out;
  std::vector<std::string> lines;

  diag::Status header() {
    lines.push_back("* hefesto testbench " + tb.name + " corner " + tb.corner);
    for (const std::string& option : tb.options) {
      lines.push_back(".option " + option);
    }
    if (tb.temperature) {
      lines.push_back(".temp " + format_g12(*tb.temperature));
    }
    std::vector<std::pair<std::string, double>> params = tb.params;
    std::sort(params.begin(), params.end());
    for (const auto& [ident, value] : params) {
      lines.push_back(".param " + ident + " = " + format_g12(value));
    }
    return diag::Done{};
  }

  diag::Status instance(const std::vector<std::string>& ports) {
    for (const PortBinding& b : tb.bindings) {
      if (std::find(ports.begin(), ports.end(), b.port) == ports.end()) {
        return diag::make("SIM-DECK-011", "Unknown DUT port " + b.port);
      }
    }
    std::string line = "Xdut";
    std::vector<std::string> extra;
    for (const std::string& port : ports) {
      const auto bound = std::find_if(tb.bindings.begin(), tb.bindings.end(),
                                      [&](const PortBinding& b) { return b.port == port; });
      if (bound != tb.bindings.end()) {
        line += " " + bound->node;
        continue;
      }
      line += " nc_" + port;
      extra.push_back("Rnc_" + port + " nc_" + port + " 0 1e+12");
      out.diagnostics.push_back(diag::make("SIM-DECK-012", "Port " + port + " left unconnected"));
    }
    lines.push_back(line + " " + tb.subckt);
    return trailer(std::move(extra));
  }

  diag::Status trailer(std::vector<std::string> extra) {
    if (diag::Status unique = check_unique(tb.stimuli); !unique.ok()) {
      return unique;
    }
    for (const Stimulus& s : tb.stimuli) {
      auto line = stimulus_line(s);
      if (!line.ok()) {
        return line.error();
      }
      lines.push_back(std::move(line.value()));
    }
    lines.insert(lines.end(), extra.begin(), extra.end());
    lines.insert(lines.end(), tb.extra_lines.begin(), tb.extra_lines.end());
    if (!tb.saves.empty()) {
      std::string save = ".save";
      for (const std::string& v : tb.saves) {
        save += " " + v;
      }
      lines.push_back(std::move(save));
    }
    lines.insert(lines.end(), tb.lines.begin(), tb.lines.end());
    lines.emplace_back(".end");
    return diag::Done{};
  }
};

/// The include context of a raw deck (its first line a title, itself the
/// first level) or of a spice source's DUT include line (not a file: the
/// DUT file is the first level).
deck::IncludeContext include_context(const DeckContext& context, std::string source, bool raw) {
  return deck::IncludeContext{context.files, context.base_dir, context.limits, std::move(source),
                              context.pdk_store, raw, raw ? std::size_t{1} : std::size_t{0}};
}

/// Sanitises, validates and delivers `lines` into `out`.
diag::Outcome<PreparedDeck> finish(std::vector<std::string> lines, bool raw, bool allow_control, PreparedDeck out,
                                   const DeckLimits& limits) {
  // M9 R2 review: a line holding a line break would pass sanitisation as
  // one line and still be hashed like the lines it holds.
  if (diag::Status breaks = deck::check_line_breaks(lines); !breaks.ok()) {
    return breaks.error();
  }
  if (diag::Status clean = deck::sanitise(lines, allow_control, out.diagnostics); !clean.ok()) {
    return clean.error();
  }
  if (diag::Status valid = deck::validate(lines, raw); !valid.ok()) {
    return valid.error();
  }
  out.lines = deck::split_long_lines(lines);
  if (deck::deck_bytes(out.lines) > limits.max_deck_bytes) {
    return diag::make("SIM-DECK-003", "prepared deck larger than the limit of " + std::to_string(limits.max_deck_bytes) +
                                          " bytes");
  }
  pdk::Sha256 hash;
  for (const std::string& line : out.lines) {
    hash.update(line);
    hash.update("\n");
  }
  out.sha256 = hash.finish_hex();
  return out;
}

diag::Outcome<PreparedDeck> prepare_raw(const Testbench& tb, const DeckContext& context) {
  auto lines = deck::inline_includes(tb.lines, include_context(context, "raw deck", true));
  if (!lines.ok()) {
    return lines.error();
  }
  return finish(std::move(lines.value()), true, tb.allow_control, PreparedDeck{}, context.limits);
}

diag::Outcome<PreparedDeck> prepare_spice(const Testbench& tb, const DeckContext& context) {
  std::filesystem::path file{tb.dut.name};
  if (file.is_relative()) {
    file = std::filesystem::path{context.base_dir} / file;
  }
  const std::string include = ".include \"" + file.lexically_normal().generic_string() + "\"";
  auto dut = deck::inline_includes({include}, include_context(context, tb.name, false));
  if (!dut.ok()) {
    return dut.error();
  }
  const auto ports = subckt_ports(dut.value(), tb.subckt);
  if (!ports) {
    return diag::make("SIM-DECK-013", "subcircuit " + tb.subckt + " is not defined in " + tb.dut.name);
  }
  PreparedDeck out;
  Composer composer{tb, out, {}};
  (void)composer.header();
  composer.lines.insert(composer.lines.end(), dut.value().begin(), dut.value().end());
  if (diag::Status s = composer.instance(*ports); !s.ok()) {
    return s.error();
  }
  return finish(std::move(composer.lines), false, tb.allow_control, std::move(out), context.limits);
}

}  // namespace

diag::Status check_limits(const DeckLimits& limits) {
  const DeckLimits defaults;
  const std::array<std::tuple<const char*, std::size_t, std::size_t>, 3> settings = {{
      {"deck size", limits.max_deck_bytes, defaults.max_deck_bytes},
      {"include depth", limits.max_include_depth, defaults.max_include_depth},
      {"include count", limits.max_includes, defaults.max_includes},
  }};
  for (const auto& [name, value, fallback] : settings) {
    if (value == 0 || value > 2 * fallback) {
      return diag::make("SIM-DECK-017", std::string{name} + " limit " + std::to_string(value) +
                                            " is zero or above twice its default " + std::to_string(fallback));
    }
  }
  return diag::Done{};
}

diag::Outcome<std::string> stimulus_line(const Stimulus& s) {
  const char type = s.name.empty() ? '\0' : static_cast<char>(s.name.front() & ~0x20);
  if (type != 'V' && type != 'I') {
    return bad_source(s, "the name must start with V or I");
  }
  if (s.plus.empty() || s.minus.empty()) {
    return bad_source(s, "a node is missing");
  }
  if (auto error = check_counts(s)) {
    return *error;
  }
  if (auto error = check_values(s)) {
    return *error;
  }
  std::string line = s.name + " " + s.plus + " " + s.minus + " " + waveform(s);
  if (s.ac_magnitude) {
    line += " AC " + format_g12(*s.ac_magnitude);
    if (s.ac_phase) {
      line += " " + format_g12(*s.ac_phase);
    }
  }
  return line;
}

diag::Outcome<ModelDescriptor> ModelDescriptor::load(std::string_view /*pdk*/) {
  return not_implemented("sim::ModelDescriptor::load");
}

diag::Outcome<PreparedDeck> prepare(const Testbench& testbench, const DeckContext& context) {
  using Kind = CircuitSource::Kind;
  if (testbench.dut.kind == Kind::layout || testbench.dut.kind == Kind::schematic) {
    return not_implemented("sim::prepare (layout and schematic sources)");
  }
  if (context.files == nullptr) {
    return diag::make("SIM-DECK-013", "deck preparation needs a file system");
  }
  if (diag::Status limits = check_limits(context.limits); !limits.ok()) {
    return limits.error();
  }
  return testbench.dut.kind == Kind::raw ? prepare_raw(testbench, context) : prepare_spice(testbench, context);
}

}  // namespace hefesto::sim
