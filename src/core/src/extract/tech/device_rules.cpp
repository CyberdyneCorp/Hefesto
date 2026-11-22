// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `device` and `fet` (capability extract-tech-section "Device declaration
// parsing"; Q-EXTRACT-2 replicate; openspec/changes/m7-extraction/design.md
// D7, tasks.md 4.5).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c ExtTechLine
// (FET :2338-2405, DEVICE :2433-2945), Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// Magic parses a device line in place: trailing parameter, limit and
// identifier arguments are cut off by lowering argc, and the positional
// parsing may still read the cut-off words (or past the end, which crashes
// Magic; here a missing word reads as empty, in both modes). Args keeps
// the words and argc the same way. A negative subcircuit terminal count
// (Magic writes before its array) counts as 0, and more than five terminals
// (Magic overflows a six-entry array) are kept, in both modes.
//
// Owner: EXTTECH.

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

#include "rule_context.hpp"

namespace hefesto::extract::detail {

namespace {

// The words of the line as Magic's argv, with argc cut by the option
// parsing. at() of a word past the end is empty.
struct Args {
  Tokens v;
  std::size_t argc = 0;

  [[nodiscard]] const std::string& at(std::size_t i) const {
    static const std::string kEmpty;
    return i < v.size() ? v[i] : kEmpty;
  }
  [[nodiscard]] std::string& last() { return v[argc - 1]; }
};

struct ClassEntry {
  std::string_view name;
  DeviceClass device_class;
};

// devTable of ExtTech.c, in its order.
constexpr std::array<ClassEntry, 14> kClasses{{
    {"mosfet", DeviceClass::mosfet},     {"bjt", DeviceClass::bjt},
    {"capacitor", DeviceClass::devcap},  {"capreverse", DeviceClass::devcaprev},
    {"resistor", DeviceClass::devres},   {"diode", DeviceClass::diode},
    {"pdiode", DeviceClass::pdiode},     {"ndiode", DeviceClass::ndiode},
    {"subcircuit", DeviceClass::subckt}, {"rsubcircuit", DeviceClass::rsubckt},
    {"msubcircuit", DeviceClass::msubckt}, {"csubcircuit", DeviceClass::csubckt},
    {"dsubcircuit", DeviceClass::dsubckt}, {"veriloga", DeviceClass::veriloga},
}};

// The positional result of one device line.
struct Parsed {
  DeviceClass device_class = DeviceClass::mosfet;
  std::vector<tech::TypeMask> terms;
  tech::TypeMask subs;
  std::optional<std::string> subs_name;
  std::int32_t nterm = 0;
  double gscap = 0.0;
  double gccap = 0.0;
};

tech::TypeMask noisy_or_empty(const RuleContext& ctx, const std::string& text) {
  return text.empty() ? tech::TypeMask{} : ctx.noisy_types(text);
}

void report_long_name(const RuleContext& ctx, const std::string& word) {
  ctx.report(kCodeBadDevice, "Parameter name " + word + " can be no more thantwo characters.", Print::tech_error);
}

// One `k=name[*scale|+offset|-offset]` argument (the '=' at `eq`).
DeviceParam parameter_option(const RuleContext& ctx, std::string& word, std::size_t eq) {
  DeviceParam param;
  param.code = word.substr(0, eq == 2 ? 2 : 1);
  if (eq > 2) {
    report_long_name(ctx, word);
  }
  std::string name = word.substr(eq + 1);
  if (const std::size_t mult = name.find('*'); mult != std::string::npos) {
    param.scale = std::atof(name.c_str() + mult + 1);
    name.resize(mult);
  } else if (const std::size_t plus = name.find('+'); plus != std::string::npos) {
    param.offset = saturate_int32(0.5 + (std::atof(name.c_str() + plus + 1) * 1000));
    name.resize(plus);
  } else if (const std::size_t minus = name.find('-'); minus != std::string::npos) {
    param.offset = saturate_int32(0.5 + (-std::atof(name.c_str() + minus + 1) * 1000));
    name.resize(minus);
  }
  word.resize(eq + 1 + name.size());  // Magic cuts the word at '*', '+' or '-'
  param.name = std::move(name);
  return param;
}

// Step 1: the trailing `k=name` arguments, prepended so the list ends up in
// declaration order.
void parameter_options(const RuleContext& ctx, Args& a, std::vector<DeviceParam>& params) {
  while (a.argc > 0) {
    std::string& word = a.last();
    const std::size_t eq = word.find('=');
    if (eq == std::string::npos || (eq > 0 && (word[eq - 1] == '>' || word[eq - 1] == '<'))) {
      return;
    }
    params.insert(params.begin(), parameter_option(ctx, word, eq));
    --a.argc;
  }
}

DeviceParam& limit_param(std::vector<DeviceParam>& params, const std::string& key) {
  const auto it = std::find_if(params.begin(), params.end(), [&](const DeviceParam& p) { return p.code == key; });
  if (it != params.end()) {
    return *it;
  }
  DeviceParam param;
  param.code = key;
  params.insert(params.begin(), param);
  return params.front();
}

// One `k>v`, `k>=v`, `k<v`, `k<=v` argument; false ends the limit parsing
// (Magic's break, the word cut at the operator).
bool limit_option(const RuleContext& ctx, Args& a, std::vector<DeviceParam>& params) {
  std::string& word = a.last();
  std::size_t at = word.find('<');
  if (at == std::string::npos) {
    at = word.find('>');
  }
  if (at == std::string::npos) {
    return false;
  }
  const char cond = word[at];
  std::string value = word.substr(at + 1);
  word.resize(at);
  if (at > 3) {
    report_long_name(ctx, word);
    return false;
  }
  DeviceParam& param = limit_param(params, word.substr(0, 2));
  const bool equal = !value.empty() && value.front() == '=';
  if (equal) {
    value.erase(0, 1);
  }
  const std::optional<double> dval = scan_double(value);
  if (!dval) {
    ctx.report(kCodeBadLimit,
               "Non-numeric limit \"" + value + "\" for parameter \"" + std::string(1, cond) + word + "\".",
               Print::plain);
    return false;
  }
  const std::int64_t ival = saturate_int32(0.5 + (*dval * 1000));
  // `ival + 1` / `ival - 1` saturate too (Q-EXTRACT-30).
  if (cond == '>') {
    param.minimum = static_cast<double>(saturate_int32(static_cast<double>(equal ? ival : ival + 1)));
  } else {
    param.maximum = static_cast<double>(saturate_int32(static_cast<double>(equal ? ival : ival - 1)));
  }
  --a.argc;
  return true;
}

void bjt(const RuleContext& ctx, const Args& a, Parsed& p) {
  p.terms = {ctx.noisy_types(a.at(4))};
  p.subs = noisy_or_empty(ctx, a.at(5));
  p.nterm = 1;
}

void asymmetric_mosfet(const RuleContext& ctx, const Args& a, Parsed& p) {
  const tech::TypeMask source = noisy_or_empty(ctx, a.at(4));
  tech::TypeMask drain = noisy_or_empty(ctx, a.at(5));
  if (source == drain) {
    drain = {};  // symmetric
  } else {
    if (source.intersects(drain)) {
      ctx.report(kCodeBadDevice, "Device mosfet " + a.at(2) + " has overlapping drain and source types!",
                 Print::tech_error);
    }
    p.device_class = DeviceClass::asymmetric;
  }
  p.terms = {source, drain};
  if (a.at(6) != "None") {
    p.subs = noisy_or_empty(ctx, a.at(6));
  }
  p.subs_name = a.at(7);
  p.gscap = a.argc > 8 ? ctx.cap_value(a.at(8)) : 0.0;
  p.gccap = a.argc > 9 ? ctx.cap_value(a.at(9)) : 0.0;
  p.nterm = 2;
}

void mosfet(const RuleContext& ctx, const Args& a, Parsed& p) {
  if (a.argc > 7 && !str_is_numeric(a.at(7))) {
    asymmetric_mosfet(ctx, a, p);
    return;
  }
  p.terms = {noisy_or_empty(ctx, a.at(4))};
  if (a.at(5) != "None") {
    p.subs = noisy_or_empty(ctx, a.at(5));
  }
  if (a.argc > 6) {
    p.subs_name = a.at(6);
  }
  p.gscap = a.argc > 7 ? ctx.cap_value(a.at(7)) : 0.0;
  p.gccap = a.argc > 8 ? ctx.cap_value(a.at(8)) : 0.0;
  p.nterm = 2;
}

// Q-EXTRACT-2 (replicate): OTHER is the terminal and the substrate types.
void diode(const RuleContext& ctx, const Args& a, Parsed& p) {
  p.terms = {noisy_or_empty(ctx, a.at(4))};
  p.nterm = 1;
  if (a.argc > 4 && a.at(4) != "None") {
    p.subs = noisy_or_empty(ctx, a.at(4));
  }
  if (a.argc > 5) {
    p.subs_name = a.at(5);
  }
}

void resistor(const RuleContext& ctx, const Args& a, Parsed& p) {
  p.terms = {noisy_or_empty(ctx, a.at(4))};
  p.nterm = 2;
  if (a.argc > 5 && a.at(5) != "None") {
    p.subs = noisy_or_empty(ctx, a.at(5));
  }
  if (a.argc > 6) {
    p.subs_name = a.at(6);
  }
}

void capacitor(const RuleContext& ctx, Args& a, Parsed& p) {
  p.terms = {noisy_or_empty(ctx, a.at(4))};
  p.nterm = 1;
  if (a.argc > 5 && str_is_numeric(a.at(a.argc - 1))) {
    p.gccap = ctx.cap_value(a.at(a.argc - 1));
    --a.argc;
    if (a.argc > 5 && str_is_numeric(a.at(a.argc - 1))) {
      p.gscap = ctx.cap_value(a.at(a.argc - 1));
      --a.argc;
    }
  }
  if (a.argc > 5 && a.at(5) != "None") {
    p.subs = noisy_or_empty(ctx, a.at(5));
  }
  if (a.argc > 6) {
    p.subs_name = a.at(6);
  }
}

// Whether `text` is a type list for the subcircuit test
// (`DBTechNameMask(...) <= 0` is false): a positive plane mask, which needs
// a user type (or a `/plane` element) and not space alone.
bool is_type_list(const RuleContext& ctx, const std::string& text) {
  const std::optional<tech::TypeMask> types = ctx.quiet_types(text);
  if (!types || types->empty() || *types == tech::TypeMask::of(tech::kSpace)) {
    return false;
  }
  for (auto t = static_cast<std::size_t>(tech::kFirstUserType); t < ctx.tech.user_type_end(); ++t) {
    if (types->test(static_cast<tech::TileType>(t))) {
      return true;
    }
  }
  return text.find('/') != std::string::npos;
}

void subcircuit(const RuleContext& ctx, Args& a, Parsed& p) {
  if (a.argc >= 2 && !is_type_list(ctx, a.at(a.argc - 1))) {
    if (a.at(a.argc - 2) != "None") {
      p.subs = noisy_or_empty(ctx, a.at(a.argc - 2));
    }
    p.subs_name = a.at(a.argc - 1);
    a.argc -= 2;
  }
  const auto available = static_cast<std::int32_t>(a.argc) - 5;
  std::size_t iterm = 4;
  std::int32_t nterm = static_cast<std::int32_t>(a.argc) - 4;
  if (str_is_int(a.at(4))) {
    nterm = std::max(0, std::atoi(a.at(4).c_str()));
    iterm = 5;
    if (nterm > available) {
      ctx.report(kCodeBadDevice,
                 "Not enough terminals for subcircuit, " + std::to_string(nterm) + " were required, " +
                     std::to_string(available) + " found.",
                 Print::tech_error);
      nterm = std::max(0, available);
    }
  }
  nterm = std::max(0, nterm);
  for (std::size_t i = iterm; i < iterm + static_cast<std::size_t>(nterm); ++i) {
    p.terms.push_back(noisy_or_empty(ctx, a.at(i)));
  }
  if (nterm == 2 && p.terms[1] == p.terms[0]) {
    p.terms[1] = {};
  }
  p.nterm = nterm;
}

void terminal_subcircuit(const RuleContext& ctx, const Args& a, Parsed& p) {
  p.nterm = p.device_class == DeviceClass::rsubckt ? 2 : 1;
  p.terms = {noisy_or_empty(ctx, a.at(4))};
  if (a.argc > 5 && a.at(5) != "None" && a.at(5).find('=') == std::string::npos) {
    p.subs = noisy_or_empty(ctx, a.at(5));
  }
  if (a.argc > 6 && a.at(6).find('=') == std::string::npos) {
    p.subs_name = a.at(6);
  }
}

void positional(const RuleContext& ctx, Args& a, Parsed& p) {
  switch (p.device_class) {
    case DeviceClass::bjt:
      bjt(ctx, a, p);
      break;
    case DeviceClass::mosfet:
      mosfet(ctx, a, p);
      break;
    case DeviceClass::diode:
    case DeviceClass::pdiode:
    case DeviceClass::ndiode:
      diode(ctx, a, p);
      break;
    case DeviceClass::devres:
      resistor(ctx, a, p);
      break;
    case DeviceClass::devcap:
    case DeviceClass::devcaprev:
      capacitor(ctx, a, p);
      break;
    case DeviceClass::subckt:
    case DeviceClass::msubckt:
    case DeviceClass::veriloga:
      subcircuit(ctx, a, p);
      break;
    default:
      terminal_subcircuit(ctx, a, p);
      break;
  }
}

// The terminal masks up to the first empty one (Magic's zero-terminated
// exts_deviceSDTypes).
std::vector<tech::TypeMask> terminal_list(const std::vector<tech::TypeMask>& terms) {
  std::vector<tech::TypeMask> out;
  for (const tech::TypeMask& m : terms) {
    if (m.empty()) {
      break;
    }
    out.push_back(m);
  }
  return out;
}

// One record per real type of the line, all sharing the pattern.
void add_pattern(RuleContext& ctx, DevicePattern pattern) {
  ExtStyle& s = ctx.style;
  s.device_types |= ctx.types1;
  tech::TypeMask real;
  for (tech::TileType t = tech::kFirstUserType; t < ctx.type_end(); ++t) {
    if (ctx.types1.test(t)) {
      real.set(t);
      s.device_conn[t] |= ctx.types1;
    }
  }
  pattern.types = real;
  s.devices.push_back(std::move(pattern));
}

// The ranges as written (tech units; DeviceParam keeps Magic's limits).
std::vector<ParamRange> written_ranges(const std::vector<DeviceParam>& params) {
  std::vector<ParamRange> out;
  for (const DeviceParam& p : params) {
    ParamRange r{p.code, std::nullopt, std::nullopt};
    if (p.minimum != 0.0) {
      r.min = p.minimum / 1000.0;
    }
    if (p.maximum != -1.0) {
      r.max = p.maximum / 1000.0;
    }
    if (r.min || r.max) {
      out.push_back(std::move(r));
    }
  }
  return out;
}

// strcasecmp(name, "error") == 0.
bool is_error_name(std::string_view name) {
  constexpr std::string_view kError = "error";
  if (name.size() != kError.size()) {
    return false;
  }
  for (std::size_t i = 0; i < name.size(); ++i) {
    const char c = name[i];
    if (((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c) != kError[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

void device_rule(RuleContext& ctx, const Tokens& t, std::string_view class_name) {
  const auto entry = std::find_if(kClasses.begin(), kClasses.end(),
                                  [&](const ClassEntry& c) { return c.name == class_name; });
  if (entry == kClasses.end()) {
    return;
  }
  Args a{t, t.size()};
  DevicePattern pattern;
  parameter_options(ctx, a, pattern.params);
  while (a.argc > 0 && limit_option(ctx, a, pattern.params)) {
  }
  if (a.argc > 0 && !a.last().empty() && a.last().front() == '+') {
    pattern.identifiers = ctx.quiet_types(a.last().substr(1)).value_or(tech::TypeMask{});
    --a.argc;
  }
  Parsed p;
  p.device_class = entry->device_class;
  positional(ctx, a, p);
  pattern.device_class = p.device_class;
  pattern.model = a.at(2);
  pattern.terminals = terminal_list(p.terms);
  pattern.substrate_types = p.subs;
  pattern.substrate_node = p.subs_name.value_or(std::string{});
  pattern.terminal_count = p.nterm;
  pattern.perimeter_cap = p.gscap;
  pattern.area_cap = p.gccap;
  pattern.ranges = written_ranges(pattern.params);
  add_pattern(ctx, std::move(pattern));
}

void fet_rule(RuleContext& ctx, const Tokens& t) {
  DevicePattern pattern;
  pattern.device_class = DeviceClass::fet;
  const tech::TypeMask terms = ctx.noisy_types(t[2]);
  if (!terms.empty()) {
    pattern.terminals = {terms};
  }
  pattern.terminal_count = std::atoi(t[3].c_str());
  pattern.model = t[4];
  pattern.substrate_node = t[5];
  const std::size_t bang = t[5].find('!');
  const bool global = bang != std::string::npos && bang + 1 == t[5].size();
  if (!global && !is_error_name(t[5])) {
    ctx.report(kCodeBadDevice, "Fet substrate node " + t[5] + " is not a global name", Print::tech_error);
  }
  if (!scan_double(t[6])) {
    pattern.substrate_types = ctx.noisy_types(t[6]);
    pattern.perimeter_cap = ctx.cap_value(t[7]);
    pattern.area_cap = t.size() > 8 ? ctx.cap_value(t[8]) : 0.0;
  } else {
    pattern.perimeter_cap = ctx.cap_value(t[6]);
    pattern.area_cap = t.size() > 7 ? ctx.cap_value(t[7]) : 0.0;
  }
  add_pattern(ctx, std::move(pattern));
}

}  // namespace hefesto::extract::detail
