// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "pdk_panel.hpp"

#include <utility>

#include "hefesto/pdk/catalog.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kPad = 14.0F;
constexpr float kRowHeight = 44.0F;
constexpr float kHeaderHeight = 80.0F;  // section label, heading and the root line
constexpr float kRootY = 56.0F;

Rgba alpha(const Rgba& c, float a) {
  return Rgba{c.r, c.g, c.b, c.a * a};
}

void paint_row(ui::Painter& p, float w, float h, const PdkRow& row, bool selected, paint::WidgetState s,
               const ThemeTokens& t) {
  if (selected) {
    p.fill(0.0F, 0.0F, w, h, static_cast<float>(kRadiusControl), t.row_selected);
  } else if (s.hovered) {
    p.fill(0.0F, 0.0F, w, h, static_cast<float>(kRadiusControl), alpha(t.row_selected, 0.6F));
  }
  p.text(TextRole::body_strong, 8.0F, 6.0F, row.title(), t.text_primary);
  const std::string libs = std::to_string(row.default_libraries) + " default libraries";
  // text_tertiary is below 4.5:1 on row_selected (dark): secondary on a highlighted row.
  const Rgba& meta = selected || s.hovered ? t.text_secondary : t.text_tertiary;
  p.text(TextRole::meta, 8.0F, 6.0F + p.line_height(TextRole::body_strong), libs, meta);
  const std::string state = row.state.label();
  const float sw = p.text_width(TextRole::meta, state);
  p.text_centered_y(TextRole::meta, w - 8.0F - sw, h / 2.0F, state, status_text_color(t, row.state.kind));
  if (s.focused) p.stroke(0.0F, 0.0F, w, h, static_cast<float>(kRadiusControl), 2.0F, t.accent);
}

}  // namespace

PdkPanel::PdkPanel(ScreenContext& ctx, std::function<void()> close)
    : ctx_(ctx),
      close_(std::move(close)),
      host_(pdk::make_local_install_host()),
      fetcher_(pdk::make_unsupported_fetcher()),
      background_(ui::Panel::create(ctx.tokens().surface1)),
      version_(ui::TextInput::create("Version hash (40 hex digits)")) {
  // Read-only use: the panel lists installs; every change runs a `pdk` command.
  manager_ = std::make_unique<pdk::PdkManager>(ctx_.core.files(), *host_, *fetcher_, pdk::Catalog::builtin());
  manager_->set_root(manager_->resolve_root(std::nullopt));

  auto header = make_painted(ctx_, [this](ui::Painter& p, float w, float, const ThemeTokens& t) {
    paint::section_label(p, kPad, 14.0F, "PDK manager", t);
    p.text(TextRole::heading, kPad, 32.0F, "Process design kits", t.text_primary);
    const auto small = [&p](std::string_view str) { return p.text_width(TextRole::small, str); };
    const float content = w - 2.0F * kPad;
    p.text(TextRole::small, kPad, kRootY, ellipsize(root_text_, content, small), t.text_secondary);
    if (root_error_.empty()) return;
    float y = kRootY + p.line_height(TextRole::small) + 2.0F;
    for (const std::string& line : wrap_to_width(root_error_, content, small)) {
      p.text(TextRole::small, kPad, y, line, t.danger_text);
      y += p.line_height(TextRole::small);
    }
  });
  header->set_size(-1.0F, kHeaderHeight);
  header_ = header.get();
  header->set_role(ui::Role::heading);
  header->set_accessible_name("PDK manager");
  auto close_button = make_interactive(ctx_, [](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hovered,
                                                bool focused) {
    paint::button(p, w, h, "Close", "", paint::ButtonStyle::ghost, paint::WidgetState{hovered, focused, true, false},
                  t);
  });
  close_button->set_size(paint::button_width(ctx_.runtime, "Close", false), static_cast<float>(kButtonHeight));
  close_button->set_role(ui::Role::button);
  close_button->set_accessible_name("Close the PDK manager");
  close_button->on_click([this](float, float) { close_(); });
  ui::Box header_row = ui::Box::horizontal();
  header_row.add(*header, 1.0F);
  ui::Box close_box = ui::Box::vertical();
  close_box.set_padding(0.0F, 10.0F, kPad, 0.0F);
  close_box.add(*close_button);
  header_row.add(close_box);
  fixed_.push_back(std::move(header));
  fixed_.push_back(std::move(close_button));

  rows_box_.set_spacing(2.0F);
  rows_box_.set_padding(6.0F, 0.0F, 6.0F, 0.0F);
  list_.add(rows_box_);
  list_.set_accessible_name("PDK variants");

  version_->set_accessible_name("Version hash");
  version_->set_size(-1.0F, static_cast<float>(kInputHeight));
  version_->set_font(ctx_.runtime.mono_font());
  version_->set_font_size(text_spec(TextRole::mono).size_px);
  version_->on_change([this](std::string text) { version_text_ = std::move(text); });
  ui::Box version_row = ui::Box::vertical();
  version_row.set_padding(kPad, 8.0F, kPad, 8.0F);
  version_row.add(*version_);

  ui::Box toggles = ui::Box::vertical();
  toggles.set_padding(kPad, 0.0F, kPad, 4.0F);
  toggles.set_spacing(4.0F);
  auto license = make_toggle("Accept the PDK license", &flags_.accept_license);
  auto unverified = make_toggle("Accept assets without SHA-256", &flags_.accept_unverified);
  toggles.add(*license);
  toggles.add(*unverified);
  fixed_.push_back(std::move(license));
  fixed_.push_back(std::move(unverified));

  // Two buttons per row, as the mockup's inspector actions (flex-grow, 8 px gap).
  ui::Box actions[3] = {ui::Box::horizontal(), ui::Box::horizontal(), ui::Box::horizontal()};
  const PdkAction order[6] = {PdkAction::remote, PdkAction::install, PdkAction::enable,
                              PdkAction::pin,    PdkAction::uninstall, PdkAction::list};
  for (std::size_t i = 0; i < 6; ++i) {
    ui::Box& row = actions[i / 2];
    if (i % 2 == 0) {
      row.set_padding(kPad, 4.0F, kPad, 4.0F);
      row.set_spacing(8.0F);
    }
    const auto style = order[i] == PdkAction::install ? paint::ButtonStyle::primary : paint::ButtonStyle::secondary;
    auto b = make_button(order[i], style);
    row.add(*b, 1.0F);
    fixed_.push_back(std::move(b));
  }

  column_.add(header_row);
  column_.add(list_, 1.0F);
  column_.add(version_row);
  column_.add(toggles);
  for (ui::Box& row : actions) column_.add(row);
  ui::Box bottom = ui::Box::vertical();
  bottom.set_size(-1.0F, static_cast<float>(kSpacing[2]));
  column_.add(bottom);
  background_.set_child(column_);
  auto edge = make_separator(ctx_, true);
  root_.add(*edge);
  root_.add(background_, 1.0F);
  fixed_.push_back(std::move(edge));
  root_.set_size(static_cast<float>(kInspectorWidth) + 36.0F, -1.0F);
  root_.set_accessible_name("PDK manager");
  refresh();
}

PdkPanel::~PdkPanel() = default;

std::unique_ptr<ui::Painted> PdkPanel::make_button(PdkAction action, paint::ButtonStyle style) {
  const std::string_view label = pdk_action_label(action);
  auto button = make_interactive(ctx_, [label, style](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                      bool hovered, bool focused) {
    paint::button(p, w, h, label, "", style, paint::WidgetState{hovered, focused, true, false}, t);
  });
  button->set_size(-1.0F, static_cast<float>(kButtonHeight));
  button->set_min_size(paint::button_width(ctx_.runtime, label, false), static_cast<float>(kButtonHeight));
  button->set_role(ui::Role::button);
  button->set_accessible_name(label);
  button->on_click([this, action](float, float) { run(action); });
  return button;
}

/// A checkbox row: a square with a check mark, plus text (state is shape +
/// text, never color only).
std::unique_ptr<ui::Painted> PdkPanel::make_toggle(std::string label, bool* value) {
  auto toggle = make_interactive(ctx_, [label, value](ui::Painter& p, float, float h, const ThemeTokens& t, bool hovered,
                                                      bool focused) {
    const float box = 14.0F;
    const float y = (h - box) / 2.0F;
    p.fill(0.0F, y, box, box, 3.0F, *value ? t.accent : t.input);
    // text_tertiary keeps the outline >= 3:1 (WCAG 1.4.11); control_border is below 2:1.
    p.stroke(0.0F, y, box, box, 3.0F, hovered ? 1.5F : 1.0F, focused ? t.accent : t.text_tertiary);
    if (*value) {
      p.line(3.0F, y + 7.0F, 6.0F, y + 10.0F, 1.6F, t.accent_text);
      p.line(6.0F, y + 10.0F, 11.0F, y + 4.0F, 1.6F, t.accent_text);
    }
    p.text_centered_y(TextRole::small, box + 8.0F, h / 2.0F, label, t.text_secondary);
  });
  toggle->set_size(-1.0F, 24.0F);
  toggle->set_role(ui::Role::checkbox);
  toggle->set_accessible_name(checkbox_accessible_name(label, *value));
  ui::Painted* raw = toggle.get();
  toggle->on_click([label, value, raw](float, float) {
    *value = !*value;
    raw->set_accessible_name(checkbox_accessible_name(label, *value));
    raw->invalidate();
  });
  return toggle;
}

void PdkPanel::refresh() {
  root_text_ = "Root: " + manager_->root().string();
  root_error_.clear();
  auto installed = manager_->installed();
  std::vector<pdk::InstalledVersion> versions;
  if (installed) {
    versions = installed.value();
  } else {
    root_error_ = diagnostic_line(StatusKind::error, installed.error().text);
  }
  rows_ = pdk_rows(manager_->catalog(), versions);
  if (selected_ >= rows_.size()) selected_ = 0;
  layout_header();
  rebuild_rows();
}

float PdkPanel::header_text_width() const {
  // The panel is fixed-width: its edge, the Close button column and the padding.
  const float panel = static_cast<float>(kInspectorWidth) + 36.0F - 1.0F;
  return panel - paint::button_width(ctx_.runtime, "Close", false) - 3.0F * kPad;
}

void PdkPanel::layout_header() {
  if (header_ == nullptr) return;
  const auto small = [this](std::string_view str) { return ctx_.runtime.text_width(TextRole::small, str); };
  const std::size_t lines = root_error_.empty() ? 0 : wrap_to_width(root_error_, header_text_width(), small).size();
  const float line_h = text_spec(TextRole::small).size_px * 1.5F;  // at least the painter's line height
  header_->set_size(-1.0F, kHeaderHeight + static_cast<float>(lines) * line_h + (lines > 0 ? 4.0F : 0.0F));
  header_->set_tooltip(root_error_.empty() ? root_text_ : root_text_ + "\n" + root_error_);
  header_->set_accessible_description(root_error_.empty() ? root_text_ : root_text_ + ". " + root_error_);
  header_->invalidate();
}

void PdkPanel::rebuild_rows() {
  rows_box_.clear();
  row_widgets_.clear();
  for (std::size_t i = 0; i < rows_.size(); ++i) {
    auto row = make_interactive(ctx_, [this, i](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hovered,
                                                bool focused) {
      paint_row(p, w, h, rows_[i], i == selected_, paint::WidgetState{hovered, focused, true, false}, t);
    });
    row->set_size(-1.0F, kRowHeight);
    row->set_role(ui::Role::list_item);
    row->set_accessible_name(rows_[i].title() + ", " + rows_[i].state.text);
    row->on_click([this, i](float, float) {
      selected_ = i;
      for (auto& w : row_widgets_) w->invalidate();
    });
    rows_box_.add(*row);
    row_widgets_.push_back(std::move(row));
  }
}

void PdkPanel::run(PdkAction action) {
  if (rows_.empty()) return;
  const PdkRow& row = rows_[selected_];
  const std::string version = version_text_.empty() ? row.installed_hash.value_or("") : version_text_;
  const auto line = pdk_command(action, row.family, row.variant, version, flags_);
  if (!line) {
    ctx_.run("echo \"" + std::string(pdk_action_label(action)) + ": enter a version hash first (Browse versions lists them)\"");
    return;
  }
  ctx_.run(*line);
  refresh();
}

void PdkPanel::repaint() {
  background_.set_fill(ctx_.tokens().surface1);
  for (auto& w : fixed_) w->invalidate();
  for (auto& w : row_widgets_) w->invalidate();
}

}  // namespace hefesto::shell
