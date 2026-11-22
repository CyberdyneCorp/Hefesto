// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The scene of the 3D window (view3d/client/scene.hpp): the layer stacks of
// the session and the layout-layer mesh of the rendered cell (design D1,
// D6, D9).
//
// Owner: WIND3D (openspec/changes/m10-3d-view/design.md D3).

#include "hefesto/view3d/client/scene.hpp"

#include <memory>
#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/tech/height_view.hpp"
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
  if (window.mode != Mode::layout_layer) {
    return hidden;  // P1: the mask visibility set applies to the mask layers of P2
  }
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const std::optional<tech::TileType> type = stack.layers()[i].type;
    hidden[i] = type && window.visibility.layout_hidden.test(*type);
  }
  return hidden;
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

Scene build_scene(const editor::EditorState& editor, const WindowState& window, bool magic_compat,
                  BuildContext& ctx) {
  Scene scene;
  scene.stack = layout_stack(editor, magic_compat);
  scene.hidden = layout_hidden(scene.stack, window);
  scene.xy_factor = window.mode == Mode::mask_layer ? window.applied_factor : 1.0F;
  const db::CellDef* def = editor.cells.find(window.cell);
  if (def == nullptr) {
    return scene;
  }
  scene.clipped = window.cut_box.has_value();
  const db::Rect& box = def->bbox();
  scene.clip = window.cut_box ? *window.cut_box : db::Rect{box.x0 - 1, box.y0 - 1, box.x1 + 1, box.y1 + 1};
  const ClipArea clip{scene.clip, scene.clipped, 0};
  if (std::optional<MeshModel> mesh = build_from_layout(scene.stack, *def, clip, ctx)) {
    scene.mesh = std::make_shared<const MeshModel>(std::move(*mesh));
  }
  return scene;
}

}  // namespace hefesto::view3d
