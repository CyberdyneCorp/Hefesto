// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Bundled corner tables, corner names and corner-set expansion
// (corners.hpp). The section words are those of simulation-corners
// "Bundled corner tables", read by the spec team from the pinned PDK
// sources.
//
// Owner: SIMADV.

#include "hefesto/sim/runset/corners.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#include "hefesto/sim/deck/spice_text.hpp"

namespace hefesto::sim {

namespace {

using Words = std::vector<std::pair<std::string, std::string>>;

const std::vector<double> kPvtSupplies{0.9, 1.0, 1.1};

CornerDefinition corner_def(std::string name, Words words, CornerKind kind = CornerKind::process,
                            std::string statistical = {}) {
  CornerDefinition c;
  c.name = std::move(name);
  c.words = std::move(words);
  c.kind = kind;
  c.statistical = std::move(statistical);
  return c;
}

// --- sky130A/B: one variable <proc> in sky130.lib.spice ---

CornerTable sky130_table(std::string_view pdk) {
  const std::vector<std::string> process{"tt",    "ss",    "ff",    "sf",    "fs",    "ll",    "hh",    "hl",
                                         "lh",    "ss_ll", "ss_hl", "ss_lh", "ss_hh", "sf_ll", "sf_hl", "sf_hh",
                                         "fs_ll", "fs_hl", "fs_lh", "fs_hh", "ff_ll", "ff_hl", "ff_lh", "ff_hh"};
  CornerTable t;
  t.pdk = std::string{pdk};
  SectionVariable proc{"proc", "sky130.lib.spice", {}, true};
  for (const std::string& w : process) {
    proc.words.push_back(w);
    t.corners.push_back(corner_def(w, {{"proc", w}}, CornerKind::process, w == "tt" ? "mc" : ""));
  }
  for (const std::string& w : process) {
    proc.words.push_back(w + "_mm");
    t.corners.push_back(corner_def(w + "_mm", {{"proc", w + "_mm"}}, CornerKind::mismatch));
  }
  proc.words.emplace_back("mc");
  t.corners.push_back(corner_def("mc", {{"proc", "mc"}}, CornerKind::statistical));
  t.variables.push_back(std::move(proc));
  t.sets.push_back(CornerSetDefinition{"pvt", {"tt", "ss", "ff", "sf", "fs"}, {-40, 27, 125}, kPvtSupplies, {}});
  t.default_corner = "tt";
  t.default_set = "pvt";
  return t;
}

// --- gf180mcuC/D: <mos> and the optional variables, all in sm141064.ngspice ---

struct GfVariable {
  const char* name;
  const char* prefix;     // `res_`, `mimcap_`, ...
  bool has_statistical;
};

constexpr GfVariable kGfOptional[] = {{"res", "res_", true},
                                       {"cap", "mimcap_", true},
                                       {"bjt", "bjt_", true},
                                       {"moscap", "moscap_", false},
                                       {"dio", "diode_", false}};

SectionVariable gf180_variable(const GfVariable& v) {
  SectionVariable var{v.name, "sm141064.ngspice", {}, false};
  for (const char* suffix : {"typical", "ff", "ss"}) {
    var.words.push_back(std::string{v.prefix} + suffix);
  }
  if (v.has_statistical) {
    var.words.push_back(std::string{v.prefix} + "statistical");
  }
  return var;
}

CornerTable gf180_table(std::string_view pdk) {
  CornerTable t;
  t.pdk = std::string{pdk};
  t.variables.push_back(SectionVariable{"mos", "sm141064.ngspice", {"typical", "ff", "ss", "fs", "sf", "statistical"}, true});
  const auto& optional = kGfOptional;
  for (const GfVariable& v : optional) {
    t.variables.push_back(gf180_variable(v));
  }
  for (const char* c : {"typical", "ff", "ss", "fs", "sf"}) {
    const std::string corner{c};
    const std::string suffix = corner == "ff" || corner == "ss" ? corner : "typical";
    Words words{{"mos", corner}};
    for (const GfVariable& v : optional) {
      words.emplace_back(v.name, std::string{v.prefix} + suffix);
    }
    t.corners.push_back(corner_def(corner, std::move(words), CornerKind::process,
                                   corner == "typical" ? "statistical" : ""));
  }
  Words stat{{"mos", "statistical"}};
  for (const GfVariable& v : optional) {
    stat.emplace_back(v.name, v.has_statistical ? std::string{v.prefix} + "statistical" : std::string{"-"});
  }
  t.corners.push_back(corner_def("statistical", std::move(stat), CornerKind::statistical));
  t.sets.push_back(CornerSetDefinition{"pvt", {"typical", "ff", "ss", "fs", "sf"}, {-40, 25, 125}, kPvtSupplies, {}});
  t.default_corner = "typical";
  t.default_set = "pvt";
  return t;
}

// --- sg13g2: independent variables, one file each ---

struct IhpVariable {
  const char* name;
  const char* file;
  std::vector<std::string> process;
  bool mismatch;     // `<word>_mismatch` for each process word
  const char* stat;  // the statistical word
};

std::vector<IhpVariable> ihp_variables() {
  const std::vector<std::string> mos{"mos_tt", "mos_ss", "mos_ff", "mos_sf", "mos_fs"};
  return {{"mos", "cornerMOSlv.lib", mos, true, "mos_tt_stat"},
          {"moshv", "cornerMOShv.lib", mos, true, "mos_tt_stat"},
          {"res", "cornerRES.lib", {"res_typ", "res_bcs", "res_wcs"}, true, "res_stat"},
          {"cap", "cornerCAP.lib", {"cap_typ", "cap_bcs", "cap_wcs"}, true, "cap_typ_stat"},
          {"hbt", "cornerHBT.lib", {"hbt_typ", "hbt_bcs", "hbt_wcs"}, true, "hbt_typ_stat"},
          {"dio", "cornerDIO.lib", {"dio_tt", "dio_ss", "dio_ff"}, false, "dio_tt_stat"},
          {"moscap", "cornerMOSCAP.lib", {"moscap_tt"}, true, "moscap_tt_stat"}};
}

void add_ihp_corners(CornerTable& t, const std::vector<IhpVariable>& vars) {
  for (const char* c : {"tt", "ss", "ff", "sf", "fs"}) {
    Words process;
    Words mismatch;
    for (const IhpVariable& v : vars) {
      const bool mos = v.process.front().starts_with("mos_");
      const std::string word = mos ? "mos_" + std::string{c} : v.process.front();
      process.emplace_back(v.name, word);
      mismatch.emplace_back(v.name, v.mismatch ? word + "_mismatch" : word);
    }
    t.corners.push_back(corner_def(c, std::move(process), CornerKind::process, std::string{c} == "tt" ? "stat" : ""));
    t.corners.push_back(corner_def(std::string{c} + "_mismatch", std::move(mismatch), CornerKind::mismatch));
  }
  Words stat;
  for (const IhpVariable& v : vars) {
    stat.emplace_back(v.name, v.stat);
  }
  t.corners.push_back(corner_def("stat", std::move(stat), CornerKind::statistical));
}

CornerTable sg13g2_table(std::string_view pdk) {
  CornerTable t;
  t.pdk = std::string{pdk};
  const std::vector<IhpVariable> vars = ihp_variables();
  for (const IhpVariable& v : vars) {
    SectionVariable var{v.name, v.file, v.process, std::string_view{v.name} == "mos"};
    if (v.mismatch) {
      for (const std::string& w : v.process) {
        var.words.push_back(w + "_mismatch");
      }
    }
    var.words.emplace_back(v.stat);
    t.variables.push_back(std::move(var));
  }
  add_ihp_corners(t, vars);
  t.sets.push_back(CornerSetDefinition{"pvt", {"tt", "ss", "ff", "sf", "fs"}, {-40, 27, 125}, kPvtSupplies, {}});
  t.default_corner = "tt";
  t.default_set = "pvt";
  return t;
}

std::vector<CornerTuple> tuples_of(const CornerSetDefinition& set) {
  if (!set.tuples.empty()) {
    return set.tuples;
  }
  std::vector<CornerTuple> out;
  for (const std::string& c : set.corners) {
    for (const double temp : set.temperatures) {
      for (const double supply : set.supplies) {
        out.push_back(CornerTuple{c, temp, supply});
      }
    }
  }
  return out;
}

template <typename T>
const T* find_named(const std::vector<T>& items, std::string_view name) {
  const auto it = std::find_if(items.begin(), items.end(), [name](const T& x) { return x.name == name; });
  return it == items.end() ? nullptr : &*it;
}

bool has_word(const SectionVariable& v, std::string_view word) {
  return std::find(v.words.begin(), v.words.end(), word) != v.words.end();
}

void add_or_replace(std::vector<CornerDefinition>& corners, const CornerDefinition& c,
                    std::vector<diag::Diagnostic>& warnings) {
  const auto it = std::find_if(corners.begin(), corners.end(), [&c](const CornerDefinition& x) { return x.name == c.name; });
  if (it == corners.end()) {
    corners.push_back(c);
    return;
  }
  warnings.push_back(diag::make("SIM-RUNSET-013", "Testbench corner " + c.name + " overrides the PDK corner"));
  *it = c;
}

}  // namespace

const CornerDefinition* CornerTable::corner(std::string_view name) const {
  return find_named(corners, name);
}

const SectionVariable* CornerTable::variable(std::string_view name) const {
  return find_named(variables, name);
}

const CornerSetDefinition* CornerTable::set(std::string_view name) const {
  return find_named(sets, name);
}

std::vector<std::pair<std::string, std::string>> CornerTable::words_of(const CornerDefinition& c) const {
  const CornerDefinition* fallback = corner(default_corner);
  const auto word_in = [](const CornerDefinition* d, const std::string& var) -> std::string {
    if (d == nullptr) {
      return "-";
    }
    const auto it = std::find_if(d->words.begin(), d->words.end(), [&var](const auto& w) { return w.first == var; });
    return it == d->words.end() ? "-" : it->second;
  };
  Words out;
  for (const SectionVariable& v : variables) {
    std::string word = word_in(&c, v.name);
    if (word == "-") {
      word = word_in(fallback, v.name);
    }
    out.emplace_back(v.name, word);
  }
  return out;
}

diag::Outcome<CornerTable> bundled_corner_table(std::string_view pdk) {
  if (pdk == "sky130A" || pdk == "sky130B") {
    return sky130_table(pdk);
  }
  if (pdk == "gf180mcuC" || pdk == "gf180mcuD") {
    return gf180_table(pdk);
  }
  if (pdk == "sg13g2" || pdk == "ihp-sg13g2") {
    return sg13g2_table(pdk);
  }
  return diag::make("SIM-RUNSET-007", "no bundled corner table for PDK " + std::string{pdk});
}

std::string corner_name(std::string_view section, double temperature, double supply_volts) {
  const long temp = std::lround(temperature);
  std::string t = temp < 0 ? "m" + std::to_string(-temp) : std::to_string(temp);
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%g", supply_volts);
  std::string s{buffer};
  std::replace(s.begin(), s.end(), '.', 'v');
  return std::string{section} + "_" + t + "_" + s;
}

diag::Outcome<std::vector<CornerLevel>> expand_corner_set(const CornerTable& table, std::string_view set_name,
                                                          double nominal_supply) {
  const CornerSetDefinition* set = table.set(set_name);
  if (set == nullptr) {
    return diag::make("SIM-RUNSET-007", "no corner set " + std::string{set_name});
  }
  std::vector<CornerLevel> out;
  std::set<std::string> names;
  for (const CornerTuple& tuple : tuples_of(*set)) {
    const CornerDefinition* c = table.corner(tuple.corner);
    if (c == nullptr || c->kind != CornerKind::process) {
      return diag::make("SIM-RUNSET-007", "corner set " + set->name + ": " + tuple.corner + " is not a process corner");
    }
    const double temp = c->temperature.value_or(tuple.temperature);
    const double supply = c->supply.value_or(tuple.supply);
    CornerLevel level{corner_name(c->name, temp, nominal_supply * supply), c->name, temp, supply};
    if (!names.insert(level.name).second) {
      return diag::make("SIM-RUNSET-007", "Duplicate corner " + level.name + " in corner set " + set->name);
    }
    out.push_back(std::move(level));
  }
  if (out.empty()) {
    return diag::make("SIM-RUNSET-002", "Run set has no points");
  }
  return out;
}

diag::Status check_corner_words(const CornerTable& table, const CornerDefinition& corner) {
  for (const auto& [var, word] : corner.words) {
    const SectionVariable* v = table.variable(var);
    if (v == nullptr || (word != "-" && !has_word(*v, word))) {
      return diag::make("SIM-RUNSET-008", "sim corners: " + var + " has no section " + word);
    }
  }
  return diag::Done{};
}

diag::Outcome<CornerTable> merge_user_corners(const CornerTable& pdk, const UserCorners& user,
                                              std::vector<diag::Diagnostic>& warnings) {
  CornerTable out = pdk;
  for (const CornerDefinition& c : user.corners) {
    if (auto status = check_corner_words(out, c); !status.ok()) {
      return status.error();
    }
    add_or_replace(out.corners, c, warnings);
  }
  for (const CornerSetDefinition& s : user.sets) {
    const auto it = std::find_if(out.sets.begin(), out.sets.end(), [&s](const auto& x) { return x.name == s.name; });
    if (it == out.sets.end()) {
      out.sets.push_back(s);
    } else {
      *it = s;
    }
  }
  return out;
}

std::vector<std::string> library_sections(std::string_view text) {
  std::vector<std::string> out;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t end = std::min(text.find('\n', pos), text.size());
    const std::vector<std::string> tokens = spice_tokens(text.substr(pos, end - pos));
    if (tokens.size() == 2 && ascii_lower(tokens[0]) == ".lib") {
      out.push_back(tokens[1]);  // `.lib <name>` opens a section; `.lib <file> <name>` refers to one
    }
    pos = end + 1;
  }
  return out;
}

diag::Status check_corner_sections(const CornerTable& table,
                                   const std::map<std::string, std::vector<std::string>>& sections) {
  for (const CornerDefinition& c : table.corners) {
    for (const auto& [var, word] : c.words) {
      const SectionVariable* v = table.variable(var);
      if (v == nullptr || word == "-") {
        continue;
      }
      const auto file = sections.find(v->file);
      const bool found = file != sections.end() &&
                         std::find(file->second.begin(), file->second.end(), word) != file->second.end();
      if (!found) {
        return diag::make("SIM-RUNSET-007", "Corner " + c.name + ": section " + word + " not found in " + v->file);
      }
    }
  }
  return diag::Done{};
}

diag::Outcome<CornerApplication> corner_application(const CornerTable& table, const CornerLevel& level) {
  const CornerDefinition* c = table.corner(level.corner);
  if (c == nullptr) {
    return diag::make("SIM-RUNSET-007", "no corner " + level.corner);
  }
  return CornerApplication{table.words_of(*c), c->params, level.temperature, level.supply};
}

}  // namespace hefesto::sim
