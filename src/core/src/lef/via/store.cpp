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

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lef/via/cut.hpp"
#include "hefesto/tech/commands.hpp"
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
ViaDefinition redefined_layer(const LayerRecord& record, std::string_view name, bool shared) {
  if (!shared) {
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

ViaDefinition* begin_via(ViaTable& vias, LayerTable& layers, std::string_view name, ViaSection section,
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
    const bool shared = shared_names(layers, name) > 1;
    ViaDefinition via = redefined_layer(*record, name, shared);
    if (shared) {
      layers.redefine_alias(name);  // Q-LEFDEF-8: `name` now names the via alone
    }
    return &*vias.insert(key, std::move(via)).value;
  }
  return &*vias.insert(key, emptied(LayerClass::cut, key)).value;
}

void ViaRectBuilder::add(std::optional<tech::TileType> layer, const db::Rect& rect, const ViaContext& context) {
  if (!layer) {
    return;  // no geometry for unknown layers
  }
  const tech::TileType named = context.compat.magic_compat && via_.type ? *via_.type : *layer;
  const db::Rect cut = grow_cut(rect, *layer, named, context);
  if (via_.area == db::Rect{}) {
    via_.area = cut;
    via_.type = layer;
    return;
  }
  ViaShape extra{*layer, cut};
  const bool promote = context.tech.is_contact(*layer) && !(via_.type && context.tech.is_contact(*via_.type));
  if (promote && via_.type) {
    // The primary record becomes the contact; the old primary moves to the list.
    extra = ViaShape{*via_.type, via_.area};
    via_.type = layer;
    via_.area = cut;
  }
  via_.shapes.push_back(extra);
}

void ViaRectBuilder::commit() {
  auto& shapes = via_.shapes;
  const auto first = shapes.begin() + static_cast<std::ptrdiff_t>(std::min(first_, shapes.size()));
  std::reverse(first, shapes.end());
  std::rotate(shapes.begin(), first, shapes.end());
  first_ = shapes.size();
}

void add_via_rect(ViaDefinition& via, std::optional<tech::TileType> layer, const db::Rect& rect,
                  const ViaContext& context) {
  ViaRectBuilder builder{via};
  builder.add(layer, rect, context);
  builder.commit();
}

namespace {

// DBScalePoint on both corners (DBScaleValue floors; infinities kept).
db::Rect scale_rect_floor(const db::Rect& r, std::int32_t n, std::int32_t d) noexcept {
  return db::Rect{tech::scale_coord(r.x0, n, d), tech::scale_coord(r.y0, n, d), tech::scale_coord(r.x1, n, d),
                  tech::scale_coord(r.y1, n, d)};
}

}  // namespace

std::optional<Dialect> via_message_dialect(std::string_view code) noexcept {
  if (code == "LEF-VIA-001" || code == "LEF-VIA-003" || code == "LEF-VIA-004") {
    return Dialect::lef;
  }
  if (code == "LEF-VIA-002" || code == "LEF-VIA-005" || code == "LEF-VIA-006") {
    return Dialect::def;
  }
  return std::nullopt;
}

void scale_via_areas(ViaTable& vias, std::int32_t n, std::int32_t d, bool magic_compat) {
  if (n <= 0 || d <= 0) {
    return;
  }
  for (const std::string& name : vias.keys()) {
    auto* entry = vias.find(name);
    if (entry == nullptr || !entry->value || entry->value->ref_count == 0) {
      continue;
    }
    ViaDefinition& via = *entry->value;
    via.area = scale_rect_floor(via.area, n, d);
    if (magic_compat) {
      continue;  // Magic leaves the lr list unscaled
    }
    for (ViaShape& shape : via.shapes) {
      shape.area = scale_rect_floor(shape.area, n, d);
    }
  }
}

}  // namespace hefesto::lef
