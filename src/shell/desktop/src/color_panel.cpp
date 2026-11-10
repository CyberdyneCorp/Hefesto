// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "color_panel.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/view/color_window.hpp"
#include "screen_paint.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kPadX = 14.0F;
constexpr float kPump = 26.0F;      // the board's square − / + pumps
constexpr float kBarRow = 52.0F;    // name above, bar below
constexpr float kColorSwatchHeight = 64.0F;

}  // namespace

ColorPanel::ColorPanel(ScreenContext& ctx) : ctx_(ctx), background_(ui::Panel::create(ctx.tokens().surface1)) {
  column_.set_padding(kPadX, 10.0F, kPadX, 12.0F);
  column_.set_spacing(8.0F);

  // Caption: "COLOR = 047" and, once the map changed, "▲ map modified".
  auto caption = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    if (!view_) return;
    p.text_centered_y(TextRole::mono, 0.0F, h / 2.0F, view_->caption, t.text_primary);
    if (view_->modified) {
      const float x = p.text_width(TextRole::mono, view_->caption) + 12.0F;
      const float cw = p.text_width(TextRole::chip, "▲ map modified") + 16.0F;
      p.fill(x, (h - 22.0F) / 2.0F, std::min(cw, w - x), 22.0F, 11.0F, Rgba{t.warning.r, t.warning.g, t.warning.b, 0.14F});
      p.text_centered_y(TextRole::chip, x + 8.0F, h / 2.0F, "▲ map modified", t.warning_text);
    }
  });
  caption->set_size(-1.0F, 28.0F);
  caption->set_role(ui::Role::heading);
  column_.add(*caption);
  widgets_.push_back(std::move(caption));

  // Index stepper: ‹ 047 · 39 decimal ›.
  ui::Box index_row = ui::Box::horizontal();
  index_row.set_spacing(6.0F);
  add_button(index_row, "‹", [this] {
    if (view_ && !view_->previous.empty()) ctx_.run(in_window(view_->previous));
  });
  auto index = make_painted(ctx_, [this](ui::Painter& p, float, float h, const ThemeTokens& t) {
    if (!view_) return;
    p.text_centered_y(TextRole::mono, 4.0F, h / 2.0F, view_->octal + " octal · " + view_->decimal, t.text_secondary);
  });
  index->set_size(-1.0F, static_cast<float>(kButtonHeight));
  index->set_role(ui::Role::text);
  index_row.add(*index, 1.0F);
  widgets_.push_back(std::move(index));
  add_button(index_row, "›", [this] {
    if (view_ && !view_->next.empty()) ctx_.run(in_window(view_->next));
  });
  column_.add(index_row);

  // The edited color under Magic's "Color Being Edited".
  auto swatch = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    if (!view_) return;
    p.text(TextRole::mono_small, 0.0F, 0.0F, view::kCurrentColorText, t.text_secondary);
    const float top = p.line_height(TextRole::mono_small) + 6.0F;
    p.fill(0.0F, top, w, h - top, 4.0F, view_->swatch);
    p.stroke(0.0F, top, w, h - top, 4.0F, 1.0F, t.control_border);
  });
  swatch->set_size(-1.0F, kColorSwatchHeight + 20.0F);
  swatch->set_role(ui::Role::image);
  column_.add(*swatch);
  widgets_.push_back(std::move(swatch));

  for (std::size_t i = 0; i < 6; ++i) build_bar(i);

  auto values = make_painted(ctx_, [this](ui::Painter& p, float, float h, const ThemeTokens& t) {
    if (!view_) return;
    // "R G B" over "H S V #hex": the strip does not fit the 284 px dock on one line.
    const std::size_t cut = view_->values.find("   H ");
    const float lh = p.line_height(TextRole::mono_small);
    p.text(TextRole::mono_small, 0.0F, (h - 2.0F * lh) / 2.0F, view_->values.substr(0, cut), t.text_tertiary);
    if (cut != std::string::npos) {
      p.text(TextRole::mono_small, 0.0F, (h - 2.0F * lh) / 2.0F + lh, view_->values.substr(cut + 3), t.text_tertiary);
    }
  });
  values->set_size(-1.0F, 36.0F);
  values->set_role(ui::Role::text);
  column_.add(*values);
  widgets_.push_back(std::move(values));

  ui::Box file_row = ui::Box::horizontal();
  file_row.set_spacing(8.0F);
  add_button(file_row, "Load…", [this] {
    if (ctx_.prompt) ctx_.prompt(in_window("load "));
  });
  add_button(file_row, "Save…", [this] {
    if (ctx_.prompt) ctx_.prompt(in_window("save "));
  }, true);
  add_button(file_row, "Close", [this] {
    if (view_) ctx_.run("closewindow " + std::to_string(view_->window));
  });
  column_.add(file_row);

  // Scrolls when the dock is shorter than the column (1024 x 640, the
  // minimum window: the value strip and the file row stay reachable).
  scroll_.add(column_);
  scroll_.set_accessible_name("Color window controls");
  background_.set_child(scroll_);
  auto edge = make_separator(ctx_, true);
  root_.add(*edge);
  widgets_.push_back(std::move(edge));
  root_.add(background_, 1.0F);
  root_.set_size(static_cast<float>(kInspectorWidth), -1.0F);
  root_.set_accessible_name("Color window");
}

std::string ColorPanel::in_window(const std::string& line) const {
  if (!view_) return line;
  // The swatch's center: inside the color window, on no button.
  editor::EditorState& ed = ctx_.editor();
  view::ColorWindow* w = ed.color_map.find(view_->window);
  if (w == nullptr) return line;
  const view::ViewMapping m = view::color_window_mapping(ed, *w);
  const db::Rect& r = view::kCurrentColorTextBox;
  return "setpoint " + std::to_string(m.screen_x((r.x0 + r.x1) / 2)) + " " +
         std::to_string(m.screen_y((r.y0 + r.y1) / 2)) + " " + std::to_string(view_->window) + "; " + line;
}

void ColorPanel::add_button(ui::Box& row, std::string label, std::function<void()> click, bool primary) {
  auto button = make_interactive(ctx_, [label, primary](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                        bool hov, bool foc) {
    paint::button(p, w, h, label, "", primary ? paint::ButtonStyle::primary : paint::ButtonStyle::secondary,
                  paint::WidgetState{hov, foc, true, false}, t);
  });
  button->set_size(paint::button_width(ctx_.runtime, label, false), static_cast<float>(kButtonHeight));
  button->set_role(ui::Role::button);
  button->set_accessible_name(label == "‹" ? "Previous color" : label == "›" ? "Next color" : label);
  button->on_click([click = std::move(click)](float, float) { click(); });
  row.add_centered(*button);
  widgets_.push_back(std::move(button));
}

void ColorPanel::build_bar(std::size_t index) {
  // One row: the component's name, then [−] bar [+]; a click on the bar
  // sets the component to the click fraction (pushbutton at that point).
  auto bar = make_interactive(ctx_, [this, index](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                                  bool foc) {
    if (!view_) return;
    const ColorBarView& b = view_->bars[index];
    const float label_h = p.line_height(TextRole::mono_small);
    p.text(TextRole::mono_small, kPump + 8.0F, 0.0F, b.name + "  " + std::to_string(b.value).substr(0, 5),
           t.text_secondary);
    const float y = label_h + 6.0F;
    const float bh = h - y;
    for (const bool plus : {false, true}) {
      const float x = plus ? w - kPump : 0.0F;
      p.stroke(x + 0.5F, y + 0.5F, kPump - 1.0F, bh - 1.0F, 3.0F, 1.0F, t.control_border);
      p.text_centered_y(TextRole::mono, x + kPump / 2.0F - 3.5F, y + bh / 2.0F, plus ? "+" : "-", t.text_primary);
    }
    const float bx = kPump + 8.0F;
    const float bw = w - 2.0F * (kPump + 8.0F);
    p.fill(bx, y, bw, bh, 2.0F, t.canvas);
    p.fill(bx, y, bw * static_cast<float>(std::clamp(b.value, 0.0, 1.0)), bh, 2.0F, b.fill);
    p.stroke(bx, y, bw, bh, 2.0F, hov || foc ? 2.0F : 1.0F, hov || foc ? t.accent : t.control_border);
  });
  bar->set_size(-1.0F, kBarRow);
  bar->set_role(ui::Role::group);
  ui::Painted* self = bar.get();
  bar->on_click([this, index, self](float x, float) {
    if (!view_) return;
    const ColorBarView& b = view_->bars[index];
    const float w = self->rect().width;
    if (x < 0.0F) return;  // Enter / Space: nothing to aim at
    if (x < kPump) {
      ctx_.run(b.minus);
    } else if (x > w - kPump) {
      ctx_.run(b.plus);
    } else {
      const float bx = kPump + 8.0F;
      const float bw = std::max(1.0F, w - 2.0F * (kPump + 8.0F));
      ctx_.run(bar_click_line(ctx_.editor(), index, static_cast<double>((x - bx) / bw)));
    }
  });
  column_.add(*bar);
  widgets_.push_back(std::move(bar));
}

bool ColorPanel::refresh() {
  view_ = color_window_view(ctx_.editor());
  if (!view_) return false;
  for (std::size_t i = 0; i < view_->bars.size(); ++i) {
    // widgets_: caption, ‹, index, ›, swatch, then the six bars.
    const ColorBarView& b = view_->bars[i];
    widgets_[5 + i]->set_accessible_name(b.name + " " + std::to_string(b.value).substr(0, 5) +
                                         ": left edge decreases, right edge increases, the bar sets");
  }
  repaint();
  return true;
}

void ColorPanel::repaint() {
  background_.set_fill(ctx_.tokens().surface1);
  for (auto& w : widgets_) w->invalidate();
}

}  // namespace hefesto::shell
