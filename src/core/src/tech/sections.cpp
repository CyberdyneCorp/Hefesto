// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Section handlers for `tech`, `version` (tech-file-format), `planes`,
// `types`, `aliases` (tech-layers-and-planes) and `styles`
// (tech-styles-section, raw: references are resolved by the viewer).
//
// Ported from Magic VLSI 8.3.694 database/DBtech.c (DBTechSetTech,
// DBTechSetVersion), database/DBtechtype.c (DBTechInitPlane,
// DBTechAddPlane, DBTechInitType, DBTechAddType, DBTechFinalType,
// DBTechAddAlias) and dbwind/DBWdisplay.c (DBWTechAddStyle),
// Copyright (C) 1985, 1990 Regents of the University of California
// (see THIRD_PARTY_NOTICES.md).

#include <array>
#include <cctype>
#include <string>

#include "tech/build_state.hpp"

namespace hefesto::tech {

namespace {

using Data = Technology::Data;

struct BuiltinType {
  TileType type;
  int plane;
  const char* names;
};

constexpr std::array<BuiltinType, 9> kBuiltinTypes = {{
    {kSpace, -1, "space"},
    {kCheckPaint, kPlaneDrcCheck, "checkpaint,CP"},
    {kCheckSubcell, kPlaneDrcCheck, "checksubcell,CS"},
    {kErrorP, kPlaneDrcError, "error_p,EP"},
    {kErrorS, kPlaneDrcError, "error_s,ES"},
    {kErrorPS, kPlaneDrcError, "error_ps,EPS"},
    {kMagnet, kPlaneMHint, "magnet,mag"},
    {kFence, kPlaneFHint, "fence,f"},
    {kRotate, kPlaneRHint, "rotate,r"},
}};

struct BuiltinPlane {
  PlaneId plane;
  const char* name;
};

// dbTechDefaultPlanes, in Magic's table order.
constexpr std::array<BuiltinPlane, 6> kBuiltinPlanes = {{
    {kPlaneRouter, "router"},
    {kPlaneDrcError, "designRuleError"},
    {kPlaneDrcCheck, "designRuleCheck"},
    {kPlaneMHint, "mhint"},
    {kPlaneFHint, "fhint"},
    {kPlaneRHint, "rhint"},
}};

// The version Hefesto claims compatibility with (Q-TECH-28).
constexpr std::array<int, 3> kMagicVersion = {8, 3, 694};

void reset_plane_names(Data& d) {
  d.plane_names.clear();
  d.plane_long.assign(kFirstUserPlane, std::string{});
  for (const auto& p : kBuiltinPlanes) {
    d.plane_long[p.plane] = add_names(d.plane_names, {p.name}, p.plane, false);
  }
}

void reset_type_names(Data& d) {
  d.type_names.clear();
  for (const auto& t : kBuiltinTypes) {
    d.type_long[t.type] = add_names(d.type_names, split_name_list(t.names), t.type, false);
    d.type_plane[t.type] = t.plane;
  }
  for (std::size_t t = kFirstUserType; t < kMaxTileTypes; ++t) {
    d.type_long[t].clear();
    d.type_plane[t] = -1;
  }
}

std::string join_tokens(Tokens tokens, std::size_t from) {
  std::string out{tokens[from]};
  for (std::size_t i = from + 1; i < tokens.size(); ++i) {
    out += '\n';
    out += tokens[i];
  }
  return out;
}

// sscanf("%d") on the text at `i`: optional blanks and sign, then digits.
bool scan_int(std::string_view text, std::size_t& i, int& value) {
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) != 0) {
    ++i;
  }
  const std::size_t start = i;
  if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
    ++i;
  }
  const std::size_t digits = i;
  while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
    ++i;
  }
  if (i == digits) {
    return false;
  }
  value = static_cast<int>(str_to_int(text.substr(start, i - start)));
  return true;
}

// sscanf(vstring, "%d.%d.%d") == 3.
bool scan_version(std::string_view text, std::array<int, 3>& v) {
  std::size_t i = 0;
  for (std::size_t part = 0; part < 3; ++part) {
    if (part > 0 && (i >= text.size() || text[i++] != '.')) {
      return false;
    }
    if (!scan_int(text, i, v[part])) {
      return false;
    }
  }
  return true;
}

bool requires_line(BuildState& s, std::string_view required) {
  std::size_t i = 0;
  while (i < required.size() && (required[i] < '0' || required[i] > '9')) {
    ++i;
  }
  std::array<int, 3> r{};
  if (!scan_version(required.substr(i), r)) {
    s.r().error("Badly formed magic version string, should be major.minor.rev");
    return false;
  }
  if (kMagicVersion >= r) {
    return true;
  }
  s.r().error("Error:  Magic version " + std::to_string(r[0]) + "." + std::to_string(r[1]) + "." +
              std::to_string(r[2]) + " is required by this techfile, but this version of magic is " +
              std::to_string(kMagicVersion[0]) + "." + std::to_string(kMagicVersion[1]) + "." +
              std::to_string(kMagicVersion[2]) + ".");
  return false;
}

bool is_format_keyword(const BuildState& s, std::string_view word) {
  if (!s.magic_compat) {
    return word == "format" || word == "version";  // Q-TECH-9, magic-compat OFF
  }
  return word.substr(0, 6) == "format" || word.substr(0, 7) == "version";
}

}  // namespace

void Reporter::emit(std::string_view code, std::string text) {
  if (where_.file.empty()) {
    sink_.emit(diag::make(code, std::move(text)));  // a command-time parse: no file location
    return;
  }
  diag::Diagnostic d = diag::make(code, std::move(text), where_);
  if (stamp_section_) {
    d.related = diag::ObjectRef{diag::ObjectKind::section, section_};
  }
  sink_.emit(d);
}

Data::Data() {
  type_plane.fill(-1);
}

Data Data::builtin() {
  Data d;
  d.name = "minimum";
  reset_plane_names(d);
  reset_type_names(d);
  return d;
}

// --- tech / version --------------------------------------------------------

void tech_init(BuildState& s) {
  s.d.format_version = 27;
}

bool tech_line(BuildState& s, Tokens tokens) {
  if (tokens.size() == 1) {
    s.d.name = tokens[0];
    return true;
  }
  if (tokens.size() == 2 && is_format_keyword(s, tokens[0])) {
    if (str_is_int(tokens[1])) {
      s.d.format_version = static_cast<int>(str_to_int(tokens[1]));
    } else {
      s.r().warning("Bad format version number. . . assuming 27");
      if (!s.magic_compat) {
        s.d.format_version = 27;
      }
    }
    return true;
  }
  s.r().error("Badly formed technology name");
  return false;
}

bool version_line(BuildState& s, Tokens tokens) {
  if (tokens.size() >= 2 && tokens[0] == "version") {
    s.d.version.version_lines = {join_tokens(tokens, 1)};
    return true;
  }
  if (tokens.size() >= 2 && tokens[0] == "description") {
    s.d.version.description_lines = {join_tokens(tokens, 1)};
    return true;
  }
  if (tokens.size() >= 2 && tokens[0] == "requires") {
    return requires_line(s, tokens[1]);
  }
  s.r().error("Badly formed version line\nUsage: {version text}|{description text}|{requires text}");
  return false;
}

// --- planes ----------------------------------------------------------------

void planes_init(BuildState& s) {
  reset_plane_names(s.d);
}

bool planes_line(BuildState& s, Tokens tokens) {
  Data& d = s.d;
  if (d.num_planes() >= kMaxPlanes) {
    s.r().error("Too many tile planes (max=64)");
    return false;
  }
  const auto names = tokens.size() == 1 ? split_name_list(tokens[0]) : std::vector<std::string>{};
  if (names.empty()) {
    s.r().error("Line must contain names for plane");
    return false;
  }
  // Q-TECH-11: the whole line is validated before any name is added.
  if (const auto dup = first_duplicate(d.plane_names, names); !dup.empty()) {
    s.r().error("Duplicate name: " + dup);
    return false;
  }
  d.plane_long.push_back(add_names(d.plane_names, names, static_cast<int>(d.num_planes()), false));
  return true;
}

// --- types -----------------------------------------------------------------

void types_init(BuildState& s) {
  Data& d = s.d;
  reset_type_names(d);
  d.aliases.clear();
  d.tech_active = TypeMask{};  // holds the locked types until types_final
  d.num_types = kFirstUserType;
}

namespace {

bool types_alias_line(BuildState& s, Tokens tokens) {
  if (tokens.size() < 3) {  // Q-TECH-12: Magic dereferences a missing token here
    s.r().warning("Line must contain at least 2 fields");
    return true;
  }
  if (tokens[2].find('*') != std::string::npos) {
    s.r().warning("Type alias \"" + tokens[2] +
                  "\" contains the wildcard character \"*\" (alias ignored).\n"
                  "Perhaps you want to define aliases in the \"alias\" section?");
    return true;
  }
  return aliases_line(s, tokens.subspan(1));
}

}  // namespace

bool types_line(BuildState& s, Tokens tokens) {
  Data& d = s.d;
  if (d.num_types >= kUsableTileTypes) {
    s.r().error("Too many tile types (max=254)");
    return false;
  }
  if (tokens.size() < 2) {
    s.r().warning("Line must contain at least 2 fields");
    return true;
  }
  if (tokens[0] == "alias") {
    return types_alias_line(s, tokens);
  }
  const auto names = split_name_list(tokens[1]);
  if (names.empty()) {
    s.r().error("Line must contain at least one type name");
    return false;
  }
  if (const auto dup = first_duplicate(d.type_names, names); !dup.empty()) {
    s.r().error("Duplicate name: " + dup);
    return false;
  }
  const bool locked = !tokens[0].empty() && tokens[0].front() == '-';
  const int plane = noisy_name_plane(d, locked ? tokens[0].substr(1) : tokens[0], s.rep, Noise::error);
  if (plane < 0) {
    return false;
  }
  const auto type = static_cast<TileType>(d.num_types);
  d.type_long[type] = add_names(d.type_names, names, type, false);
  d.type_plane[type] = plane;
  d.tech_active.set(type, locked);
  ++d.num_types;
  return true;
}

void types_final(BuildState& s) {
  Data& d = s.d;
  d.num_user = d.num_types;
  // The locked types collected in tech_active become the active set (DBTechFinalType).
  TypeMask active;
  for (std::size_t t = kMagnet; t < kMaxTileTypes; ++t) {
    active.set(static_cast<TileType>(t), !d.tech_active.test(static_cast<TileType>(t)));
  }
  d.active = active;
  d.tech_active = active;
  d.plane_types[kPlaneRouter] = TypeMask{};
  for (std::size_t p = 1; p < kMaxPlanes; ++p) {
    d.plane_types[p] = TypeMask::of(kSpace);
  }
}

// --- aliases ---------------------------------------------------------------

bool aliases_line(BuildState& s, Tokens tokens) {
  Data& d = s.d;
  if (tokens.size() < 2) {
    s.r().warning("Line must contain at least 2 fields");
    return true;
  }
  if (name_type_exact(d, tokens[0]) >= 0) {
    s.r().warning("Type alias \"" + tokens[0] + "\" shadows a defined type");
    return true;
  }
  const TypeList list = name_mask(d, tokens[1], s.rep, Noise::warning);
  const int single = name_type(d, tokens[1]);
  if (single >= 0 && list.types == TypeMask::of(static_cast<TileType>(single))) {
    if (const auto dup = first_duplicate(d.type_names, {tokens[0]}); !dup.empty()) {
      s.r().warning("Duplicate name: " + dup);
    } else {
      add_names(d.type_names, {tokens[0]}, single, true);
    }
    return true;
  }
  if (d.aliases.contains(tokens[0])) {
    s.r().warning("Type \"" + tokens[0] + "\" is already defined and cannot be an alias");
    return true;
  }
  d.aliases.emplace(tokens[0], list.types);
  return true;
}

// --- styles ----------------------------------------------------------------

void styles_init(BuildState& s) {
  s.d.styles = StylesSection{};  // Q-TECH-24: cleared at every styles section
}

bool styles_line(BuildState& s, Tokens tokens) {
  Data& d = s.d;
  if (tokens.size() < 2) {
    s.r().warning("Badly formed line in \"style\" section");
    return true;
  }
  if (tokens[0] == "styletype") {
    d.styles.style_family = tokens[1].substr(0, 49);
    d.styles.search_dirs.assign(tokens.begin() + 2, tokens.end());
    d.styles.lines.clear();  // a styletype line resets the mapping
    return true;
  }
  const int t = noisy_name_type(d, tokens[0], s.rep, Noise::error);
  if (t < 0) {
    return false;
  }
  StyleRefLine line;
  line.type = static_cast<TileType>(t);
  line.refs.assign(tokens.begin() + 1, tokens.end());
  line.line = s.r().where().line;
  // "Contact style propagation": stacked types existing now, same home plane.
  if (d.is_contact[line.type] && line.type < d.num_user) {
    for (std::size_t r = d.num_user; r < d.num_types; ++r) {
      if (d.residues[r].test(line.type) && d.type_plane[r] == d.type_plane[line.type]) {
        line.stacked.set(static_cast<TileType>(r));
      }
    }
  }
  d.styles.lines.push_back(std::move(line));
  return true;
}

}  // namespace hefesto::tech
