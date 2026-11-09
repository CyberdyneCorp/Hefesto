// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `contact` section (alias `images`): contact declarations, residue
// validation, the `stackable` statement, generated stacked types and the
// contact finalization (capability tech-contacts-and-stacking).
//
// Ported from Magic VLSI 8.3.694 database/DBtcontact.c (DBTechInitContact,
// DBTechAddContact, dbTechAddStackedContacts, dbTechAddOneStackedContact,
// dbTechContactResidues, dbTechMatchResidues, DBTechFindStacking,
// DBTechFinalContact) and database/DBtechtype.c (dbTechNewStackedType),
// Copyright (C) 1985, 1990 Regents of the University of California
// (see THIRD_PARTY_NOTICES.md).

#include <string>

#include "tech/build_state.hpp"

namespace hefesto::tech {

namespace {

using Data = Technology::Data;

// dbTechAddOneStackedContact results that are not a type number.
constexpr int kNotStackable = -1;
constexpr int kAlreadyExists = -2;
constexpr int kTypeLimit = -3;

std::string long_name(const Data& d, int type) {
  return type >= 0 && static_cast<std::size_t>(type) < kMaxTileTypes ? d.type_long[static_cast<std::size_t>(type)]
                                                                     : std::string{"ERROR"};
}

}  // namespace

/// dbTechMatchResidues(contactsOnly = TRUE): user contacts with exactly `residues`.
TypeMask match_residues(const Data& d, const TypeMask& residues) {
  TypeMask out;
  for (std::size_t t = kFirstUserType; t < d.num_user; ++t) {
    if (d.is_contact[t] && d.residues[t] == residues) {
      out.set(static_cast<TileType>(t));
    }
  }
  return out;
}

/// DBTechFindStacking: the stacked type whose two lowest residues are
/// {a, b} in either order, or -1.
int find_stacking(const Data& d, TileType a, TileType b) {
  for (std::size_t s = d.num_user; s < d.num_types; ++s) {
    int r1 = -1;
    int r2 = -1;
    for (std::size_t r = kFirstUserType; r < d.num_user; ++r) {
      if (!d.residues[s].test(static_cast<TileType>(r))) {
        continue;
      }
      if (r1 < 0) {
        r1 = static_cast<int>(r);
      } else {
        r2 = static_cast<int>(r);
        break;
      }
    }
    if ((r1 == a && r2 == b) || (r1 == b && r2 == a)) {
      return static_cast<int>(s);
    }
  }
  return -1;
}

namespace {

// dbTechNewStackedType: the next type number, named "<short(a)>+<short(b)>".
int new_stacked_type(BuildState& s, TileType a, TileType b) {
  Data& d = s.d;
  if (d.num_types >= kUsableTileTypes) {
    s.r().error("Too many types to generate a new contact.  Maximum=254");
    return -1;
  }
  const std::string name = type_short_name(d, a) + "+" + type_short_name(d, b);
  if (name_lookup_exact(d.type_names, name) != kNotFound) {
    s.r().error("Duplicate name: " + name + "\nCouldn't generate new stacking type " + name);
    return -1;
  }
  const auto type = static_cast<TileType>(d.num_types);
  d.type_long[type] = add_names(d.type_names, {name}, type, false);
  ++d.num_types;
  return type;
}

int add_one_stacked(BuildState& s, TileType a, TileType b) {
  Data& d = s.d;
  if (!d.is_contact[a] || !d.is_contact[b]) {
    return kNotStackable;
  }
  const PlaneMask shared_planes = d.image_planes[a] & d.image_planes[b];
  if ((shared_planes & (shared_planes - 1)) != 0) {
    return kNotStackable;  // more than one shared plane
  }
  const TypeMask shared = d.residues[a] & d.residues[b];
  if (shared.empty()) {
    return kNotStackable;
  }
  if (!match_residues(d, d.residues[a] | d.residues[b]).empty() || find_stacking(d, a, b) != -1) {
    return kAlreadyExists;
  }
  const int stacked = new_stacked_type(s, a, b);
  if (stacked < 0) {
    return kTypeLimit;
  }
  const auto t = static_cast<std::size_t>(stacked);
  d.is_contact[t] = true;
  d.residues[t] = TypeMask::of(a) | TypeMask::of(b);
  d.image_planes[t] = d.image_planes[a] | d.image_planes[b];
  for (std::size_t r = kFirstUserType; r < d.num_user; ++r) {
    if (shared.test(static_cast<TileType>(r))) {
      d.type_plane[t] = d.type_plane[r];
      break;
    }
  }
  d.contact_list.push_back(static_cast<TileType>(stacked));
  return stacked;
}

// `stackable` with no arguments: every pair of the contacts present now.
void stack_all(BuildState& s) {
  const std::size_t user_contacts = s.d.contact_list.size();
  for (std::size_t m = 0; m < user_contacts; ++m) {
    for (std::size_t n = m + 1; n < user_contacts; ++n) {
      if (add_one_stacked(s, s.d.contact_list[m], s.d.contact_list[n]) == kTypeLimit) {
        return;
      }
    }
  }
}

// `stackable A`: A with every other entry of the (growing) contact list.
bool stack_with_all(BuildState& s, TileType a) {
  for (std::size_t n = 0; n < s.d.contact_list.size(); ++n) {
    const TileType other = s.d.contact_list[n];
    if (other != a && add_one_stacked(s, a, other) == kTypeLimit) {
      return false;
    }
  }
  return true;
}

// `stackable A x1 x2 ...`: pairs and legacy alias names; never rejects.
void stack_listed(BuildState& s, TileType a, Tokens rest) {
  Data& d = s.d;
  int last_result = kNotStackable;
  for (const std::string& token : rest) {
    const int type = name_type(d, token);
    if (type >= 0) {
      last_result = add_one_stacked(s, a, static_cast<TileType>(type));
      if (last_result == kNotStackable) {
        s.r().error("Contact types " + long_name(d, a) + " and " + long_name(d, type) + " do not stack");
      }
      continue;
    }
    if (last_result < 0) {
      s.r().error("Contact type " + token + " unknown or contact missing in stackable statement");
    } else if (const auto dup = first_duplicate(d.type_names, {token}); !dup.empty()) {
      s.r().error("Duplicate name: " + dup);
    } else {
      add_names(d.type_names, {token}, last_result, true);
    }
  }
}

bool stackable_line(BuildState& s, Tokens args) {
  if (args.empty()) {
    stack_all(s);
    return true;
  }
  const int a = noisy_name_type(s.d, args[0], s.rep, Noise::error);
  if (a < 0) {
    return false;
  }
  if (args.size() == 1) {
    return stack_with_all(s, static_cast<TileType>(a));
  }
  stack_listed(s, static_cast<TileType>(a), args.subspan(1));
  return true;
}

struct ResidueCheck {
  TypeMask residues;
  PlaneMask planes = 0;
};

// One residue of dbTechContactResidues; false rejects the line.
bool check_residue(BuildState& s, const std::string& token, int home, ResidueCheck& acc, bool& on_home) {
  const Data& d = s.d;
  const int r = noisy_name_type(d, token, s.rep, Noise::error);
  if (r < 0) {
    return false;
  }
  const auto rt = static_cast<std::size_t>(r);
  if (d.is_contact[rt]) {
    s.r().error("Residue type " + d.type_long[rt] + " is a contact itself");
    return false;
  }
  const int plane = d.type_plane[rt];
  if (plane < 0) {
    s.r().error("Residue type " + d.type_long[rt] + " doesn't have a home plane");
    return false;
  }
  if ((acc.planes & plane_bit(static_cast<PlaneId>(plane))) != 0) {
    s.r().error("Contact residues (" + d.type_long[rt] + ") must be on different planes");
    return false;
  }
  acc.planes |= plane_bit(static_cast<PlaneId>(plane));
  on_home = on_home || plane == home;
  acc.residues.set(static_cast<TileType>(r));
  return true;
}

// Q-TECH-22: "Contact residues for c identical to those for ..." (a warning).
void warn_identical(BuildState& s, TileType contact, const TypeMask& residues) {
  TypeMask same = match_residues(s.d, residues);
  same.clear_type(contact);
  if (same.empty()) {
    return;
  }
  std::string text = "Contact residues for " + s.d.type_long[contact] + " identical to those for ";
  for (std::size_t t = kFirstUserType; t < s.d.num_types; ++t) {
    if (same.test(static_cast<TileType>(t))) {
      text += s.d.type_long[t] + " ";
    }
  }
  s.r().warning(std::move(text));
}

bool declare_contact(BuildState& s, TileType contact, Tokens residues) {
  Data& d = s.d;
  if (!s.magic_compat && d.is_contact[contact]) {
    s.r().error("Contact type " + d.type_long[contact] + " is already declared");  // Q-TECH-17, OFF
    return false;
  }
  ResidueCheck acc;
  bool on_home = false;
  for (const std::string& token : residues) {
    if (!check_residue(s, token, d.type_plane[contact], acc, on_home)) {
      return false;
    }
  }
  if (!on_home) {
    s.r().error("Contact type " + d.type_long[contact] + " missing a residue on its home plane");
    return false;
  }
  warn_identical(s, contact, acc.residues);
  d.is_contact[contact] = true;
  d.residues[contact] |= acc.residues;  // a redeclaration merges (Q-TECH-17)
  d.image_planes[contact] = acc.planes;
  d.contact_list.push_back(contact);
  return true;
}

bool is_contact_keyword(std::string_view word) noexcept {
  return word == "contact" || word == "device";
}

}  // namespace

void contact_init(BuildState& s) {
  Data& d = s.d;
  d.is_contact.fill(false);
  d.image_planes.fill(0);
  d.residues.fill(TypeMask{});
  d.contact_list.clear();
}

bool contact_line(BuildState& s, Tokens tokens) {
  const Data& d = s.d;
  const bool keyword_first = !s.magic_compat && (is_contact_keyword(tokens[0]) || tokens[0] == "stackable");
  const int type = keyword_first ? kNotFound : name_type(d, tokens[0]);
  if (type >= 0) {
    return declare_contact(s, static_cast<TileType>(type), tokens.subspan(1));
  }
  if (is_contact_keyword(tokens[0])) {
    if (tokens.size() < 2) {
      s.r().error("Line must contain a contact type after \"" + tokens[0] + "\"");
      return false;
    }
    const int base = noisy_name_type(d, tokens[1], s.rep, Noise::error);
    return base >= 0 && declare_contact(s, static_cast<TileType>(base), tokens.subspan(2));
  }
  if (tokens[0] == "stackable") {
    return stackable_line(s, tokens.subspan(1));
  }
  (void)noisy_name_type(d, tokens[0], s.rep, Noise::error);
  return false;
}

namespace {

// Planes(t) and OnPlane(p) for one type with home plane p > 0.
void place_images(Data& d, std::size_t t, int home) {
  d.type_planes[t] = plane_bit(static_cast<PlaneId>(home));
  if (!d.is_contact[t]) {
    d.plane_types[static_cast<std::size_t>(home)].set(static_cast<TileType>(t));
    return;
  }
  d.type_planes[t] |= d.image_planes[t];
  for (std::size_t q = kFirstUserPlane; q < d.num_planes(); ++q) {
    if ((d.image_planes[t] & plane_bit(static_cast<PlaneId>(q))) != 0) {
      d.plane_types[q].set(static_cast<TileType>(t));
    }
  }
}

}  // namespace

void contact_final(BuildState& s) {
  Data& d = s.d;
  for (std::size_t t = 0; t < d.num_types; ++t) {
    if (!d.is_contact[t] && d.type_plane[t] > 0) {
      d.image_planes[t] = plane_bit(static_cast<PlaneId>(d.type_plane[t]));
      d.residues[t] = TypeMask::of(static_cast<TileType>(t));
    }
  }
  d.type_planes[kSpace] = ~plane_bit(kPlaneRouter);
  for (std::size_t t = 0; t < d.num_types; ++t) {
    if (d.type_plane[t] > 0) {
      place_images(d, t, d.type_plane[t]);
    }
  }
  d.home_plane_types.fill(TypeMask{});
  for (std::size_t t = 1; t < d.num_types; ++t) {
    if (d.type_plane[t] >= 0) {
      d.home_plane_types[static_cast<std::size_t>(d.type_plane[t])].set(static_cast<TileType>(t));
    }
  }
}

}  // namespace hefesto::tech
