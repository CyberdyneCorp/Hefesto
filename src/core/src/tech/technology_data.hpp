// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// TECH-private representation behind tech::Technology: the name tables, the
// per-type layer information, the paint/erase tables and the connectivity
// tables, laid out like Magic's globals so the ported algorithms read the
// same. Built by the section handlers (sections_*.cpp) through BuildState.
//
// Portions ported from Magic VLSI 8.3.694 (database/DBtechtype.c,
// DBtcontact.c, DBtpaint.c, DBtpaint2.c, DBtech.c, DBtechname.c),
// Copyright (C) 1985, 1990 Regents of the University of California,
// distributed under the permissive Magic license (see THIRD_PARTY_NOTICES.md).
//
// Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/tech/technology.hpp"

namespace hefesto::tech {

/// Lookup results that are not a number (Magic's -1 / -2).
inline constexpr int kAmbiguous = -1;
inline constexpr int kNotFound = -2;

/// One name of a plane or type table (Magic's NameList entry).
struct NameEntry {
  std::string name;
  int value = 0;
  bool primary = false;  // the shortest name of its declaring line
  bool alias = false;    // added by an alias or stackable statement; never primary
};

/// A name table kept sorted byte-wise, as Magic's dbTechNameAddOne does.
using NameTable = std::vector<NameEntry>;

/// One saved contact compose/decompose rule (dbSavedRules).
struct SavedRule {
  bool compose = true;
  TileType result = kSpace;
  std::vector<std::pair<TileType, TileType>> pairs;
};

struct Technology::Data {
  // --- identity --------------------------------------------------------------
  std::string name;
  std::string file_name;
  int format_version = 27;
  TechVersion version;
  bool magic_compat = true;  // lookups follow the mode the technology was loaded in

  // --- planes ----------------------------------------------------------------
  NameTable plane_names;
  std::vector<std::string> plane_long;  // index = plane number (K entries)

  // --- types -----------------------------------------------------------------
  NameTable type_names;
  std::array<std::string, kMaxTileTypes> type_long{};
  std::array<int, kMaxTileTypes> type_plane{};  // home plane (DBPlane); -1 = none
  std::size_t num_types = kFirstUserType;       // N (DBNumTypes)
  std::size_t num_user = kFirstUserType;        // U (DBNumUserLayers)
  std::map<std::string, TypeMask, std::less<>> aliases;  // multi-type aliases

  // --- layer info (dbLayerInfo) ------------------------------------------------
  std::array<bool, kMaxTileTypes> is_contact{};
  std::array<TypeMask, kMaxTileTypes> residues{};       // l_residues
  std::array<PlaneMask, kMaxTileTypes> image_planes{};  // l_pmask
  std::vector<TileType> contact_list;                   // dbContactInfo order
  std::vector<ContactInfo> contact_infos;               // public view, same order

  // --- derived type/plane tables -------------------------------------------
  std::array<PlaneMask, kMaxTileTypes> type_planes{};      // DBTypePlaneMaskTbl
  std::array<TypeMask, kMaxPlanes> plane_types{};          // DBPlaneTypes
  std::array<TypeMask, kMaxPlanes> home_plane_types{};     // DBHomePlaneTypes

  // --- locking ---------------------------------------------------------------
  TypeMask tech_active;  // DBTechActiveLayerBits
  TypeMask active;       // DBActiveLayerBits (the current active set)

  // --- paint / erase ---------------------------------------------------------
  std::size_t table_planes = 0;          // planes stored in the tables below
  std::vector<std::uint8_t> paint;       // [p][t][h]
  std::vector<std::uint8_t> erase;       // [p][t][h]
  std::array<TypeMask, kMaxTileTypes> not_default_paint{};
  std::array<TypeMask, kMaxTileTypes> not_default_erase{};
  std::array<PlaneMask, kMaxTileTypes> paint_planes{};
  std::array<PlaneMask, kMaxTileTypes> erase_planes{};
  std::vector<SavedRule> saved_rules;

  // --- connectivity ----------------------------------------------------------
  std::array<TypeMask, kMaxTileTypes> connect{};
  std::array<TypeMask, kMaxTileTypes> not_connect{};
  std::array<PlaneMask, kMaxTileTypes> conn_planes{};
  std::array<PlaneMask, kMaxTileTypes> all_conn_planes{};

  // --- other sections ------------------------------------------------------
  StylesSection styles;
  std::vector<RawSection> raw_sections;
  std::optional<CifGridInfo> cif_grid;
  bool fringe_halo = false;

  Data();

  [[nodiscard]] std::size_t num_planes() const noexcept { return plane_long.size(); }

  [[nodiscard]] static std::size_t index(std::size_t p, TileType t, TileType h) noexcept {
    return (p * kMaxTileTypes + t) * kMaxTileTypes + h;
  }
  [[nodiscard]] TileType paint_entry(TileType h, TileType t, std::size_t p) const noexcept {
    return p < table_planes ? paint[index(p, t, h)] : h;
  }
  [[nodiscard]] TileType erase_entry(TileType h, TileType t, std::size_t p) const noexcept {
    return p < table_planes ? erase[index(p, t, h)] : h;
  }
  void set_paint(TileType h, TileType t, std::size_t p, TileType r) noexcept {
    if (p < table_planes) {
      paint[index(p, t, h)] = static_cast<std::uint8_t>(r);
    }
  }
  void set_erase(TileType h, TileType t, std::size_t p, TileType r) noexcept {
    if (p < table_planes) {
      erase[index(p, t, h)] = static_cast<std::uint8_t>(r);
    }
  }
  /// SETPAINT / SETERASE of DBtpaint2.c: only default entries of types on the plane.
  void set_default_paint(TileType h, TileType t, std::size_t p, TileType r) noexcept {
    // p is out of range for a type without a home plane (found by fuzz_tech_file).
    if (p < plane_types.size() && !not_default_paint[h].test(t) && plane_types[p].test(h)) {
      set_paint(h, t, p, r);
    }
  }
  void set_default_erase(TileType h, TileType t, std::size_t p, TileType r) noexcept {
    // p is out of range for a type without a home plane (found by fuzz_tech_file).
    if (p < plane_types.size() && !not_default_erase[h].test(t) && plane_types[p].test(h)) {
      set_erase(h, t, p, r);
    }
  }

  /// DBPlane(t) as a plane number, or -1.
  [[nodiscard]] int home(TileType t) const noexcept { return t < kMaxTileTypes ? type_plane[t] : -1; }
  /// LayerPlaneMask(t) (l_pmask).
  [[nodiscard]] PlaneMask layer_planes(TileType t) const noexcept { return image_planes[t]; }

  /// The built-in planes 0..5 and types 0..8 (DBTechInitPlane + DBTechInitType).
  [[nodiscard]] static Data builtin();
};

// --- name tables (names.cpp) -------------------------------------------------

/// dbTechNameLookup: unique prefix, else exact, else ambiguous / not found.
/// With `same_value_resolves` (magic-compat OFF), a prefix whose candidates
/// all carry one value resolves to it (Q-TECH-14).
[[nodiscard]] int name_lookup(const NameTable& table, std::string_view name, bool same_value_resolves);
/// dbTechNameLookupExact.
[[nodiscard]] int name_lookup_exact(const NameTable& table, std::string_view name);
/// Splits a comma-separated name list, skipping empty names.
[[nodiscard]] std::vector<std::string> split_name_list(std::string_view names);
/// The first name of `names` already in `table` or repeated in `names`; empty if none.
[[nodiscard]] std::string first_duplicate(const NameTable& table, const std::vector<std::string>& names);
/// dbTechNameAdd after validation: inserts every name with `value`; the
/// shortest (first of equally short) becomes primary unless `alias`.
/// Returns the first name.
std::string add_names(NameTable& table, const std::vector<std::string>& names, int value, bool alias);
/// The primary name carrying `value`, or nullptr.
[[nodiscard]] const std::string* primary_name(const NameTable& table, int value);

/// Whether the token is an integer in Magic's StrIsInt sense (optional
/// sign, then digits; the empty string qualifies).
[[nodiscard]] bool str_is_int(std::string_view text) noexcept;
/// atoi() of a StrIsInt token (saturating, 0 for an empty digit string).
[[nodiscard]] long long str_to_int(std::string_view text) noexcept;

/// utils/lookup.c Lookup(): case-insensitive unique prefix against `table`
/// entries (each compared up to its first blank); exact wins. Returns the
/// index, kAmbiguous or kNotFound. `strip_namespace` skips "::magic::"/"magic::".
[[nodiscard]] int keyword_lookup(std::string_view text, const std::vector<std::string_view>& table,
                                 bool strip_namespace = false);

}  // namespace hefesto::tech
