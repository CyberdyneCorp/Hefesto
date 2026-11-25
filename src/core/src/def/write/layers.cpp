// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Magic's LefInfo order from the layer table's shared key order, and the
// magic-to-LEF layer map of DEF write (capability def-write "Magic-to-LEF
// layer map", "Table enumeration order"; Q-LEFDEF-92, 116, 135).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defWrite.c defGetType and
// defMakeInverseLayerMap, Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "def/write/internal.hpp"

namespace hefesto::def::write_detail {

namespace {

const std::string kNoName;

/// defGetType's test of one record against `type`: the class of the type
/// (cut for a contact; route or masterslice otherwise), then the normal or
/// obstruction type.
bool maps(const InfoEntry& e, tech::TileType type, bool contact) {
  const lef::LayerClass cls = e.layer_class();
  const bool class_ok = contact ? cls == lef::LayerClass::cut
                                : (cls == lef::LayerClass::route || cls == lef::LayerClass::masterslice);
  return class_ok && (e.type() == type || e.obstruction_type() == type);
}

LefMapping mapping(const InfoEntry& e) {
  LefMapping m;
  m.name = e.canonical_name();
  m.has_record = true;
  m.via_class = e.layer_class() == lef::LayerClass::cut;
  if (e.layer != nullptr) {
    m.route_width = e.layer->width;
  }
  return m;
}

}  // namespace

std::string g10(double value) {
  std::array<char, 64> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.10g", value);
  return buffer.data();
}

lef::LayerClass InfoEntry::layer_class() const noexcept {
  if (via != nullptr) {
    return via->layer_class;
  }
  return layer != nullptr ? layer->layer_class : lef::LayerClass::ignore;
}

std::optional<tech::TileType> InfoEntry::type() const noexcept {
  if (via != nullptr) {
    return via->type;
  }
  if (layer != nullptr && layer->type != tech::kSpace) {
    return layer->type;
  }
  return std::nullopt;
}

std::optional<tech::TileType> InfoEntry::obstruction_type() const noexcept {
  if (via != nullptr) {
    return via->obstruction_type;
  }
  if (layer != nullptr && layer->obstruction_type != tech::kSpace) {
    return layer->obstruction_type;
  }
  return std::nullopt;
}

const std::string& InfoEntry::canonical_name() const noexcept {
  if (via != nullptr) {
    return via->canonical_name;
  }
  return layer != nullptr ? layer->canonical_name : kNoName;
}

std::vector<InfoEntry> info_order(const lef::LayerTable& layers, const lef::ViaTable& vias) {
  std::vector<InfoEntry> out;
  layers.enumerate_keys([&](std::string_view key, const lef::LayerRecord* record) {
    InfoEntry entry{std::string{key}, record, nullptr};
    if (const auto* via = vias.find(entry.key); via != nullptr && via->value) {
      entry = InfoEntry{std::string{key}, nullptr, &*via->value};  // a via shadows a layer name
    }
    out.push_back(std::move(entry));
  });
  return out;
}

LayerMap::LayerMap(const std::vector<InfoEntry>& order, const tech::Technology& tech) {
  map_.resize(tech.type_count());
  for (std::size_t t = tech::kFirstUserType; t < map_.size(); ++t) {
    const auto type = static_cast<tech::TileType>(t);
    const bool contact = tech.is_contact(type);
    for (const InfoEntry& e : order) {
      if (e.live() && maps(e, type, contact)) {
        map_[t] = mapping(e);
        break;
      }
    }
  }
}

const LefMapping& LayerMap::at(tech::TileType type) const noexcept {
  return type < map_.size() ? map_[type] : none_;
}

void Context::say(std::string_view code, std::string text) const {
  messages.push_back(diag::make(code, std::move(text)));
}

void Context::say_native(std::string_view code, std::string text) const {
  messages.push_back(diag::make_native(code, std::move(text)));
}

bool Context::active(tech::TileType type) const {
  const auto types = extract_style.active_types();
  return types && types->test(type);
}

std::optional<tech::TileType> node_type(const Context& c, std::string_view layer) {
  const auto found = c.tech.find_type(layer);
  if (!found.ok()) {
    return std::nullopt;
  }
  return *found.id;
}

}  // namespace hefesto::def::write_detail
