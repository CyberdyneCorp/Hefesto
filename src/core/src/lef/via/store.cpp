// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared LEF/DEF via table: VIA, VIARULE and DEF VIAS definitions,
// redefinition and via rectangles (lef/lefRead.c:611-680 LefRedefined,
// 2646-2690 LefAddViaGeometry, 3058-3080; lef/defRead.c:1972-1995;
// capabilities lef-read, def-read; Q-LEFDEF-8 fixed). Magic is (c) the
// Regents of the University of California, BSD-style licence (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/lef/via/store.hpp"

#include <string>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lef/via/cut.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

void report_redefined(std::string_view name, ViaSection section, const ViaContext& context) {
  const std::string quoted = "\"" + std::string{name} + "\" redefined.";
  if (section == ViaSection::def_via) {
    context.sink.emit(diag::make("LEF-VIA-002", "Composite via " + quoted));
  } else {
    context.sink.emit(diag::make("LEF-VIA-001", "Cut type " + quoted));
  }
}

/// A record emptied for redefinition, keeping `layer_class` and the name.
ViaDefinition emptied(LayerClass layer_class, std::string canonical_name) {
  ViaDefinition via;
  via.layer_class = layer_class;
  via.canonical_name = std::move(canonical_name);
  return via;
}

/// The redefinition of a tech `lef` layer name (LefRedefined on a layer
/// entry). One name: the entry is reused with its class. Several: `name`
/// alone gets a new entry, of class VIA (Q-LEFDEF-8 fix).
ViaDefinition redefined_layer(const LayerTable& layers, const LayerRecord& record, std::string_view name) {
  if (shared_names(layers, name) <= 1) {
    return emptied(record.layer_class, record.canonical_name.empty() ? std::string{name} : record.canonical_name);
  }
  return emptied(LayerClass::cut, std::string{name});
}

}  // namespace

std::size_t shared_names(const LayerTable& layers, std::string_view name) {
  const LayerRecord* record = layers.find(name);
  if (record == nullptr) {
    return 0;
  }
  std::size_t count = 0;
  layers.enumerate([&](std::string_view, const LayerRecord& visited) { count += &visited == record ? 1 : 0; });
  return count;
}

ViaDefinition* begin_via(ViaTable& vias, const LayerTable& layers, std::string_view name, ViaSection section,
                         const ViaContext& context) {
  const std::string key{name};
  if (auto* entry = vias.find(key); entry != nullptr && entry->value) {
    if (section == ViaSection::lef_viarule) {
      return nullptr;  // VIA followed by VIARULE: not reprocessed
    }
    report_redefined(name, section, context);
    ViaDefinition& via = *entry->value;
    via = emptied(via.layer_class, via.canonical_name);
    return &via;
  }
  if (const LayerRecord* record = layers.find(name); record != nullptr) {
    if (section == ViaSection::lef_viarule) {
      return nullptr;
    }
    report_redefined(name, section, context);
    return &*vias.insert(key, redefined_layer(layers, *record, name)).value;
  }
  return &*vias.insert(key, emptied(LayerClass::cut, key)).value;
}

void add_via_rect(ViaDefinition& via, std::optional<tech::TileType> layer, const db::Rect& rect,
                  const ViaContext& context) {
  if (!layer) {
    return;  // no geometry for unknown layers
  }
  const tech::TileType named = context.compat.magic_compat && via.type ? *via.type : *layer;
  const db::Rect cut = grow_cut(rect, *layer, named, context);
  if (via.area == db::Rect{}) {
    via.area = cut;
    via.type = layer;
    return;
  }
  ViaShape extra{*layer, cut};
  const bool promote = context.tech.is_contact(*layer) && !(via.type && context.tech.is_contact(*via.type));
  if (promote && via.type) {
    // The primary record becomes the contact; the old primary moves to the list.
    extra = ViaShape{*via.type, via.area};
    via.type = layer;
    via.area = cut;
  }
  via.shapes.insert(via.shapes.begin(), extra);
}

}  // namespace hefesto::lef
