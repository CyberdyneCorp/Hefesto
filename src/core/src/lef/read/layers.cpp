// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF layer name resolution: LefReadLayers (lefRead.c:697-765) and the
// lowercase magic-name fallback (lefRead.c:579-591; DBtechname.c:64-104).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/read/layers.hpp"

#include <string>

#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

constexpr std::string_view kErrorCode = "LEF-READ-001";

std::optional<tech::TileType> some(tech::TileType type) {
  return type == 0 ? std::nullopt : std::optional<tech::TileType>{type};
}

// Step 1 for an obstruction lookup: the obstruction type, else the normal
// type unless the entry is IGNORE; a VIA entry with an obstruction type
// also sets the secondary type (the dangling else of lefRead.c:718-723).
void obstruction_type(const LayerEntry& entry, LayerState& state) {
  state.type = some(entry.obstruction);
  if (!state.type && entry.layer_class != LayerClass::ignore) {
    state.type = some(entry.type);
  } else if (entry.layer_class == LayerClass::cut) {
    state.secondary = some(entry.via_obstruction);
  }
}

void table_type(const LayerEntry& entry, LayerLookup lookup, LayerState& state) {
  if (lookup == LayerLookup::obstruction) {
    obstruction_type(entry, state);
  } else if (entry.layer_class != LayerClass::ignore) {
    state.type = some(entry.type);
  }
  state.contact_area = entry.layer_class == LayerClass::cut ? entry.contact_area : std::nullopt;
}

}  // namespace

std::optional<tech::TileType> magic_type(const tech::Technology& tech, std::string_view name) {
  if (const auto found = tech.find_type(name); found.ok()) {
    return found.id;
  }
  std::string lower{name.substr(0, 255)};
  for (char& c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return tech.find_type(lower).id;
}

namespace {

// The name has no entry: the magic-name fallback (lefRead.c:745-752),
// unless Magic's HashLookOnly finds it as a key with no record. Then
// LefReadLayer (no result pointers) resolves nothing; with magic-compat OFF
// the key is treated as absent, as for LefReadLayers (Q-LEFDEF-4, 151).
std::optional<tech::TileType> unlisted_type(std::string_view token, LayerSource source, const LayerContext& context) {
  if (source == LayerSource::read_layer && context.compat.magic_compat && context.layers.keyed_without_entry(token)) {
    return std::nullopt;
  }
  return magic_type(context.tech, token);
}

}  // namespace

void resolve_layer(std::string_view token, LayerLookup lookup, LayerState& state, const LayerContext& context,
                   LayerSource source) {
  if (!context.compat.magic_compat) {
    // Q-LEFDEF-3 OFF: each LAYER paints only its own material at the file's size.
    state.secondary.reset();
    state.contact_area.reset();
  }
  state.type.reset();
  if (token.starts_with(';')) {
    context.messages.report(Dialect::lef, diag::Severity::error, kErrorCode, "Bad Layer statement");
    return;
  }
  const auto entry = context.layers.find(token);
  if (entry) {
    table_type(*entry, lookup, state);
  } else {
    state.type = unlisted_type(token, source, context);
  }
  if (!state.type && !(entry && entry->layer_class == LayerClass::ignore)) {
    const std::string name{token};
    context.messages.report(Dialect::lef, diag::Severity::error, kErrorCode,
                            "Don't know how to parse layer \"" + name + "\"");
    context.messages.report(Dialect::lef, diag::Severity::error, kErrorCode,
                            "Try adding this name to the LEF techfile section");
  }
}

}  // namespace hefesto::lef
