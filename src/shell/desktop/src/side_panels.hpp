// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The side panels of the Desktop-Layout screen
// (docs/design/mockups/Desktop-Layout.dc.html): the left panel (Layers
// palette with filter, Hierarchy tree of the loaded cell) and the right
// Inspector. Their content comes from the view models (view_models.hpp),
// their colors from the forge tokens, layer swatches from the PDK display
// styles. Raven-free: built from the raven_ui wrappers.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "hefesto/shell/editing_models.hpp"
#include "hefesto/shell/view_models.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::view {
class LayoutWindow;
}

namespace hefesto::shell {

/// A 1 px line in the border token (the mockup's panel borders).
std::unique_ptr<ui::Painted> make_separator(ScreenContext& ctx, bool vertical);

/// A painted widget that draws one call of `paint` with the current tokens.
std::unique_ptr<ui::Painted> make_painted(ScreenContext& ctx,
                                          std::function<void(ui::Painter&, float, float, const ThemeTokens&)> paint);

/// The interaction state of a painted widget, for the paint functions.
using StatePaint = std::function<void(ui::Painter&, float, float, const ThemeTokens&, bool hovered, bool focused)>;
/// Like make_painted, with the widget's hover and focus state.
std::unique_ptr<ui::Painted> make_interactive(ScreenContext& ctx, StatePaint paint);

class LeftPanel {
 public:
  explicit LeftPanel(ScreenContext& ctx);
  [[nodiscard]] ui::Widget& widget() noexcept { return root_; }

  /// Rebuilds the layer rows and the hierarchy from the session.
  void refresh(const view::LayoutWindow* window);
  void repaint();

 private:
  void rebuild_layers();
  void rebuild_hierarchy(const view::LayoutWindow* window);
  void toggle_expansion(std::size_t index);
  void layer_click(std::size_t index, float x);
  void build_paint_footer();

  ScreenContext& ctx_;
  ui::Box root_ = ui::Box::horizontal();
  ui::Panel background_;  // surface1
  ui::Box column_ = ui::Box::vertical();
  std::unique_ptr<ui::Painted> layers_title_;
  std::unique_ptr<ui::TextInput> filter_;
  ui::ScrollBox layer_list_ = ui::ScrollBox::create();
  ui::Box layer_rows_box_ = ui::Box::vertical();
  // "Paint layer ■ name  [Paint on box] [Erase on box]" (M2-Layer-Palette board).
  ui::Box paint_footer_ = ui::Box::vertical();
  std::unique_ptr<ui::Painted> paint_label_;
  std::unique_ptr<ui::Painted> paint_button_;
  std::unique_ptr<ui::Painted> erase_button_;
  std::unique_ptr<ui::Painted> divider_;
  std::unique_ptr<ui::Painted> hierarchy_title_;
  ui::ScrollBox tree_list_ = ui::ScrollBox::create();
  ui::Box tree_rows_box_ = ui::Box::vertical();
  std::unique_ptr<ui::Painted> edge_;

  std::string filter_text_;
  const view::LayoutWindow* window_ = nullptr;
  std::vector<LayerRow> layers_;
  std::vector<std::unique_ptr<ui::Painted>> layer_widgets_;
  std::vector<HierarchyRow> tree_;
  std::vector<std::unique_ptr<ui::Painted>> tree_widgets_;
};

class InspectorPanel {
 public:
  explicit InspectorPanel(ScreenContext& ctx);
  [[nodiscard]] ui::Widget& widget() noexcept { return root_; }
  /// The selection in `window_root` when there is one, else the box or the cell.
  void refresh(const UnitScale& scale, const db::CellDef* window_root = nullptr);
  void repaint();

 private:
  void rebuild_actions(std::vector<InspectorAction> actions);

  ScreenContext& ctx_;
  ui::Box root_ = ui::Box::horizontal();
  ui::Box column_ = ui::Box::vertical();
  std::unique_ptr<ui::Painted> edge_;
  std::unique_ptr<ui::Painted> body_;
  ui::Box actions_box_ = ui::Box::vertical();
  std::vector<InspectorAction> actions_;
  std::vector<std::unique_ptr<ui::Painted>> action_widgets_;
  InspectorView view_;
};

}  // namespace hefesto::shell
