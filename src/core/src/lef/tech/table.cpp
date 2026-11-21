// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF layer table (capability lef-tech-section; design D5): lookups,
// enumeration, the magic-to-LEF map, scaling and the default fill. The
// section reader is in section.cpp.
//
// Ported from Magic VLSI 8.3.694 lef/lefTech.c (LefTechScale,
// LefTechSetDefaults), lef/defWrite.c (defGetType) and
// database/DBcellsrch.c (DBScalePoint), Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFTECH (design D2).

#include "hefesto/lef/tech/table.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>

#include "hefesto/drc/rules/query.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

// LefTechScale's route arithmetic: `v *= n; v /= d;` in C int, so both steps
// truncate toward zero. The product is taken in 64 bits and saturated (an
// int overflow is undefined in Magic; fixed in both modes).
db::Coord scale_route_value(db::Coord v, std::int32_t n, std::int32_t d) noexcept {
  const std::int64_t scaled = std::int64_t{v} * n / d;
  return static_cast<db::Coord>(std::clamp<std::int64_t>(scaled, std::numeric_limits<db::Coord>::min(),
                                                         std::numeric_limits<db::Coord>::max()));
}

void scale_route(LayerRecord& r, std::int32_t n, std::int32_t d) {
  for (std::optional<db::Coord>* value : {&r.width, &r.spacing, &r.pitch}) {
    if (*value) {
      **value = scale_route_value(**value, n, d);
    }
  }
}

// DBScalePoint on both corners: DBScaleValue floors.
void scale_area(LayerRecord& r, std::int32_t n, std::int32_t d) {
  if (!r.contact_area) {
    return;
  }
  db::Rect& a = *r.contact_area;
  a = db::Rect{tech::scale_coord(a.x0, n, d), tech::scale_coord(a.y0, n, d), tech::scale_coord(a.x1, n, d),
               tech::scale_coord(a.y1, n, d)};
}

constexpr db::Rect kSentinelArea{-1, -1, -1, -1};

// The DRC defaults of a record's normal type; a record with no normal type
// (an obstruction-only record) gets 0, where Magic calls
// DRCGetDefaultLayerWidth(-1) (an error message and 0) and
// DRCGetDefaultLayerSpacing(-1, -1) (an out-of-bounds read).
struct Defaults {
  const drc::RuleSet& rules;
  const tech::Technology& tech;

  [[nodiscard]] db::Coord width(tech::TileType type) const {
    return type == tech::kSpace ? 0 : drc::default_width(rules, tech, type);
  }
  [[nodiscard]] db::Coord spacing(tech::TileType type) const {
    return type == tech::kSpace ? 0 : drc::default_spacing(rules, tech, type, type);
  }
  [[nodiscard]] db::Rect area(tech::TileType type) const {
    const db::Coord w = width(type);
    return db::Rect{-w, -w, w, w};
  }
};

// LefTechSetDefaults (magic-compat ON): only exact sentinels are filled, and
// the default spacing overwrites the width (Q-LEFDEF-2).
void fill_compat(LayerRecord& r, const Defaults& defaults) {
  if (r.layer_class == LayerClass::cut) {
    if (r.contact_area == kSentinelArea) {
      r.contact_area = defaults.area(r.type);
    }
    return;
  }
  if (r.width == -1) {
    r.width = defaults.width(r.type);
  }
  if (r.spacing == -1) {
    r.width = defaults.spacing(r.type);
  }
}

// magic-compat OFF: every unset value gets its own default.
void fill_native(LayerRecord& r, const Defaults& defaults) {
  if (r.layer_class == LayerClass::cut) {
    if (!r.contact_area) {
      r.contact_area = defaults.area(r.type);
    }
    return;
  }
  if (!r.width) {
    r.width = defaults.width(r.type);
  }
  if (!r.spacing) {
    r.spacing = defaults.spacing(r.type);
  }
}

bool matches_type(const LayerRecord& r, tech::TileType type, bool contact) noexcept {
  const bool class_ok = contact ? r.layer_class == LayerClass::cut
                                : (r.layer_class == LayerClass::route || r.layer_class == LayerClass::masterslice);
  return class_ok && (r.type == type || r.obstruction_type == type);
}

}  // namespace

LayerTable LayerTable::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                    diag::DiagnosticSink& sink) {
  return from_section(section, tech, nullptr, compat::CompatSettings{}, sink);
}

std::vector<std::size_t> LayerTable::live_records() const {
  std::vector<bool> seen(records_.size(), false);
  std::vector<std::size_t> live;
  names_.for_each([&](const auto& entry) {
    if (entry.value && !seen[*entry.value]) {
      seen[*entry.value] = true;
      live.push_back(*entry.value);
    }
  });
  return live;
}

void LayerTable::fill_defaults(const drc::RuleSet& rules, const tech::Technology& tech,
                               const compat::CompatSettings& compat) {
  const Defaults defaults{rules, tech};
  for (const std::size_t index : live_records()) {
    LayerRecord& r = records_[index];
    if (r.layer_class != LayerClass::cut && r.layer_class != LayerClass::route) {
      continue;
    }
    if (compat.magic_compat) {
      fill_compat(r, defaults);
    } else {
      fill_native(r, defaults);
    }
  }
}

void LayerTable::scale(std::int32_t n, std::int32_t d) {
  if (n <= 0 || d <= 0) {
    return;  // a grid factor is strictly positive; never divide by zero
  }
  for (const std::size_t index : live_records()) {
    LayerRecord& r = records_[index];
    if (r.layer_class == LayerClass::cut) {
      scale_area(r, n, d);
    } else if (r.layer_class == LayerClass::route) {
      scale_route(r, n, d);
    }
  }
}

const LayerRecord* LayerTable::find(std::string_view lef_name) const {
  const auto* entry = names_.find(std::string{lef_name});
  return entry != nullptr && entry->value ? &records_[*entry->value] : nullptr;
}

LayerRecord* LayerTable::find_mutable(std::string_view lef_name) {
  auto* entry = names_.find(std::string{lef_name});
  return entry != nullptr && entry->value ? &records_[*entry->value] : nullptr;
}

LayerRecord& LayerTable::add(std::string_view lef_name, LayerRecord record) {
  if (record.canonical_name.empty()) {
    record.canonical_name = std::string{lef_name};
  }
  records_.push_back(std::move(record));
  names_.insert(std::string{lef_name}, records_.size() - 1);
  return records_.back();
}

void LayerTable::release(std::string_view lef_name) {
  (void)names_.clear_value(std::string{lef_name});
}

void LayerTable::note_key(std::string_view lef_name) {
  (void)names_.find_or_insert(std::string{lef_name});
}

void LayerTable::redefine_alias(std::string_view lef_name) {
  const std::string key{lef_name};
  const auto* entry = names_.find(key);
  if (entry == nullptr || !entry->value) {
    return;
  }
  const std::size_t index = *entry->value;
  std::optional<std::string> alias;
  names_.for_each([&](const auto& other) {
    if (!alias && other.value == index && other.key != key) {
      alias = other.key;
    }
  });
  (void)names_.clear_value(key);
  LayerRecord& record = records_[index];
  if (record.canonical_name == key && alias) {
    record.canonical_name = *alias;
  }
}

const LayerRecord* LayerTable::magic_to_lef(tech::TileType type) const {
  if (type == tech::kSpace || type >= tech::kMaxTileTypes) {
    return nullptr;  // "none" is not a magic type
  }
  const bool contact = contact_types_.test(type);
  const LayerRecord* found = nullptr;
  names_.for_each([&](const auto& entry) {
    if (found == nullptr && entry.value && matches_type(records_[*entry.value], type, contact)) {
      found = &records_[*entry.value];
    }
  });
  return found;
}

void LayerTable::enumerate(const Visitor& visit) const {
  names_.for_each([&](const auto& entry) {
    if (entry.value) {
      visit(entry.key, records_[*entry.value]);
    }
  });
}

void LayerTable::enumerate_keys(const KeyVisitor& visit) const {
  names_.for_each([&](const auto& entry) { visit(entry.key, entry.value ? &records_[*entry.value] : nullptr); });
}

}  // namespace hefesto::lef
