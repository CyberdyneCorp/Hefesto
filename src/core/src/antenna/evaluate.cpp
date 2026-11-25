// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The check planes, the restricted connectivity and the ratio test of
// `antennacheck` (antenna/measure.hpp; capability antenna-check
// "Per-plane progressive evaluation" steps 1 and 5, "Antenna ratio test"):
// Q-MISC-6 (fix, both modes), Q-MISC-7 and Q-MISC-1 (compat, per mode).
//
// Ported from Magic 8.3.694 (fc5daa9e) extflat/EFantenna.c:437-714
// (antennacheckVisit), Copyright (C) 1985, 1990 Regents of the University
// of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: ANTENNA.

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "hefesto/antenna/measure.hpp"

namespace hefesto::antenna {

namespace {

// The plane whose order is `order` (Magic keeps the last match).
std::optional<tech::PlaneId> plane_with_order(const RuleView& rules, std::int32_t order) {
  std::optional<tech::PlaneId> found;
  for (std::size_t p = 0; p < rules.plane_count(); ++p) {
    if (rules.plane_order(static_cast<tech::PlaneId>(p)) == order) {
      found = static_cast<tech::PlaneId>(p);
    }
  }
  return found;
}

// Magic's restriction: the connect mask of `contact` keeps the types whose
// plane number (ON) or plane order (OFF) is at most the check plane's.
tech::TypeMask restrict_mask(const RuleView& rules, const tech::Technology& tech, tech::TileType contact,
                             const CheckPlane& check, bool magic_compat) {
  const tech::TypeMask& full = tech.connects_to(contact);
  tech::TypeMask kept;
  for (std::size_t j = 0; j < tech.type_count(); ++j) {
    const auto type = static_cast<tech::TileType>(j);
    const std::optional<tech::PlaneId> home = tech.home_plane(type);
    if (!full.test(type) || !home) {
      continue;
    }
    const bool below = magic_compat ? *home <= check.plane : rules.plane_order(*home) <= check.order;
    kept.set(type, below);
  }
  return kept;
}

// One term of a sum: a non-contact type to S_metal; a contact to S_via
// when its home plane is the check plane (partial model), or always
// (cumulative model, magic-compat OFF; Q-MISC-1).
void add_term(Evaluation& e, const MeasureSetup& setup, tech::TileType type, double term, const CheckPlane& check) {
  const tech::Technology& tech = *setup.tech;
  const bool contact = tech.is_contact(type);
  if (setup.rules->partial()) {
    if (!contact) {
      e.metal += term;
    } else if (tech.home_plane(type) == check.plane) {
      e.via += term;
    }
    return;
  }
  if (setup.magic_compat) {
    return;  // Q-MISC-1 ON: the cumulative model sums nothing
  }
  (contact ? e.via : e.metal) += term;
}

// No tie-down area: the gate ratio alone.
Evaluation gate_only(const MeasureSetup& setup, const std::vector<std::int64_t>& areas, std::int64_t gate,
                     const CheckPlane& check) {
  Evaluation e;
  const RuleView& rules = *setup.rules;
  for (std::size_t i = 0; i < setup.tech->user_type_end() && i < areas.size(); ++i) {
    const auto type = static_cast<tech::TileType>(i);
    const float ratio = rules.ratio_gate(type);
    if (ratio > 0) {
      add_term(e, setup, type, static_cast<double>(areas[i]) / static_cast<double>(ratio), check);
    }
    e.limit = std::max(e.limit, ratio);
  }
  // A net with no gate area is not a violation (Magic avoids an infinite ratio).
  e.metal_violation = gate > 0 && e.metal > static_cast<double>(gate);
  e.via_violation = gate > 0 && e.via > static_cast<double>(gate);
  return e;
}

// Tie-down area D > 0: the gate ratio raised by the diffusion terms; a type
// whose constant diffusion ratio is `none` (+inf) has no rule.
Evaluation with_diffusion(const MeasureSetup& setup, const std::vector<std::int64_t>& areas, std::int64_t gate,
                          std::int64_t diffusion, const CheckPlane& check) {
  Evaluation e;
  const RuleView& rules = *setup.rules;
  for (std::size_t i = 0; i < setup.tech->user_type_end() && i < areas.size(); ++i) {
    const auto type = static_cast<tech::TileType>(i);
    if (std::isinf(rules.ratio_diff_const(type))) {
      continue;
    }
    const auto total = static_cast<float>(static_cast<double>(rules.ratio_gate(type) + rules.ratio_diff_const(type)) +
                                          static_cast<double>(rules.ratio_diff_prop(type)) *
                                              static_cast<double>(diffusion));
    if (total > 0) {
      add_term(e, setup, type, static_cast<double>(areas[i]) / static_cast<double>(total), check);
    }
    e.limit = std::max(e.limit, total);
  }
  e.metal_violation = e.metal > static_cast<double>(gate);
  e.via_violation = e.via > static_cast<double>(gate);
  return e;
}

}  // namespace

std::vector<CheckPlane> check_planes(const RuleView& rules, const tech::Technology& tech, tech::PlaneId gate_plane) {
  const std::int32_t first = rules.plane_order(gate_plane) + 1;
  std::int32_t last = first;
  for (std::size_t p = tech::kFirstUserPlane; p < tech.plane_count(); ++p) {
    last = std::max(last, rules.plane_order(static_cast<tech::PlaneId>(p)));
  }
  std::vector<CheckPlane> out;
  for (std::int32_t order = first; order <= last; ++order) {
    // Q-MISC-6 (fix, both modes): an order no plane has is skipped; Magic
    // reuses the previous check plane (or an uninitialised one).
    if (const std::optional<tech::PlaneId> plane = plane_with_order(rules, order)) {
      out.push_back(CheckPlane{*plane, order});
    }
  }
  return out;
}

connect::ConnectTable restricted_connect(const RuleView& rules, const tech::Technology& tech,
                                         const CheckPlane& check, bool magic_compat) {
  auto table = std::make_shared<std::vector<tech::TypeMask>>();
  table->reserve(tech.type_count());
  bool restricted = false;
  for (std::size_t i = 0; i < tech.type_count(); ++i) {
    const auto type = static_cast<tech::TileType>(i);
    // Q-MISC-7: ON restricts only the first contact homed on the check
    // plane (by type number), OFF every such contact.
    const bool target = tech.is_contact(type) && tech.home_plane(type) == check.plane && !(magic_compat && restricted);
    if (target) {
      restricted = true;
      table->push_back(restrict_mask(rules, tech, type, check, magic_compat));
    } else {
      table->push_back(tech.connects_to(type));
    }
  }
  return [table, &tech](tech::TileType type) -> tech::TypeMask {
    return type < table->size() ? (*table)[type] : tech.connects_to(type);
  };
}

Evaluation evaluate(const MeasureSetup& setup, const std::vector<std::int64_t>& areas, std::int64_t gate,
                    std::int64_t diffusion, const CheckPlane& check) {
  return diffusion == 0 ? gate_only(setup, areas, gate, check) : with_diffusion(setup, areas, gate, diffusion, check);
}

float effective_ratio(float limit, double sum, std::int64_t gate) {
  if (gate == 0) {
    return std::numeric_limits<float>::infinity();
  }
  return limit * static_cast<float>(sum) / static_cast<float>(gate);
}

}  // namespace hefesto::antenna
