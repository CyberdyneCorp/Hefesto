// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The style defaults and the scale, step, connectivity and substrate
// keywords (capability extract-tech-section "Style defaults", "Units,
// lambda, cscale and rscale", "Step size and side halo parsing"
// (Q-EXTRACT-19), "Connectivity overrides", "Substrate declaration";
// openspec/changes/m7-extraction/design.md D7, tasks.md 4.2, 4.5). M10's
// TECH reads `lambda`, `units` and the plane order (style_reader.cpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c extTechStyleInit
// (:837-978) and ExtTechLine (CONNECT :2277-2289, CSCALE :2314-2321,
// DISCONNECT :2322-2337, SIDEHALO :3362-3375, RSCALE :3441-3443, STEP
// :3444-3452, SUBSTRATE :3453-3495), Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH.

#include <cstdlib>
#include <optional>
#include <string>

#include "rule_context.hpp"

namespace hefesto::extract::detail {

void init_style(ExtStyle& style, const tech::Technology& tech, bool magic_compat) {
  style = ExtStyle{};
  style.magic_compat = magic_compat;
  style.step_size = 100;
  style.step_exact = 100.0;
  style.side_halo = 0;
  style.units_per_lambda = 100.0F;
  style.rscale = 1000;
  style.cscale = 1000;
  style.plane_ordering = true;
  style.fringe_model = tech.fringe_halo_default() ? FringeModel::fringe_halo : FringeModel::legacy;
  for (tech::TileType t = tech::kMagnet; t < tech::kMaxTileTypes; ++t) {
    style.active.set(t);  // DBAllButSpaceAndDRCBits
  }
  style.node_conn.assign(tech::kMaxTileTypes, tech::TypeMask{});
  style.device_conn.assign(tech::kMaxTileTypes, tech::TypeMask{});
  style.resist_conn.assign(tech::kMaxTileTypes, tech::TypeMask{});
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    style.node_conn[t] = tech.connects_to(static_cast<tech::TileType>(t));
  }
}

void connect_rule(RuleContext& ctx, const Tokens& t, bool connect) {
  const tech::TypeMask types2 = ctx.noisy_types(t[2]);
  std::vector<tech::TypeMask>& conn = ctx.style.node_conn;
  for (tech::TileType a = 0; a < ctx.type_end(); ++a) {
    if (!ctx.types1.test(a)) {
      continue;
    }
    for (tech::TileType b = 0; b < ctx.type_end(); ++b) {
      if (types2.test(b)) {
        conn[a].set(b, connect);
        conn[b].set(a, connect);
      }
    }
  }
}

void cscale_rule(RuleContext& ctx, const Tokens& t) {
  char* end = nullptr;
  const long value = std::strtol(t[1].c_str(), &end, 10);
  if (end == t[1].c_str()) {
    ctx.report(kCodeBadNumber, "Cannot parse cap scale value \"" + t[1] + "\"", Print::tech_error);
    ctx.style.cscale = 1;
    return;
  }
  ctx.style.cscale = static_cast<double>(static_cast<std::int32_t>(value));
}

void rscale_rule(RuleContext& ctx, const Tokens& t) {
  ctx.style.rscale = std::atoi(t[1].c_str());
}

// Q-EXTRACT-19: magic-compat ON truncates (`(int)atof`); OFF keeps the
// fractional value (rounded after unit conversion, micron_convert()).
bool step_rule(RuleContext& ctx, const Tokens& t) {
  const double value = std::atof(t[1].c_str());
  const bool positive = ctx.magic_compat() ? static_cast<std::int32_t>(value) > 0 : value > 0.0;
  if (!positive) {
    ctx.report(kCodeBadStep, "Hierarchical interaction step size must be > 0", Print::tech_error);
    return false;
  }
  ctx.style.step_size = static_cast<std::int32_t>(value);
  ctx.style.step_exact = ctx.magic_compat() ? static_cast<double>(ctx.style.step_size) : value;
  return true;
}

// `%lg` x 1000, truncated (milli-units until micron_convert()); a value
// that does not convert is 0 (Magic reads an uninitialised double).
void sidehalo_rule(RuleContext& ctx, const Tokens& t) {
  const double value = scan_double(t[1]).value_or(0.0);
  ctx.style.side_halo = static_cast<std::int32_t>(value * 1000.0);
  ctx.style.side_halo_exact = value;
}

// An unknown plane leaves no substrate plane (Magic stores -2 and later
// indexes its tables with it; fixed in both modes). The node name and the
// default type keep an earlier line's value when this line gives none,
// as in Magic.
void substrate_rule(RuleContext& ctx, const Tokens& t) {
  std::size_t argc = t.size();
  tech::TypeMask shields;
  if (!t.back().empty() && t.back().front() == '-') {
    shields = ctx.quiet_types(t.back().substr(1)).value_or(tech::TypeMask{});
    ctx.mentioned |= shields;
    --argc;
  }
  SubstrateDecl& sub = ctx.style.substrate;
  sub.types = ctx.types1;
  sub.shield_types = shields;
  sub.plane = ctx.noisy_plane(t[2]);
  if (sub.plane) {
    for (auto ty = static_cast<std::size_t>(tech::kFirstUserType); ty < ctx.tech.user_type_end(); ++ty) {
      const auto type = static_cast<tech::TileType>(ty);
      if (sub.types.test(type) && ctx.plane_of(type) == *sub.plane) {
        sub.default_type = type;
        break;
      }
    }
  }
  if (argc == 4) {
    sub.node_name = t[3];
  }
}

}  // namespace hefesto::extract::detail
