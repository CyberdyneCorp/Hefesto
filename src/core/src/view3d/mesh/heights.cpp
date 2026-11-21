// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The h and t each layer is drawn with in one build
// (hefesto-3d-visualization "Mesh generation from layout"; openspec/
// changes/m10-3d-view/design.md D10 "Contacts and vias", J1 decided (a)
// on 2026-11-13). By default every layer takes the stack's draw_height:
// contacts and vias keep their technology h and t, as in Magic. With the
// view option "Span connected layers" a contact type is drawn as a solid
// from the top of its lowest residue (h + t) to the bottom of its highest
// (h), the residues coming from tech-contacts-and-stacking (the full
// residue set of a stacked type) and their h and t from the stack. A
// contact whose residues give no gap keeps its own values.
//
// Owner: MESH (design D3).

#include "internal.hpp"

namespace hefesto::view3d::detail {

namespace {

[[nodiscard]] DrawHeight height_of_type(const LayerStack& stack, tech::TileType type) {
  db::TileView view;
  view.type = type;
  return stack.tile_height(view);
}

}  // namespace

std::optional<DrawHeight> spanned_height(const LayerStack& stack, const tech::Technology& tech,
                                         tech::TileType contact) {
  if (!tech.is_contact(contact)) {
    return std::nullopt;
  }
  const tech::TypeMask residues = tech.full_residues(contact);
  std::optional<DrawHeight> lowest;
  std::optional<DrawHeight> highest;
  for (std::size_t type = 1; type < tech::kMaxTileTypes; ++type) {
    if (!residues.test(static_cast<tech::TileType>(type))) {
      continue;
    }
    const DrawHeight z = height_of_type(stack, static_cast<tech::TileType>(type));
    if (!lowest || z.height < lowest->height) {
      lowest = z;
    }
    if (!highest || z.height > highest->height) {
      highest = z;
    }
  }
  if (!lowest) {
    return std::nullopt;
  }
  const float bottom = lowest->height + lowest->thickness;
  if (highest->height <= bottom) {
    return std::nullopt;
  }
  return DrawHeight{bottom, highest->height - bottom};
}

HeightTable::HeightTable(const LayerStack& stack, const BuildContext& ctx) : stack_(&stack) {
  const bool span = ctx.span_connected_layers && ctx.technology != nullptr && stack.mode() == Mode::layout_layer;
  if (span) {
    spanned_.resize(tech::kMaxTileTypes);
    for (const tech::ContactInfo& info : ctx.technology->contacts()) {
      if (info.type < tech::kMaxTileTypes) {
        spanned_[info.type] = spanned_height(stack, *ctx.technology, info.type);
      }
    }
  }
  layers_.reserve(stack.size());
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const std::optional<tech::TileType>& type = stack.layers()[i].type;
    if (type && *type < spanned_.size() && spanned_[*type]) {
      layers_.push_back(*spanned_[*type]);
    } else {
      layers_.push_back(stack.draw_height(static_cast<LayerId>(i)));
    }
  }
}

DrawHeight HeightTable::layer(LayerId layer) const {
  return layer < layers_.size() ? layers_[layer] : DrawHeight{};
}

DrawHeight HeightTable::type(tech::TileType type) const {
  if (type < spanned_.size() && spanned_[type]) {
    return *spanned_[type];
  }
  return height_of_type(*stack_, type);
}

}  // namespace hefesto::view3d::detail
