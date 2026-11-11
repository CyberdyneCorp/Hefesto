// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "pcell_dialog.hpp"

#include <utility>

#include "hefesto/editor/editor_state.hpp"

namespace hefesto::shell {

namespace {

constexpr int kDialogWidth = 460;
constexpr int kDialogHeight = 640;
constexpr float kPad = 16.0F;
constexpr float kLabelWidth = 200.0F;
constexpr float kEntryWidth = 150.0F;  // board note 4: 150 px mono inputs

ui::Box header_line(std::string_view key, std::string_view value) {
  ui::Box line = ui::Box::horizontal();
  line.set_spacing(8.0F);
  ui::Label k = ui::Label::create(key);
  k.set_size(90.0F, -1.0F);
  line.add(k);
  ui::Label v = ui::Label::create(value);
  v.set_selectable(true);
  line.add(v, 1.0F);
  return line;
}

}  // namespace

PCellDialogWindow::PCellDialogWindow(ScreenContext& ctx, Run run) : ctx_(ctx), run_(std::move(run)) {}

PCellDialogWindow::~PCellDialogWindow() {
  if (window_) window_->destroy();
}

pcell::DialogModel* PCellDialogWindow::model() noexcept {
  auto& slot = ctx_.editor().script.gencell.dialog;
  return slot ? &*slot : nullptr;
}

void PCellDialogWindow::sync() {
  if (close_requested_) {
    close_requested_ = false;
    ctx_.editor().script.gencell.dialog.reset();
  }
  const pcell::DialogModel* m = model();
  if (m == nullptr) {
    if (window_) window_->destroy();
    window_.reset();
    shown_.reset();
    edits_.clear();
    pending_.clear();
    return;
  }
  if (shown_ && *shown_ == *m) return;
  if (shown_ && (shown_->device != m->device || shown_->mode != m->mode || shown_->instance != m->instance)) {
    edits_.clear();  // another device or instance (Reset, Create, select cell)
  }
  build(*m);
  shown_ = *m;
}

void PCellDialogWindow::build(const pcell::DialogModel& m) {
  if (!window_) {
    window_ = std::make_unique<ui::Window>("Device parameters", kDialogWidth, kDialogHeight);
    window_->set_min_size(380, 320);
    window_->set_topmost(true);  // a utility window above the layout window (board note 5)
    window_->on_close_request([this] { close_requested_ = true; });
  }
  window_->set_clear_color(ctx_.tokens().surface1);
  inputs_.clear();
  checks_.clear();
  combos_.clear();
  buttons_.clear();
  pending_.clear();
  const PCellDialogView view =
      pcell_dialog_view(m, native_generator_available(technology_pdk(*ctx_.editor().technology), m.device), edits_);
  ui::Box column = ui::Box::vertical();
  column.set_padding(kPad, kPad, kPad, kPad);
  column.set_spacing(10.0F);
  add_header(column, view);
  if (!view.banner.empty()) {
    ui::Label banner = ui::Label::create(view.banner);
    banner.set_wrap(true);
    banner.set_accessible_name(view.banner);
    column.add(banner);
    ui::announce(view.banner, false);
  }
  ui::ScrollBox body = ui::ScrollBox::create();
  ui::Box rows = ui::Box::vertical();
  rows.set_spacing(6.0F);
  for (const PCellRow& row : view.rows) add_row(rows, row);
  body.add(rows);
  column.add(body, 1.0F);  // the body scrolls; header and buttons stay
  add_buttons(column, view);
  column.set_accessible_name("Device parameters");
  window_->set_content(column);
  if (!view.announcement.empty()) ui::announce(view.announcement, false);  // board note 8: polite
}

void PCellDialogWindow::add_header(ui::Box& column, const PCellDialogView& view) {
  column.add(header_line(view.mode, view.device + "    " + view.generator.label()));
  column.add(header_line("Library:", view.library));
  column.add(header_line("Cellname:", view.cell_name));
  ui::Box line = ui::Box::horizontal();
  line.set_spacing(8.0F);
  ui::Label key = ui::Label::create("Instance:");
  key.set_size(90.0F, -1.0F);
  line.add(key);
  auto instance = ui::TextInput::create("(default)");
  instance->set_text(view.instance);
  instance->set_font(ctx_.runtime.mono_font());
  instance->set_size(kEntryWidth + 30.0F, -1.0F);
  instance->set_accessible_name("Instance name");
  // The instance name is read by the next Create / Apply (not a command itself).
  instance->on_change([this](std::string text) {
    if (pcell::DialogModel* m = model()) {
      m->instance = text;
      if (shown_) shown_->instance = std::move(text);
    }
  });
  line.add(*instance);
  inputs_.push_back(std::move(instance));
  column.add(line);
}

void PCellDialogWindow::add_row(ui::Box& body, const PCellRow& row) {
  ui::Box line = ui::Box::horizontal();
  line.set_spacing(8.0F);
  ui::Label label = ui::Label::create(row.label);
  label.set_size(kLabelWidth, -1.0F);
  label.set_wrap(true);
  line.add(label);
  const std::string p = row.parameter;
  switch (row.kind) {
    case pcell::WidgetKind::entry: {
      auto input = ui::TextInput::create({});
      input->set_text(row.value);
      input->set_font(ctx_.runtime.mono_font());
      input->set_size(kEntryWidth, -1.0F);
      input->set_accessible_name(row.label);
      input->on_change([this, p](std::string text) { pending_[p] = std::move(text); });
      input->on_commit([this, p](std::string text) { edit(p, text); });
      line.add(*input);
      inputs_.push_back(std::move(input));
      break;
    }
    case pcell::WidgetKind::checkbox: {
      auto check = ui::CheckBox::create({});
      check->set_checked(row.checked);
      check->set_accessible_name(row.label);
      check->on_toggle([this, p](bool on) { edit(p, on ? "1" : "0"); });
      line.add(*check);
      checks_.push_back(std::move(check));
      break;
    }
    case pcell::WidgetKind::selectlist:
    case pcell::WidgetKind::selectindex: {
      auto combo = ui::ComboBox::create(row.choices);
      combo->set_selected(row.choice ? static_cast<int>(*row.choice) : -1);
      combo->set_accessible_name(row.label);
      const bool by_index = row.kind == pcell::WidgetKind::selectindex;
      combo->on_select([this, p, by_index, choices = row.choices](int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= choices.size()) return;
        edit(p, by_index ? std::to_string(index) : choices[static_cast<std::size_t>(index)]);
      });
      line.add(*combo);
      combos_.push_back(std::move(combo));
      break;
    }
    case pcell::WidgetKind::message: {
      ui::Label text = ui::Label::create(row.value);
      text.set_wrap(true);
      line.add(text, 1.0F);
      break;
    }
  }
  body.add(line);
  if (row.corrected) {
    ui::Label typed = ui::Label::create("▲ you typed " + row.typed);
    typed.set_accessible_name(row.label + ": you typed " + row.typed + ", corrected to " + row.value);
    body.add(typed);
  } else if (row.updated) {
    body.add(ui::Label::create("◆ updated to " + row.value));
  }
}

void PCellDialogWindow::add_buttons(ui::Box& column, const PCellDialogView& view) {
  ui::Box row = ui::Box::horizontal();
  row.set_spacing(8.0F);
  bool first = true;
  for (const PCellButton& b : view.buttons) {
    auto button = ui::Button::create(b.label, b.primary);
    button->on_click([this, role = b.role, closes = b.closes] { press(role, closes); });
    row.add(*button);
    if (first) {  // Reset on the left, the rest on the right (board)
      ui::Box spacer = ui::Box::horizontal();
      row.add(spacer, 1.0F);
      first = false;
    }
    if (b.primary) button->focus();
    buttons_.push_back(std::move(button));
  }
  column.add(row);
}

void PCellDialogWindow::edit(const std::string& parameter, const std::string& value) {
  pending_.erase(parameter);
  pcell::DialogModel* m = model();
  if (m == nullptr) return;
  pcell::DialogWidget* widget = m->find(parameter);
  if (widget == nullptr) return;
  edits_.begin_edit(*m, parameter, value);
  (void)run_(pcell_value_script(*widget, value));
}

void PCellDialogWindow::commit_pending() {
  const auto pending = std::exchange(pending_, {});
  for (const auto& [parameter, value] : pending) edit(parameter, value);
}

void PCellDialogWindow::press(PCellButtonRole role, bool closes) {
  if (role == PCellButtonRole::close) {
    close_requested_ = true;
    return;
  }
  commit_pending();
  pcell::DialogModel* m = model();
  if (m == nullptr) return;
  const cmd::CommandResult result = run_(pcell_button_script(*m, role));
  if (result.ok() && closes) close_requested_ = true;
}

}  // namespace hefesto::shell
