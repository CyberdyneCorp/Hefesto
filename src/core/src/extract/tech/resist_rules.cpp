// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `resist`, `contact`, `devresist` / `fetresist` (capability
// extract-tech-section "Resistance classes and contact resistance";
// openspec/changes/m7-extraction/design.md D7, tasks.md 4.3).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c ExtTechLine
// (CONTACT :2290-2313, DEVRESIST :2946-2966, RESIST :3391-3440, aToRes),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH.

#include <cstdlib>
#include <optional>
#include <string>

#include "rule_context.hpp"

namespace hefesto::extract::detail {

namespace {

// aToRes: `%d`, 0 and reported when nothing converts.
std::int32_t res_value(const RuleContext& ctx, const std::string& text) {
  const std::optional<std::int32_t> value = scan_int(text);
  if (!value) {
    ctx.report(kCodeBadNumber, "Resistance value " + text + " must be a number", Print::tech_error);
    return 0;
  }
  return *value;
}

// A `resist` value that is not an integer: `None`, a decimal value or
// garbage. Returns the class value to create, or nullopt for none.
std::optional<std::int32_t> non_integer_resist(RuleContext& ctx, const std::string& value) {
  if (value == "None") {
    for (tech::TileType t = tech::kFirstUserType; t < ctx.type_end(); ++t) {
      if (ctx.types1.test(t)) {
        ctx.style.active.clear_type(t);
      }
    }
    return std::nullopt;
  }
  const std::size_t dot = value.find('.');
  if (dot == std::string::npos) {
    ctx.report(kCodeBadResist, "Resist argument must be integer or \"None\".", Print::plain);
    return std::nullopt;
  }
  const std::string whole = value.substr(0, dot);
  if (!str_is_int(whole)) {
    ctx.report(kCodeBadResist, "Unparseable resist argument \"" + whole + "\"", Print::plain);
    return std::nullopt;
  }
  const std::int32_t truncated = res_value(ctx, whole);
  if (value.substr(dot + 1) != "0") {
    ctx.report(kCodeBadResist, "Resist argument truncated to integer value.", Print::plain);
  }
  // Magic leaves the switch here, so the truncated value makes no class
  // (magic-compat ON); with it OFF the class is made, as the message says.
  if (ctx.magic_compat()) {
    return std::nullopt;
  }
  return truncated;
}

}  // namespace

void resist_rule(RuleContext& ctx, const Tokens& t) {
  std::optional<std::int32_t> value;
  if (str_is_int(t[2])) {
    value = res_value(ctx, t[2]);
  } else {
    value = non_integer_resist(ctx, t[2]);
  }
  if (!value) {
    return;
  }
  const float chop = t.size() == 4 ? static_cast<float>(std::atof(t[3].c_str())) : 1.0F;
  ExtStyle& s = ctx.style;
  const auto cls = static_cast<std::int32_t>(s.resist_classes.size());
  for (tech::TileType ty = tech::kFirstUserType; ty < ctx.type_end(); ++ty) {
    if (ctx.types1.test(ty)) {
      s.sheet_resistance[ty] = *value;
      s.corner_chop[ty] = chop;
      s.resist_class[ty] = cls;
    }
  }
  ResistClass rc;
  rc.types = ctx.types1;
  rc.sheet_resistance = *value;
  s.resist_classes.push_back(rc);
}

void contact_rule(RuleContext& ctx, const Tokens& t) {
  if (t.size() == 4) {
    ctx.report(kCodeContactSize, "Contact size value ignored (using GDS generation rules).", Print::plain);
  } else if (t.size() > 4) {
    ctx.report(kCodeContactSize, "Contact size, spacing, and border values ignored (using GDS generation rules).",
               Print::plain);
  }
  const std::string& value = t.back();
  if (!str_is_int(value)) {
    ctx.report(kCodeBadNumber,
               "Contact resistivity " + value + " must be an integer value (in milliohms/square).",
               Print::tech_error);
    return;
  }
  const std::int32_t res = res_value(ctx, value);
  for (tech::TileType ty = tech::kFirstUserType; ty < ctx.type_end(); ++ty) {
    if (ctx.types1.test(ty)) {
      ctx.style.via_resistance[ty] = res;
    }
  }
}

namespace {

void set_device_resist(DevicePattern& pattern, tech::TileType type, const std::string& region, std::int32_t value) {
  for (DeviceResist& r : pattern.resist) {
    if (r.type == type && r.region == region) {
      r.value = value;
      return;
    }
  }
  pattern.resist.push_back(DeviceResist{type, region, value});
}

}  // namespace

// The records of type T that exist when the line is read: the device lines
// already read that name T.
void devresist_rule(RuleContext& ctx, const Tokens& t) {
  if (!str_is_numeric(t[3])) {
    ctx.report(kCodeBadNumber, "Device resistivity " + t[3] + " must be numeric", Print::tech_error);
    return;
  }
  const std::int32_t value = res_value(ctx, t[3]);
  for (tech::TileType ty = tech::kFirstUserType; ty < ctx.type_end(); ++ty) {
    if (!ctx.types1.test(ty)) {
      continue;
    }
    for (DevicePattern& pattern : ctx.style.devices) {
      if (pattern.types.test(ty)) {
        set_device_resist(pattern, ty, t[2], value);
      }
    }
  }
}

}  // namespace hefesto::extract::detail
