// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M4 managers and the Tcl console in the main window
// (openspec/changes/m4-scripting/design.md D13; tasks 10.4, 10.5; boards
// M4-Managers and M4-Console): the Cells and Libraries tabs of the left
// dock, the technology manager in the right dock, the text helper and
// reorder windows, the Console tab as the project's Tcl console shared with
// the command bar, the Interrupt card, and the native FilePicker host of
// `promptload` / `promptsave`. The Tcl procedures Magic's manager scripts
// define and the wrapper menus call (`magic::update_texthelper`,
// `magic::mgrselect`, `magic::reorderToolbar`, `addcommandentry`,
// `deletecommandentry`) are defined in the project's interpreter here.
// Raven-free: built from the raven_ui wrappers.
//
// Owner: SHELL (SHELL-B).

#include <utility>

#include "hefesto/gui/toolbar_presets.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/shell/gui_script.hpp"
#include "hefesto/shell/script_consent_model.hpp"
#include "main_window.hpp"
#include "manager_panels.hpp"

namespace hefesto::shell {

namespace {

ui::FileDialogOptions dialog_options(const NativeFileDialogRequest& request) {
  ui::FileDialogOptions options;
  options.save = request.save;
  options.title = request.title;
  options.directory = request.directory;
  options.file_name = request.file_name;
  options.default_extension = request.default_extension;
  for (const NativeFileFilter& f : request.filters) options.filters.push_back(ui::FileDialogFilter{f.name, f.patterns});
  return options;
}

constexpr float kTclConsoleHeight = 280.0F;

cmd::CommandResult ok() {
  return cmd::CommandResult::success();
}

}  // namespace

void MainWindow::build_m4b_console() {
  console_session_ = std::make_unique<TclConsoleSession>(ctx_.core.editor(), ctx_.core.dispatcher());
  tcl_console_ = std::make_unique<TclConsole>(
      *console_session_, [this]() -> const std::vector<std::string>& { return command_model_.history(); });
  console_session_->set_transcript(&tcl_console_->transcript());
  console_panel_ = std::make_unique<ConsolePanel>(ctx_, *tcl_console_, [this]() {
    return ui::file_dialog(window_.id(), ui::FileDialogOptions{true, "Save the console transcript", {}, "console.txt",
                                                               {{"Text files", "*.txt"}, {"All files", "*"}}, "txt"});
  });
  // The run draws frames between its commands so the Interrupt card, Ctrl-C
  // and the canvas stay live (Rdraw_frame from the key event that started it).
  // Input that would run a command waits until the script ends (board
  // M4-Console note 8): ScriptInputBlock while the frame is drawn.
  console_session_->set_pump([this] {
    const ScriptInputBlock no_input;
    console_panel_->refresh();
    show_script_progress();
    ui::pump_frame();
    if (!card_capture_.empty() && console_panel_->card_shown()) {
      card_captured_ = capture(card_capture_);
      card_capture_.clear();
    }
  });
  // A script that is not the console's (menu callbacks, PDK files) gets the
  // same card from its progress report (board M4-Script-Consent tile 7).
  console_panel_->set_script_card(
      [this]() -> std::optional<InterruptCardView> {
        const auto& progress = console_session_->progress();
        if (!progress || progress->done || console_session_->evaluating()) return std::nullopt;
        return interrupt_card(running_script(*progress));
      },
      [this] { console_session_->interrupt(); });
  // The left dock's tabs (board: Hierarchy becomes Cells, new Libraries tab)
  // and the technology manager of the right dock.
  managers_ = std::make_unique<ManagerPanels>(ctx_);
  left_tabs_.add(left_->widget(), "Layers");
  left_tabs_.add(managers_->cells_widget(), "Cells");
  left_tabs_.add(managers_->libraries_widget(), "Libraries");
  left_tabs_.set_size(static_cast<float>(kLeftPanelWidth), -1.0F);
  left_->set_hierarchy_visible(false);
}

ui::Widget& MainWindow::left_dock() {
  return managers_ ? static_cast<ui::Widget&>(left_tabs_) : left_->widget();
}

void MainWindow::build_m4b() {
  text_helper_ = std::make_unique<TextHelperWindow>(ctx_);
  reorder_ = std::make_unique<ReorderWindow>(ctx_, [this](std::vector<std::string> order) {
    left_->set_toolbar_order(std::move(order));
    dirty_ = true;  // the palette is rebuilt on the next frame
    window_.request_redraw();
  });
  // The Tcl console needs room for its transcript (board: the bottom dock, expanded).
  console_tabs_.set_size(-1.0F, kTclConsoleHeight);
  canvas_->add_overlay(console_panel_->card_widget());
  // Board M4-Script-Consent tile 7: the Interrupt card at the canvas's top centre.
  console_panel_->card_widget().place_top_center(static_cast<float>(kRulerSize + 12));
  // promptload / promptsave and intercepted tk_getOpenFile ask the native dialog.
  file_picker_ = std::make_unique<DesktopFilePicker>(
      [this](const NativeFileDialogRequest& request) { return ui::file_dialog(window_.id(), dialog_options(request)); });
  ctx_.core.editor().file_picker = file_picker_.get();
  if (script::Interpreter* interp = ctx_.core.editor().script.interp.get()) attach_interpreter(*interp);
}

void MainWindow::teardown_m4b() {
  if (ctx_.core.editor().file_picker == file_picker_.get()) ctx_.core.editor().file_picker = nullptr;
  if (text_helper_) text_helper_->close();
  if (reorder_) reorder_->close();
  if (console_session_) console_session_->set_pump({});
}

void MainWindow::attach_interpreter(script::Interpreter& interp) {
  if (console_session_->attached()) return;
  (void)console_session_->attach(interp);
  tcl_console_->transcript().add(ConsoleLineKind::banner, "Tcl 8.6.16 · Hefesto sandbox · one interpreter per project");
  // Magic's manager procedures, answered by the shell's screens.
  interp.define_command("::magic::update_texthelper", [this](const cmd::Invocation&) {
    // The `select` tag only refreshes an open helper; the Edit menu opens it.
    if (select_tag_pending_) {
      select_tag_pending_ = false;
      text_helper_->update();
    } else {
      text_helper_->show();
    }
    return ok();
  });
  interp.define_command("::magic::mgrselect", [](const cmd::Invocation&) { return ok(); });  // ManagerPanels
  interp.define_command("::magic::reorderToolbar", [this](const cmd::Invocation&) {
    open_reorder();
    return ok();
  });
  interp.define_command("::addcommandentry", [this](const cmd::Invocation& inv) {
    tcl_console_->set_target_window(inv.args.empty() ? std::string{} : inv.args.front());
    if (!console_visible_) toggle_console();
    console_tabs_.show(0);
    console_panel_->focus();
    return ok();
  });
  interp.define_command("::deletecommandentry", [this](const cmd::Invocation&) {
    tcl_console_->set_target_window({});
    return ok();
  });
}

void MainWindow::open_reorder() {
  gui::ToolbarModel toolbar;
  with_command_context(ctx_.core, [&](cmd::CommandContext& c) { toolbar = gui::build_toolbar(c, {}); });
  reorder_->show(toolbar);
}

bool MainWindow::m4b_shortcut(int key, std::uint8_t modifiers) {
  return console_panel_ && console_panel_->on_key(key, modifiers);
}

bool MainWindow::run_console_line(const std::string& line) {
  if (!console_session_ || !console_session_->attached() || console_session_->interpreter() == nullptr) return false;
  // The command bar's line runs in the console's interpreter; its transcript shows there.
  if (tcl_console_->running()) {
    (void)tcl_console_->run(line);  // queued until the running command ends
    return true;
  }
  in_command_ = true;
  command_problem_.clear();
  const auto evaluation = tcl_console_->run(line);
  in_command_ = false;
  if (evaluation) {
    const CommandOutcome outcome = TclConsoleSession::outcome_of(*evaluation);
    const StatusKind kind = outcome.ok() ? StatusKind::success : StatusKind::error;
    set_message(kind, diagnostic_line(kind, result_line(outcome.message).text()));
    if (!outcome.ok()) announce(kind, message_);
  }
  console_panel_->refresh();
  dirty_ = true;
  window_.request_redraw();
  if (ctx_.core.quit_requested()) ctx_.runtime.quit();
  return true;
}

void MainWindow::note_m4b_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  select_tag_pending_ = invocation.name == "select";
  if (managers_) {
    managers_->note_command(invocation, result);
    // Options › Cell / Library / Tech Manager (`<manager> create`): show it.
    const bool create = !invocation.args.empty() && invocation.args.front() == "create";
    if (create && invocation.name == "cellmanager") left_tabs_.show(1);
    if (create && invocation.name == "libmanager") left_tabs_.show(2);
    if (create && invocation.name == "techmanager") {
      tech_shown_ = !tech_shown_;
      update_right_dock();
    }
  }
  apply_drc_manager_request(invocation);
  if (console_session_) console_session_->on_command();
}

void MainWindow::apply_drc_manager_request(const cmd::Invocation& invocation) {
  // magic::drcmanager create / Options › DRC Manager: the DRC panel is the
  // DRC manager (M3 boards), shown and focused, or hidden when unchecked.
  std::optional<std::string> opts;
  if (const script::Interpreter* interp = ctx_.core.editor().script.interp.get()) {
    if (const auto value = interp->get_var("Opts(drcmgr)")) opts = value->to_string();
  }
  const std::optional<std::string_view> opts_view = opts ? std::optional<std::string_view>{*opts} : std::nullopt;
  switch (drc_manager_request(invocation.name, invocation.args, opts_view)) {
    case DrcManagerRequest::show:
      (void)panel_command({"drc", "show"});
      drc_panel_->focus();
      break;
    case DrcManagerRequest::hide:
      (void)panel_command({"drc", "hide"});
      break;
    case DrcManagerRequest::none:
      break;
  }
}

void MainWindow::refresh_m4b() {
  if (managers_) managers_->refresh();
  // Lines typed while another script ran (ConsoleEvaluator::busy) run once it ended.
  if (tcl_console_ && !ScriptInputBlock::active() && tcl_console_->run_held() > 0) {
    dirty_ = true;
    window_.request_redraw();
  }
  if (console_panel_) {
    console_panel_->refresh();
    canvas_->set_dim(script_dim(console_panel_->card_shown(), canvas_overlay_tokens(ctx_.mode)));
  }
}

void MainWindow::show_script_progress() {
  canvas_->set_dim(script_dim(console_panel_->card_shown(), canvas_overlay_tokens(ctx_.mode)));
  // The edits so far (board tile 7 shows the layout under the card). Only
  // the canvas and the header: the panels rebuild their widgets, which the
  // callback that started the script may own, so they wait for on_frame
  // (dirty_ stays set).
  if (dirty_ || canvas_dirty_) {
    canvas_->update_scene();
    canvas_->refresh_highlights();
    refresh_header();
  }
}

}  // namespace hefesto::shell
