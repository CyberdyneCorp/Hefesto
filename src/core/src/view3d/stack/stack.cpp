// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LayerStack (openspec/changes/m10-3d-view/design.md D5): the layout-layer
// stack from the extract heights and the display type-style bindings
// (Q-MISC-77), the mask-layer stack from the output style's `render`
// statements (view-3d "Render statement in the cifoutput section"), and
// the Q-MISC-72 draw heights.
//
// Owner: TECH (design D3).

#include "hefesto/view3d/stack/layer_stack.hpp"

#include <algorithm>
#include <set>
#include <string_view>
#include <utility>

namespace hefesto::view3d {

namespace {

constexpr std::string_view kNoRenderHeight = "VIEW3D-STACK-001";
constexpr std::string_view kBadRenderStyle = "VIEW3D-STACK-002";

ValueSource source_of(const std::optional<extract::TypeHeight>& h) {
  if (!h) {
    return ValueSource::none;
  }
  return h->source == extract::HeightSource::height_line ? ValueSource::extract : ValueSource::extract_default;
}

// The (style, type) pairs of `styles` in drawing order, without repeats.
std::vector<std::pair<std::size_t, tech::TileType>> style_type_pairs(const display::TypeStyleMap& styles) {
  std::vector<std::pair<std::size_t, tech::TileType>> pairs;
  const std::size_t types = std::min(styles.styles_of_type.size(), tech::kMaxTileTypes);
  for (std::size_t type = tech::kFirstUserType; type < types; ++type) {
    for (const std::size_t style : styles.styles_of_type[type]) {
      pairs.emplace_back(style, static_cast<tech::TileType>(type));
    }
  }
  std::sort(pairs.begin(), pairs.end());
  pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
  return pairs;
}

// The `render` statements of one mask layer, resolved: the last one whose
// style resolves applies.
class RenderResolver {
 public:
  RenderResolver(const display::StyleTable* styles, diag::DiagnosticSink& sink) : styles_(styles), sink_(sink) {}

  void apply(const mask::OutputLayer& layer, StackLayer& out) {
    for (const mask::RenderAttrs& attrs : layer.render) {
      const std::optional<std::size_t> style = resolve(attrs.style);
      if (styles_ != nullptr && !style) {
        continue;
      }
      out.style = style.value_or(out.style);
      out.height = attrs.height;
      out.thickness = attrs.thickness;
      out.source = ValueSource::render;
    }
  }

 private:
  std::optional<std::size_t> resolve(const std::string& ref) {
    if (styles_ == nullptr) {
      return std::nullopt;
    }
    std::optional<std::size_t> style = styles_->find_layout_style(ref);
    if (!style && reported_.insert(ref).second) {
      sink_.emit(diag::make(kBadRenderStyle, "Error:  Bad render style for CIF layer."));
    }
    return style;
  }

  const display::StyleTable* styles_;
  diag::DiagnosticSink& sink_;
  std::set<std::string> reported_;
};

}  // namespace

DrawHeight LayerStack::draw_height(LayerId layer) const {
  if (layer >= layers_.size() || layers_[layer].source == ValueSource::none) {
    return DrawHeight{};
  }
  const StackLayer& l = layers_[layer];
  const bool convert = mode_ == Mode::mask_layer && !magic_compat_ && microns_per_unit_ > 0.0;
  if (!convert) {
    return DrawHeight{l.height, l.thickness};
  }
  return DrawHeight{static_cast<float>(static_cast<double>(l.height) / microns_per_unit_),
                    static_cast<float>(static_cast<double>(l.thickness) / microns_per_unit_)};
}

std::optional<PhysicalHeight> LayerStack::physical_height_um(LayerId layer) const {
  if (layer >= layers_.size() || layers_[layer].source == ValueSource::none) {
    return std::nullopt;
  }
  const StackLayer& l = layers_[layer];
  const double scale = mode_ == Mode::mask_layer ? 1.0 : microns_per_unit_;
  return PhysicalHeight{static_cast<double>(l.height) * scale, static_cast<double>(l.thickness) * scale};
}

DrawHeight LayerStack::tile_height(const db::TileView& tile) const {
  const tech::TileType type = tile.split ? tile.left_type : tile.type;
  if (mode_ != Mode::layout_layer || type >= type_heights_.size()) {
    return DrawHeight{};
  }
  return type_heights_[type];
}

std::optional<LayerId> LayerStack::find(tech::TileType type, std::size_t style) const {
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    if (layers_[i].type == type && layers_[i].style == style) {
      return static_cast<LayerId>(i);
    }
  }
  return std::nullopt;
}

LayerStack build_layout_stack(const tech::Technology& tech, const extract::HeightView& heights,
                              const display::TypeStyleMap& styles, bool magic_compat, double microns_per_unit) {
  std::vector<DrawHeight> type_heights(tech::kMaxTileTypes);
  for (std::size_t type = 0; type < type_heights.size(); ++type) {
    const std::optional<extract::TypeHeight> h = heights.height(static_cast<tech::TileType>(type));
    type_heights[type] = h ? DrawHeight{h->height, h->thickness} : DrawHeight{};
  }
  std::vector<StackLayer> layers;
  for (const auto& [style, type] : style_type_pairs(styles)) {
    StackLayer layer;
    layer.name = std::string{tech.type_name(type)};
    layer.type = type;
    layer.style = style;
    const std::optional<extract::TypeHeight> h = heights.height(type);
    layer.source = source_of(h);
    if (h) {
      layer.height = h->height;
      layer.thickness = h->thickness;
    }
    layers.push_back(std::move(layer));
  }
  return LayerStack{std::move(layers), std::move(type_heights), magic_compat, microns_per_unit};
}

LayerStack build_mask_stack(const mask::OutputStyle& style, const display::StyleTable* styles, bool magic_compat,
                            diag::DiagnosticSink& sink) {
  RenderResolver resolver{styles, sink};
  std::vector<StackLayer> layers;
  layers.reserve(style.layers.size());
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    StackLayer layer;
    layer.name = style.layers[i].name;
    layer.mask_layer = i;
    layer.style = kPaleHighlightStyle;
    resolver.apply(style.layers[i], layer);
    layers.push_back(std::move(layer));
  }
  // µm per internal unit: cs_scaleFactor / (100 * cs_expander) (Q-MISC-72).
  const double expander = style.scale.expander > 0 ? style.scale.expander : 1;
  const double microns = static_cast<double>(style.scale.scale_factor) / (100.0 * expander);
  return LayerStack{Mode::mask_layer, std::move(layers), magic_compat, microns};
}

std::optional<diag::Diagnostic> missing_height_warning(const LayerStack& stack, const mask::OutputStyle& style) {
  if (stack.mode() != Mode::mask_layer) {
    return std::nullopt;
  }
  std::vector<std::string> names;
  for (const StackLayer& layer : stack.layers()) {
    const bool real = layer.mask_layer && *layer.mask_layer < style.layers.size() &&
                      !style.layers[*layer.mask_layer].temporary;
    if (real && layer.source == ValueSource::none &&
        std::find(names.begin(), names.end(), layer.name) == names.end()) {
      names.push_back(layer.name);
    }
  }
  if (names.empty()) {
    return std::nullopt;
  }
  std::string text = "No render height for mask layer(s):";
  for (const std::string& name : names) {
    text += " " + name;
  }
  text += "; they are left out (the output style has no `render` statement for them; the layout-layer view takes "
          "heights from the extract style)";
  return diag::make_native(kNoRenderHeight, std::move(text));
}

}  // namespace hefesto::view3d
