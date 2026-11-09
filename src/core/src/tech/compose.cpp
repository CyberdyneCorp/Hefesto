// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `compose` section and the paint/erase tables (capability
// tech-compose-and-connect): default rules, compose/decompose and explicit
// paint/erase rules, saved contact rules, the finalization steps 1-4, the
// affected-plane tables and contact locking.
//
// Ported from Magic VLSI 8.3.694 database/DBtpaint.c (DBTechInitCompose,
// dbTechBitTypeInit, DBTechAddCompose, dbTechSaveCompose,
// dbTechAddPaintErase) and database/DBtpaint2.c (DBTechFinalCompose,
// dbTechPaintErasePlanes, dbComposePaintAllImages, dbComposeResidues,
// dbComposeContacts, dbComposePaintContact, dbComposeSubsetResidues,
// dbComposeEraseContact, DBLockContact, DBUnlockContact,
// dbComposeSavedRules, dbComposeDecompose, dbComposeCompose),
// Copyright (C) 1985, 1990 Regents of the University of California
// (see THIRD_PARTY_NOTICES.md).

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "tech/build_state.hpp"

namespace hefesto::tech {

namespace {

using Data = Technology::Data;

enum class RuleKind : std::uint8_t { compose, decompose, paint, erase };

PlaneMask bit(std::size_t p) noexcept {
  return plane_bit(static_cast<PlaneId>(p));
}

TileType tt(std::size_t t) noexcept {
  return static_cast<TileType>(t);
}

std::size_t home_of(const Data& d, TileType t) noexcept {
  return static_cast<std::size_t>(d.type_plane[t]);
}

// --- initialization (DBTechInitCompose) ---------------------------------

void init_home_plane_rules(Data& d) {
  for (std::size_t s = 0; s < d.num_types; ++s) {
    const int ps = d.type_plane[s];
    if (ps <= 0) {
      continue;
    }
    const auto p = static_cast<std::size_t>(ps);
    for (std::size_t t = 0; t < d.num_types; ++t) {
      if (d.type_plane[t] > 0) {
        d.set_erase(tt(s), tt(t), p, tt(s));
        d.set_paint(tt(s), tt(t), p, d.type_plane[t] == ps ? tt(t) : tt(s));
      }
    }
    d.set_erase(tt(s), tt(s), p, kSpace);
    d.set_paint(kSpace, tt(s), p, tt(s));
  }
}

void init_drc_rules(Data& d) {
  constexpr std::array<TileType, 4> kErrorBits = {kSpace, kErrorP, kErrorS, kErrorPS};
  for (std::size_t i = 0; i < kErrorBits.size(); ++i) {
    for (std::size_t j = 0; j < kErrorBits.size(); ++j) {
      d.set_paint(kErrorBits[i], kErrorBits[j], kPlaneDrcError, kErrorBits[i | j]);
      d.set_erase(kErrorBits[i], kErrorBits[j], kPlaneDrcError, kErrorBits[i & ~j]);
    }
  }
  d.set_paint(kSpace, kCheckPaint, kPlaneDrcCheck, kCheckPaint);
  d.set_paint(kSpace, kCheckSubcell, kPlaneDrcCheck, kCheckSubcell);
  d.set_paint(kCheckPaint, kCheckSubcell, kPlaneDrcCheck, kCheckPaint);
  d.set_paint(kCheckSubcell, kCheckPaint, kPlaneDrcCheck, kCheckPaint);
}

// --- compose / decompose rules -------------------------------------------

bool add_noncontact_rule(BuildState& s, RuleKind kind, TileType result, Tokens pairs) {
  Data& d = s.d;
  for (std::size_t i = 0; i + 1 < pairs.size(); i += 2) {
    const int r = noisy_name_type(d, pairs[i], s.rep, Noise::error);
    const int t = r < 0 ? kNotFound : noisy_name_type(d, pairs[i + 1], s.rep, Noise::error);
    if (r < 0 || t < 0) {
      return false;
    }
    const TileType a = tt(static_cast<std::size_t>(r));
    const TileType b = tt(static_cast<std::size_t>(t));
    if (d.is_contact[a] || d.is_contact[b]) {
      s.r().error("Can't have contact layers on RHS of non-contact rule");
      return false;
    }
    if (!s.magic_compat && d.type_plane[a] != d.type_plane[b]) {
      s.r().error("Compose rule components " + d.type_long[a] + " and " + d.type_long[b] +
                  " are on different planes");  // Q-TECH-21, magic-compat OFF
      return false;
    }
    const std::size_t p = static_cast<std::size_t>(d.type_plane[a]);
    if (kind == RuleKind::compose) {
      d.set_paint(a, b, p, result);
      d.set_paint(b, a, p, result);
      d.not_default_paint[a].set(b);
      d.not_default_paint[b].set(a);
    }
    d.set_paint(result, a, p, result);
    d.set_paint(result, b, p, result);
    d.set_erase(result, a, p, b);
    d.set_erase(result, b, p, a);
    d.not_default_paint[result].set(a);
    d.not_default_paint[result].set(b);
    d.not_default_erase[result].set(a);
    d.not_default_erase[result].set(b);
  }
  return true;
}

// One pair of dbTechSaveCompose; false rejects the line.
bool check_saved_pair(BuildState& s, RuleKind kind, TileType result, int r, int t) {
  const Data& d = s.d;
  if (r < 0 || t < 0) {
    return false;
  }
  const auto a = static_cast<std::size_t>(r);
  const auto b = static_cast<std::size_t>(t);
  if (d.is_contact[a] && d.is_contact[b]) {
    s.r().error("Only one type in each pair may be a contact");
    return false;
  }
  const PlaneMask pair_planes = d.image_planes[a] | d.image_planes[b];
  if ((pair_planes & ~d.image_planes[result]) != 0) {
    s.r().error("Component planes are a superset of result planes");
    return false;
  }
  if (kind == RuleKind::compose && pair_planes != d.image_planes[result]) {
    s.r().error("Union of pair planes must = result planes");
    return false;
  }
  return true;
}

bool save_contact_rule(BuildState& s, RuleKind kind, TileType result, Tokens pairs) {
  Data& d = s.d;
  d.saved_rules.push_back(SavedRule{kind == RuleKind::compose, result, {}});
  for (std::size_t i = 0; i + 1 < pairs.size(); i += 2) {
    const int r = noisy_name_type(d, pairs[i], s.rep, Noise::error);
    const int t = noisy_name_type(d, pairs[i + 1], s.rep, Noise::error);
    if (!check_saved_pair(s, kind, result, r, t)) {
      if (!s.magic_compat) {
        d.saved_rules.pop_back();  // Q-TECH-18, magic-compat OFF: the whole rule is discarded
      }
      return false;
    }
    d.saved_rules.back().pairs.emplace_back(tt(static_cast<std::size_t>(r)), tt(static_cast<std::size_t>(t)));
  }
  return true;
}

bool add_compose_rule(BuildState& s, RuleKind kind, Tokens args) {
  const int result = noisy_name_type(s.d, args[0], s.rep, Noise::error);
  if (result < 0) {
    return false;
  }
  const Tokens pairs = args.subspan(1);
  if (pairs.size() % 2 != 0) {
    s.r().error("Types on RHS of rule must be in pairs");
    return false;
  }
  const TileType t = tt(static_cast<std::size_t>(result));
  return s.d.is_contact[t] ? save_contact_rule(s, kind, t, pairs) : add_noncontact_rule(s, kind, t, pairs);
}

// --- explicit paint / erase rules (dbTechAddPaintErase) -----------------

void set_rule_entry(Data& d, RuleKind kind, TileType h, TileType t, std::size_t p, TileType r) {
  if (kind == RuleKind::paint) {
    d.set_paint(h, t, p, r);
  } else {
    d.set_erase(h, t, p, r);
  }
}

// Pm before removing the result planes; std::nullopt rejects the line.
std::optional<PlaneMask> rule_target_planes(BuildState& s, Tokens args, TileType h) {
  const Data& d = s.d;
  if (args.size() > 3) {
    const int plane = noisy_name_plane(d, args[3], s.rep, Noise::error);
    return plane < 0 ? std::nullopt : std::optional<PlaneMask>{bit(static_cast<std::size_t>(plane))};
  }
  if (h == kSpace) {
    s.r().error("<" + args[0] + ", " + args[1] + ", " + args[2] +
                ">:\nMust specify plane in paint table for painting space");
    return std::nullopt;
  }
  return d.image_planes[h];
}

// Steps 4 and 5: the result types on their planes, space on the others.
void apply_rule_results(Data& d, RuleKind kind, TileType h, TileType t, const TypeList& result, PlaneMask planes) {
  for (std::size_t r = 0; r < d.num_user; ++r) {
    if (!result.types.test(tt(r))) {
      continue;
    }
    for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
      if ((result.planes & d.type_planes[r] & bit(p)) != 0) {
        set_rule_entry(d, kind, h, t, p, tt(r));
      }
    }
  }
  for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
    if ((planes & bit(p)) != 0) {
      set_rule_entry(d, kind, h, t, p, kSpace);
    }
  }
}

bool add_paint_erase_rule(BuildState& s, RuleKind kind, Tokens args) {
  Data& d = s.d;
  if (args.size() < 3) {
    s.r().error("Line must contain at least 3 types");
    return false;
  }
  const int t1 = noisy_name_type(d, args[0], s.rep, Noise::error);
  const int t2 = t1 < 0 ? kNotFound : noisy_name_type(d, args[1], s.rep, Noise::error);
  if (t1 < 0 || t2 < 0) {
    return false;
  }
  const TypeList result = name_mask(d, args[2], s.rep, Noise::warning);
  if (result.types.empty() || result.errors) {
    return false;
  }
  const TileType h = tt(static_cast<std::size_t>(t1));
  const TileType t = tt(static_cast<std::size_t>(t2));
  const auto planes = rule_target_planes(s, args, h);
  if (!planes) {
    return false;
  }
  apply_rule_results(d, kind, h, t, result, *planes & ~result.planes);
  if (kind == RuleKind::erase && !s.magic_compat) {
    d.not_default_erase[h].set(t);  // Q-TECH-20, magic-compat OFF
  } else {
    d.not_default_paint[h].set(t);
  }
  return true;
}

// --- finalization ----------------------------------------------------------

// Step 1 (dbComposePaintAllImages).
void paint_all_images(Data& d) {
  for (const TileType c : d.contact_list) {
    if (c >= d.num_user) {
      continue;
    }
    for (std::size_t r = kFirstUserType; r < d.num_types; ++r) {
      if (!d.residues[c].test(tt(r))) {
        continue;
      }
      const int p = d.type_plane[r];
      for (std::size_t s = kFirstUserType; s < d.num_types; ++s) {
        if (d.type_plane[s] == p) {
          d.set_default_paint(tt(s), c, static_cast<std::size_t>(p), c);
        }
      }
      if (!d.not_default_paint[kSpace].test(c)) {
        d.set_paint(kSpace, c, static_cast<std::size_t>(p), c);
      }
    }
  }
}

bool paint_affects(const Data& d, TileType t, TileType s) {
  return t != kSpace && d.paint_entry(t, s, home_of(d, t)) != t;
}

// Step 2 (dbComposeResidues).
void compose_residues(Data& d) {
  for (const TileType c : d.contact_list) {
    for (std::size_t r = kFirstUserType; r < d.num_user; ++r) {
      if (!d.residues[c].test(tt(r))) {
        continue;
      }
      for (std::size_t s = kFirstUserType; s < d.num_user; ++s) {
        if (!paint_affects(d, tt(r), tt(s))) {
          d.set_default_paint(c, tt(s), home_of(d, tt(r)), c);
        }
      }
    }
  }
}

// Step 3a, case A1a with a stacked type built from the pair.
void paint_contact_stacked(Data& d, TileType image, TileType paint, TileType stacked) {
  const PlaneMask shared = d.image_planes[image] & d.image_planes[paint];
  for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
    if ((shared & bit(p)) != 0) {
      d.set_default_paint(image, paint, p, stacked);
    }
  }
}

// Step 3a, case A1a for a contact painted over a user contact: the paint
// contact where the residues are shared, the image's residues elsewhere.
void paint_contact_over_user_contact(Data& d, TileType image, TileType paint) {
  for (std::size_t r = kFirstUserType; r < d.num_user; ++r) {
    if (d.residues[image].test(tt(r))) {
      d.set_default_paint(image, paint, home_of(d, tt(r)), d.residues[paint].test(tt(r)) ? paint : tt(r));
    }
  }
}

// Step 3a, case A1a for a contact painted over a stacked type it is not part of.
void paint_contact_over_stacked(Data& d, TileType image, TileType paint) {
  for (std::size_t r = kFirstUserType; r < d.num_user; ++r) {
    if (d.residues[image].test(tt(r)) && d.residues[paint].test(tt(r))) {
      d.set_default_paint(image, paint, home_of(d, tt(r)), paint);
    }
  }
}

// Step 3a, case A1a (no explicit contact, or a stacked type exists).
void paint_contact_merge(Data& d, TileType image, TileType paint, int stacked) {
  if (stacked >= static_cast<int>(d.num_user)) {
    paint_contact_stacked(d, image, paint, tt(static_cast<std::size_t>(stacked)));
  } else if (d.is_contact[paint] && image < d.num_user) {
    paint_contact_over_user_contact(d, image, paint);
  } else if (d.is_contact[paint] && !d.residues[image].test(paint)) {
    paint_contact_over_stacked(d, image, paint);
  } else {
    d.set_default_paint(image, paint, home_of(d, image), image);
  }
}

// Paints `paint` over `image` as contact `k` on every plane of k.
void paint_as_contact(Data& d, TileType image, TileType paint, TileType k) {
  for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
    if ((d.image_planes[k] & bit(p)) != 0) {
      d.set_default_paint(image, paint, p, k);
    }
  }
}

// The lowest type of `mask` among the user types, or -1.
int lowest_user_type(const Data& d, const TypeMask& mask) {
  for (std::size_t t = kFirstUserType; t < d.num_user; ++t) {
    if (mask.test(tt(t))) {
      return static_cast<int>(t);
    }
  }
  return -1;
}

// The image's residue on the first shared plane where the residues of
// `image` and `paint` differ, or -1 when they agree on every shared plane.
int first_residue_mismatch(const Data& d, TileType image, TileType paint, PlaneMask shared) {
  for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
    if ((shared & bit(p)) == 0) {
      continue;
    }
    const TileType ir = plane_to_residue(d, image, static_cast<int>(p));
    if (plane_to_residue(d, paint, static_cast<int>(p)) != ir) {
      return ir;
    }
  }
  return -1;
}

// Step 3a, case A (the planes overlap).
void paint_contact_overlap(Data& d, bool magic_compat, TileType image, TileType paint, PlaneMask shared) {
  const int residue_at_break = first_residue_mismatch(d, image, paint, shared);
  if (residue_at_break >= 0) {  // A2, incompatible
    for (std::size_t r = kFirstUserType; r < d.num_user; ++r) {
      if (d.residues[image].test(tt(r)) && static_cast<int>(r) != residue_at_break) {
        d.set_default_paint(image, paint, home_of(d, tt(r)), tt(r));
      }
    }
    return;
  }
  const TypeMask merged = match_residues(d, d.residues[image] | d.residues[paint]);
  const int stacked = find_stacking(d, image, paint);
  if (merged.empty() || stacked != -1) {
    paint_contact_merge(d, image, paint, stacked);
    return;
  }
  // A1b (Q-TECH-22): the lowest explicit contact; with magic-compat OFF the image keeps its type.
  const int k = !magic_compat && merged.test(image) ? image : lowest_user_type(d, merged);
  paint_as_contact(d, image, paint, tt(static_cast<std::size_t>(k)));
}

// Step 3a (dbComposePaintContact).
void paint_contact(Data& d, bool magic_compat, TileType image, TileType paint) {
  const PlaneMask shared = d.image_planes[image] & d.image_planes[paint];
  if (shared != 0) {
    paint_contact_overlap(d, magic_compat, image, paint, shared);
    return;
  }
  if (!d.is_contact[paint]) {
    return;  // case C
  }
  const TypeMask merged = match_residues(d, d.residues[image] | d.residues[paint]);
  for (std::size_t k = kFirstUserType; k < d.num_user; ++k) {  // case B
    if (merged.test(tt(k))) {
      paint_as_contact(d, image, paint, tt(k));
    }
  }
}

// dbComposeSubsetResidues: the candidate contacts and the overlap flag.
bool subset_residues(const Data& d, TileType image, TileType erase, TypeMask& out) {
  TypeMask image_residues;
  if (image >= d.num_user) {
    for (const TileType c : d.contact_list) {
      if (d.residues[image].test(c)) {
        image_residues |= d.residues[c];
      }
    }
  } else {
    image_residues = d.residues[image];
  }
  out = TypeMask{};
  TypeMask seen;
  bool overlap = false;
  for (const TileType c : d.contact_list) {
    if (!image_residues.contains(d.residues[c]) || d.residues[c].contains(d.residues[erase])) {
      continue;
    }
    out.set(c);
    if (seen.intersects(d.residues[c])) {
      overlap = true;
    } else {
      seen |= d.residues[c];
    }
  }
  return overlap;
}

bool erases_on(const Data& d, TileType image, std::size_t p) {
  return image < d.num_user || static_cast<int>(p) == d.type_plane[image];
}

// Step 3b, no-overlap branch.
void erase_contact_to_candidates(Data& d, TileType image, TileType erase, const TypeMask& candidates) {
  PlaneMask left = d.image_planes[image] & ~d.image_planes[erase];
  for (std::size_t k = kFirstUserType; k < d.num_types; ++k) {
    if (!candidates.test(tt(k))) {
      continue;
    }
    left &= ~d.image_planes[k];
    for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
      if ((d.image_planes[k] & bit(p)) != 0 && erases_on(d, image, p)) {
        d.set_default_erase(image, erase, p, tt(k));
      }
    }
  }
  for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
    if ((left & bit(p)) != 0) {
      d.set_default_erase(image, erase, p, plane_to_residue(d, image, static_cast<int>(p)));
    }
  }
}

}  // namespace

// Step 3b (dbComposeEraseContact); also used to unlock a contact.
void erase_contact(Data& d, TileType image, TileType erase) {
  for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
    if ((d.image_planes[erase] & bit(p)) != 0 && erases_on(d, image, p)) {
      d.set_default_erase(image, erase, p, kSpace);
    }
  }
  if (image == erase || (d.image_planes[image] & d.image_planes[erase]) == 0) {
    return;
  }
  TypeMask candidates;
  if (subset_residues(d, image, erase, candidates)) {
    for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
      if ((d.image_planes[image] & bit(p)) != 0) {
        d.set_default_erase(image, erase, p, image);
      }
    }
    return;
  }
  erase_contact_to_candidates(d, image, erase, candidates);
}

namespace {

// Step 3c: stacked types as applied types.
void compose_stacked_applied(Data& d) {
  for (std::size_t h = 0; h < d.num_types; ++h) {
    for (std::size_t g = d.num_user; g < d.num_types; ++g) {
      for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
        TileType painted = tt(h);
        TileType erased = tt(h);
        for (std::size_t c = kFirstUserType; c < d.num_user; ++c) {
          if (d.residues[g].test(tt(c))) {
            painted = d.paint_entry(painted, tt(c), p);
            erased = d.erase_entry(erased, tt(c), p);
          }
        }
        d.set_default_paint(tt(h), tt(g), p, painted);
        d.set_default_erase(tt(h), tt(g), p, erased);
      }
    }
  }
}

// Step 3 (dbComposeContacts).
void compose_contacts(Data& d, bool magic_compat) {
  for (const TileType image : d.contact_list) {
    for (std::size_t n = kFirstUserType; n < d.num_user; ++n) {
      if (image != n) {
        paint_contact(d, magic_compat, image, tt(n));
      }
      erase_contact(d, image, tt(n));
    }
  }
  compose_stacked_applied(d);
}

// dbComposeDecompose.
void compose_decompose(Data& d, TileType image, TileType component) {
  const std::size_t p = home_of(d, image);
  d.set_paint(image, component, p, image);
  d.not_default_paint[image].set(component);
  d.set_erase(image, component, p, plane_to_residue(d, image, static_cast<int>(p)));
  d.not_default_erase[image].set(component);
}

// dbComposeCompose.
void compose_compose(Data& d, TileType image, TileType existing, TileType paint) {
  const std::size_t p = home_of(d, image);
  if ((d.image_planes[existing] & bit(p)) != 0) {
    d.set_paint(existing, paint, p, image);
    d.not_default_paint[existing].set(paint);
  }
}

// Step 4 (dbComposeSavedRules).
void compose_saved_rules(Data& d) {
  for (const SavedRule& rule : d.saved_rules) {
    for (const auto& [a, b] : rule.pairs) {
      compose_decompose(d, rule.result, a);
      compose_decompose(d, rule.result, b);
      if (rule.compose) {
        compose_compose(d, rule.result, a, b);
        compose_compose(d, rule.result, b, a);
      }
    }
  }
}

// Step 5 (dbTechPaintErasePlanes).
void paint_erase_planes(Data& d) {
  d.paint_planes[kSpace] = ~plane_bit(kPlaneRouter);
  d.erase_planes[kSpace] = ~plane_bit(kPlaneRouter);
  for (std::size_t t = 1; t < d.num_types; ++t) {
    d.paint_planes[t] = 0;
    d.erase_planes[t] = 0;
    for (std::size_t p = 1; p < d.num_planes(); ++p) {
      for (std::size_t s = 0; s < d.num_types; ++s) {
        if (d.paint_entry(tt(s), tt(t), p) != s) {
          d.paint_planes[t] |= bit(p);
        }
        if (d.erase_entry(tt(s), tt(t), p) != s) {
          d.erase_planes[t] |= bit(p);
        }
      }
    }
  }
}

// Step 6: lock the locked user contacts and the stacked types built on them.
void lock_finalized_contacts(Data& d) {
  for (std::size_t t = kFirstUserType; t < d.num_user; ++t) {
    if (!d.active.test(tt(t)) && d.is_contact[t]) {
      lock_contact(d, tt(t));
    }
  }
  for (std::size_t t = d.num_user; t < d.num_types; ++t) {
    if (!d.active.contains(d.residues[t])) {
      d.active.clear_type(tt(t));
      lock_contact(d, tt(t));
    }
  }
}

}  // namespace

void compose_init(BuildState& s) {
  Data& d = s.d;
  d.table_planes = d.num_planes();
  d.paint.assign(d.table_planes * kMaxTileTypes * kMaxTileTypes, 0);
  for (std::size_t p = 0; p < d.table_planes; ++p) {
    for (std::size_t t = 0; t < kMaxTileTypes; ++t) {
      for (std::size_t h = 0; h < kMaxTileTypes; ++h) {
        d.paint[Data::index(p, tt(t), tt(h))] = static_cast<std::uint8_t>(h);
      }
    }
  }
  d.erase = d.paint;
  d.not_default_paint.fill(TypeMask{});
  d.not_default_erase.fill(TypeMask{});
  d.saved_rules.clear();  // Q-TECH-19: cleared at every compose section
  init_home_plane_rules(d);
  init_drc_rules(d);
}

bool compose_line(BuildState& s, Tokens tokens) {
  if (tokens.size() < 4) {
    s.r().error("Line must contain at least ruletype, result + pair");
    return false;
  }
  static const std::vector<std::string_view> kRuleNames = {"compose", "decompose", "paint", "erase"};
  const int rule = keyword_lookup(tokens[0], kRuleNames, true);
  if (rule < 0) {
    s.r().error(std::string{rule == kAmbiguous ? "Ambiguous" : "Unknown"} + " rule type " + tokens[0] +
                ".  Must be one of:\n\t\"compose\" \"decompose\" \"paint\" \"erase\" ");
    return false;
  }
  const auto kind = static_cast<RuleKind>(rule);
  if (kind == RuleKind::paint || kind == RuleKind::erase) {
    return add_paint_erase_rule(s, kind, tokens.subspan(1));
  }
  return add_compose_rule(s, kind, tokens.subspan(1));
}

void compose_final(BuildState& s) {
  Data& d = s.d;
  paint_all_images(d);
  compose_residues(d);
  compose_contacts(d, s.magic_compat);
  compose_saved_rules(d);
  paint_erase_planes(d);
  lock_finalized_contacts(d);
}

void lock_contact(Data& d, TileType contact) {
  for (std::size_t n = kFirstUserType; n < d.num_types; ++n) {
    if (n == contact) {
      continue;
    }
    if (contact >= d.num_user && d.residues[contact].test(tt(n)) && d.active.test(tt(n))) {
      continue;  // e.g. pc+v minus pc is still v while pc is active
    }
    for (std::size_t p = kFirstUserPlane; p < d.num_planes(); ++p) {
      if ((d.image_planes[contact] & bit(p)) != 0) {
        d.set_default_erase(contact, tt(n), p, contact);
      }
    }
  }
}

void unlock_contact(Data& d, TileType contact) {
  for (std::size_t n = kFirstUserType; n < d.num_user; ++n) {
    erase_contact(d, contact, tt(n));
  }
}

}  // namespace hefesto::tech
