// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The text helper and reorder dialog windows (see utility_windows.hpp).
// (Magic ref: tcltk/texthelper.tcl:1-420, tcltk/reorderLayers.tcl:1-316)

#include "utility_windows.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/diag/format.hpp"
#include "manager_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kPad = 14.0F;
constexpr float kLabelWidth = 130.0F;
constexpr float kTextSize = 12.0F;  // board M4-Managers tiles 4 and 6: 12 px rows
constexpr float kJustifySize = 26.0F;  // the 3×3 justification buttons

std::vector<std::string> split_words(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = std::min(text.find(' ', start), text.size());
    if (end > start) out.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  return out;
}

long long to_number(const std::string& text) {
  try {
    return std::stoll(text);
  } catch (...) {
    return 0;
  }
}

ui::Label muted_label(std::string_view text, const ScreenContext& ctx, float size = kTextSize) {
  ui::Label label = ui::Label::create(text);
  label.set_font_size(size);
  label.set_color(ctx.tokens().text_secondary);
  return label;
}

/// "Label   [field]" on one row; `fill` lets the field take the rest.
ui::Box field_row(std::string_view label, ui::Widget& field, const ScreenContext& ctx, float fill = 1.0F) {
  ui::Box row = ui::Box::horizontal();
  row.set_spacing(8.0F);
  ui::Label text = muted_label(label, ctx);
  text.set_size(kLabelWidth, -1.0F);
  row.add_centered(text);
  row.add(field, fill);
  return row;
}

/// A mono 12 px entry; `width` > 0 fixes its width (short values).
std::unique_ptr<ui::TextInput> input(std::string_view accessible, const ScreenContext& ctx, float width = -1.0F) {
  auto in = ui::TextInput::create("");
  in->set_size(width, static_cast<float>(kInputHeight));
  in->set_font(ctx.runtime.mono_font());
  in->set_font_size(kTextSize);
  in->set_accessible_name(accessible);
  return in;
}

/// Empty space that takes the free room of a row.
ui::Box gap() {
  return ui::Box::horizontal();
}

}  // namespace

// --- text helper ---------------------------------------------------------------------------

TextHelperWindow::TextHelperWindow(ScreenContext& ctx) : ctx_(ctx) {}

TextHelperWindow::~TextHelperWindow() {
  close();
}

void TextHelperWindow::close() {
  if (!window_) return;
  window_->destroy();
  window_.reset();
  justify_.clear();
  buttons_.clear();
}

void TextHelperWindow::show() {
  if (!window_) build();
  read_selection();
}

void TextHelperWindow::update() {
  if (window_) read_selection();
}

void TextHelperWindow::build() {
  window_ = std::make_unique<ui::Window>("Text helper", 440, 600);
  window_->set_topmost(true);
  window_->on_close_request([this] { close(); });
  root_ = ui::Box::vertical();
  root_.set_padding(kPad, kPad, kPad, kPad);
  root_.set_spacing(8.0F);
  title_.emplace(muted_label("New label", ctx_, 11.0F));
  root_.add(*title_);
  text_ = input("Text string", ctx_);
  root_.add(field_row("Text string", *text_, ctx_));
  fonts_ = {"default"};
  for (std::string& f : split_words(engine_query(ctx_.core, {"setlabel", "fontlist"}))) fonts_.push_back(std::move(f));
  font_ = ui::ComboBox::create(fonts_);
  font_->set_accessible_name("Font");
  font_->on_select([this](int i) {
    if (i >= 0 && static_cast<std::size_t>(i) < fonts_.size()) view_.model().font = fonts_[static_cast<std::size_t>(i)];
    preview_->set_text(view_.preview(port_exists_, units_));
  });
  font_->set_font_size(kTextSize);
  root_.add(field_row("Font", *font_, ctx_));
  size_ = input("Size in microns", ctx_, 90.0F);
  root_.add(field_row("Size (um)", *size_, ctx_, 0.0F));
  // The justification grid: NW N NE / W center E / SW S SE, then center and default.
  ui::Box grid = ui::Box::vertical();
  grid.set_spacing(4.0F);
  const auto& choices = justification_choices();
  for (std::size_t r = 0; r < 4; ++r) {
    ui::Box row = ui::Box::horizontal();
    row.set_spacing(4.0F);
    for (std::size_t c = 0; c < 3 && r * 3 + c < choices.size(); ++c) {
      const std::string_view just = choices[r * 3 + c];
      auto b = ui::Button::create(just == "center" && r == 1 ? "c" : just, false);
      b->set_accessible_name("Justification " + std::string{just});
      b->set_font_size(11.0F);
      // the 3×3 grid as small squares; center / default as two wider buttons
      b->set_size(r < 3 ? kJustifySize + 16.0F : kJustifySize * 2.0F + 36.0F, kJustifySize);
      b->on_click([this, just] {
        fields_to_model();
        view_.set_justification(just);
        mark_justification();
        preview_->set_text(view_.preview(port_exists_, units_));
      });
      row.add(*b);
      justify_.push_back(b.get());
      buttons_.push_back(std::move(b));
    }
    grid.add(row);
  }
  root_.add(field_row("Justification", grid, ctx_, 0.0F));
  rotation_ = input("Rotation in degrees", ctx_, 90.0F);
  root_.add(field_row("Rotation", *rotation_, ctx_, 0.0F));
  offset_ = input("Offset from reference, x y", ctx_, 140.0F);
  root_.add(field_row("Offset from reference", *offset_, ctx_, 0.0F));
  layer_ = input("Attach to layer", ctx_, 140.0F);
  root_.add(field_row("Attach to layer", *layer_, ctx_, 0.0F));
  ui::Box checks = ui::Box::horizontal();
  checks.set_spacing(12.0F);
  default_layer_ = ui::CheckBox::create("default");
  sticky_ = ui::CheckBox::create("sticky");
  checks.add(*default_layer_);
  checks.add(*sticky_);
  root_.add(checks);
  port_ = ui::CheckBox::create("Port enable");
  port_index_ = input("Port index", ctx_, 90.0F);
  ui::Box port = ui::Box::horizontal();
  port.set_spacing(8.0F);
  port.add_centered(*port_);
  port.add(*port_index_);
  root_.add(field_row("Port", port, ctx_, 0.0F));
  // The command Apply / Okay issue (board: mono, muted).
  preview_.emplace(muted_label("", ctx_, 11.0F));
  preview_->set_font(ctx_.runtime.mono_font());
  preview_->set_wrap(true);
  preview_->set_selectable(true);
  root_.add(*preview_);
  for (ui::TextInput* in : {text_.get(), size_.get(), rotation_.get(), offset_.get(), layer_.get(), port_index_.get()}) {
    in->on_change([this](const std::string&) {
      fields_to_model();
      preview_->set_text(view_.preview(port_exists_, units_));
    });
  }
  ui::Box actions = ui::Box::horizontal();
  actions.set_spacing(8.0F);
  const auto add = [&](std::string_view label, bool primary, std::function<void()> click) {
    auto b = ui::Button::create(label, primary);
    b->on_click(std::move(click));
    actions.add(*b);
    buttons_.push_back(std::move(b));
  };
  add("Apply", false, [this] { apply(); });
  add("Okay", true, [this] {
    apply();
    close();
  });
  actions.add(gap(), 1.0F);  // Cancel on the right (board)
  add("Cancel", false, [this] { close(); });
  root_.add(actions);
  window_->set_content(root_);
}

void TextHelperWindow::read_selection() {
  CoreSession& core = ctx_.core;
  const std::string text = engine_query(core, {"setlabel", "text"});
  units_ = engine_query(core, {"units"});
  if (units_.empty()) units_ = "lambda";
  if (text.empty()) {
    view_.show_new_label(to_number(engine_query(core, {"port", "last"})));
  } else {
    // update_texthelper reads the selected label's fields in microns.
    gui::TextHelperModel m;
    (void)engine_query(core, {"units", "microns"});
    m.text = text;
    m.font = engine_query(core, {"setlabel", "font"});
    m.justification = engine_query(core, {"setlabel", "justify"});
    m.size = engine_query(core, {"setlabel", "size"});
    m.offset = engine_query(core, {"setlabel", "offset"});
    m.rotation = engine_query(core, {"setlabel", "rotate"});
    m.layer = engine_query(core, {"setlabel", "layer"});
    m.default_layer = m.layer.empty();
    m.sticky = engine_query(core, {"setlabel", "sticky"}) == "1";
    port_exists_ = engine_query(core, {"port", "exists"}) == "1";
    m.port = port_exists_;
    m.port_index = port_exists_ ? engine_query(core, {"port", "index"}) : std::string{};
    (void)engine_query(core, {"units", units_});
    view_.show_selection(m, 1);
  }
  model_to_fields();
}

void TextHelperWindow::fields_to_model() {
  gui::TextHelperModel& m = view_.model();
  m.text = text_->text();
  m.size = size_->text();
  m.rotation = rotation_->text();
  m.offset = offset_->text();
  m.layer = layer_->text();
  m.port_index = port_index_->text();
}

void TextHelperWindow::model_to_fields() {
  const gui::TextHelperModel& m = view_.model();
  title_->set_text(view_.title());
  text_->set_text(m.text);
  size_->set_text(m.size);
  rotation_->set_text(m.rotation);
  offset_->set_text(m.offset);
  layer_->set_text(m.layer);
  default_layer_->set_checked(m.default_layer);
  sticky_->set_checked(m.sticky);
  port_->set_checked(m.port);
  port_index_->set_text(m.port_index);
  const auto font = std::ranges::find(fonts_, m.font);
  font_->set_selected(font == fonts_.end() ? 0 : static_cast<int>(std::distance(fonts_.begin(), font)));
  preview_->set_text(view_.preview(port_exists_, units_));
  mark_justification();
  default_layer_->on_toggle([this](bool on) { view_.model().default_layer = on; });
  sticky_->on_toggle([this](bool on) { view_.model().sticky = on; });
  port_->on_toggle([this](bool on) { view_.model().port = on; });
}

void TextHelperWindow::mark_justification() {
  const auto marks = view_.justification_marks();
  const auto& choices = justification_choices();
  for (std::size_t i = 0; i < justify_.size() && i < marks.size(); ++i) {
    justify_[i]->set_primary(marks[i]);
    // Raven buttons have no checked state for screen readers: the name says it.
    justify_[i]->set_accessible_name("Justification " + std::string{choices[i]} + (marks[i] ? ", chosen" : ""));
  }
}

void TextHelperWindow::apply() {
  fields_to_model();
  for (const std::string& line : view_.apply_lines(port_exists_, units_)) ctx_.run(line);
  const std::string last = engine_query(ctx_.core, {"port", "last"});
  view_.after_apply(last.empty() ? std::nullopt : std::optional<long long>{to_number(last)});
  model_to_fields();
}

// --- reorder dialog ---------------------------------------------------------------------------

ReorderWindow::ReorderWindow(ScreenContext& ctx, std::function<void(std::vector<std::string>)> confirm)
    : ctx_(ctx), confirm_(std::move(confirm)) {}

ReorderWindow::~ReorderWindow() {
  close();
}

void ReorderWindow::close() {
  if (!window_) return;
  window_->destroy();
  window_.reset();
  buttons_.clear();
}

void ReorderWindow::show(const gui::ToolbarModel& toolbar) {
  std::vector<std::string> presets;
  with_command_context(ctx_.core, [&](cmd::CommandContext& c) { presets = gui::toolbar_preset_names(c); });
  model_.emplace(toolbar.layers, std::move(presets));
  presets_path_ = toolbar.presets_file;
  if (!window_) build();
  reset_rows();
}

void ReorderWindow::build() {
  window_ = std::make_unique<ui::Window>("Reorder layers", 500, 480);
  window_->set_topmost(true);
  window_->on_close_request([this] { close(); });
  root_ = ui::Box::vertical();
  root_.set_padding(kPad, kPad, kPad, kPad);
  root_.set_spacing(8.0F);
  search_input_ = ui::TextInput::create("Search layer");
  search_input_->set_accessible_name("Search layer");
  search_input_->on_change([this](const std::string& text) {
    search_ = text;
    reset_rows();
  });
  search_input_->set_font_size(kTextSize);
  // Search and Preset on one row (board tile 6).
  ui::Box top = ui::Box::horizontal();
  top.set_spacing(8.0F);
  top.add(*search_input_, 1.0F);
  presets_ = ui::ComboBox::create(model_->presets());
  presets_->set_accessible_name("Preset");
  presets_->set_font_size(kTextSize);
  presets_->set_size(170.0F, -1.0F);
  presets_->on_select([this](int i) { preset_ = static_cast<std::size_t>(std::max(i, 0)); });
  top.add(*presets_);
  root_.add(top);
  table_ = ui::Table::create();
  table_->add_column("Layer", 160.0F, 1.0F, false);
  table_->add_column("Toolbar row", 90.0F, 0.0F, false);
  table_->set_accessible_name("Toolbar order");
  // The table with the edit buttons in a column at its right.
  ui::Box middle = ui::Box::horizontal();
  middle.set_spacing(8.0F);
  middle.add(*table_, 1.0F);
  ui::Box edit = ui::Box::vertical();
  edit.set_spacing(6.0F);
  ui::Box files = ui::Box::horizontal();
  files.set_spacing(6.0F);
  const auto add = [&](ui::Box& row, std::string_view label, bool primary, std::function<void()> click) {
    auto b = ui::Button::create(label, primary);
    b->on_click(std::move(click));
    row.add(*b);
    buttons_.push_back(std::move(b));
  };
  const auto add_wide = [&](ui::Box& column, std::string_view label, std::function<void()> click) {
    auto b = ui::Button::create(label, false);
    b->set_size(130.0F, -1.0F);
    b->on_click(std::move(click));
    column.add(*b);
    buttons_.push_back(std::move(b));
  };
  add_wide(edit, "Move up", [this] { move_selected(-1); });
  add_wide(edit, "Move down", [this] { move_selected(1); });
  add_wide(edit, "Group with above", [this] {
    const auto at = selected_layer();
    if (!at || *at == 0) return;
    model_->set_selection({*at - 1, *at});
    model_->group_selected();
    reset_rows();
  });
  add_wide(edit, "Delete layer", [this] {
    if (const auto at = selected_layer()) model_->set_selection({*at});
    model_->delete_selected();
    reset_rows();
  });
  middle.add(edit);
  root_.add(middle, 1.0F);
  note_.emplace(muted_label(
      "◆ Presets come from <tech>_toolbars.tcl, read as data (only set <name> {...} lines). Save rewrites its "
      "set current_toolbar line.",
      ctx_, 11.0F));
  note_->set_wrap(true);
  root_.add(*note_);
  add(files, "Load", false, [this] { load_preset(); });
  add(files, "Save", false, [this] { save_preset(); });
  files.add(gap(), 1.0F);  // Confirm on the right (board)
  add(files, "Confirm", true, [this] {
    if (confirm_) confirm_(model_->layers());
    close();
  });
  root_.add(files);
  window_->set_content(root_);
}

std::optional<std::size_t> ReorderWindow::selected_layer() const {
  const int row = table_ ? table_->selected() : -1;
  if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return std::nullopt;
  return rows_[static_cast<std::size_t>(row)];
}

void ReorderWindow::move_selected(int delta) {
  const auto at = selected_layer();
  if (!at || (delta < 0 && *at == 0)) return;
  const std::size_t to = delta < 0 ? *at - 1 : *at + 1;
  model_->move(*at, to);
  reset_rows();
  const auto row = std::ranges::find(rows_, std::min(to, model_->layers().size() - 1));
  if (row != rows_.end()) table_->set_selected(static_cast<int>(std::distance(rows_.begin(), row)));
}

void ReorderWindow::reset_rows() {
  rows_ = model_->filtered(search_);
  table_->set_rows(static_cast<int>(rows_.size()), [this](int row, int column) {
    const auto all = model_->rows();
    const auto r = static_cast<std::size_t>(row);
    if (r >= rows_.size() || rows_[r] >= all.size()) return std::string{};
    const ReorderRow& entry = all[rows_[r]];
    return column == 0 ? entry.layer : std::to_string(entry.row);
  });
}

void ReorderWindow::load_preset() {
  if (preset_ >= model_->presets().size()) return;
  diag::Outcome<std::vector<std::string>> loaded = std::vector<std::string>{};
  with_command_context(ctx_.core,
                       [&](cmd::CommandContext& c) { loaded = gui::load_toolbar_preset(c, model_->presets()[preset_]); });
  if (!loaded) {
    note_->set_text(loaded.error().text);  // names <tech>_toolbars.tcl
    return;
  }
  model_->replace(std::move(loaded.value()));
  reset_rows();
}

void ReorderWindow::save_preset() {
  // No presets file: the order applies only in memory (Q-SCRIPT-28).
  if (presets_path_.empty()) {
    note_->set_text("No <tech>_toolbars.tcl in the working directory: Confirm applies the order in memory.");
    return;
  }
  const platform::Path path{presets_path_};
  auto text = ctx_.core.files().read(path);
  const std::string current = text ? std::move(text.value()) : std::string{};
  const auto written = ctx_.core.files().write_atomic(path, model_->saved_text(current));
  note_->set_text(written.ok() ? "Saved the current_toolbar line of " + presets_path_
                               : "Cannot write " + presets_path_);
}

}  // namespace hefesto::shell
