// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Single type-name resolution and the type-list language (capability
// tech-layers-and-planes, "Single type-name resolution", "Type aliases",
// "Type-list language", "Derived type and plane tables").
//
// Ported from Magic VLSI 8.3.694 database/DBtechname.c (DBTechNameType,
// DBTechNameMask0, DBTechTypesToPlanes, DBTypeShortName) and
// database/DBtcontact.c (DBPlaneToResidue, DBFullResidueMask),
// Copyright (C) 1985, 1990 Regents of the University of California
// (see THIRD_PARTY_NOTICES.md).

#include <string>

#include "tech/build_state.hpp"

namespace hefesto::tech {

namespace {

using Data = Technology::Data;

std::string quoted(std::string_view text) {
  return "\"" + std::string{text} + "\"";
}

// The alias-table step of DBTechNameType: one user type, ambiguous, or none.
int alias_single_type(const Data& d, const TypeMask& mask) {
  int found = kNotFound;
  for (std::size_t t = kFirstUserType; t < d.num_user; ++t) {
    if (!mask.test(static_cast<TileType>(t))) {
      continue;
    }
    if (found != kNotFound) {
      return kAmbiguous;
    }
    found = static_cast<int>(t);
  }
  return found;
}

int lookup_base_type(const Data& d, std::string_view base) {
  const auto alias = d.aliases.find(base);
  if (!d.magic_compat && alias != d.aliases.end()) {
    return alias_single_type(d, alias->second);  // Q-TECH-13, magic-compat OFF
  }
  const int type = name_lookup(d.type_names, base, !d.magic_compat);
  if (type >= 0 || alias == d.aliases.end()) {
    return type;
  }
  return alias_single_type(d, alias->second);
}

struct Segment {
  TypeMask types;
  PlaneMask planes = 0;
};

class TypeListParser {
 public:
  TypeListParser(const Data& d, Reporter* rep, Noise noise) : d_(d), rep_(rep), noise_(noise) {}

  void report(std::string text) const {
    if (rep_ != nullptr) {
      rep_->report(noise_, std::move(text));
    }
  }

  Segment segment(std::string_view text, std::size_t& i) {
    const bool all_but = text[i] == '~';
    if (all_but) {
      ++i;
    }
    Segment seg;
    if (i < text.size() && text[i] == '(') {
      seg = group(text, i);
    } else {
      seg.types = atom_or_zero(text, i);
    }
    if (all_but) {
      seg.types = ~seg.types;
    }
    if (i < text.size() && text[i] == '/') {
      plane_restriction(text, i, seg);
    } else {
      seg.planes = user_type_planes(seg.types) | seg.planes;
    }
    return seg;
  }

  Segment group(std::string_view text, std::size_t& i) {
    ++i;
    int nesting = 0;
    std::size_t j = i;
    for (;; ++j) {
      if (j == text.size()) {
        report("Unmatched parenthesis in layer name " + quoted(text) + ".");
        break;
      }
      if (text[j] == '(') {
        ++nesting;
      } else if (text[j] == ')' && --nesting < 0) {
        break;
      }
    }
    TypeListParser inner(d_, rep_, noise_);
    const TypeList sub = inner.parse(text.substr(i, j - i));
    errors_ = errors_ || sub.errors;
    i = j < text.size() ? j + 1 : j;
    return Segment{sub.types, sub.planes};
  }

  TypeList parse(std::string_view text) {
    TypeList out;
    std::size_t i = 0;
    while (i < text.size()) {
      const Segment seg = segment(text, i);
      out.types |= seg.types;
      out.planes |= seg.planes;
      if (replaced_) {
        out.planes = replacement_;
        replaced_ = false;
      }
      while (i < text.size() && text[i] == ',') {
        ++i;
      }
    }
    if ((out.types.empty() || out.types == TypeMask::of(kSpace)) && out.planes == 0) {
      out.planes = d_.type_planes[kSpace];
    }
    out.errors = errors_;
    return out;
  }

  TypeMask atom_or_zero(std::string_view text, std::size_t& i) {
    std::size_t j = i;
    while (j < text.size() && text[j] != '/' && text[j] != ',') {
      ++j;
    }
    TypeMask mask;
    const std::string_view atom = text.substr(i, j - i);
    if (atom.empty()) {
      report("Missing layer name in " + quoted(text) + ".");
    } else if (!(text.substr(i) == "0" || (!d_.magic_compat && atom == "0"))) {
      mask = atom_mask(atom);
    }
    i = j;
    return mask;
  }

  TypeMask atom_mask(std::string_view atom) const {
    if (const auto alias = d_.aliases.find(atom); alias != d_.aliases.end()) {
      return alias->second;
    }
    const bool all_residues = atom.front() == '*';
    const std::string_view name = all_residues ? atom.substr(1) : atom;
    const int t = noise_ == Noise::quiet ? name_type(d_, name) : noisy_name_type(d_, name, rep_, noise_);
    TypeMask mask;
    if (t >= 0) {
      mask.set(static_cast<TileType>(t));
      if (all_residues) {
        add_contacts_with_residue(mask, static_cast<TileType>(t));
      }
    }
    add_stacked_types(mask);
    return mask;
  }

  void add_contacts_with_residue(TypeMask& mask, TileType t) const {
    for (std::size_t r = kFirstUserType; r < d_.num_user; ++r) {
      if (d_.residues[r].test(t)) {
        mask.set(static_cast<TileType>(r));
      }
    }
  }

  void add_stacked_types(TypeMask& mask) const {
    for (std::size_t r = d_.num_user; r < d_.num_types; ++r) {
      if (!(d_.residues[r] & mask).empty()) {
        mask.set(static_cast<TileType>(r));
      }
    }
  }

  void plane_restriction(std::string_view text, std::size_t& i, Segment& seg) {
    std::size_t j = i + 1;
    while (j < text.size() && text[j] != ',') {
      ++j;
    }
    const std::string_view plane_name = text.substr(i + 1, j - i - 1);
    i = j;
    const Noise noise = !d_.magic_compat && noise_ != Noise::quiet ? Noise::error : noise_;
    const int plane = noise == Noise::quiet ? name_plane(d_, plane_name)
                                            : noisy_name_plane(d_, plane_name, rep_, noise);
    if (plane <= 0) {
      if (!d_.magic_compat && plane == 0) {
        report("Plane \"router\" cannot restrict a type list");  // Q-TECH-16, magic-compat OFF
      }
      errors_ = errors_ || (!d_.magic_compat && noise_ != Noise::quiet);
      return;  // no restriction, and no plane added (DBTechNameMask0)
    }
    seg.types &= d_.plane_types[static_cast<std::size_t>(plane)];
    if (d_.magic_compat) {
      replaced_ = true;  // Q-TECH-16: a qualified segment replaces the plane set
      replacement_ = plane_bit(static_cast<PlaneId>(plane));
      seg.planes = 0;
    } else {
      seg.planes = plane_bit(static_cast<PlaneId>(plane));
    }
  }

  [[nodiscard]] PlaneMask user_type_planes(const TypeMask& types) const {
    PlaneMask planes = 0;
    for (std::size_t t = kFirstUserType; t < d_.num_user; ++t) {
      if (types.test(static_cast<TileType>(t))) {
        planes |= d_.type_planes[t];
      }
    }
    return planes;
  }

  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
 private:
  const Data& d_;
  Reporter* rep_;
  Noise noise_;
  bool replaced_ = false;
  bool errors_ = false;
  PlaneMask replacement_ = 0;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace

int name_type(const Data& d, std::string_view name) {
  const auto slash = name.find('/');
  const int type = lookup_base_type(d, name.substr(0, slash));
  if (slash == std::string_view::npos || type < 0) {
    return type;
  }
  const int plane = name_lookup(d.plane_names, name.substr(slash + 1), !d.magic_compat);
  if (plane < 0) {
    return kNotFound;
  }
  return (d.image_planes[static_cast<std::size_t>(type)] & plane_bit(static_cast<PlaneId>(plane))) != 0 ? type
                                                                                                         : kNotFound;
}

int noisy_name_type(const Data& d, std::string_view name, Reporter* rep, Noise noise) {
  const int type = name_type(d, name);
  if (rep != nullptr && type == kAmbiguous) {
    rep->report(noise, "Ambiguous layer (type) name " + quoted(name));
  } else if (rep != nullptr && type == kNotFound) {
    rep->report(noise, "Unrecognized layer (type) name " + quoted(name));
  }
  return type;
}

int name_type_exact(const Data& d, std::string_view name) {
  if (name.find('/') != std::string_view::npos) {
    return kAmbiguous;
  }
  return name_lookup_exact(d.type_names, name);
}

int name_plane(const Data& d, std::string_view name) {
  return name_lookup(d.plane_names, name, !d.magic_compat);
}

int noisy_name_plane(const Data& d, std::string_view name, Reporter* rep, Noise noise) {
  const int plane = name_plane(d, name);
  if (rep != nullptr && plane == kAmbiguous) {
    rep->report(noise, "Ambiguous plane name " + quoted(name));
  } else if (rep != nullptr && plane == kNotFound) {
    rep->report(noise, "Unrecognized plane name " + quoted(name));
  }
  return plane;
}

TypeList name_mask(const Data& d, std::string_view text, Reporter* rep, Noise noise) {
  TypeListParser parser(d, rep, noise);
  return parser.parse(text);
}

PlaneMask types_to_planes(const Data& d, const TypeMask& mask) {
  const PlaneMask no_router = ~plane_bit(kPlaneRouter);
  if (mask.test(kSpace)) {
    const std::size_t k = d.num_planes();
    const PlaneMask all = k >= kMaxPlanes ? ~PlaneMask{0} : plane_bit(static_cast<PlaneId>(k)) - 1;
    return all & no_router;
  }
  PlaneMask planes = 0;
  for (std::size_t t = 0; t < d.num_types; ++t) {
    if (mask.test(static_cast<TileType>(t))) {
      planes |= d.type_planes[t];
    }
  }
  return planes & no_router;
}

std::string type_short_name(const Data& d, TileType type) {
  if (const std::string* primary = primary_name(d.type_names, type)) {
    return *primary;
  }
  if (type < kMaxTileTypes && !d.type_long[type].empty()) {
    return d.type_long[type];
  }
  return "???";
}

TypeMask full_residues(const Data& d, TileType type) {
  if (type < d.num_user) {
    return d.residues[type];
  }
  TypeMask mask;
  for (std::size_t t = kFirstUserType; t < d.num_user; ++t) {
    if (d.residues[type].test(static_cast<TileType>(t))) {
      mask |= d.residues[t];
    }
  }
  return mask;
}

TileType plane_to_residue(const Data& d, TileType type, int plane) {
  for (std::size_t rt = kFirstUserType; rt < d.num_user; ++rt) {
    if (!d.residues[type].test(static_cast<TileType>(rt))) {
      continue;
    }
    if (type >= d.num_user) {
      for (std::size_t rt2 = kFirstUserType; rt2 < d.num_user; ++rt2) {
        if (d.residues[rt].test(static_cast<TileType>(rt2)) && d.type_plane[rt2] == plane) {
          return static_cast<TileType>(rt2);
        }
      }
    } else if (d.type_plane[rt] == plane) {
      return static_cast<TileType>(rt);
    }
  }
  return kSpace;
}

}  // namespace hefesto::tech
