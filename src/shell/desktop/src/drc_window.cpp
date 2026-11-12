// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The main window's DRC parts (tasks.md 9.2-9.4; DESIGN boards
// M3-DRC-Status and DRC-Browser): the chip and its menu, the status-bar
// segment, the DRC panel's state, the `drc why` popover and the
// job-completion notice. Kept apart from main_window.cpp, which builds the
// rest of the Desktop-Layout screen.

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "hefesto/drc/commands/reporting.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "main_window.hpp"
#include "screen_paint.hpp"

namespace hefesto::shell {

namespace {

std::vector<std::string> printed_lines(const cmd::CommandResult& result) {
  std::vector<std::string> out;
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.severity == diag::Severity::info) out.push_back(d.text);
  }
  return out;
}

const db::CellDef* shown_root(const editor::EditorState& ed) {
  const view::LayoutWindow* win = ed.windows.current();
  return win != nullptr ? win->root() : nullptr;
}

}  // namespace

void MainWindow::build_drc_chip(ui::Box& row) {
  TopBarGroup& chips = top_groups_[1];
  auto chip = make_interactive(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t, bool, bool foc) {
    const StatusChip shown{drc_view_.chip.kind, drc_view_.chip.text + "  ▾"};
    paint::chip(p, w, h, shown, t);
    if (foc) p.stroke(0.0F, 0.0F, w, h, h / 2.0F, 2.0F, t.accent);
  });
  // Wide enough for "■ 88,888 DRC errors ▾": the bar does not shift.
  const float width = paint::chip_width(ctx_.runtime, StatusChip{StatusKind::error, "88,888 DRC errors  ▾"});
  chip->set_size(width, static_cast<float>(kChipHeight));
  chips.width += width;
  chips.widgets.push_back(chip.get());
  chip->set_role(ui::Role::button);
  // Click opens the DRC panel; right click (or the Menu key) opens the menu.
  chip->on_click([this](float x, float /*y*/) {
    const ui::Rect r = drc_chip_->rect();
    if (x > r.width - 24.0F) {  // the chevron
      open_drc_menu(r.x + r.width - 260.0F, r.y + r.height + 6.0F);
      return;
    }
    run_line(std::string(kDrcOpenPanelCommand));
  });
  chip->on_context([this](float x, float y) { open_drc_menu(x, y); });
  row.add_centered(*chip);
  drc_chip_ = chip.get();
  painted_.push_back(std::move(chip));
}

void MainWindow::open_drc_menu(float x, float y) {
  editor::EditorState& ed = ctx_.editor();
  DrcMenuInput in;
  in.has_root = shown_root(ed) != nullptr;
  in.busy = drc_link_->busy();
  in.background_on = ed.drc.background != drc::BackgroundMode::off;
  in.styles.assign(ed.drc.styles.names.begin(), ed.drc.styles.names.end());
  in.style = ed.drc.style;
  open_menu("DRC", drc_chip_menu(in), x, y);
}

std::size_t MainWindow::count_root_errors(const db::CellDef* root) const {
  const view::LayoutWindow* win = ctx_.core.editor().windows.current();
  if (root == nullptr || win == nullptr) return 0;
  // `drc list count total`: DRCCount over the window's area (Magic's DRC=N).
  const db::DatabaseLock::Shared reading(ctx_.core.editor().database_lock);
  std::size_t total = 0;
  const bool compat = ctx_.core.editor().drc.compat_settings().magic_compat;
  for (const drc::CellErrorCount& c : drc::count_errors(*root, win->mapping().surface, false, compat)) total += c.count;
  return total;
}

NearbyRulesView MainWindow::nearby_rules(const db::CellDef* root) const {
  const editor::EditorState& ed = ctx_.core.editor();
  const std::optional<db::Rect> box = visible_box(ed.box, ed.box_root, ed.edit_cell);
  const DrcState state = drc_state(drc_status_input(root));
  std::size_t under = 0;
  if (root != nullptr && (state == DrcState::errors || state == DrcState::clean)) {
    // `drc list count total` over the box (or the cell): DRCCount of the root only.
    const db::DatabaseLock::Shared reading(ctx_.core.editor().database_lock);
    const bool compat = ed.drc.compat_settings().magic_compat;
    for (const drc::CellErrorCount& c : drc::count_errors(*root, box.value_or(root->bbox()), false, compat)) {
      under += c.count;
    }
  }
  return nearby_rules_view(state, under, box.has_value());
}

DrcStatusInput MainWindow::drc_status_input(const db::CellDef* root) const {
  const editor::EditorState& ed = ctx_.core.editor();
  DrcStatusInput in;
  in.cell = root != nullptr ? root->name() : std::string();
  in.has_rules = ed.drc.rules && !ed.drc.rules->all_rules().empty();
  in.background_on = ed.drc.background != drc::BackgroundMode::off;
  in.busy = drc_link_->busy();
  in.error_tiles = drc_errors_;
  in.checked = drc_link_->root_checked() || drc_errors_ > 0;
  in.style = ed.drc.style;
  return in;
}

void MainWindow::refresh_drc() {
  drc_serial_ = drc_link_->serial();
  const db::CellDef* root = shown_root(ctx_.editor());
  drc_link_->set_root(root);
  if (root != find_root_) {
    last_find_.reset();
    find_box_.reset();
    find_root_ = root;
  }
  // Board note 6: "■ #k why-text" on the found tile while the box stays there.
  const editor::EditorState& ed = ctx_.editor();
  const bool box_shown = ed.box && root != nullptr && ed.box_root == root;
  canvas_->set_drc_tag(drc_canvas_tag(last_find_, find_box_, box_shown ? ed.box : std::nullopt));
  // The count is read while idle, as Magic's `magic::drcstate idle` does.
  if (!drc_link_->busy()) drc_errors_ = count_root_errors(root);
  const DrcStatusView view = drc_status_view(drc_status_input(root));
  if (view != drc_view_) {
    const bool state_changed = view.state != drc_view_.state || view.chip.text != drc_view_.chip.text;
    drc_view_ = view;
    drc_chip_->set_accessible_name(view.accessible_name);
    drc_chip_->set_tooltip(view.tooltip + " · click: DRC panel · right-click or ▾: menu");
    drc_chip_->invalidate();
    if (state_changed) ui::announce(view.announcement, false);  // polite (board note 1)
    // The inspector's "Design rules nearby" card follows the checker too.
    if (state_changed) inspector_->refresh(canvas_->unit_scale(), root, nearby_rules(root));
    update_status_accessibility();
    status_bar_->invalidate();
  }
  // Board note 5: an indeterminate Dock progress while checking.
  if (drc_link_->busy() != drc_progress_) {
    drc_progress_ = drc_link_->busy();
    (void)window_.set_busy_progress(drc_progress_);
  }
  if (drc_visible_) refresh_drc_panel(root);
}

void MainWindow::refresh_drc_panel(const db::CellDef* root) {
  editor::EditorState& ed = ctx_.editor();
  const bool busy = drc_link_->busy();
  const bool background_on = ed.drc.background != drc::BackgroundMode::off;
  const bool checked = drc_link_->root_checked() || drc_errors_ > 0;
  // Rebuilt when the checker went idle after the list's edits (or a
  // `drc catchup` ran with DRC off), as the board's stale state says.
  const bool outdated = root != list_root_ || drc_link_->stamp() != list_stamp_;
  if (root != nullptr && !busy && checked && outdated && (background_on || rebuild_requested_ || root != list_root_)) {
    rebuild_drc_list(*root);
  }
  rebuild_requested_ = false;
  if (root == nullptr && drc_panel_->model().has_list()) {
    drc_panel_->model().clear_list();
    list_root_ = nullptr;
    drc_rows_changed_ = true;
  }
  drc_panel_->model().set_units(canvas_->unit_scale());
  DrcPanelState s;
  s.status = drc_view_;
  const bool stale = drc_panel_->model().has_list() && drc_link_->stamp() != list_stamp_;
  s.browser = drc_browser_state(drc_view_.state, drc_panel_->model(), stale);
  s.error_tiles = drc_errors_;
  s.stepper = DrcStepper{last_find_ ? std::optional<std::int64_t>(last_find_->index) : std::nullopt, drc_errors_};
  s.cell = root != nullptr ? root->name() : std::string();
  s.style = ed.drc.style;
  s.styles.assign(ed.drc.styles.names.begin(), ed.drc.styles.names.end());
  s.busy = busy;
  s.has_root = root != nullptr;
  if (ed.box && ed.box_root == root) s.box = ed.box;
  DrcInspectorInput in{last_find_, drc_errors_, s.cell, s.box, s.style, canvas_->unit_scale()};
  s.inspector = drc_inspector_view(in);
  if (last_find_ && !last_find_->whys.empty()) {
    s.ignore_hint = drc_rule_tag(last_find_->whys.front());
  } else if (!drc_panel_->model().groups().empty()) {
    s.ignore_hint = drc_rule_tag(drc_panel_->model().groups().front().text);
  }
  drc_panel_->refresh(s, std::exchange(drc_rows_changed_, false));
}

void MainWindow::rebuild_drc_list(const db::CellDef& root) {
  editor::EditorState& ed = ctx_.editor();
  std::vector<DrcGroup> groups;
  {
    // DRCWhyAll over the root cell: what `select top cell; drc listall why`
    // reports, without moving the box or the selection (board note 3).
    const db::DatabaseLock::Exclusive writer(ed.database_lock);
    std::set<std::string> ignored;
    for (const drc::Rule& rule : ed.drc.rules->all_rules()) {
      if (ed.drc.ignore.contains(rule.why)) ignored.insert(drc::why_text(ed, rule));
    }
    drc::WhyOptions options;
    options.all = true;
    const db::Rect area = drc::error_area(root, ed.drc.compat_settings().magic_compat);
    for (drc::WhyGroup& g : drc::why_errors(ed, root, area, options)) {
      const bool is_ignored = ignored.contains(g.text);
      groups.push_back(DrcGroup{std::move(g.text), std::move(g.areas), is_ignored});
    }
  }
  drc_panel_->model().set_list(std::move(groups));
  list_root_ = &root;
  list_stamp_ = drc_link_->stamp();
  drc_rows_changed_ = true;
}

void MainWindow::note_drc_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  // The `drc why` popover closes on the next command (board note 4).
  if (why_open_) {
    window_.close_popups();
    why_open_ = false;
  }
  if (invocation.name != "drc" && invocation.name.find("::drc") == std::string::npos) return;
  const std::vector<std::string> printed = printed_lines(result);
  if (is_drc_option(invocation.name, invocation.args, "find")) {
    if (auto report = parse_find_report(printed)) {
      last_find_ = std::move(report);
      find_root_ = shown_root(ctx_.editor());
      find_box_ = ctx_.editor().box;  // `drc find` moved the box onto the error
    }
  }
  if (auto popover = drc_why_popover(invocation.name, invocation.args, printed)) pending_why_ = std::move(popover);
  if (is_drc_option(invocation.name, invocation.args, "catchup")) rebuild_requested_ = true;
  if (is_drc_option(invocation.name, invocation.args, "ignore") || is_drc_option(invocation.name, invocation.args, "style")) {
    list_stamp_ = DrcLink::Stamp{~std::uint64_t{0}, 0, 0};  // the groups' ignored flags or texts changed
    rebuild_requested_ = true;
  }
}

void MainWindow::show_why_popover() {
  const DrcWhyPopover popover = std::move(*pending_why_);
  pending_why_.reset();
  const editor::EditorState& ed = ctx_.editor();
  if (!ed.box || ed.box_root != shown_root(ed)) return;
  std::vector<MenuEntry> entries;
  const StatusKind kind = popover.clean ? StatusKind::success : StatusKind::error;
  for (const std::string& row : popover.rows) {
    entries.push_back({std::string(status_glyph(kind)) + " " + row, "", false, false, true, false});
  }
  entries.push_back({"Rechecked over the box; ignored rules left out.", "", false, false, false, true});
  const auto [x, y] = canvas_->popover_point(*ed.box);
  open_menu(popover.title, std::move(entries), x, y);
  why_open_ = true;
}

void MainWindow::post_drc_job_notice(double seconds) {
  const editor::EditorState& ed = ctx_.editor();
  const db::CellDef* root = shown_root(ed);
  DrcJobInput in;
  in.seconds = seconds;
  in.window_active = window_.active();
  const auto setting = ed.settings.effective("notifications");
  in.enabled = notifications_enabled(setting ? std::optional<std::string>(setting->value) : std::nullopt);
  in.cell = root != nullptr ? root->name() : std::string();
  in.error_tiles = count_root_errors(root);
  in.style = ed.drc.style;
  const auto notice = drc_job_notice(in);
  if (!notice) return;
  // Raven 0.2.2 has no system notification: the Dock icon asks for
  // attention and the toast waits on the canvas (m3-ui-conformance gap).
  toast_.show(notice->title + "  " + notice->body, in.error_tiles > 0 ? StatusKind::error : StatusKind::success, 0.0);
  window_.request_attention();
  append_console("# " + notice->title, notice->body);
}

void MainWindow::on_drc_service(std::size_t committed) {
  if (committed > 0) {
    ctx_.editor().windows.update_display();  // the committed squares' areas (DrcLink)
    canvas_dirty_ = true;
  }
  window_.request_redraw();
}

}  // namespace hefesto::shell
