// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "drc_panel.hpp"

#include <algorithm>
#include <utility>

#include "screen_paint.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kPadX = 14.0F;
constexpr float kListWidth = 320.0F;  // the board's DRC dock
constexpr float kTitleHeight = 28.0F;
constexpr float kLineHeight = 22.0F;
constexpr float kNoticeHeight = 96.0F;
constexpr float kInspectorRow = 22.0F;
constexpr float kStepperGap = 6.0F;           // the board's gap between Prev, #k of n and Next
constexpr float kStepperLabelWidth = 120.0F;  // "#12,408 of 12,408" in JetBrains Mono 11

std::unique_ptr<ui::Painted> title_row(ScreenContext& ctx, std::string_view text) {
  auto title = make_painted(ctx, [text](ui::Painter& p, float, float h, const ThemeTokens& t) {
    paint::section_label(p, kPadX, h - 4.0F - p.line_height(TextRole::section_label), text, t);
  });
  title->set_size(-1.0F, kTitleHeight);
  title->set_role(ui::Role::heading);
  title->set_accessible_name(text);
  return title;
}

/// Wrapped text lines at `x`, from `y`, no wider than `width`.
float wrapped(ui::Painter& p, TextRole role, float x, float y, float width, std::string_view text, const Rgba& c) {
  const auto measure = [&p, role](std::string_view s) { return p.text_width(role, s); };
  for (const std::string& line : wrap_to_width(text, width, measure)) {
    p.text(role, x, y, line, c);
    y += p.line_height(role) + 2.0F;
  }
  return y;
}

void paint_notice(ui::Painter& p, float w, const DrcBrowserNotice& n, const ThemeTokens& t) {
  const std::string head = std::string(status_glyph(n.kind)) + " " + n.title;
  p.text(TextRole::body_strong, kPadX, 8.0F, head, status_text_color(t, n.kind));
  (void)wrapped(p, TextRole::small, kPadX, 30.0F, w - 2.0F * kPadX, n.text, t.text_secondary);
}

void paint_inspector(ui::Painter& p, float w, float h, const DrcInspectorView& v, const ThemeTokens& t) {
  paint::fill_rect(p, w, h, t.surface1);
  paint::section_label(p, kPadX, 12.0F, v.title, t);
  float y = 34.0F;
  if (!v.empty.empty()) {
    (void)wrapped(p, TextRole::small, kPadX, y, w - 2.0F * kPadX, v.empty, t.text_secondary);
    return;
  }
  const float width = w - 2.0F * kPadX;
  y = wrapped(p, TextRole::body_strong, kPadX, y, width, v.headline, status_text_color(t, StatusKind::error));
  for (const std::string& why : v.whys) y = wrapped(p, TextRole::small, kPadX, y + 2.0F, width, why, t.text_primary);
  y = wrapped(p, TextRole::small, kPadX, y + 4.0F, width, "why-texts under the box, as drc why reports them",
              t.text_tertiary);
  y += 8.0F;
  for (const auto& [label, value] : v.rows) {
    p.text(TextRole::small, kPadX, y, label, t.text_tertiary);
    p.text(TextRole::mono_small, kPadX + 74.0F, y, value, t.text_primary);
    y += kInspectorRow;
  }
}

}  // namespace

DrcPanel::DrcPanel(ScreenContext& ctx)
    : ctx_(ctx),
      list_background_(ui::Panel::create(ctx.tokens().surface1)),
      inspector_background_(ui::Panel::create(ctx.tokens().surface1)) {
  build_list();
  build_inspector();
}

std::unique_ptr<ui::Painted> DrcPanel::make_button(std::string label, std::string tooltip,
                                                   std::function<bool()> enabled,
                                                   std::function<void(float, float)> click, bool on_canvas) {
  auto button = make_interactive(ctx_, [this, label, enabled, on_canvas](ui::Painter& p, float w, float h,
                                                                          const ThemeTokens& theme, bool hov, bool foc) {
    // Over the canvas the controls keep the dark canvas tokens in both themes.
    const ThemeTokens& t = on_canvas ? canvas_overlay_tokens(ctx_.mode) : theme;
    paint::button(p, w, h, label, "", paint::ButtonStyle::secondary, paint::WidgetState{hov, foc, enabled(), false}, t);
  });
  button->set_size(paint::button_width(ctx_.runtime, label, false), static_cast<float>(kButtonHeight));
  button->set_role(ui::Role::button);
  button->set_accessible_name(label);
  button->set_tooltip(tooltip);
  button->on_click([is_enabled = std::move(enabled), on_click = std::move(click)](float x, float y) {
    if (is_enabled()) on_click(x, y);
  });
  return button;
}

void DrcPanel::build_list() {
  auto title = title_row(ctx_, "DRC");
  list_column_.add(*title);
  painted_.push_back(std::move(title));

  // Style ▾ · Re-check · Fix… (M11)
  ui::Box tools = ui::Box::horizontal();
  tools.set_padding(kPadX, 0.0F, kPadX, 6.0F);
  tools.set_spacing(6.0F);
  auto style = make_interactive(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                             bool foc) {
    const std::string label = "Style " + (state_.style.empty() ? std::string("—") : state_.style) + " ▾";
    paint::button(p, w, h, label, "", paint::ButtonStyle::secondary,
                  paint::WidgetState{hov, foc, !state_.busy && !state_.styles.empty(), false}, t);
  });
  style->set_size(136.0F, static_cast<float>(kButtonHeight));
  style->set_role(ui::Role::button);
  style->set_accessible_name("DRC style");
  style->set_tooltip("drc listall style: choosing one issues drc style <name> (disabled while checking)");
  ui::Painted* style_anchor = style.get();
  style->on_click([this, style_anchor](float, float) {
    if (!state_.busy && !state_.styles.empty()) open_style_menu(*style_anchor);
  });
  tools.add_centered(*style);
  painted_.push_back(std::move(style));
  auto recheck = make_interactive(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                               bool foc) {
    paint::button(p, w, h, recheck_label(state_.browser), "", paint::ButtonStyle::secondary,
                  paint::WidgetState{hov, foc, state_.has_root && !state_.busy, false}, t);
  });
  recheck->set_size(paint::button_width(ctx_.runtime, "Re-check", false), static_cast<float>(kButtonHeight));
  recheck->set_role(ui::Role::button);
  recheck->set_accessible_name("Re-check the top cell");
  recheck->set_tooltip(std::string(kDrcRecheckCommand) + " (disabled while checking)");
  recheck->on_click([this](float, float) {
    if (state_.has_root && !state_.busy) ctx_.run(std::string(kDrcRecheckCommand));
  });
  tools.add_centered(*recheck);
  painted_.push_back(std::move(recheck));
  const DeferredRow fix = drc_deferred_rows().back();  // Fix with agent
  auto fix_button = make_button("Fix…", fix.tooltip, [] { return false; }, [](float, float) {});
  fix_button->set_enabled(false);
  tools.add_centered(*fix_button);
  painted_.push_back(std::move(fix_button));
  list_column_.add(tools);

  summary_ = make_painted(ctx_, [this](ui::Painter& p, float, float h, const ThemeTokens& t) {
    const std::string text = model_.summary(state_.error_tiles);
    const StatusKind kind = model_.summary_kind(state_.error_tiles);
    p.text_centered_y(TextRole::small, kPadX, h / 2.0F, text, status_text_color(t, kind));
  });
  summary_->set_size(-1.0F, kLineHeight);
  summary_->set_role(ui::Role::text);
  list_column_.add(*summary_);

  build_stepper();

  notice_ = make_painted(ctx_, [this](ui::Painter& p, float w, float, const ThemeTokens& t) {
    if (const auto n = drc_browser_notice(state_.browser, state_.cell, state_.style)) paint_notice(p, w, *n, t);
  });
  notice_->set_size(-1.0F, kNoticeHeight);
  notice_->set_role(ui::Role::text);
  list_column_.add(*notice_);

  filter_ = ui::TextInput::create("Filter why-text");
  filter_->set_accessible_name("Filter why-text");
  filter_->set_size(-1.0F, static_cast<float>(kInputHeight));
  filter_->set_font(ctx_.runtime.font(TextRole::small));
  filter_->set_font_size(text_spec(TextRole::small).size_px);
  filter_->on_change([this](std::string text) {
    model_.set_filter(std::move(text));
    reset_rows();
  });
  ui::Box filter_row = ui::Box::vertical();
  filter_row.set_padding(kPadX, 0.0F, kPadX, 6.0F);
  filter_row.add(*filter_);
  list_column_.add(filter_row);

  table_ = ui::Table::create();
  table_->add_column("Why-text", 200.0F, 1.0F, true);
  table_->add_column("Areas", 64.0F, 0.0F, true);
  table_->set_accessible_name("DRC errors by why-text");
  table_->show_sort(1, true);
  table_->on_select([this](int row) { on_row_selected(row); });
  table_->on_activate([this](int row) { on_row_activated(row); });
  table_->on_sort([this](int column, bool descending) {
    model_.sort(column == 0 ? DrcSortKey::text : DrcSortKey::count, descending);
    reset_rows();
  });
  list_column_.add(*table_, 1.0F);

  const DeferredRow signoff = drc_deferred_rows()[3];
  auto signoff_row = make_painted(ctx_, [signoff](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    p.text_centered_y(TextRole::section_label, kPadX, h / 2.0F, "▸ SIGN-OFF", t.text_tertiary);
    const float mw = p.text_width(TextRole::mono_small, signoff.milestone);
    p.text_centered_y(TextRole::mono_small, w - kPadX - mw, h / 2.0F, signoff.milestone, t.text_tertiary);
  });
  signoff_row->set_size(-1.0F, kTitleHeight);
  signoff_row->set_enabled(false);
  signoff_row->set_accessible_name("Sign-off, arrives in M13");
  signoff_row->set_tooltip(signoff.tooltip);
  list_column_.add(*signoff_row);
  painted_.push_back(std::move(signoff_row));

  list_background_.set_child(list_column_);
  list_root_.add(list_background_, 1.0F);
  auto edge = make_separator(ctx_, true);
  list_root_.add(*edge);
  painted_.push_back(std::move(edge));
  list_root_.set_size(kListWidth, -1.0F);
  list_root_.set_accessible_name("DRC results");
  reset_rows();
}

void DrcPanel::build_stepper() {
  // ◂ Prev  #k of n  Next ▸ over the canvas, top right (board; explicit
  // `drc find k±1`, Q-DRC-15). Real buttons: focusable and named.
  stepper_.set_spacing(kStepperGap);
  stepper_.set_accessible_name("DRC error stepper");
  auto prev = make_button("◂ Prev", "drc find k−1 (wraps to the last error)",
                          [this] { return state_.stepper.enabled(); },
                          [this](float, float) { ctx_.run(state_.stepper.prev_command()); }, true);
  float width = paint::button_width(ctx_.runtime, "◂ Prev", false);
  stepper_.add_centered(*prev);
  painted_.push_back(std::move(prev));
  stepper_label_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens&) {
    const ThemeTokens& t = canvas_overlay_tokens(ctx_.mode);
    p.fill(0.0F, 0.0F, w, h, static_cast<float>(kRadiusControl), t.surface2);
    p.stroke(0.0F, 0.0F, w, h, static_cast<float>(kRadiusControl), 1.0F, t.control_border);
    const std::string label = state_.stepper.label();
    const float tw = p.text_width(TextRole::mono_small, label);
    p.text_centered_y(TextRole::mono_small, (w - tw) / 2.0F, h / 2.0F, label, t.text_primary);
  });
  stepper_label_->set_size(kStepperLabelWidth, static_cast<float>(kButtonHeight));
  stepper_label_->set_role(ui::Role::text);
  stepper_.add_centered(*stepper_label_);
  auto next = make_button("Next ▸", "drc find k+1 (wraps to the first error); keys: ! is drc find; findbox zoom",
                          [this] { return state_.stepper.enabled(); },
                          [this](float, float) { ctx_.run(state_.stepper.next_command()); }, true);
  width += kStepperGap + kStepperLabelWidth + kStepperGap + paint::button_width(ctx_.runtime, "Next ▸", false);
  stepper_.add_centered(*next);
  painted_.push_back(std::move(next));
  stepper_.set_size(width, static_cast<float>(kButtonHeight));
}

void DrcPanel::build_inspector() {
  auto edge = make_separator(ctx_, true);
  inspector_root_.add(*edge);
  painted_.push_back(std::move(edge));
  inspector_body_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint_inspector(p, w, h, state_.inspector, t);
  });
  inspector_body_->set_role(ui::Role::pane);
  inspector_body_->set_accessible_name("DRC error area");
  inspector_column_.add(*inspector_body_, 1.0F);

  ui::Box actions = ui::Box::horizontal();
  actions.set_padding(kPadX, 0.0F, kPadX, 10.0F);
  actions.set_spacing(6.0F);
  auto zoom = make_button("Zoom to", std::string(kDrcZoomCommand), [this] { return state_.box.has_value(); },
                          [this](float, float) { ctx_.run(std::string(kDrcZoomCommand)); });
  actions.add(*zoom);
  painted_.push_back(std::move(zoom));
  auto why = make_button("Why here", std::string(kDrcWhyCommand), [this] { return state_.has_root; },
                         [this](float, float) { ctx_.run(std::string(kDrcWhyCommand)); });
  actions.add(*why);
  painted_.push_back(std::move(why));
  inspector_column_.add(actions);

  auto ignore_title = title_row(ctx_, "Ignore rules whose text contains");
  inspector_column_.add(*ignore_title);
  painted_.push_back(std::move(ignore_title));
  ui::Box ignore_row = ui::Box::horizontal();
  ignore_row.set_padding(kPadX, 0.0F, kPadX, 4.0F);
  ignore_row.set_spacing(6.0F);
  ignore_input_ = ui::TextInput::create("rule text, e.g. met1.2");
  ignore_input_->set_accessible_name("Ignore rules whose text contains");
  ignore_input_->set_size(-1.0F, static_cast<float>(kInputHeight));
  ignore_input_->set_font(ctx_.runtime.mono_font());
  ignore_row.add(*ignore_input_, 1.0F);
  auto ignore = make_button("Ignore", "drc ignore <text> (case-sensitive substring of the unsubstituted why-text)",
                            [this] { return state_.has_root; }, [this](float, float) {
                              const std::string text = ignore_input_->text();
                              if (!text.empty()) ctx_.run(drc_ignore_command(text));
                            });
  ignore_row.add_centered(*ignore);
  painted_.push_back(std::move(ignore));
  inspector_column_.add(ignore_row);
  auto ignore_note = make_painted(ctx_, [](ui::Painter& p, float w, float, const ThemeTokens& t) {
    (void)wrapped(p, TextRole::small, kPadX, 2.0F, w - 2.0F * kPadX,
                  "Ignored rules are left out of why, find and this list, but still mark error tiles: the chip keeps "
                  "counting them.",
                  t.text_tertiary);
  });
  ignore_note->set_size(-1.0F, 54.0F);
  ignore_note->set_hit_testable(false);
  inspector_column_.add(*ignore_note);
  painted_.push_back(std::move(ignore_note));

  auto later = title_row(ctx_, "Later milestones");
  inspector_column_.add(*later);
  painted_.push_back(std::move(later));
  const std::vector<DeferredRow> rows = drc_deferred_rows();
  for (std::size_t i = 0; i < 3; ++i) {
    const DeferredRow d = rows[i];
    auto row = make_painted(ctx_, [d](ui::Painter& p, float w, float h, const ThemeTokens& t) {
      p.text_centered_y(TextRole::small, kPadX, h / 2.0F, d.label, t.text_tertiary);
      const float mw = p.text_width(TextRole::mono_small, d.milestone);
      p.text_centered_y(TextRole::mono_small, w - kPadX - mw, h / 2.0F, d.milestone, t.text_tertiary);
    });
    row->set_size(-1.0F, 26.0F);
    row->set_enabled(false);
    row->set_accessible_name(d.label + ", arrives in " + d.milestone);
    row->set_tooltip(d.tooltip);
    inspector_column_.add(*row);
    painted_.push_back(std::move(row));
  }
  inspector_background_.set_child(inspector_column_);
  inspector_root_.add(inspector_background_, 1.0F);
  inspector_root_.set_size(static_cast<float>(kInspectorWidth), -1.0F);
  inspector_root_.set_accessible_name("DRC inspector");
}

void DrcPanel::reset_rows() {
  table_->set_rows(static_cast<int>(model_.row_count()),
                   [this](int row, int column) { return model_.cell_text(static_cast<std::size_t>(row), column); });
  table_->show_sort(model_.sort_key() == DrcSortKey::text ? 0 : 1, model_.sort_descending());
  if (state_.box) {
    if (const auto row = model_.row_at(*state_.box)) table_->set_selected(static_cast<int>(*row));
  }
}

void DrcPanel::on_row_selected(int row) {
  const auto index = static_cast<std::size_t>(row);
  if (index >= model_.row_count() || model_.row(index).kind != DrcRowKind::area) return;
  // Magic's DRC manager: selecting a rectangle moves the box there and zooms.
  if (const auto command = model_.command(index)) ctx_.run(*command);
}

void DrcPanel::on_row_activated(int row) {
  const auto index = static_cast<std::size_t>(row);
  if (model_.toggle(index)) {
    reset_rows();
    table_->set_selected(row);
    return;
  }
  if (index < model_.row_count() && model_.row(index).kind == DrcRowKind::ignored_heading) {
    ctx_.run(std::string(kDrcClearIgnoresCommand));
  }
}

void DrcPanel::open_style_menu(const ui::Widget& anchor) {
  const ui::Rect r = anchor.rect();
  if (ctx_.menu) ctx_.menu("DRC style", drc_style_menu(state_.styles, state_.style), r.x, r.y + r.height + 4.0F);
}

void DrcPanel::refresh(const DrcPanelState& state, bool rows_changed) {
  const bool box_moved = state.box != state_.box;
  state_ = state;
  if (state_.ignore_hint != ignore_prefill_) {
    ignore_prefill_ = state_.ignore_hint;
    ignore_input_->set_text(ignore_prefill_);
  }
  notice_->set_visible(drc_browser_notice(state_.browser, state_.cell, state_.style).has_value());
  stepper_label_->set_accessible_name(state_.stepper.accessible_name());
  summary_->set_accessible_name(model_.summary(state_.error_tiles));
  if (rows_changed) {
    reset_rows();
  } else if (box_moved && state_.box) {
    if (const auto row = model_.row_at(*state_.box)) {
      table_->set_selected(static_cast<int>(*row));
      table_->scroll_to_row(static_cast<int>(*row));
    }
  }
  repaint();
}

void DrcPanel::repaint() {
  list_background_.set_fill(ctx_.tokens().surface1);
  inspector_background_.set_fill(ctx_.tokens().surface1);
  for (auto& w : painted_) w->invalidate();
  for (ui::Painted* w : {summary_.get(), stepper_label_.get(), notice_.get(), inspector_body_.get()}) w->invalidate();
}

}  // namespace hefesto::shell
