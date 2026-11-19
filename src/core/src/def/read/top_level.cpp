// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DefRead's top-level loop (capability def-read "Top-level DEF grammar and
// section dispatch", "Unit scaling", "Design cell selection", "Die area"):
// the section table, UNITS (Q-LEFDEF-79), DESIGN and the fallback design
// name (Q-LEFDEF-78), TECHNOLOGY, DIEAREA (Q-LEFDEF-77, 68), FILLS, SLOTS
// and STYLES (Q-LEFDEF-84), and DefNewCell.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c:2409-2747, Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#include <algorithm>
#include <array>
#include <string>

#include "def/read/reader.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/lef/read/units.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::def::detail {

namespace {

// DefRead's `sections` table, in its order, then the sections Hefesto
// knows with magic-compat OFF (Q-LEFDEF-84).
enum Section : std::size_t {
  kVersion,
  kNamesCaseSensitive,
  kUnits,
  kDesign,
  kRegions,
  kRow,
  kTracks,
  kGCellGrid,
  kDividerChar,
  kBusBitChars,
  kPropertyDefinitions,
  kDefaultCap,
  kTechnology,
  kHistory,
  kDieArea,
  kComponents,
  kVias,
  kPins,
  kPinProperties,
  kSpecialNets,
  kNets,
  kIoTimings,
  kScanChains,
  kConstraints,
  kGroups,
  kExtension,
  kBlockages,
  kNonDefaultRules,
  kEnd,
  kFills,
  kSlots,
  kStyles,
};

constexpr std::array<std::string_view, 32> kSections{
    "VERSION",   "NAMESCASESENSITIVE", "UNITS",     "DESIGN",          "REGIONS",     "ROW",
    "TRACKS",    "GCELLGRID",          "DIVIDERCHAR", "BUSBITCHARS",   "PROPERTYDEFINITIONS",
    "DEFAULTCAP", "TECHNOLOGY",        "HISTORY",   "DIEAREA",         "COMPONENTS",  "VIAS",
    "PINS",      "PINPROPERTIES",      "SPECIALNETS", "NETS",          "IOTIMINGS",   "SCANCHAINS",
    "CONSTRAINTS", "GROUPS",           "BEGINEXT",  "BLOCKAGES",       "NONDEFAULTRULES", "END",
    "FILLS",     "SLOTS",              "STYLES",
};

constexpr std::size_t kMagicSections = kEnd + 1;  // FILLS, SLOTS, STYLES are not Magic's

bool ignored_statement(std::size_t section) noexcept {
  switch (section) {
    case kVersion:
    case kNamesCaseSensitive:
    case kRow:
    case kTracks:
    case kGCellGrid:
    case kDividerChar:
    case kBusBitChars:
    case kHistory:
      return true;
    default:
      return false;
  }
}

bool skipped_section(std::size_t section) noexcept {
  switch (section) {
    case kRegions:
    case kPropertyDefinitions:
    case kDefaultCap:
    case kPinProperties:
    case kIoTimings:
    case kScanChains:
    case kConstraints:
    case kGroups:
    case kExtension:
      return true;
    default:
      return false;
  }
}

bool counted(std::size_t section) noexcept {
  switch (section) {
    case kComponents:
    case kVias:
    case kPins:
    case kSpecialNets:
    case kNets:
    case kBlockages:
    case kNonDefaultRules:
      return true;
    default:
      return false;
  }
}

}  // namespace

db::CellDef* Reader::run() {
  s0_ = lef::lef_scale(context_.style);
  oscale_ = s0_;
  while (const auto token = next()) {
    progress();
    if (cancelled() || !top_statement(*token)) {
      break;
    }
  }
  return design_;
}

// One top-level statement; false when reading stops (END DESIGN).
bool Reader::top_statement(const std::string& keyword) {
  auto index = lef::lookup_full(keyword, kSections);
  if (index && *index >= kMagicSections && magic_compat()) {
    index.reset();  // Q-LEFDEF-84 ON: FILLS, SLOTS and STYLES are unknown keywords
  }
  if (!index) {
    message("Unknown keyword \"" + keyword + "\" in DEF file; ignoring.");
    skip_statement();
    return true;
  }
  const std::size_t section = *index;
  if (ignored_statement(section)) {
    skip_statement();
  } else if (skipped_section(section)) {
    skip_section(kSections[section]);
  } else if (section >= kMagicSections) {
    message(std::string{kSections[section]} + " section not supported; skipped.");
    skip_section(kSections[section]);
  } else if (counted(section)) {
    return counted_section(section);
  } else if (section == kEnd) {
    if (end_matches("DESIGN")) {
      return false;
    }
    error("END statement out of context.");
  } else if (section == kUnits) {
    units();
  } else if (section == kDesign) {
    design_statement();
  } else if (section == kTechnology) {
    technology_statement();
  } else {
    die_area();
  }
  return true;
}

// `<SECTION> <count> ... ;` and the section body.
bool Reader::counted_section(std::size_t section) {
  const std::int32_t total = parse_int(take()).value_or(0);
  skip_statement();
  const bool annotate = context_.options.annotate;
  if ((section == kComponents && annotate) ||
      (section == kBlockages && (annotate || context_.options.no_blockage))) {
    skip_section(kSections[section]);
    return true;
  }
  switch (section) {
    case kVias:
      vias(total);
      break;
    case kComponents:
      components(total);
      break;
    case kPins:
      pins(total);
      break;
    case kSpecialNets:
    case kNets:
      nets(total, section == kSpecialNets);
      break;
    case kNonDefaultRules:
      nondefault_rules(total);
      break;
    default:
      blockages(total);
      break;
  }
  return true;
}

// UNITS DISTANCE MICRONS <n> ; (Q-LEFDEF-79).
void Reader::units() {
  (void)take();
  (void)take();
  const std::string token = take();
  const auto value = magic_compat() ? parse_int(token) : parse_whole_int(token);
  std::int32_t dscale = 100;
  if (!value || (!magic_compat() && *value <= 0)) {
    error("Invalid syntax for UNITS statement.");
    message("Assuming default value of 100");
  } else {
    dscale = *value;
  }
  if (magic_compat()) {
    oscale_ *= static_cast<float>(dscale);
  } else {
    if (units_seen_) {
      fix_warning("Repeated UNITS statement; the new value replaces the previous one.");
    }
    oscale_ = s0_ * static_cast<float>(dscale);
  }
  units_seen_ = true;
  skip_statement();
}

void Reader::design_statement() {
  const auto name = next();
  if (!name) {
    return;
  }
  if (design_ == nullptr) {
    (void)new_cell(*name);
  } else if (design_->name() != *name) {
    (void)context_.cells.rename_definition(*design_, *name);  // DBCellRenameDef
  }
  skip_statement();
}

void Reader::technology_statement() {
  const std::string name = take();
  if (name != context_.tech.name()) {
    warning("DEF technology name \"" + name + "\" does not match current magic technology name \"" +
            context_.tech.name() + "\"");
  }
  skip_statement();
}

// DIEAREA: FIXED_BBOX from the first two points (Q-LEFDEF-77 ON) or the
// bounding box of every point (OFF). A malformed point skips the statement
// (Q-LEFDEF-68: Magic dereferences NULL).
void Reader::die_area() {
  std::optional<db::Rect> die;
  bool semicolon = false;
  if (magic_compat()) {
    die = read_rect(true, oscale_, semicolon);
  } else {
    die = die_rect_all_points();
    semicolon = true;
  }
  if (die) {
    set_die_area(*die);
  }
  if (!semicolon) {
    skip_statement();
  }
}

// Every `( x y )` or `x y` point up to `;`, as their bounding box.
std::optional<db::Rect> Reader::die_rect_all_points() {
  std::optional<db::Rect> box;
  bool bad = false;
  std::size_t points = 0;
  // Out-parameter form: GCC 11 flags `*opt` after a conditional optional as
  // maybe-uninitialized (-Werror).
  const auto scaled_into = [this](const std::optional<float>& v, db::Coord& out) {
    const std::optional<db::Coord> s = v ? scaled(*v) : std::nullopt;
    out = s.value_or(0);
    return s.has_value();
  };
  for (std::string token = take(); !token.starts_with(';') && !ended(); token = take()) {
    const bool paren = token == "(";
    const auto x = lef::parse_number(paren ? take() : token);
    const auto y = lef::parse_number(take());
    db::Coord px = 0;
    db::Coord py = 0;
    if (!scaled_into(x, px) || !scaled_into(y, py) || (paren && take() != ")")) {
      bad = true;
      continue;
    }
    ++points;
    const db::Rect p{px, py, px, py};
    box = box ? db::Rect{std::min(box->x0, p.x0), std::min(box->y0, p.y0), std::max(box->x1, p.x1),
                         std::max(box->y1, p.y1)}
              : p;
  }
  if (bad || points < 2) {
    lef_error("Bad port geometry: RECT requires 4 values.");
    return std::nullopt;
  }
  return box;
}

void Reader::set_die_area(const db::Rect& die) {
  db::CellDef& cell = design();
  if (created_ == nullptr && !die_change_.changed) {
    die_change_.changed = true;
    if (const auto* old = cell.properties().find("FIXED_BBOX")) {
      if (const auto* dims = std::get_if<db::DimensionList>(old); dims != nullptr && dims->values.size() == 4) {
        die_change_.before = db::Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
      }
    }
  }
  db::set_property(cell, "FIXED_BBOX", db::PropertyValue{db::DimensionList{{die.x0, die.y0, die.x1, die.y1}}});
}

// --- the design cell -------------------------------------------------------------------

// Q-LEFDEF-78: the name as typed (ON), or its basename without `.def` (OFF).
std::string Reader::fallback_name() const {
  const std::string& typed = context_.file_argument;
  if (magic_compat()) {
    return typed;
  }
  std::string base = typed.substr(typed.find_last_of('/') == std::string::npos ? 0 : typed.find_last_of('/') + 1);
  if (base.size() > 4 && base.ends_with(".def")) {
    base.resize(base.size() - 4);
  }
  return base;
}

db::CellDef& Reader::design() {
  if (design_ == nullptr) {
    return new_cell(fallback_name());
  }
  return *design_;
}

// DefNewCell: an existing cell keeps its contents; a new one is empty and
// available.
db::CellDef& Reader::new_cell(const std::string& name) {
  // The name create() would store (a trailing ".mag" stripped, an empty
  // name "(UNNAMED)"): an existing cell of that name is used, so create()
  // cannot fail.
  const std::string stored = name.size() > 4 && name.ends_with(".mag") ? name.substr(0, name.size() - 4) : name;
  if (db::CellDef* existing = context_.cells.find(stored.empty() ? "(UNNAMED)" : stored)) {
    design_ = existing;
    return *existing;
  }
  design_ = context_.cells.create(name, context_.tech.plane_count()).value();
  design_->flags().available = true;
  created_ = design_;
  // The cell's creation is the read's undo event (def/read/undo.hpp); its
  // contents are not recorded as edits.
  suspend_ = std::make_unique<undo::ScopedSuspend>(context_.undo);
  return *design_;
}

}  // namespace hefesto::def::detail
