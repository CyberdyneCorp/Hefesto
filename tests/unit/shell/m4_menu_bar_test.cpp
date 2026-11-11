// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop menu bar model (board M4-Menu-Bar; openspec/changes/
// m4-scripting tasks 10.1): Magic's wrapper menus with Magic's labels, the
// PDK items inserted through intercepted widget calls, the toolkit menus
// after a divider, and every item's logged command.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <unistd.h>

#include <filesystem>
#include <functional>
#include <memory>

#include "hefesto/app/project_file.hpp"
#include "hefesto/app/startup.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/wrapper.hpp"
#include "hefesto/platform/file_picker.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/shell/gui_script.hpp"
#include "hefesto/shell/menu_bar_model.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

const MenuBarMenu& menu(const MenuBarView& view, std::string_view title) {
  const auto found = std::find_if(view.menus.begin(), view.menus.end(),
                                  [title](const MenuBarMenu& m) { return m.title == title; });
  REQUIRE(found != view.menus.end());
  return *found;
}

const MenuBarItem* item(const MenuBarMenu& m, std::string_view label) {
  for (const MenuBarItem& i : m.items) {
    if (i.label == label) return &i;
  }
  return nullptr;
}

std::vector<std::string> labels(const MenuBarMenu& m) {
  std::vector<std::string> out;
  for (const MenuBarItem& i : m.items) out.push_back(i.kind == MenuBarItem::Kind::separator ? "-" : i.label);
  return out;
}

/// sky130's addtechmenu: two toolkit menus on the wrapper's layout frame.
pcell::ToolkitMenus sky130_toolkit() {
  using K = pcell::ToolkitItem::Kind;
  pcell::ToolkitMenus menus;
  menus.menus.push_back({".layout1.pane.top", "pdk1", "Devices 1",
                         {{K::command, "nmos (MOSFET)", "magic::gencell sky130::sky130_fd_pr__nfet_01v8"},
                          {K::command, "pmos (MOSFET)", "magic::gencell sky130::sky130_fd_pr__pfet_01v8"},
                          {K::separator, "", ""},
                          {K::command, "n-diode", "magic::gencell sky130::sky130_fd_pr__diode_pw2nd_05v5"}}});
  menus.menus.push_back({".layout1.pane.top", "pdk2", "Devices 2",
                         {{K::command, "poly resistor - 319.8 Ohm/sq",
                           "magic::gencell sky130::sky130_fd_pr__res_generic_po"}}});
  return menus;
}

/// What the interceptor leaves after sky130.tcl's `file.toolmenu insert 4`
/// and `drc.toolmenu add command` (PDKINT 6.5).
script::MenuModel sky130_edits() {
  script::MenuModel model;
  std::vector<script::MenuItem> file = script::magic_file_menu();
  for (auto& i : file) model.append("File", i);
  model.insert("File", 4, {script::MenuItem::Kind::command, "Import SPICE", "sky130::importspice", false});
  model.insert("File", 4, {script::MenuItem::Kind::separator, "", "", false});
  model.append("Drc", {script::MenuItem::Kind::command, "DRC Routing", "drc style drc(routing)", false});
  return model;
}

MenuBarInputs inputs_with(std::map<std::string, bool> options) {
  MenuBarInputs inputs;
  inputs.option = [options = std::move(options)](std::string_view v) -> std::optional<bool> {
    const auto found = options.find(std::string{v});
    if (found == options.end()) return std::nullopt;
    return found->second;
  };
  return inputs;
}

}  // namespace

TEST_CASE("menu bar items issue the wrapper's commands", "[cap:ui-gui-managers][req:Wrapper menus]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  std::vector<std::string> titles;
  for (const MenuBarMenu& m : view.menus) titles.push_back(m.title);
  CHECK(titles == std::vector<std::string>{"File", "Edit", "Cell", "Window", "Layers", "Drc", "Options"});
  CHECK(view.divider == 7);

  // Scenario "Menu dispatch": Window -> Zoom Box runs magic::findbox zoom.
  const MenuBarItem* zoom_box = item(menu(view, "Window"), "Zoom Box");
  REQUIRE(zoom_box != nullptr);
  CHECK(menu_item_script(*zoom_box) == "magic::findbox zoom");
  // Q-SCRIPT-36 (fix): Rotate 90 degree runs magic::clockwise 90.
  CHECK(item(menu(view, "Edit"), "Rotate 90 degree")->script == "magic::clockwise 90");
  // Magic's labels with double spaces collapsed.
  CHECK(item(menu(view, "Edit"), "Mirror Up/Down") != nullptr);
  CHECK(item(menu(view, "Cell"), "Up hierarchy")->script == "magic::popstack");
  CHECK(item(menu(view, "Layers"), "Unlock Base Layers")->script == "magic::tech unlock *");
  CHECK(item(menu(view, "Drc"), "DRC Fast")->script == "drc style drc(fast)");
  // Every item has a command and describes it (status bar, screen readers).
  for (const MenuBarMenu& m : view.menus) {
    for (const MenuBarItem& i : m.items) {
      if (i.kind == MenuBarItem::Kind::separator) continue;
      CHECK_FALSE(i.script.empty());
      CHECK(i.description() == i.label + " · " + i.script);
    }
  }
}

TEST_CASE("later-milestone menu items are present and disabled", "[cap:ui-gui-managers][req:Wrapper menus]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  const MenuBarItem* read_gds = item(menu(view, "File"), "Read GDS");
  REQUIRE(read_gds != nullptr);
  CHECK_FALSE(read_gds->enabled);
  CHECK(read_gds->tooltip == "Read GDS: arrives in M5");
  CHECK_FALSE(item(menu(view, "Options"), "Netlist Window")->enabled);
  CHECK(item(menu(view, "Options"), "3D Display")->tooltip == "3D Display: arrives in M10");
  CHECK(item(menu(view, "Options"), "Cell Manager")->enabled);
  CHECK(later_milestone("magic::flush") == std::nullopt);
}

TEST_CASE("menu bar check items follow and toggle their option variables",
          "[cap:ui-gui-managers][req:Wrapper menus]") {
  const MenuBarView view =
      menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, inputs_with({{"Opts(drcmgr)", true}}));
  const MenuBarItem* drc = item(menu(view, "Options"), "DRC Manager");
  REQUIRE(drc != nullptr);
  CHECK(drc->kind == MenuBarItem::Kind::check);
  CHECK(drc->checked);
  // Tk's checkbutton: the variable takes the toggled value, then the command runs.
  CHECK(menu_item_script(*drc) == "set ::Opts(drcmgr) 0 ; magic::drcmanager create");
  const MenuBarItem* cells = item(menu(view, "Options"), "Cell Manager");
  CHECK_FALSE(cells->checked);
  CHECK(menu_item_script(*cells) == "set ::Opts(cellmgr) 1 ; magic::cellmanager create");
}

TEST_CASE("accelerators are shown only for an exact key macro match", "[cap:ui-gui-managers][req:Wrapper menus]") {
  gui::WrapperMenuModel wrapper = gui::wrapper_menus(".layout1");
  // `z` is `zoom .5`, which does not equal Zoom In's `zoom 0.5` (board note 6).
  gui::apply_accelerators(wrapper, {{"delete", "d"}, {"zoom .5", "z"}, {"magic::redraw", "Control_L"}});
  const MenuBarView view = menu_bar_view(wrapper, {}, {}, {});
  CHECK(item(menu(view, "Edit"), "Delete")->accelerator == "d");
  CHECK(item(menu(view, "Window"), "Zoom In")->accelerator.empty());
  CHECK(item(menu(view, "Window"), "Redraw")->accelerator == "^L");
}

TEST_CASE("PDK widget edits land in the wrapper menus",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), sky130_edits(), {}, {});
  // Import SPICE between two separators after Flush changes (board note 7).
  CHECK(labels(menu(view, "File")) == std::vector<std::string>{"Open...", "Save...", "-", "Flush changes", "-",
                                                               "Import SPICE", "-", "Read GDS", "Write GDS", "-",
                                                               "Save All and Quit", "Quit"});
  const MenuBarItem* spice = item(menu(view, "File"), "Import SPICE");
  CHECK(spice->pdk);
  CHECK(spice->script == "sky130::importspice");
  // The native entries keep the wrapper's state (Read GDS still waits for M5).
  CHECK_FALSE(item(menu(view, "File"), "Read GDS")->enabled);
  CHECK_FALSE(item(menu(view, "File"), "Quit")->pdk);
  // DRC Routing appended after the wrapper's Drc items.
  const MenuBarMenu& drc = menu(view, "Drc");
  CHECK(drc.items.back().label == "DRC Routing");
  CHECK(drc.items.back().pdk);
  CHECK(item(drc, "DRC update") != nullptr);
}

TEST_CASE("toolkit menus follow the wrapper menus after a divider", "[cap:pcell-toolkit][req:Toolkit menu hooks]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, sky130_toolkit(), {});
  REQUIRE(view.menus.size() == 9);
  CHECK(view.divider == 7);
  CHECK(view.menus[7].title == "Devices 1");
  CHECK(view.menus[7].pdk);
  CHECK(view.menus[7].source == "Devices 1 · pdk1");
  CHECK(view.menus[8].title == "Devices 2");
  // Scenario "PDK device menu entry": the item runs the gencell command.
  const MenuBarItem* nmos = item(view.menus[7], "nmos (MOSFET)");
  REQUIRE(nmos != nullptr);
  CHECK(menu_item_script(*nmos) == "magic::gencell sky130::sky130_fd_pr__nfet_01v8");
  CHECK(labels(view.menus[7]) == std::vector<std::string>{"nmos (MOSFET)", "pmos (MOSFET)", "-", "n-diode"});
  // Another frame's menus are not shown when a frame is given.
  MenuBarInputs other;
  other.frame = ".layout2";
  CHECK(menu_bar_view(gui::wrapper_menus(".layout2"), {}, sky130_toolkit(), other).menus.size() == 7);
}

TEST_CASE("declined PDK scripts leave no PDK menus", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  MenuBarInputs declined;
  declined.scripts_declined = true;
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), sky130_edits(), sky130_toolkit(), declined);
  CHECK(view.menus.size() == 7);
  CHECK(item(menu(view, "File"), "Import SPICE") == nullptr);
  CHECK(item(menu(view, "Drc"), "DRC Routing") == nullptr);
}

TEST_CASE("menu bar signature changes with the menus", "[cap:ui-gui-managers][req:Wrapper menus]") {
  const auto plain = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  const auto pdk = menu_bar_view(gui::wrapper_menus(".layout1"), sky130_edits(), sky130_toolkit(), {});
  CHECK(menu_bar_signature(plain) == menu_bar_signature(menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {})));
  CHECK(menu_bar_signature(plain) != menu_bar_signature(pdk));
  const auto checked =
      menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, inputs_with({{"Opts(cellmgr)", true}}));
  CHECK(menu_bar_signature(plain) != menu_bar_signature(checked));
}

TEST_CASE("skipped widget statements become Diagnostics rows and one status message",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  const script::SkippedStatement a{"wm geometry .layout1 900x700", {"/pdk/libs.tech/magic/mymenus.tcl", 57}, {}};
  const script::SkippedStatement b{".layout1.titlebar.mbuttons.cell.toolmenu add command -label X -command y",
                                   {"/pdk/libs.tech/magic/mymenus.tcl", 42}, "::sky130::addtechmenu"};
  CHECK(skipped_statement_row(a) == "▲ SCRIPT-PDK-004 skipped widget statement mymenus.tcl:57\nwm geometry .layout1 900x700");
  // Board note 11: the procedure the statement ran in, as the board writes it.
  CHECK(skipped_statement_row(b) ==
        "▲ SCRIPT-PDK-004 skipped widget statement mymenus.tcl:42\n"
        ".layout1.titlebar.mbuttons.cell.toolmenu add command -label X -command y\n"
        "in sky130::addtechmenu · evaluation continued");
  CHECK(skipped_statements_message({a, b}) == "▲ 2 menu statements of mymenus.tcl were skipped");
  CHECK(skipped_statements_message({a}) == "▲ 1 menu statement of mymenus.tcl was skipped");
  CHECK(skipped_statements_message({}).empty());
  CHECK(collapse_spaces("Lock   Cell") == "Lock Cell");
}

// ---- the live menus of a real PDK startup ----------------------------------------------

namespace {

/// The sky130A reference version (tests/unit/common/reference_data.hpp
/// kPdkVersion; that header is not on the shell tests' include path).
constexpr std::string_view kPdkVersion = "5740c4a81c42f9fe4af5ef2ee258a99151053d28";

/// True when scripts/fetch_pdk.py put the sky130A reference PDK in .cache/pdks.
bool have_sky130a() {
  const std::filesystem::path variant = std::filesystem::path{HEFESTO_SOURCE_DIR} / ".cache/pdks/ciel/sky130/versions" /
                                        std::string{kPdkVersion} / "sky130A";
  std::error_code ec;
  return std::filesystem::exists(variant / "libs.tech/magic/sky130A.magicrc", ec) &&
         std::filesystem::exists(variant / "libs.tech/magic/sky130A.tech", ec);
}

/// What the script layer reports during the startup and the adoption.
class Skips final : public script::ScriptObserver {
 public:
  void on_skipped_statement(const script::SkippedStatement& s) override { seen.push_back(s); }
  std::vector<script::SkippedStatement> seen;
};

/// A desktop session as hefesto's window opens one (shell.cpp run_startup):
/// a project pinned to the sky130A reference version, `app::run_startup`
/// with the PDK store at .cache/pdks, then the window becomes the wrapper
/// frame `.layout1` (gui::adopt_wrapper) and the bar is session_menu_bar.
struct DesktopStartup {
  platform::PortableServices services{platform::PortableOptions{{}, {}, false, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script_session{project};
  cmd::CommandRegistry registry;
  platform::ScriptedFilePicker picker;  // outlives the editor, which points at it
  Skips skips;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  std::filesystem::path dir = std::filesystem::temp_directory_path() /
                              ("hefesto-shell-menus-" + std::to_string(::getpid()));
  platform::LocalFileSystem fs;
  app::StartupEnvironment env;
  diag::CollectingSink sink;
  std::function<cmd::CommandResult(cmd::CommandContext&)> probe;

  DesktopStartup() {
    REQUIRE(cmd::register_all_commands(registry));
    REQUIRE_FALSE(registry
                      .add({"shell_test_probe", "", "",
                            [this](cmd::CommandContext& ctx, const cmd::Invocation&) { return probe(ctx); }})
                      .has_value());
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script_session);
    dispatcher->set_editor(&editor);
    editor.file_picker = &picker;
    editor.script.events.add(skips);
    std::filesystem::remove_all(dir);
    env.home = dir / "home";
    env.working_directory = dir / "work";
    std::filesystem::create_directories(env.home);
    std::filesystem::create_directories(env.working_directory);
    env.pdk_root = std::filesystem::path{HEFESTO_SOURCE_DIR} / ".cache" / "pdks";
  }
  ~DesktopStartup() {
    editor.script.events.remove(skips);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }
  DesktopStartup(const DesktopStartup&) = delete;
  DesktopStartup& operator=(const DesktopStartup&) = delete;

  /// Opens the pinned project with its PDK scripts trusted, then adopts the window.
  void open_trusted_project() {
    app::ProjectFile file;
    file.pdk = app::PdkPin{"sky130", "sky130A", std::string{kPdkVersion}, {}};
    const auto folder = dir / "proj";
    std::filesystem::create_directories(folder);
    REQUIRE(app::save_project(fs, folder, file).ok());
    env.project_folder = folder;
    editor.script.pdk_trust.trust(script::PdkIdentity{"sky130", "sky130A", std::string{kPdkVersion}});
    const auto started = app::run_startup(*dispatcher, project, fs, {}, env, sink);
    for (const auto& d : sink.snapshot()) UNSCOPED_INFO(d.code << " " << d.text);
    REQUIRE(started.ok());
    REQUIRE(editor.script.interp != nullptr);
    REQUIRE(editor.technology->name() == "sky130A");
    REQUIRE(run("openwindow").ok());  // the desktop's layout window
    probe = [](cmd::CommandContext& ctx) { return gui::adopt_wrapper(ctx, ".layout1"); };
    REQUIRE(run("shell_test_probe").ok());
  }

  cmd::CommandResult run(std::string_view line) {
    return dispatcher->execute(cmd::Invocation{std::string{line}, {}, cmd::Origin::gui, std::nullopt});
  }
};

std::vector<std::string> menu_titles(const MenuBarView& view) {
  std::vector<std::string> out;
  for (const MenuBarMenu& m : view.menus) out.push_back(m.title);
  return out;
}

}  // namespace

TEST_CASE("the trusted sky130A startup puts its device menus on the menu bar",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  if (!have_sky130a()) {
    SKIP("the sky130A reference PDK is not in .cache/pdks (scripts/fetch_pdk.py)");
  }
  DesktopStartup s;
  s.open_trusted_project();
  // Scenario "sky130 menus in a new window", as the desktop's bar shows it.
  const MenuBarView view = session_menu_bar(s.editor, false);
  CHECK(menu_titles(view) == std::vector<std::string>{"File", "Edit", "Cell", "Window", "Layers", "Drc", "Options",
                                                 "Devices 1", "Devices 2"});
  REQUIRE(view.divider == 7);
  const MenuBarMenu& devices1 = menu(view, "Devices 1");
  const MenuBarMenu& devices2 = menu(view, "Devices 2");
  CHECK(devices1.pdk);
  CHECK(devices2.pdk);
  CHECK(devices1.source == "Devices 1 · pdk1");
  const MenuBarItem* nmos = item(devices1, "nmos (MOSFET)");
  REQUIRE(nmos != nullptr);
  // toolkit.tcl's add_toolkit_command: `magic::gencell <lib>::<device> <args>` (no args: a trailing blank).
  CHECK(nmos->script == "magic::gencell sky130::sky130_fd_pr__nfet_01v8 ");
  CHECK(devices2.items.size() > 5);
  // The intercepted wrapper-menu items: DRC Routing last in DRC, Import
  // SPICE between two separators after Flush changes in File.
  const MenuBarMenu& drc = menu(view, "Drc");
  REQUIRE_FALSE(drc.items.empty());
  CHECK(drc.items.back().label == "DRC Routing");
  CHECK(drc.items.back().pdk);
  CHECK(drc.items.back().script == "drc style drc(routing)");
  const auto file = labels(menu(view, "File"));
  const auto flush = std::find(file.begin(), file.end(), "Flush changes");
  REQUIRE(std::distance(flush, file.end()) >= 5);
  CHECK(std::vector<std::string>(flush, flush + 5) ==
        std::vector<std::string>{"Flush changes", "-", "Import SPICE", "-", "Read GDS"});
  // No skipped-statement diagnostic.
  CHECK(s.skips.seen.empty());
  for (const auto& d : s.sink.snapshot()) CHECK(d.code != "SCRIPT-PDK-004");

  // Choosing the items runs them in the project's interpreter as the window
  // does (origin `script`, S-01): Import SPICE asks the native picker, nmos
  // opens the parameter dialog.
  const MenuBarItem* spice = item(menu(view, "File"), "Import SPICE");
  REQUIRE(spice != nullptr);
  s.picker.push_answer(std::nullopt);  // cancelled
  CHECK(run_gui_script(s.editor, *s.dispatcher, menu_item_script(*spice), cmd::Origin::script).ok());
  CHECK(s.picker.requests().size() == 1);
  const auto opened = run_gui_script(s.editor, *s.dispatcher, menu_item_script(*nmos), cmd::Origin::script);
  INFO(gui_result_text(opened));
  CHECK(opened.ok());
  REQUIRE(s.editor.script.gencell.dialog.has_value());
  CHECK(s.editor.script.gencell.dialog->device == "sky130_fd_pr__nfet_01v8");
}

TEST_CASE("declined sky130A scripts leave the bar without device menus",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  if (!have_sky130a()) {
    SKIP("the sky130A reference PDK is not in .cache/pdks (scripts/fetch_pdk.py)");
  }
  DesktopStartup s;
  app::ProjectFile file;
  file.pdk = app::PdkPin{"sky130", "sky130A", std::string{kPdkVersion}, {}};
  const auto folder = s.dir / "proj";
  std::filesystem::create_directories(folder);
  REQUIRE(app::save_project(s.fs, folder, file).ok());
  s.env.project_folder = folder;
  // No consent host: the untrusted version is declined (the batch default).
  REQUIRE(app::run_startup(*s.dispatcher, s.project, s.fs, {}, s.env, s.sink).ok());
  REQUIRE(s.run("openwindow").ok());
  s.probe = [](cmd::CommandContext& ctx) { return gui::adopt_wrapper(ctx, ".layout1"); };
  REQUIRE(s.run("shell_test_probe").ok());
  const MenuBarView view = session_menu_bar(s.editor, true);
  CHECK(view.divider == view.menus.size());
  CHECK(menu_titles(view) == std::vector<std::string>{"File", "Edit", "Cell", "Window", "Layers", "Drc", "Options"});
  CHECK(s.editor.script.gencell.menus.menus.empty());  // the PDK script never ran
}
