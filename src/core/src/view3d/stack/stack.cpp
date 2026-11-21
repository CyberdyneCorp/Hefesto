// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LayerStack (openspec/changes/m10-3d-view/design.md D5). Scaffold stubs
// (M10 task 2.3): the builders return an empty stack of the right mode, and
// every layer draws flat with no physical height. TECH replaces them in
// tasks 3.3 and 3.4.
//
// Owner: TECH (design D3).

#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

DrawHeight LayerStack::draw_height(LayerId /*layer*/) const {
  return DrawHeight{};
}

std::optional<PhysicalHeight> LayerStack::physical_height_um(LayerId /*layer*/) const {
  return std::nullopt;
}

LayerStack build_layout_stack(const tech::Technology& /*tech*/, const extract::HeightView& /*heights*/,
                              const display::TypeStyleMap& /*styles*/, bool magic_compat, double microns_per_unit) {
  return LayerStack{Mode::layout_layer, {}, magic_compat, microns_per_unit};
}

LayerStack build_mask_stack(const mask::OutputStyle& /*style*/, bool magic_compat) {
  return LayerStack{Mode::mask_layer, {}, magic_compat, 0.0};
}

}  // namespace hefesto::view3d
