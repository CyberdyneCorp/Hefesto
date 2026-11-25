// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The scene of the 3D window (view3d/client/scene.hpp): the layer stacks of
// the session and the mesh of the rendered cell, from the layout layers or
// from the generated mask layers (design D1, D6, D9, D15).
//
// Owner: WIND3D (openspec/changes/m10-3d-view/design.md D3); the
// mask-layer source: MASKSRC (round 5).

#include "hefesto/view3d/client/scene.hpp"

#include <memory>
#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/mask/style/style.hpp"
#include "view3d/client/internal.hpp"

namespace hefesto::view3d {

namespace {

// µm per internal unit of the current output style (rescaled to the
// internal grid): cs_scaleFactor / (100 * cs_expander); 0 without one.
double microns_per_unit(const editor::EditorState& editor) {
  const mask::OutputStyle* style = detail::current_style(editor);
  if (style == nullptr || style->scale.expander <= 0) {
    return 0.0;
  }
  return static_cast<double>(style->scale.scale_factor) / (100.0 * style->scale.expander);
}

std::vector<bool> layout_hidden(const LayerStack& stack, const WindowState& window) {
  std::vector<bool> hidden(stack.size(), false);
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const std::optional<tech::TileType> type = stack.layers()[i].type;
    hidden[i] = type && window.visibility.layout_hidden.test(*type);
  }
  return hidden;
}

// Q-MISC-71's mask set, and with magic-compat OFF the temporary layers.
std::vector<bool> mask_hidden(const editor::EditorState& editor, const LayerStack& stack, const WindowState& window,
                              bool magic_compat) {
  const std::vector<bool>& set = window.visibility.mask_hidden;
  std::vector<bool> hidden = excluded_layers(editor, stack, magic_compat);
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const std::optional<std::size_t> index = stack.layers()[i].mask_layer;
    if (index && *index < set.size() && set[*index]) {
      hidden[i] = true;
    }
  }
  return hidden;
}

db::Point centre_of(const db::Rect& r) {
  return db::Point{static_cast<db::Coord>((std::int64_t{r.x0} + r.x1) / 2),
                   static_cast<db::Coord>((std::int64_t{r.y0} + r.y1) / 2)};
}

// Layout-layer mode: the cut box, or the bounding box grown by 1 (Q-MISC-70).
void layout_inputs(const db::CellDef& def, const WindowState& window, BuildInputs& in) {
  const db::Rect& box = def.bbox();
  const db::Rect area = window.cut_box ? *window.cut_box : db::Rect{box.x0 - 1, box.y0 - 1, box.x1 + 1, box.y1 + 1};
  in.clip = ClipArea{area, window.cut_box.has_value(), 0};
}

// Mask-layer mode: generate mask_generation_area, clip to it in mask units.
void mask_inputs(const editor::EditorState& editor, const WindowState& window, bool magic_compat, BuildInputs& in) {
  MaskSource source;
  source.style = editor.mask.output_styles.current_shared();
  source.generation = mask::generation_source(editor);
  source.generation.compat.magic_compat = magic_compat;
  source.generation.find_interaction = {};  // flat generation never asks; the worker must not reach the editor
  source.area = mask_generation_area(editor, window, magic_compat);
  in.clip = mask_clip(source, window.cut_box.has_value());
  in.mask = std::move(source);
}

}  // namespace

LayerStack layout_stack(const editor::EditorState& editor, bool magic_compat) {
  diag::NullSink quiet;
  const extract::HeightView heights =
      extract::HeightView::from_technology(*editor.technology, quiet)
          .rescaled(static_cast<std::int32_t>(editor.grid.lambda), static_cast<std::int32_t>(editor.grid.internal));
  const display::TypeStyleMap none;
  const display::TypeStyleMap& styles = editor.styles ? editor.styles->types : none;
  return build_layout_stack(*editor.technology, heights, styles, magic_compat, microns_per_unit(editor));
}

LayerStack mask_stack(const editor::EditorState& editor, bool magic_compat, diag::DiagnosticSink& sink) {
  const mask::OutputStyle* style = detail::current_style(editor);
  if (style == nullptr) {
    return LayerStack{Mode::mask_layer, {}, magic_compat, 0.0};
  }
  const display::StyleTable* table = editor.styles ? &editor.styles->table : nullptr;
  const LayerStack built = build_mask_stack(*style, table, magic_compat, sink);
  const Session& session = editor.view3d;
  if (session.render_overrides.empty() || !(session.render_key == detail::current_key(editor))) {
    return built;
  }
  std::vector<StackLayer> layers(built.layers().begin(), built.layers().end());
  for (const auto& [index, value] : session.render_overrides) {
    if (index >= layers.size()) {
      continue;
    }
    layers[index].height = value.height;
    layers[index].thickness = value.thickness;
    layers[index].source = ValueSource::render;
    if (value.style_position) {
      layers[index].style = static_cast<std::size_t>(*value.style_position) + display::kFixedStyleCount;
    }
  }
  return LayerStack{Mode::mask_layer, std::move(layers), magic_compat, built.microns_per_unit()};
}

db::Rect mask_generation_area(const editor::EditorState& editor, const WindowState& window, bool magic_compat) {
  const db::CellDef* def = editor.cells.find(window.cell);
  const mask::OutputStyle* style = detail::current_style(editor);
  if (def == nullptr || style == nullptr) {
    return db::Rect{};
  }
  if (window.cut_box) {
    return *window.cut_box;
  }
  if (magic_compat) {
    return def->bbox();
  }
  compat::CompatSettings settings = editor.drc.compat_settings();
  settings.magic_compat = false;
  return db::expand(def->bbox(), static_cast<db::Coord>(mask::generation_radius(*style, settings)));
}

std::vector<bool> excluded_layers(const editor::EditorState& editor, const LayerStack& stack, bool magic_compat) {
  std::vector<bool> excluded(stack.size(), false);
  const mask::OutputStyle* style = detail::current_style(editor);
  if (magic_compat || stack.mode() != Mode::mask_layer || style == nullptr) {
    return excluded;
  }
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const std::optional<std::size_t> index = stack.layers()[i].mask_layer;
    excluded[i] = index && *index < style->layers.size() && style->layers[*index].temporary;
  }
  return excluded;
}

std::vector<bool> hidden_layers(const editor::EditorState& editor, const LayerStack& stack, const WindowState& window,
                                bool magic_compat) {
  return stack.mode() == Mode::mask_layer ? mask_hidden(editor, stack, window, magic_compat)
                                          : layout_hidden(stack, window);
}

WindowBuild window_build(const editor::EditorState& editor, const WindowState& window, bool magic_compat,
                         diag::DiagnosticSink& sink) {
  WindowBuild out;
  const bool masks = window.mode == Mode::mask_layer && detail::current_style(editor) != nullptr;
  out.stack = std::make_shared<const LayerStack>(masks ? mask_stack(editor, magic_compat, sink)
                                                       : layout_stack(editor, magic_compat));
  out.hidden = hidden_layers(editor, *out.stack, window, magic_compat);
  BuildInputs& in = out.inputs;
  in.stack = out.stack;
  in.technology = editor.technology.get();
  in.cell = editor.cells.find(window.cell);
  if (in.cell == nullptr) {
    return out;
  }
  if (masks) {
    mask_inputs(editor, window, magic_compat, in);
  } else {
    layout_inputs(*in.cell, window, in);
  }
  in.centre = centre_of(in.clip.area);
  return out;
}

Scene build_scene(const editor::EditorState& editor, const WindowState& window, bool magic_compat,
                  BuildContext& ctx) {
  diag::NullSink quiet;
  const WindowBuild build = window_build(editor, window, magic_compat, quiet);
  Scene scene;
  scene.stack = *build.stack;
  scene.hidden = build.hidden;
  if (build.inputs.cell == nullptr) {
    return scene;
  }
  scene.clip = build.inputs.clip.area;
  scene.clipped = build.inputs.clip.cut_box;
  if (std::optional<MeshModel> mesh = build_once(build.inputs, ctx)) {
    scene.mesh = std::make_shared<const MeshModel>(std::move(*mesh));
  }
  return scene;
}

}  // namespace hefesto::view3d
