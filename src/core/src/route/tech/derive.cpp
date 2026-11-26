// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// derive_after_load: Magic's MZAfterTech on a copy of the written styles
// (capability router-maze "Width and spacing derivation from DRC"; design
// D6), plus the derived `irouter` style of IRAfterTech's caller
// (router-maze-iroute "Derived irouter style", design D13).
//
// Order, as in MZAfterTech: widths, lengths and spacings from the DRC
// rules (drc::default_width / default_spacing, DRCGetDefaultLayerWidth /
// Spacing), then the explicit non-SUBCELL spacings, then (magic-compat OFF
// only) the `width` statements (Q-LEGACY-33), then every SUBCELL spacing =
// the maximum of the type's spacings (at least 0), then the explicit
// SUBCELL spacings. Magic walks its prepended spacing list, so of two
// explicit spacings for the same pair the first in the file wins; Hefesto
// keeps that order. Spacing tables cover the defined tile types only
// (Q-LEGACY-35).
//
// Ported from Magic VLSI 8.3.694 mzrouter/mzTech.c (MZAfterTech),
// Copyright (C) 1988, 1990 Michael H. Arnold and the Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: MZTECH.

#include <algorithm>

#include "hefesto/drc/rules/query.hpp"
#include "hefesto/route/tech/styles.hpp"
#include "hefesto/tech/technology.hpp"
#include "route/tech/detail.hpp"

namespace hefesto::route {

namespace {

// DRCGetDefaultLayerWidth and DRCGetDefaultLayerSpacing for every defined
// tile type; 0 everywhere without rules.
void derive_from_drc(RouteType& route, const tech::Technology& tech, const drc::RuleSet* rules) {
  const std::size_t count = tech.type_count();
  route.width = rules != nullptr ? drc::default_width(*rules, tech, route.type) : 0;
  route.length = route.width;  // square contacts, no minimum area
  route.spacing.assign(count, 0);
  if (rules == nullptr) {
    return;
  }
  for (std::size_t u = 0; u < count; ++u) {
    route.spacing[u] = drc::default_spacing(*rules, tech, route.type, static_cast<tech::TileType>(u));
  }
}

// The explicit spacings with `subcell` equal to `subcell`, in Magic's list
// order (last statement first, so the first statement wins).
void apply_explicit_spacings(MazeStyle& style, bool subcell) {
  for (auto it = style.spacings.rbegin(); it != style.spacings.rend(); ++it) {
    if (it->subcell != subcell) {
      continue;
    }
    RouteType& route = style.route_type(it->route);
    if (subcell) {
      route.subcell_spacing = it->value;
    } else if (it->type < route.spacing.size()) {
      route.spacing[it->type] = it->value;
    }
  }
}

}  // namespace

void apply_width_statements(MazeStyle& style, bool magic_compat) {
  if (magic_compat) {
    return;  // MZAfterTech overwrites them with the DRC widths (Q-LEGACY-33 ON)
  }
  for (const WidthStatement& statement : style.widths) {
    RouteType& route = style.route_type(statement.route);
    route.width = statement.width;
    route.length = statement.length;
  }
}

void derive_style(MazeStyle& style, const tech::Technology& tech, const drc::RuleSet* rules, bool magic_compat) {
  for (const RouteTypeRef ref : style.types) {
    derive_from_drc(style.route_type(ref), tech, rules);
  }
  apply_explicit_spacings(style, false);
  apply_width_statements(style, magic_compat);
  for (const RouteTypeRef ref : style.types) {
    RouteType& route = style.route_type(ref);
    const auto widest = std::max_element(route.spacing.begin(), route.spacing.end());
    route.subcell_spacing = widest == route.spacing.end() ? 0 : std::max(0, *widest);
  }
  apply_explicit_spacings(style, true);
}

void derive_after_load(MazeStyles& styles, const tech::Technology& tech, const drc::RuleSet* rules,
                       const lef::LayerTable& lef_layers, bool magic_compat, diag::DiagnosticSink& /*sink*/) {
  (void)maybe_derive_irouter(styles, tech, lef_layers, magic_compat);
  for (MazeStyle& style : styles.styles) {
    derive_style(style, tech, rules, magic_compat);
  }
}

}  // namespace hefesto::route
