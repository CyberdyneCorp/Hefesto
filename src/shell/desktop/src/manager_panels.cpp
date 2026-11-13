// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M4 manager panels (see manager_panels.hpp).
// (Magic ref: tcltk/cellmgr.tcl, libmgr.tcl, wrapper.tcl:286-531)

#include "manager_panels.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "screen_paint.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kPadX = 14.0F;
constexpr float kTitleHeight = 28.0F;
constexpr float kLineHeight = 22.0F;
constexpr float kTechFieldsHeight = 9 * kLineHeight + 48.0F;

class DropSink final : public diag::DiagnosticSink {
 public:
  void emit(const diag::Diagnostic&) override {}
};

/// One engine query nested in `ctx` (exact command names, as Magic's
/// manager scripts call them); its value as text, empty on failure.
std::string query(cmd::CommandContext& ctx, const std::vector<std::string>& words, const std::string& client) {
  if (words.empty()) return {};
  const cmd::CommandLookup found = ctx.registry().resolve(words.front(), client, true, cmd::NameLookup::exact);
  if (found.status != cmd::LookupStatus::found) return {};
  const cmd::Invocation inv{found.spec->name, {words.begin() + 1, words.end()}, cmd::Origin::internal, std::nullopt};
  const cmd::CommandResult r = found.spec->handler(ctx, inv);
  return r.ok() ? r.value.to_string() : std::string{};
}

std::vector<std::string> split(std::string_view text, char separator) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = std::min(text.find(separator, start), text.size());
    if (end > start) out.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  return out;
}

/// The path variables shown unexpanded in the Libraries tab.
std::vector<std::pair<std::string, std::string>> path_variables(const editor::EditorState& editor) {
  std::vector<std::pair<std::string, std::string>> out;
  for (const char* name : {"PDKPATH", "PDK_ROOT", "PDK_PATH", "CAD_ROOT"}) {
    if (const auto value = editor.path_variables.get(name)) out.emplace_back(name, *value);
  }
  return out;
}

}  // namespace

void with_command_context(CoreSession& core, const std::function<void(cmd::CommandContext&)>& body) {
  DropSink sink;
  cmd::CommandContext ctx(core.services(), core.project(), core.dispatcher().session(), sink, core.registry(),
                          &core.editor());
  body(ctx);
}

std::string engine_query(CoreSession& core, const std::vector<std::string>& words) {
  std::string value;
  with_command_context(core, [&](cmd::CommandContext& c) { value = query(c, words, core.dispatcher().client()); });
  return value;
}

ManagerPanels::ManagerPanels(ScreenContext& ctx) : ctx_(ctx) {
  cells_ = CellsView([this](const gui::CellManagerRow& row) {
    std::vector<gui::CellManagerRow> rows;
    with_command_context(ctx_.core, [&](cmd::CommandContext& c) { rows = gui::cell_manager_children(c, row); });
    return rows;
  });
  build_cells();
  build_libraries();
  build_tech();
}

ManagerPanels::~ManagerPanels() = default;

std::unique_ptr<ui::Painted> ManagerPanels::title_row(std::string_view text) {
  auto title = make_painted(ctx_, [text = std::string{text}](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint::fill_rect(p, w, h, t.surface1);
    paint::section_label(p, kPadX, h - 4.0F - p.line_height(TextRole::section_label), text, t);
  });
  title->set_size(-1.0F, kTitleHeight);
  title->set_role(ui::Role::heading);
  title->set_accessible_name(text);
  return title;
}

std::unique_ptr<ui::Painted> ManagerPanels::make_button(std::string label, std::string tooltip,
                                                        std::function<bool()> enabled,
                                                        std::function<void(float, float)> click) {
  auto button = make_interactive(ctx_, [label, enabled](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                        bool hov, bool foc) {
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

void ManagerPanels::run_lines(const std::vector<std::string>& lines) {
  for (const std::string& line : lines) ctx_.run(line);
}

// --- Cells -------------------------------------------------------------------------------

void ManagerPanels::build_cells() {
  auto title = title_row("Cells");
  cells_root_.add(*title);
  painted_.push_back(std::move(title));
  cells_target_ = make_painted(ctx_, [this](ui::Painter& p, float, float h, const ThemeTokens& t) {
    p.text_centered_y(TextRole::small, kPadX, h / 2.0F, "Target " + cells_.target(), t.text_secondary);
  });
  cells_target_->set_size(-1.0F, kLineHeight);
  cells_root_.add(*cells_target_);
  cells_table_ = ui::Table::create();
  cells_table_->add_column("Cell", 150.0F, 1.0F, false);
  cells_table_->add_column("Instance", 74.0F, 0.0F, false);  // "(top cell)", "xM3": room for the cell name
  cells_table_->set_accessible_name("Cells: top cells and their children");
  cells_table_->on_select([this](int row) { cells_.select(static_cast<std::size_t>(row)); });
  // Return or a double click opens or closes a node (the keyboard's tree control).
  cells_table_->on_activate([this](int row) {
    if (cells_.toggle(static_cast<std::size_t>(row))) dirty_.cells = true;
  });
  // Left closes a node or moves to its parent, Right opens it or moves to its
  // first child (gap G-1): the table's event callback sees them first.
  cells_table_->on_key([this](int key, std::uint8_t modifiers) {
    const int row = cells_table_->selected();
    if (row < 0 || !cells_.tree_key(key, modifiers, static_cast<std::size_t>(row))) return false;
    show_cells_rows();
    return true;
  });
  cells_root_.add(*cells_table_, 1.0F);
  using gui::CellManagerAction;
  const auto action = [this](CellManagerAction a) { return [this, a] { run_lines(cells_.commands(a)); }; };
  ui::Box buttons = button_row({{"Load", action(CellManagerAction::load)},
                                {"Edit", action(CellManagerAction::edit)},
                                {"Expand", action(CellManagerAction::expand)},
                                {"Place", action(CellManagerAction::place)},
                                {"Zoom", action(CellManagerAction::zoom)},
                                {"Refresh", [this] { ctx_.run("cellmanager update"); }}});
  cells_root_.add(buttons);
  cells_root_.set_size(static_cast<float>(kLeftPanelWidth), -1.0F);
}

ui::Box ManagerPanels::button_row(const std::vector<std::pair<std::string, std::function<void()>>>& buttons) {
  // Two rows of three (the dock is 240 px wide).
  ui::Box column = ui::Box::vertical();
  column.set_padding(kPadX, 6.0F, kPadX, 8.0F);
  column.set_spacing(6.0F);
  ui::Box row = ui::Box::horizontal();
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    if (i % 3 == 0) {
      row = ui::Box::horizontal();
      row.set_spacing(6.0F);
      column.add(row);
    }
    const auto& [label, click] = buttons[i];
    auto button = make_button(label, label, [] { return true; }, [click](float, float) { click(); });
    row.add(*button);
    painted_.push_back(std::move(button));
  }
  return column;
}

void ManagerPanels::refresh_cells() {
  gui::CellManagerModel model;
  with_command_context(ctx_.core, [&](cmd::CommandContext& c) { model = gui::cell_manager_model(c); });
  cells_.reset(model);
  cells_table_->set_rows(static_cast<int>(cells_.visible_count()), [this](int row, int column) {
    return cells_.cell_text(static_cast<std::size_t>(row), column);
  });
  if (const auto selected = cells_.selected()) cells_table_->set_selected(static_cast<int>(*selected));
  cells_target_->invalidate();
}

void ManagerPanels::highlight_selection(const cmd::CommandResult& result) {
  const std::string sstr = result.value.to_string();
  if (sstr.empty() || cells_.visible_count() == 0) return;
  std::vector<std::pair<std::string, std::string>> path;
  std::string top;
  with_command_context(ctx_.core, [&](cmd::CommandContext& c) {
    const std::string client = ctx_.core.dispatcher().client();
    top = query(c, {"cellname", "list", "window"}, client);
    // A 5-element result names the top cell (Magic's mgrselect).
    if (split(sstr, ' ').size() == 5) return;
    for (const std::string& instance : split(sstr, '/')) {
      path.emplace_back(instance, query(c, {"instance", "list", "celldef", instance}, client));
    }
  });
  if (top.empty()) top = cells_.visible_row(0).cell;
  if (!cells_.select_instance(top, path, sstr)) return;
  show_cells_rows();
}

void ManagerPanels::show_cells_rows() {
  cells_table_->set_rows(static_cast<int>(cells_.visible_count()), [this](int row, int column) {
    return cells_.cell_text(static_cast<std::size_t>(row), column);
  });
  if (const auto selected = cells_.selected()) {
    cells_table_->set_selected(static_cast<int>(*selected));
    cells_table_->scroll_to_row(static_cast<int>(*selected));
  }
}

// --- Libraries ---------------------------------------------------------------------------

void ManagerPanels::build_libraries() {
  auto title = title_row("Libraries");
  libraries_root_.add(*title);
  painted_.push_back(std::move(title));
  libraries_filter_ = make_interactive(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                    bool hov, bool foc) {
    const std::string tech = tech_.model().technology.empty() ? std::string{"technology"} : tech_.model().technology;
    const std::string text = std::string{library_filter_ ? "☑" : "☐"} + " Filter: only " + tech + " cells";
    if (hov || foc) paint::fill_rect(p, w, h, t.surface2);
    p.text_centered_y(TextRole::small, kPadX, h / 2.0F, text, t.text_primary);
  });
  libraries_filter_->set_size(-1.0F, kLineHeight);
  libraries_filter_->set_role(ui::Role::checkbox);
  libraries_filter_->set_accessible_name("Filter: only cells of the current technology");
  libraries_filter_->on_click([this](float, float) {
    library_filter_ = !library_filter_;
    dirty_.libraries = true;
    libraries_filter_->invalidate();
  });
  libraries_root_.add(*libraries_filter_);
  libraries_table_ = ui::Table::create();
  libraries_table_->add_column("Directory / cell", 160.0F, 1.0F, false);
  libraries_table_->add_column("Technology", 80.0F, 0.0F, false);
  libraries_table_->set_accessible_name("Libraries: cells of the search path");
  libraries_table_->on_select([this](int row) { libraries_.select(static_cast<std::size_t>(row)); });
  libraries_table_->on_activate([this](int row) {
    if (libraries_.toggle(static_cast<std::size_t>(row))) {
      libraries_table_->set_rows(static_cast<int>(libraries_.visible_count()), [this](int r, int c) {
        return libraries_.cell_text(static_cast<std::size_t>(r), c);
      });
    }
  });
  libraries_root_.add(*libraries_table_, 1.0F);
  using gui::LibraryManagerAction;
  const auto action = [this](LibraryManagerAction a) { return [this, a] { run_lines(libraries_.commands(a)); }; };
  ui::Box buttons = button_row({{"Load", action(LibraryManagerAction::load)},
                                {"Place", action(LibraryManagerAction::place)},
                                {"Pick", action(LibraryManagerAction::pick)},
                                {"Refresh", [this] { ctx_.run("libmanager update"); }}});
  libraries_root_.add(buttons);
  libraries_root_.set_size(static_cast<float>(kLeftPanelWidth), -1.0F);
}

void ManagerPanels::refresh_libraries() {
  gui::LibraryManagerModel model;
  with_command_context(ctx_.core,
                       [&](cmd::CommandContext& c) { model = gui::library_manager_model(c, library_filter_); });
  libraries_.reset(model, path_variables(ctx_.editor()));
  libraries_table_->set_rows(static_cast<int>(libraries_.visible_count()), [this](int row, int column) {
    return libraries_.cell_text(static_cast<std::size_t>(row), column);
  });
  libraries_filter_->invalidate();
}

// --- Technology manager --------------------------------------------------------------------

void ManagerPanels::build_tech() {
  auto title = title_row("Technology");
  tech_root_.add(*title);
  painted_.push_back(std::move(title));
  tech_body_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint::fill_rect(p, w, h, t.surface1);
    p.push_clip(0.0F, 0.0F, w, h);
    const auto measure_small = [&p](std::string_view s) { return p.text_width(TextRole::small, s); };
    const auto measure_strong = [&p](std::string_view s) { return p.text_width(TextRole::body_strong, s); };
    const float width = w - 2.0F * kPadX;
    float y = 6.0F;
    for (const std::string& line : wrap_to_width(tech_.title(), width, measure_strong)) {
      p.text(TextRole::body_strong, kPadX, y, line, t.text_primary);
      y += kLineHeight - 2.0F;
    }
    const auto lines = wrap_to_width(tech_.model().description, width, measure_small);
    for (std::size_t i = 0; i < lines.size() && i < 2; ++i, y += kLineHeight - 4.0F) {
      p.text(TextRole::small, kPadX, y, lines[i], t.text_secondary);
    }
    y += 6.0F;
    for (const TechField& field : tech_.fields()) {
      p.text(TextRole::small, kPadX, y, field.label, t.text_tertiary);
      p.text(TextRole::mono_small, kPadX + 150.0F, y, field.value, t.text_primary);
      y += kLineHeight;
    }
    p.text(TextRole::small, kPadX, y + 4.0F, "Edit internal units per lambda (Return: scalegrid)", t.text_tertiary);
    p.pop_clip();
  });
  tech_body_->set_size(-1.0F, kTechFieldsHeight);
  tech_body_->set_role(ui::Role::group);
  tech_body_->set_accessible_name("Technology");
  tech_root_.add(*tech_body_);

  // Internal units per lambda: two entries; Return issues scalegrid.
  ui::Box units = ui::Box::horizontal();
  units.set_padding(kPadX, 4.0F, kPadX, 4.0F);
  units.set_spacing(6.0F);
  units_numerator_ = ui::TextInput::create("1");
  units_denominator_ = ui::TextInput::create("1");
  for (ui::TextInput* input : {units_numerator_.get(), units_denominator_.get()}) {
    input->set_size(56.0F, static_cast<float>(kInputHeight));
    input->set_tooltip("Internal units per lambda; Return issues scalegrid");
    input->on_commit([this](const std::string&) { apply_units(); });
    units.add(*input);
  }
  units_numerator_->set_accessible_name("Internal units per lambda, numerator");
  units_denominator_->set_accessible_name("Internal units per lambda, denominator");
  tech_root_.add(units);

  ui::Box menus = ui::Box::vertical();
  menus.set_padding(kPadX, 6.0F, kPadX, 8.0F);
  menus.set_spacing(6.0F);
  const auto add = [&](std::string label, std::string tooltip, std::function<void(const ui::Widget&)> open) {
    auto button = make_button(label, std::move(tooltip), [] { return true; }, {});
    ui::Painted* anchor = button.get();
    button->on_click([anchor, open = std::move(open)](float, float) { open(*anchor); });
    menus.add(*button);
    painted_.push_back(std::move(button));
  };
  add("Technology ▾", "tech load <tech>, or a PDK's rc file (asks for consent)",
      [this](const ui::Widget& a) { open_tech_menu(a); });
  add("CIF input ▾", "cif istyle <style>", [this](const ui::Widget& a) { open_style_menu(TechStyle::cif_input, a); });
  add("CIF output ▾", "cif ostyle <style>",
      [this](const ui::Widget& a) { open_style_menu(TechStyle::cif_output, a); });
  add("Extract ▾", "extract style <style>", [this](const ui::Widget& a) { open_style_menu(TechStyle::extract, a); });
  add("DRC ▾", "drc style <style>", [this](const ui::Widget& a) { open_style_menu(TechStyle::drc, a); });
  tech_root_.add(menus);
  tech_root_.set_size(static_cast<float>(kInspectorWidth), -1.0F);
}

void ManagerPanels::refresh_tech() {
  gui::TechManagerModel model;
  with_command_context(ctx_.core, [&](cmd::CommandContext& c) { model = gui::tech_manager_model(c); });
  tech_.reset(std::move(model));
  units_numerator_->set_text(tech_.model().lambda_numerator);
  units_denominator_->set_text(tech_.model().lambda_denominator);
  tech_body_->invalidate();
}

void ManagerPanels::apply_units() {
  if (const auto line = tech_.scalegrid_command(units_numerator_->text(), units_denominator_->text())) ctx_.run(*line);
}

void ManagerPanels::open_tech_menu(const ui::Widget& anchor) {
  std::vector<MenuEntry> entries;
  for (const gui::TechMenuEntry& entry : tech_.model().tech_menu) {
    entries.push_back(MenuEntry{entry.label, entry.command, false, entry.label == tech_.model().technology, true, false});
  }
  if (entries.empty()) entries.push_back(MenuEntry{"No technology files found", {}, false, false, false, false});
  const ui::Rect r = anchor.rect();
  ctx_.menu("Technology", entries, r.x, r.y + r.height);
}

void ManagerPanels::open_style_menu(TechStyle style, const ui::Widget& anchor) {
  std::vector<MenuEntry> entries;
  for (const std::string& name : tech_.styles(style)) {
    entries.push_back(MenuEntry{name, TechView::style_command(style, name), false, name == tech_.current(style), true,
                                false});
  }
  if (entries.empty()) entries.push_back(MenuEntry{"No styles", {}, false, false, false, false});
  const ui::Rect r = anchor.rect();
  ctx_.menu("Style", entries, r.x, r.y + r.height);
}

// --- updates -------------------------------------------------------------------------------

void ManagerPanels::note_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  const std::string& name = invocation.name;
  static const std::vector<std::string> kCells = {"load", "getcell", "cellmanager", "edit", "delete", "cellname"};
  static const std::vector<std::string> kLibraries = {"addpath", "path", "save", "writeall", "libmanager"};
  static const std::vector<std::string> kTech = {"tech", "cif", "extract", "drc", "scalegrid", "techmanager"};
  const auto in = [&name](const std::vector<std::string>& names) {
    return std::ranges::find(names, name) != names.end();
  };
  dirty_.cells = dirty_.cells || in(kCells);
  dirty_.libraries = dirty_.libraries || in(kLibraries) || name == "tech";
  dirty_.tech = dirty_.tech || in(kTech) || name == "load";
  if (name == "techmanager") tech_requested_ = true;
  if (name == "select" && result.ok()) highlight_selection(result);
}

void ManagerPanels::invalidate_all() {
  dirty_ = Dirty{};
}

void ManagerPanels::refresh() {
  if (dirty_.cells) refresh_cells();
  if (dirty_.libraries) refresh_libraries();
  if (dirty_.tech) refresh_tech();
  dirty_ = Dirty{false, false, false};
}

void ManagerPanels::repaint() {
  for (const auto& p : painted_) p->invalidate();
  cells_target_->invalidate();
  libraries_filter_->invalidate();
  tech_body_->invalidate();
}

}  // namespace hefesto::shell
