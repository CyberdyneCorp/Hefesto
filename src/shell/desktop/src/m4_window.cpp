// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M4 screens of the main window (openspec/changes/m4-scripting/design.md
// D13; tasks 10.1-10.3): the menu bar (board M4-Menu-Bar), Place device and
// the PCell dialog (M4-PCell-Dialog), the consent prompt, the declined
// banner and sandbox-denial status (M4-Script-Consent). Every action runs a
// Tcl script in the project's interpreter (gui_script.hpp), so it is a
// logged command. Raven-free: built from the raven_ui wrappers.
//
// Owner: SHELL (SHELL-A).

#include <algorithm>
#include <cstdlib>

#include "hefesto/script/project.hpp"
#include "hefesto/shell/gui_script.hpp"
#include "main_window.hpp"

namespace hefesto::shell {

namespace {

constexpr float kMenuBarHeight = 30.0F;  // board note 1 (Linux)

std::vector<ui::MenuBarMenuSpec> menu_specs(const MenuBarView& view) {
  std::vector<ui::MenuBarMenuSpec> specs;
  for (const MenuBarMenu& menu : view.menus) {
    ui::MenuBarMenuSpec spec{menu.title, {}};
    for (const MenuBarItem& item : menu.items) {
      ui::MenuBarEntry entry;
      entry.separator = item.kind == MenuBarItem::Kind::separator;
      entry.label = item.pdk && !menu.pdk ? item.label + "  (PDK)" : item.label;
      entry.checkable = item.kind == MenuBarItem::Kind::check;
      entry.checked = item.checked;
      entry.enabled = item.enabled;
      entry.description = item.enabled ? item.description() : item.tooltip;
      spec.entries.push_back(std::move(entry));
    }
    specs.push_back(std::move(spec));
  }
  return specs;
}

std::vector<ui::MenuItem> place_items(const PlaceDeviceView& view) {
  std::vector<ui::MenuItem> items;
  for (const PlaceDeviceEntry& e : view.entries) {
    ui::MenuItem item;
    item.label = e.heading ? e.label : "    " + e.label;
    item.enabled = !e.heading;
    item.separator_before = (e.heading || e.recent) && !items.empty();
    items.push_back(std::move(item));
  }
  return items;
}

}  // namespace

// --- setup ------------------------------------------------------------------------------

void MainWindow::build_m4() {
  // The menu bar under the top bar: the system menu bar on macOS (no room
  // taken in the window), a 30 px bar in the window elsewhere.
  menu_bar_ = ui::MenuBar::create();
  if (menu_in_window_) menu_bar_->set_native(false);
  menu_bar_->set_size(-1.0F, kMenuBarHeight);
  root_.insert(*menu_bar_, 2);
  ctx_.editor().script.events.add(script_events_);
  pcell_dialog_ = std::make_unique<PCellDialogWindow>(
      ctx_, [this](const std::string& script) { return run_script_action(script); });

  // The declined banner over the canvas (hidden until the scripts are declined).
  declined_bar_.set_padding(16.0F, 8.0F, 16.0F, 8.0F);
  declined_bar_.set_spacing(12.0F);
  declined_text_.emplace(ui::Label::create({}));
  declined_text_->set_wrap(true);
  declined_bar_.add(*declined_text_, 1.0F);
  review_button_ = ui::Button::create("Review scripts...", false);
  review_button_->on_click([this] { review_scripts(); });
  declined_bar_.add(*review_button_);
  declined_bar_.set_visible(false);
  center_.insert(declined_bar_, 0);
  refresh_menu_bar();
}

void MainWindow::teardown_m4() {
  editor::EditorState& editor = ctx_.core.editor();
  if (consent_host_ != nullptr) {
    editor.script.events.remove(*consent_host_);
    if (editor.script.pdk_trust.consent_host() == consent_host_) editor.script.pdk_trust.set_consent_host(nullptr);
    consent_host_ = nullptr;
  }
  editor.script.events.remove(script_events_);
  pcell_dialog_.reset();
}

void MainWindow::set_consent_host(DesktopConsentHost* host) {
  editor::EditorState& editor = ctx_.core.editor();
  if (consent_host_ != nullptr) editor.script.events.remove(*consent_host_);
  consent_host_ = host;
  editor.script.pdk_trust.set_consent_host(host);
  if (host != nullptr) editor.script.events.add(*host);
}

ui::DialogLook MainWindow::dialog_look(std::string_view icon) const {
  const ThemeTokens& t = ctx_.tokens();
  ui::DialogLook look;
  look.icon = icon.empty() ? ui::kNoWidget : ctx_.runtime.icon(icon);
  look.title_font = ctx_.runtime.font(FontFace{FontFamily::ui, 600});
  look.mono = ctx_.runtime.mono_font();
  look.inset = t.surface1;
  look.border = t.border;
  look.icon_color = t.icon;
  look.muted = t.text_tertiary;
  look.body = t.text_secondary;
  look.ok = t.success;
  look.ok_text = t.success_text;
  look.warning = t.warning;
  look.warning_text = t.warning_text;
  return look;
}

script::ConsentDecision MainWindow::ask_consent(const ConsentPromptView& view) {
  const char* home = std::getenv("HOME");
  const PromptDialog dialog = consent_dialog(view, home != nullptr ? home : "");
  const ui::DialogLook look = dialog_look(dialog.icon);
  if (capture_mode_) {
    (void)window_.run_dialog(dialog, false, look);  // shown for the capture; nothing runs without an answer
    return ConsentPromptView::escape_decision();
  }
  return consent_decision(view, window_.run_dialog(dialog, true, look));
}

// --- per frame --------------------------------------------------------------------------

void MainWindow::refresh_m4() {
  drain_script_events();
  refresh_menu_bar();
  if (pcell_dialog_) pcell_dialog_->sync();
  refresh_declined();
}

cmd::CommandResult MainWindow::run_script_action(const std::string& script, cmd::Origin origin) {
  const cmd::CommandResult result = run_gui_script(ctx_.core.editor(), ctx_.core.dispatcher(), script, origin);
  append_console(script, gui_result_text(result));
  if (!result.ok()) {
    // Board M4-PCell-Dialog tile 9: a failure goes to the Console and the status bar.
    set_message(StatusKind::error, diagnostic_line(StatusKind::error, gui_result_text(result)));
  }
  dirty_ = true;
  window_.request_redraw();
  return result;
}

// --- menu bar ---------------------------------------------------------------------------

void MainWindow::refresh_menu_bar() {
  if (!menu_bar_) return;
  MenuBarView view = session_menu_bar(ctx_.core.editor(), scripts_declined());
  // The system menu bar (macOS) takes no room in the window; Raven decides
  // once the bar is in a shown window.
  const bool native = menu_bar_->native();
  if (!menu_built_ || native != menu_native_) {
    menu_native_ = native;
    menu_bar_->set_size(-1.0F, native ? 0.0F : kMenuBarHeight);
    // Raven still lays a system bar out at its own height over the row
    // below it: it must not take that row's clicks (the panel headers).
    menu_bar_->set_hit_testable(!native);
  }
  const std::size_t signature = menu_bar_signature(view);
  if (menu_built_ && signature == menu_signature_) {
    ++menu_quiet_frames_;
    return;
  }
  menu_view_ = std::move(view);
  menu_signature_ = signature;
  menu_built_ = true;
  menu_quiet_frames_ = 0;
  menu_bar_->set_menus(menu_specs(menu_view_), [this](std::size_t menu, std::size_t entry) {
    choose_menu_item(menu, entry);
  });
}

void MainWindow::choose_menu_item(std::size_t menu, std::size_t entry) {
  if (menu >= menu_view_.menus.size() || entry >= menu_view_.menus[menu].items.size()) return;
  const MenuBarItem item = menu_view_.menus[menu].items[entry];  // a copy: the script may rebuild the bar
  if (item.kind == MenuBarItem::Kind::separator || !item.enabled) return;
  set_message(StatusKind::info, diagnostic_line(StatusKind::info, item.description()));
  // A PDK's menu text runs as a script (S-01); Magic's own wrapper items as GUI actions.
  (void)run_script_action(menu_item_script(item), item.pdk ? cmd::Origin::script : cmd::Origin::gui);
}

// --- script events ----------------------------------------------------------------------

void MainWindow::drain_script_events() {
  if (!script_events_.denied.empty()) {
    // Board M4-Script-Consent tile 7: the Diagnostics row is the
    // SCRIPT-SANDBOX diagnostic; the status bar names the last denial.
    set_message(StatusKind::error, denial_status(script_events_.denied.back()));
    result_has_more_ = false;  // Show (a click on the bar) opens Diagnostics, not the Console
    announce(StatusKind::error, message_);
    script_events_.denied.clear();
  }
  if (!script_events_.skipped.empty()) {
    for (const script::SkippedStatement& s : script_events_.skipped) diagnostics_.add_item(skipped_statement_row(s));
    set_message(StatusKind::warning, skipped_statements_message(script_events_.skipped));
    script_events_.skipped.clear();
  }
  script_events_.menus_changed = false;  // refresh_menu_bar compares the whole bar
}

// --- Place device -----------------------------------------------------------------------

bool MainWindow::scripts_declined() const {
  return consent_host_ != nullptr && consent_host_->declined().has_value();
}

PlaceDeviceView MainWindow::place_device_state(std::string_view filter) const {
  editor::EditorState& editor = ctx_.core.editor();
  return place_device_view(editor.script.gencell.menus, editor.technology->name(), scripts_declined(), filter,
                           recent_device_);
}

void MainWindow::open_place_device() {
  const PlaceDeviceView view = place_device_state();
  if (!view.enabled) {
    set_message(StatusKind::warning, diagnostic_line(StatusKind::warning, view.tooltip));
    status_bar_->invalidate();
    return;
  }
  place_entries_ = view.entries;
  place_menu_.set_items(view.title, place_items(view), [this](std::size_t index) {
    if (index >= place_entries_.size() || place_entries_[index].heading) return;
    const std::string script = place_entries_[index].script;
    recent_device_ = script;
    (void)run_script_action(script, cmd::Origin::script);  // a PDK's device entry (S-01)
  });
  float x = static_cast<float>(kToolRailWidth);
  float y = 0.0F;
  for (const RailButton& b : rail_) {
    if (b.tool.icon == "place") y = b.widget->rect().y;
  }
  place_menu_.open(window_, x, y);
}

// --- declined ---------------------------------------------------------------------------

std::optional<script::PdkIdentity> MainWindow::declined_pdk() const {
  if (scripts_declined()) return consent_host_->declined();
  // A layout of a PDK declined without this window's prompt (another
  // window's answer, `pdk untrust`): the cell names its technology.
  const editor::EditorState& editor = ctx_.core.editor();
  const view::LayoutWindow* win = editor.windows.current();
  if (win == nullptr || win->root() == nullptr) return std::nullopt;
  const auto tech = editor.cell_session.declined.technology_of(*win->root());
  if (!tech || !cell::is_declined(editor, *win->root())) return std::nullopt;
  return script::PdkIdentity{*tech, *tech, ""};
}

void MainWindow::refresh_declined() {
  // Board M4-Script-Consent tile 6: the banner stays while the scripts are
  // declined or the window's layout was opened without them; Review
  // scripts... that reopens the layout hides it.
  const auto pdk = declined_pdk();
  const std::string banner = pdk ? declined_view(*pdk, header_.title).banner : std::string{};
  if (banner == declined_banner_) return;
  declined_banner_ = banner;
  declined_bar_.set_visible(!banner.empty());
  if (banner.empty()) return;
  declined_text_->set_text(banner);
  declined_text_->set_accessible_name(banner);
  ui::announce(banner, false);
}

void MainWindow::review_scripts() {
  if (consent_host_ == nullptr || !consent_host_->declined()) {
    set_message(StatusKind::warning,
                diagnostic_line(StatusKind::warning, "Reopen the project to be asked about its PDK scripts again"));
    return;
  }
  const script::PdkIdentity pdk = *consent_host_->declined();
  const auto script = consent_host_->script_of(pdk);
  if (!script) return;
  consent_host_->forget(pdk);  // ask again; the core's TrustStore keeps the decline until reopen_declined
  const ReviewPlan plan = review_plan(pdk, consent_host_->ask(pdk, *script));
  if (!plan.reopen) return;  // Don't run again: nothing changes
  if (plan.trust_command) run_line(*plan.trust_command);  // logged, as the PDK panel's Trust scripts
  reopen_declined_layout(pdk, *script);
}

void MainWindow::reopen_declined_layout(const script::PdkIdentity& pdk, const platform::Path& script) {
  // The PDK's script runs and the declined cells are read again with its
  // technology (cell::reopen_declined); messages go to the Diagnostics panel.
  class Report final : public diag::DiagnosticSink {
   public:
    explicit Report(cmd::Dispatcher& dispatcher) : dispatcher_(dispatcher) {}
    void emit(const diag::Diagnostic& d) override { dispatcher_.report(d); }

   private:
    cmd::Dispatcher& dispatcher_;
  };
  CoreSession& core = ctx_.core;
  Report sink(core.dispatcher());
  cmd::CommandContext context(core.services(), core.project(), core.dispatcher().session(), sink, core.registry(),
                              &core.editor());
  const cmd::CommandResult result = cell::reopen_declined(context, pdk, script);
  for (const diag::Diagnostic& d : result.diagnostics) core.dispatcher().report(d);
  if (result.ok()) {
    set_message(StatusKind::info, diagnostic_line(StatusKind::info, "Reopened with the " + pdk.variant + " scripts"));
  } else {
    consent_host_->restore_decline(pdk);
    set_message(StatusKind::error, diagnostic_line(StatusKind::error, gui_result_text(result)));
  }
  dirty_ = true;
  window_.request_redraw();
}

}  // namespace hefesto::shell
