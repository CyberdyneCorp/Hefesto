// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef::SessionLayers: the LEF reader's LayerAccess over the session's LEF
// layer table and via table (lefRead.c HashFind/HashLookOnly on LefInfo,
// 3093-3148 for a new ROUTE entry; Q-LEFDEF-5 fixed).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/read/session_layers.hpp"

#include "hefesto/drc/rules/query.hpp"

namespace hefesto::lef {

namespace {

constexpr db::Coord kDefaultWidth = 3;    // DEFAULT_WIDTH (lefInt.h)
constexpr db::Coord kDefaultSpacing = 4;  // DEFAULT_SPACING

tech::TileType secondary_type(const LayerRecord& record) {
  for (std::size_t t = 1; t < tech::kMaxTileTypes; ++t) {
    const auto type = static_cast<tech::TileType>(t);
    if (type != record.obstruction_type && record.obstruction_types.test(type)) {
      return type;
    }
  }
  return 0;
}

}  // namespace

LayerEntry via_entry(const ViaDefinition& via) {
  LayerEntry entry;
  entry.layer_class = via.layer_class;
  entry.type = via.type.value_or(0);
  entry.obstruction = via.obstruction_type.value_or(0);
  if (!via.area.is_null()) {
    entry.contact_area = via.area;
  }
  return entry;
}

LayerEntry layer_entry(const LayerRecord& record) {
  LayerEntry entry;
  entry.layer_class = record.layer_class;
  entry.type = record.type;
  entry.obstruction = record.obstruction_type;
  if (record.layer_class == LayerClass::cut) {
    entry.via_obstruction = secondary_type(record);
    entry.contact_area = record.contact_area;
  }
  return entry;
}

std::optional<LayerEntry> SessionLayers::find(std::string_view name) const {
  if (const auto* via = vias_.find(std::string{name}); via != nullptr && via->value) {
    return via_entry(*via->value);
  }
  if (const LayerRecord* record = layers_.find(name)) {
    return layer_entry(*record);
  }
  return std::nullopt;
}

void SessionLayers::add_route(std::string_view name, tech::TileType type) {
  const db::Coord width = rules_ != nullptr ? drc::default_width(*rules_, tech_, type) : 0;
  const db::Coord spacing = rules_ != nullptr ? drc::default_spacing(*rules_, tech_, type, type) : 0;
  LayerRecord record;
  record.layer_class = LayerClass::route;
  record.type = type;
  record.width = width != 0 ? width : kDefaultWidth;
  record.spacing = spacing != 0 ? spacing : kDefaultSpacing;
  record.pitch = 0;
  record.direction = RouteDirection::horizontal;
  record.canonical_name = std::string{name};
  (void)layers_.add(name, std::move(record));
}

void SessionLayers::update_route(std::string_view name, const RouteUpdate& update) {
  LayerRecord* record = layers_.find_mutable(name);
  if (record == nullptr) {
    return;
  }
  if (update.width) {
    record->width = update.width;
  }
  if (update.spacing) {
    record->spacing = update.spacing;
  }
  if (update.pitch) {
    record->pitch = update.pitch;
  }
  if (update.direction) {
    record->direction = *update.direction;
  }
}

void SessionLayers::reserve(std::string_view name) {
  if (layers_.find(name) != nullptr || !reserved_.emplace(name).second) {
    return;
  }
  (void)layers_.add(name, LayerRecord{});
  layers_.release(name);
}

}  // namespace hefesto::lef
