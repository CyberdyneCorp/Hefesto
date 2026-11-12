// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "side_panels.hpp"

#include "hefesto/shell/toolbar_views.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/display/scene.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/view/window.hpp"
#include "screen_paint.hpp"

namespace hefesto::shell {

namespace {

/// M4 layer toolbar order (toolbar_views.hpp): the rows in `order` first.
void apply_toolbar_order(std::vector<LayerRow>& rows, const std::vector<std::string>& order) {
  if (order.empty()) return;
  gui::ToolbarModel toolbar;
  toolbar.layers = order;
  std::vector<std::string> names;
  names.reserve(rows.size());
  for (const LayerRow& row : rows) names.push_back(row.name);
  std::vector<LayerRow> sorted;
  sorted.reserve(rows.size());
  for (const std::size_t i : toolbar_order(names, toolbar)) sorted.push_back(std::move(rows[i]));
  rows = std::move(sorted);
}

constexpr float kPanelPadX = 14.0F;  // the mockup's 14 px panel padding
constexpr float kTitleHeight = 28.0F;  // 12 px top padding + 11 px label + gap
constexpr float kTreeRowHeight = 20.0F;  // 12 px JetBrains Mono + 4 px gap (mockup)
constexpr float kTreeHeight = 200.0F;

std::unique_ptr<ui::Painted> section_title(ScreenContext& ctx, std::string_view text) {
  auto title = make_painted(ctx, [text](ui::Painter& p, float, float h, const ThemeTokens& t) {
    paint::section_label(p, kPanelPadX, h - 4.0F - p.line_height(TextRole::section_label), text, t);
  });
  title->set_size(-1.0F, kTitleHeight);
  title->set_role(ui::Role::heading);
  title->set_accessible_name(text);
  return title;
}

}  // namespace

std::unique_ptr<ui::Painted> make_painted(ScreenContext& ctx,
                                          std::function<void(ui::Painter&, float, float, const ThemeTokens&)> paint) {
  return ui::Painted::create(ctx.runtime, [&ctx, paint = std::move(paint)](ui::Painter& p, float w, float h) {
    paint(p, w, h, ctx.tokens());
  });
}

std::unique_ptr<ui::Painted> make_interactive(ScreenContext& ctx, StatePaint paint) {
  return ui::Painted::create_stateful(
      ctx.runtime, [&ctx, paint = std::move(paint)](ui::Painter& p, float w, float h, const ui::Painted& self) {
        paint(p, w, h, ctx.tokens(), self.hovered(), self.focused());
      });
}

std::unique_ptr<ui::Painted> make_separator(ScreenContext& ctx, bool vertical) {
  auto line = make_painted(ctx, [](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint::fill_rect(p, w, h, t.border);
  });
  line->set_size(vertical ? 1.0F : -1.0F, vertical ? -1.0F : 1.0F);
  line->set_hit_testable(false);
  return line;
}

// --- left panel ----------------------------------------------------------------

LeftPanel::LeftPanel(ScreenContext& ctx)
    : ctx_(ctx), background_(ui::Panel::create(ctx.tokens().surface1)), filter_(ui::TextInput::create("Filter layers")) {
  layers_title_ = section_title(ctx_, "Layers");
  hierarchy_title_ = section_title(ctx_, "Hierarchy");
  divider_ = make_separator(ctx_, false);
  edge_ = make_separator(ctx_, true);

  filter_->set_accessible_name("Filter layers");
  filter_->set_size(-1.0F, static_cast<float>(kInputHeight));
  filter_->set_font(ctx_.runtime.font(TextRole::small));
  filter_->set_font_size(text_spec(TextRole::small).size_px);
  filter_->on_change([this](std::string text) {
    filter_text_ = std::move(text);
    rebuild_layers();
  });
  ui::Box filter_row = ui::Box::vertical();
  filter_row.set_padding(kPanelPadX, 0.0F, kPanelPadX, 8.0F);
  filter_row.add(*filter_);

  layer_rows_box_.set_spacing(2.0F);
  layer_rows_box_.set_padding(6.0F, 0.0F, 6.0F, 8.0F);
  layer_list_.add(layer_rows_box_);
  layer_list_.set_accessible_name("Layers");
  tree_rows_box_.set_spacing(4.0F);
  tree_rows_box_.set_padding(kPanelPadX, 0.0F, kPanelPadX, 12.0F);
  tree_list_.add(tree_rows_box_);
  tree_list_.set_accessible_name("Cell hierarchy");
  tree_list_.set_size(-1.0F, kTreeHeight);

  build_paint_footer();
  column_.add(*layers_title_);
  column_.add(filter_row);
  column_.add(layer_list_, 1.0F);
  column_.add(paint_footer_);
  column_.add(*divider_);
  column_.add(*hierarchy_title_);
  column_.add(tree_list_);
  background_.set_child(column_);
  root_.add(background_, 1.0F);
  root_.add(*edge_);
  root_.set_size(static_cast<float>(kLeftPanelWidth), -1.0F);
  root_.set_accessible_name("Layers and hierarchy");
}

void LeftPanel::refresh(const view::LayoutWindow* window) {
  window_ = window;
  rebuild_layers();
  rebuild_hierarchy(window);
  for (ui::Painted* w : {paint_label_.get(), paint_button_.get(), erase_button_.get()}) w->invalidate();
}

void LeftPanel::repaint() {
  background_.set_fill(ctx_.tokens().surface1);
  for (ui::Painted* w : {layers_title_.get(), hierarchy_title_.get(), divider_.get(), edge_.get(), paint_label_.get(),
                        paint_button_.get(), erase_button_.get()}) {
    w->invalidate();
  }
  for (auto& w : layer_widgets_) w->invalidate();
  for (auto& w : tree_widgets_) w->invalidate();
}

void LeftPanel::set_hierarchy_visible(bool visible) {
  divider_->set_visible(visible);
  hierarchy_title_->set_visible(visible);
  tree_list_.set_visible(visible);
}

void LeftPanel::rebuild_layers() {
  // Raven deletes the row widgets first; their callbacks go after.
  layer_rows_box_.clear();
  layer_widgets_.clear();
  layers_.clear();
  const editor::EditorState& ed = ctx_.editor();
  if (ed.technology && ed.styles) {
    const tech::TypeMask visible = window_ != nullptr ? window_->visible_types() : tech::TypeMask::all();
    layers_ = layer_rows(display::layer_palette(*ed.technology, *ed.styles), visible, filter_text_, ed.locked_types,
                         ctx_.paint_layer);
    apply_toolbar_order(layers_, toolbar_order_);
  }
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    auto row = make_interactive(ctx_, [this, i](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                                bool foc) {
      paint::layer_row(p, w, h, layers_[i], paint::WidgetState{hov, foc, true, false}, t);
    });
    row->set_size(-1.0F, static_cast<float>(kLayerRowHeight));
    row->set_role(ui::Role::toggle_button);
    row->set_accessible_name(layers_[i].accessible_name());
    row->set_tooltip("Click: paint layer · eye: " + layers_[i].toggle_command() + " · lock: " +
                     layers_[i].lock_command());
    row->on_click([this, i](float x, float) { layer_click(i, x); });
    row->on_context([this, i](float x, float y) {
      if (ctx_.menu && i < layers_.size()) ctx_.menu(layers_[i].name, layer_menu(layers_[i]), x, y);
    });
    layer_rows_box_.add(*row);
    layer_widgets_.push_back(std::move(row));
  }
  if (layers_.empty()) {
    const std::string tech_name = ed.technology ? ed.technology->name() : std::string();
    const std::string hint = layer_empty_hint(tech_name, ed.styles.has_value());
    auto empty = make_painted(ctx_, [hint](ui::Painter& p, float, float h, const ThemeTokens& t) {
      p.text_centered_y(TextRole::meta, 8.0F, h / 2.0F, hint, t.text_tertiary);
    });
    empty->set_size(-1.0F, static_cast<float>(kLayerRowHeight));
    layer_rows_box_.add(*empty);
    layer_widgets_.push_back(std::move(empty));
  }
}

void LeftPanel::layer_click(std::size_t index, float x) {
  if (index >= layers_.size()) return;
  const LayerRow& row = layers_[index];
  const float width = static_cast<float>(kLeftPanelWidth) - 12.0F;  // the rows box's 6 px padding on each side
  const float meta_width = ctx_.runtime.text_width(TextRole::meta, row.meta());
  // Enter / Space (x < 0) select the paint layer, like a click on the name.
  switch (x < 0.0F ? LayerRowHit::paint : layer_row_hit(x, width, meta_width)) {
    case LayerRowHit::visibility: ctx_.run(row.toggle_command()); return;
    case LayerRowHit::lock: ctx_.run(row.lock_command()); return;
    case LayerRowHit::paint:
      // The paint layer chooses what the Paint tool and "Paint" / "Erase" paint.
      ctx_.run(row.paint ? std::string("palette paint none") : "palette paint " + row.name);
      return;
  }
}

void LeftPanel::build_paint_footer() {
  paint_footer_.set_padding(kPanelPadX, 8.0F, kPanelPadX, 10.0F);
  paint_footer_.set_spacing(6.0F);
  paint_label_ = make_painted(ctx_, [this](ui::Painter& p, float, float h, const ThemeTokens& t) {
    const std::string name = ctx_.paint_layer.empty() ? std::string("none: click a layer") : ctx_.paint_layer;
    p.text_centered_y(TextRole::meta, 0.0F, h / 2.0F, "Paint layer", t.text_tertiary);
    p.text_centered_y(TextRole::mono, 76.0F, h / 2.0F, name,
                      ctx_.paint_layer.empty() ? t.text_tertiary : t.text_primary);
  });
  paint_label_->set_size(-1.0F, 20.0F);
  paint_label_->set_role(ui::Role::text);
  paint_label_->set_accessible_name("Paint layer");
  paint_footer_.add(*paint_label_);
  ui::Box buttons = ui::Box::horizontal();
  buttons.set_spacing(6.0F);
  for (const bool erase : {false, true}) {
    // The board's "Paint on box" / "Erase on box" do not fit the 240 px panel side by side.
    const std::string_view label = erase ? "Erase" : "Paint";
    auto button = make_interactive(ctx_, [this, erase, label](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                             bool hov, bool foc) {
      paint::button(p, w, h, label, erase ? "" : "paint",
                    erase ? paint::ButtonStyle::secondary : paint::ButtonStyle::primary,
                    paint::WidgetState{hov, foc, !ctx_.paint_layer.empty(), false}, t);
    });
    button->set_size(paint::button_width(ctx_.runtime, label, !erase), static_cast<float>(kButtonHeight));
    button->set_role(ui::Role::button);
    button->set_accessible_name(label);
    button->set_tooltip(erase ? "erase <paint layer> on the box" : "paint <paint layer> on the box");
    button->on_click([this, erase](float, float) {
      if (!ctx_.paint_layer.empty()) ctx_.run((erase ? "erase " : "paint ") + ctx_.paint_layer);
    });
    buttons.add(*button);
    (erase ? erase_button_ : paint_button_) = std::move(button);
  }
  paint_footer_.add(buttons);
}

void LeftPanel::rebuild_hierarchy(const view::LayoutWindow* window) {
  tree_rows_box_.clear();
  tree_widgets_.clear();
  tree_.clear();
  const db::CellDef* root = window != nullptr ? window->root() : nullptr;
  if (root != nullptr) {
    tree_ = hierarchy_rows(*root, [window](const db::CellUse& use) { return window->is_expanded(use); });
  } else {
    tree_.push_back(HierarchyRow{0, "No cell loaded", false, false, nullptr, {}, {}, false});
  }
  for (std::size_t i = 0; i < tree_.size(); ++i) {
    auto row = make_interactive(ctx_, [this, i](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                                bool foc) {
      paint::hierarchy_row(p, w, h, tree_[i], paint::WidgetState{hov, foc, true, false}, t);
    });
    row->set_size(-1.0F, kTreeRowHeight);
    row->set_role(ui::Role::tree_item);
    row->set_accessible_name(tree_[i].accessible_name());
    if (tree_[i].expandable) {
      // The full text: the painted name may be ellipsized.
      row->set_tooltip(tree_[i].text + (tree_[i].expanded ? ": unexpand" : ": expand one level"));
      row->on_click([this, i](float, float) { toggle_expansion(i); });
    }
    tree_rows_box_.add(*row);
    tree_widgets_.push_back(std::move(row));
  }
}

void LeftPanel::toggle_expansion(std::size_t index) {
  if (index >= tree_.size()) return;
  // Through the Dispatcher like a typed command: `expand instance` /
  // `unexpand instance` on the row's instance paths (the box is untouched).
  const std::string command = tree_[index].toggle_command();
  if (!command.empty()) ctx_.run(command);
}

// --- inspector ---------------------------------------------------------------------

InspectorPanel::InspectorPanel(ScreenContext& ctx) : ctx_(ctx) {
  edge_ = make_separator(ctx_, true);
  body_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint::fill_rect(p, w, h, t.surface1);
    paint::inspector(p, w, h, view_, t);
  });
  body_->set_role(ui::Role::pane);
  body_->set_accessible_name("Inspector");
  actions_box_.set_padding(kPanelPadX, 0.0F, kPanelPadX, 12.0F);
  actions_box_.set_spacing(6.0F);
  column_.add(*body_, 1.0F);
  column_.add(actions_box_);
  root_.add(*edge_);
  root_.add(column_, 1.0F);
  root_.set_size(static_cast<float>(kInspectorWidth), -1.0F);
}

void InspectorPanel::rebuild_actions(std::vector<InspectorAction> actions) {
  const bool same = actions.size() == actions_.size() &&
                    std::equal(actions.begin(), actions.end(), actions_.begin(), [](const auto& a, const auto& b) {
                      return a.label == b.label && a.command == b.command;
                    });
  if (same) return;
  actions_box_.clear();
  action_widgets_.clear();
  actions_ = std::move(actions);
  ui::Box row = ui::Box::horizontal();
  float used = 0.0F;
  constexpr float kRowWidth = static_cast<float>(kInspectorWidth) - 2.0F * kPanelPadX;
  for (std::size_t i = 0; i < actions_.size(); ++i) {
    const std::string label = actions_[i].label;
    const float width = paint::button_width(ctx_.runtime, label, false);
    if (used > 0.0F && used + width > kRowWidth) {
      actions_box_.add(row);
      row = ui::Box::horizontal();
      used = 0.0F;
    }
    auto button = make_interactive(ctx_, [label](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                                 bool foc) {
      paint::button(p, w, h, label, "", paint::ButtonStyle::secondary, paint::WidgetState{hov, foc, true, false}, t);
    });
    button->set_size(width, static_cast<float>(kButtonHeight));
    button->set_role(ui::Role::button);
    button->set_accessible_name(label);
    button->set_tooltip(actions_[i].command);
    ui::Painted* placed = button.get();
    button->on_click([this, i, placed](float, float) {
      if (actions_[i].menu) {
        open_action_menu(i, *placed);
        return;
      }
      // A command that needs an argument goes to the command bar.
      const std::string& command = actions_[i].command;
      if (!command.empty() && command.back() == ' ') {
        if (ctx_.prompt) ctx_.prompt(command);
      } else {
        ctx_.run(command);
      }
    });
    row.add(*button);
    used += width + 6.0F;
    row.set_spacing(6.0F);
    action_widgets_.push_back(std::move(button));
  }
  if (!actions_.empty()) actions_box_.add(row);
}

void InspectorPanel::open_action_menu(std::size_t index, const ui::Widget& button) {
  if (!ctx_.menu || actions_[index].command != "orient") return;
  std::string current;
  for (const InspectorRow& row : view_.rows) {
    if (row.key == "Orientation") current = row.value;
  }
  const ui::Rect r = button.rect();
  ctx_.menu("Orientation", orient_menu(current), r.x, r.y + r.height + 2.0F);
}

void InspectorPanel::refresh(const UnitScale& scale, const db::CellDef* window_root) {
  const editor::EditorState& ed = ctx_.editor();
  std::vector<InspectorAction> actions;
  if (auto selection = selection_inspector_view(ed, window_root, scale, &actions)) {
    view_ = std::move(*selection);
  } else {
    view_ = inspector_view(visible_box(ed.box, ed.box_root, ed.edit_cell), ed.edit_cell, scale, window_root);
    actions.clear();
  }
  rebuild_actions(std::move(actions));
  body_->set_accessible_description(view_.title + ". " + view_.subtitle);
  body_->set_tooltip(view_.title + " — " + view_.subtitle);  // the painted title and subtitle may be ellipsized
  body_->invalidate();
}

bool InspectorPanel::focus() {
  if (action_widgets_.empty()) return false;
  action_widgets_.front()->focus();
  return true;
}

void InspectorPanel::repaint() {
  edge_->invalidate();
  body_->invalidate();
  for (auto& w : action_widgets_) w->invalidate();
}

}  // namespace hefesto::shell
