// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Monte Carlo modes, levels, sample decks and early stopping
// (montecarlo.hpp).
//
// Owner: SIMADV.

#include "hefesto/sim/runset/montecarlo.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "hefesto/sim/deck/spice_text.hpp"
#include "sim/runset/words.hpp"
#include "hefesto/sim/runset/random.hpp"

namespace hefesto::sim {

namespace {

// The bundled mode definitions of one PDK family ("Monte Carlo modes").
struct ModeTable {
  std::string display;        // the PDK name in refusals
  std::string mismatch_corner;  // `<c>` + this suffix names the mismatch corner; empty: the corner itself
  std::vector<std::string> process_params;
  std::vector<std::string> mismatch_params;
  std::vector<std::string> both_params;
  bool both = true;
};

ModeTable mode_table(std::string_view pdk) {
  if (pdk.starts_with("sky130")) {
    return {std::string{pdk}, "_mm", {}, {}, {".param MC_MM_SWITCH=1"}, true};
  }
  if (pdk.starts_with("gf180")) {
    return {std::string{pdk}, "", {".param sw_stat_global=1"}, {".param sw_stat_mismatch=1"},
            {".param sw_stat_global=1", ".param sw_stat_mismatch=1"}, true};
  }
  return {"sg13g2", "_mismatch", {}, {}, {}, false};
}

diag::Diagnostic not_at_corner(McMode mode, std::string_view corner) {
  return diag::make("SIM-RUNSET-009", "Monte Carlo mode " + std::string{mc_mode_name(mode)} +
                                          " is not defined at corner " + std::string{corner});
}

diag::Outcome<McApplication> statistical(const CornerTable& table, const CornerDefinition& corner, McMode mode,
                                         const std::vector<std::string>& params) {
  const CornerDefinition* stat = corner.statistical.empty() ? nullptr : table.corner(corner.statistical);
  if (stat == nullptr) {
    return not_at_corner(mode, corner.name);
  }
  return McApplication{table.words_of(*stat), params};
}

diag::Outcome<McApplication> mismatch(const CornerTable& table, const CornerDefinition& corner,
                                      const ModeTable& modes) {
  const CornerDefinition* mm = modes.mismatch_corner.empty() ? &corner : table.corner(corner.name + modes.mismatch_corner);
  if (mm == nullptr) {
    return not_at_corner(McMode::mismatch, corner.name);
  }
  return McApplication{table.words_of(*mm), modes.mismatch_params};
}

std::string lower_token(std::string_view token) {
  return ascii_lower(token);
}

bool sets_variable(const std::vector<std::string>& tokens, std::string_view name) {
  return std::any_of(tokens.begin(), tokens.end(), [name](const std::string& t) {
    const std::string l = lower_token(t);
    return l == name || l.starts_with(std::string{name} + "=");
  });
}

bool is_seed_option(const std::string& token) {
  return lower_token(token).starts_with("seed=") || lower_token(token) == "seed";
}

// A top-level `.option` line without its `seed=` assignment; nullopt when it
// carried none, an empty string when nothing else is left.
std::optional<std::string> without_seed(const std::vector<std::string>& tokens) {
  std::vector<std::string> kept;
  bool removed = false;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (i > 0 && is_seed_option(tokens[i])) {
      removed = true;
      const bool split = lower_token(tokens[i]) == "seed";  // `seed = 12`
      i += split && i + 2 < tokens.size() && tokens[i + 1] == "=" ? 2U : 0U;
      continue;
    }
    kept.push_back(tokens[i]);
  }
  if (!removed) {
    return std::nullopt;
  }
  if (kept.size() <= 1) {
    return std::string{};
  }
  std::string line = kept.front();
  for (std::size_t i = 1; i < kept.size(); ++i) {
    line += " " + kept[i];
  }
  return line;
}

bool is_control_seed(const std::vector<std::string>& tokens) {
  if (tokens.empty()) {
    return false;
  }
  const std::string first = lower_token(tokens[0]);
  return first == "setseed" || (first == "set" && tokens.size() > 1 && lower_token(tokens[1]).starts_with("rndseed"));
}

diag::Status check_supported(const std::vector<std::string>& tokens) {
  for (const std::string_view variable : {"stat_param_as_func", "statlocal"}) {
    if (sets_variable(tokens, variable)) {
      return diag::make("SIM-RUNSET-011", std::string{variable} + " is not supported in Monte Carlo runs");
    }
  }
  return diag::Done{};
}

// The line without its seed setting (empty: drop the line), or nullopt.
std::optional<std::string> seed_change(const std::vector<std::string>& tokens, const std::string& first, bool control) {
  if (control) {
    return is_control_seed(tokens) ? std::optional<std::string>{std::string{}} : std::nullopt;
  }
  return first == ".option" || first == ".options" ? without_seed(tokens) : std::nullopt;
}

bool is_pdk_device(std::string_view model) {
  constexpr std::array<std::string_view, 8> kPrefixes{"sg13_", "npn13g2", "pnpmpa", "rsil",
                                                      "rppd",  "rhigh",   "cap_cmim", "cap_rfcmim"};
  const std::string m = ascii_lower(model);
  return std::any_of(kPrefixes.begin(), kPrefixes.end(), [&m](std::string_view p) { return m.starts_with(p); });
}

}  // namespace

std::string_view mc_mode_name(McMode mode) noexcept {
  switch (mode) {
    case McMode::process: return "process";
    case McMode::mismatch: return "mismatch";
    case McMode::both: return "both";
  }
  return "process";
}

diag::Status check_monte_carlo(const MonteCarloSpec& spec, std::uint32_t max_samples) {
  if (spec.runs < 1 || spec.runs > max_samples) {
    return diag::make("SIM-RUNSET-010", "sim mc: runs must be between 1 and " + std::to_string(max_samples));
  }
  if (spec.base_seed < 1 || spec.base_seed > kMaxBaseSeed) {
    return diag::make("SIM-RUNSET-010", "sim mc: seed must be between 1 and " + std::to_string(kMaxBaseSeed));
  }
  if (spec.stop && !(*spec.stop > 0.0 && *spec.stop < 1.0)) {
    return diag::make("SIM-RUNSET-010", "sim mc: stop must be between 0 and 1");
  }
  return diag::Done{};
}

std::string McLevel::label() const {
  return nominal ? std::string{"nom"} : std::to_string(index);
}

std::uint32_t McLevel::seed(std::uint32_t base_seed) const {
  return nominal ? kNominalSeed : sample_seed_value(base_seed, index);
}

std::vector<McLevel> mc_levels(const MonteCarloSpec& spec) {
  std::vector<McLevel> out;
  out.reserve(spec.runs + (spec.nominal ? 1U : 0U));
  if (spec.nominal) {
    out.push_back(McLevel{true, 0});
  }
  for (std::uint32_t i = 0; i < spec.runs; ++i) {
    out.push_back(McLevel{false, i});
  }
  return out;
}

diag::Outcome<McApplication> mc_application(const CornerTable& table, McMode mode, std::string_view corner_name) {
  const ModeTable modes = mode_table(table.pdk);
  if (mode == McMode::both && !modes.both) {
    return diag::make("SIM-RUNSET-009", modes.display + " does not define Monte Carlo mode both");
  }
  const CornerDefinition* corner = table.corner(corner_name);
  if (corner == nullptr || corner->kind != CornerKind::process) {
    return not_at_corner(mode, corner_name);
  }
  switch (mode) {
    case McMode::process: return statistical(table, *corner, mode, modes.process_params);
    case McMode::mismatch: return mismatch(table, *corner, modes);
    case McMode::both: return statistical(table, *corner, mode, modes.both_params);
  }
  return not_at_corner(mode, corner_name);
}

diag::Outcome<std::string> stat_parameter_line(const StatParameter& p, McMode mode, bool nominal) {
  static constexpr std::array<std::string_view, 5> kNames{"agauss", "gauss", "aunif", "unif", "limit"};
  const auto d = static_cast<std::size_t>(p.distribution);
  const std::size_t arity = d < 2 ? 3 : 2;
  if (p.arguments.size() != arity) {
    return diag::make("SIM-RUNSET-010", "sim mc: " + std::string{kNames[d]} + " needs " + std::to_string(arity) +
                                            " arguments");
  }
  const bool varies = mode == McMode::both || (mode == McMode::mismatch) == p.mismatch;
  if (nominal || !varies) {
    return ".param " + p.name + "=" + format_g12(p.arguments[0]);
  }
  std::string args;
  for (const double a : p.arguments) {
    args += (args.empty() ? "" : ",") + format_g12(a);
  }
  return ".param " + p.name + "='" + std::string{kNames[d]} + "(" + args + ")'";
}

diag::Outcome<std::vector<std::string>> strip_seed_settings(const std::vector<std::string>& lines,
                                                           std::vector<diag::Diagnostic>& warnings) {
  std::vector<std::string> out;
  bool control = false;
  for (std::size_t n = 0; n < lines.size(); ++n) {
    const std::vector<std::string> tokens = runset_text::words(lines[n]);
    if (auto status = check_supported(tokens); !status.ok()) {
      return status.error();
    }
    const std::string first = tokens.empty() ? std::string{} : lower_token(tokens[0]);
    control = (control || first == ".control") && first != ".endc";
    const std::optional<std::string> changed = seed_change(tokens, first, control);
    if (!changed) {
      out.push_back(lines[n]);
      continue;
    }
    warnings.push_back(diag::make("SIM-RUNSET-012", "Removed seed setting on line " + std::to_string(n + 1) +
                                                        "; Monte Carlo seeds are set by Hefesto"));
    if (!changed->empty()) {
      out.push_back(*changed);
    }
  }
  return out;
}

std::vector<diag::Diagnostic> mm_ok_warnings(const std::vector<std::string>& dut_lines) {
  std::vector<diag::Diagnostic> out;
  for (const std::string& line : dut_lines) {
    const std::vector<std::string> tokens = runset_text::words(line);
    if (tokens.size() < 2 || (tokens[0][0] != 'X' && tokens[0][0] != 'x')) {
      continue;
    }
    const auto model = std::find_if(tokens.rbegin(), tokens.rend(),
                                    [](const std::string& t) { return t.find('=') == std::string::npos; });
    if (model == tokens.rend() || !is_pdk_device(*model)) {
      continue;
    }
    const bool ok = std::any_of(tokens.begin(), tokens.end(), [](const std::string& t) {
      const std::string l = ascii_lower(t);
      return l.starts_with("mm_ok=") && parse_spice_number(l.substr(6)).value_or(0.0) != 0.0;
    });
    if (!ok) {
      out.push_back(diag::make("SIM-RUNSET-014", "Monte Carlo mismatch: instance " + tokens[0] +
                                                     " has mm_ok=0; it will not vary"));
    }
  }
  return out;
}

std::optional<double> wilson_half_width(std::size_t passes, std::size_t counted) {
  if (counted == 0) {
    return std::nullopt;
  }
  constexpr double z = 1.959963984540054;
  const double m = static_cast<double>(counted);
  const double y = static_cast<double>(passes) / m;
  return z * std::sqrt(y * (1.0 - y) / m + z * z / (4.0 * m * m)) / (1.0 + z * z / m);
}

std::optional<std::size_t> early_stop(const std::vector<SampleOutcome>& samples, double h, bool exclude_failed) {
  std::size_t passes = 0;
  std::size_t counted = 0;
  constexpr std::size_t kCheckpoint = 50;
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const SampleOutcome& s = samples[i];
    if (!s.final) {
      return std::nullopt;  // a checkpoint uses samples 0..m-1 only when all of them are final
    }
    if (!(exclude_failed && s.failed)) {
      ++counted;
      passes += s.pass ? 1U : 0U;
    }
    if ((i + 1) % kCheckpoint == 0) {
      const auto hw = wilson_half_width(passes, counted);
      if (hw && *hw <= h) {
        return i + 1;
      }
    }
  }
  return std::nullopt;
}

}  // namespace hefesto::sim
