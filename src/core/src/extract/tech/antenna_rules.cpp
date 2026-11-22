// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `antenna`, `model`, `tiedown` (capability extract-tech-section "Antenna
// keywords parse-level reference"; Q-MISC-2, Q-MISC-3, Q-MISC-5;
// openspec/changes/m7-extraction/design.md D7, tasks.md 4.5). The
// semantics belong to antenna-check.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c ExtTechLine
// (ANTENNA :2990-3085, MODEL :3086-3105, TIEDOWN :3106-3108), Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Fixes applied in both modes (X-2 tier 1): the model flags start empty
// for every style (Q-MISC-2, init_style); a line without a calculation
// type and no default stores "none" (Q-MISC-3); a line with a calculation
// type and one ratio is malformed and ignored (Q-MISC-5).
//
// Owner: EXTTECH.

#include <cstdlib>
#include <limits>
#include <optional>
#include <string>

#include "rule_context.hpp"

namespace hefesto::extract::detail {

namespace {

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    if (lower(a[i]) != lower(b[i])) {
      return false;
    }
  }
  return true;
}

std::optional<AntennaModel> calc_type(std::string_view word) {
  if (word == "surface" || word == "area") {
    return AntennaModel::surface;
  }
  if (word == "sidewall" || word == "perimeter") {
    return AntennaModel::sidewall;
  }
  return std::nullopt;
}

AntennaRule& rule_of(ExtStyle& style, tech::TileType type) {
  for (AntennaRule& rule : style.antenna) {
    if (rule.type == type) {
      return rule;
    }
  }
  AntennaRule rule;
  rule.type = type;
  rule.ratio_diff = {0.0, 0.0};
  style.antenna.push_back(rule);
  return style.antenna.back();
}

template <typename F>
void for_types(RuleContext& ctx, F&& set) {
  for (tech::TileType t = tech::kFirstUserType; t < ctx.type_end(); ++t) {
    if (ctx.types1.test(t)) {
      set(rule_of(ctx.style, t));
    }
  }
}

// The default calculation type from the `model` flags; Q-MISC-3 when none.
AntennaModel default_calc_type(const RuleContext& ctx, const Tokens& t) {
  if ((ctx.style.antenna_model & kAntennaSurface) != 0) {
    return AntennaModel::surface;
  }
  if ((ctx.style.antenna_model & kAntennaSidewall) != 0) {
    return AntennaModel::sidewall;
  }
  ctx.report(kCodeBadAntenna,
             "No antenna calculation type given for layer(s) " + t[1] + "  and no default calculation type found.",
             Print::tech_error);
  return AntennaModel::none;
}

// A diffusion ratio: a number, or `none` (+inf) for the first one.
std::optional<float> diff_ratio(const RuleContext& ctx, const std::string& word, bool allow_none) {
  if (str_is_numeric(word)) {
    return static_cast<float>(std::strtod(word.c_str(), nullptr));
  }
  if (allow_none && iequals(word, "none")) {
    return std::numeric_limits<float>::infinity();
  }
  ctx.report(kCodeBadAntenna, "Diff layer antenna ratio " + word + " must be numeric", Print::tech_error);
  return std::nullopt;
}

}  // namespace

void antenna_rule(RuleContext& ctx, const Tokens& t) {
  std::optional<AntennaModel> model;
  if (!str_is_numeric(t[2])) {
    model = calc_type(t[2]);
    if (!model) {
      ctx.report(kCodeBadAntenna,
                 "Error in layer antenna calculation type \"" + t[2] + "\";  must be \"surface\" or \"sidewall\"",
                 Print::tech_error);
      return;
    }
    if (t.size() == 4) {  // Q-MISC-5
      ctx.report(kCodeMalformed,
                 "Malformed line for keyword antenna.  Correct usage:\n\tantenna type [calc-type] "
                 "[antenna-ratio-proportional] antenna-ratio-const",
                 Print::tech_error);
      return;
    }
  }
  const AntennaModel area_type = model ? *model : default_calc_type(ctx, t);
  for_types(ctx, [&](AntennaRule& r) { r.model = area_type; });
  std::size_t idx = model ? 3 : 2;
  if (!str_is_numeric(t[idx])) {
    ctx.report(kCodeBadAntenna, "Gate layer antenna ratio " + t[idx] + " must be numeric", Print::tech_error);
    return;
  }
  const auto gate = static_cast<float>(std::strtod(t[idx].c_str(), nullptr));
  for_types(ctx, [&](AntennaRule& r) { r.ratio_gate = static_cast<double>(gate); });
  ++idx;
  const std::optional<float> diff_b = diff_ratio(ctx, t[idx], true);
  if (!diff_b) {
    return;
  }
  for_types(ctx, [&](AntennaRule& r) { r.ratio_diff[0] = static_cast<double>(*diff_b); });
  ++idx;
  float diff_a = 0.0F;
  if (idx < t.size()) {
    const std::optional<float> value = diff_ratio(ctx, t[idx], false);
    if (!value) {
      return;
    }
    diff_a = *value;
  }
  for_types(ctx, [&](AntennaRule& r) { r.ratio_diff[1] = static_cast<double>(diff_a); });
}

void model_rule(RuleContext& ctx, const Tokens& t) {
  ExtStyle& s = ctx.style;
  if (t[1] == "partial") {
    s.antenna_model |= kAntennaPartial;
  } else if (t[1] == "cumulative") {
    s.antenna_model |= kAntennaCumulative;
  } else {
    ctx.report(kCodeBadModel, "Unknown antenna model \"" + t[1] + "\":  Use \"partial\" or \"cumulative\"",
               Print::plain);
  }
  if (t.size() <= 2) {
    return;
  }
  const std::optional<AntennaModel> calc = calc_type(t[2]);
  if (!calc) {
    ctx.report(kCodeBadModel, "Unknown antenna model \"" + t[2] + "\":  Use \"surface\" or \"sidewall\"",
               Print::plain);
    return;
  }
  s.antenna_model |= *calc == AntennaModel::surface ? kAntennaSurface : kAntennaSidewall;
}

void tiedown_rule(RuleContext& ctx, const Tokens& /*t*/) {
  ctx.style.tiedown |= ctx.types1;
}

}  // namespace hefesto::extract::detail
