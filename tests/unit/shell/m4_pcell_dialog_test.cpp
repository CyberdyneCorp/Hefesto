// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Place device and the PCell parameter dialog (board M4-PCell-Dialog;
// openspec/changes/m4-scripting tasks 10.2): the popover of the PDK's
// device menus, the dialog view of pcell::DialogModel, and the scripts its
// edits and buttons run. The end-to-end cases run the test PDK library
// `tpdk` (tests/data/pcell/framework/tpdk.tcl) in a bound project
// interpreter, as the desktop does.

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <filesystem>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/shell/gui_script.hpp"
#include "hefesto/shell/pcell_dialog_model.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

const std::string kData = std::string{HEFESTO_SOURCE_DIR} + "/tests/data/pcell/framework";

pcell::ToolkitMenus sky130_menus() {
  using K = pcell::ToolkitItem::Kind;
  pcell::ToolkitMenus menus;
  menus.menus.push_back({".layout1.pane.top", "pdk1", "Devices 1",
                         {{K::command, "nmos (MOSFET)", "magic::gencell sky130::sky130_fd_pr__nfet_01v8"},
                          {K::command, "pmos (MOSFET)", "magic::gencell sky130::sky130_fd_pr__pfet_01v8"},
                          {K::separator, "", ""},
                          {K::command, "n-diode", "magic::gencell sky130::sky130_fd_pr__diode_pw2nd_05v5"}}});
  menus.menus.push_back({".layout1.pane.top", "pdk2", "Devices 2",
                         {{K::command, "vpp 11.5x11.7 m1-m2", "sky130::sky130_fd_pr__cap_vpp_11p5x11p7_m1m2_draw"}}});
  return menus;
}

pcell::DialogModel nfet_model() {
  pcell::DialogModel m;
  m.device = "sky130_fd_pr__nfet_01v8";
  m.library = "sky130";
  m.cell_name = "(default)";
  m.instance = "(default)";
  m.widgets = {
      {pcell::WidgetKind::entry, "w", "Width (um)", "0.420", {}, "blue", {"magic::update_dialog {} w x sky130"}},
      {pcell::WidgetKind::entry, "l", "Length (um)", "0.150", {}, "blue", {}},
      {pcell::WidgetKind::selectlist, "gencell", "Device type", "sky130_fd_pr__nfet_01v8",
       {"sky130_fd_pr__nfet_01v8", "sky130_fd_pr__nfet_g5v0d10v5"}, "blue", {}},
      {pcell::WidgetKind::checkbox, "poverlap", "Overlap at poly contact", "0", {}, "blue", {}},
      {pcell::WidgetKind::checkbox, "doverlap", "Overlap at diffusion contact", "1", {}, "blue", {}},
      {pcell::WidgetKind::message, "info", "Info", "brown text", {}, "brown", {}},
  };
  m.buttons = {{"Create", "x", false}, {"Create and Close", "x", true},
               {"Reset", "magic::gencell_dialog {} sky130_fd_pr__nfet_01v8 sky130 {}", false}, {"Close", "", true}};
  return m;
}

const PCellRow& row(const PCellDialogView& view, std::string_view parameter) {
  for (const PCellRow& r : view.rows) {
    if (r.parameter == parameter) return r;
  }
  FAIL("no row " << parameter);
  return view.rows.front();
}

/// A fresh current directory (generated cells are written there).
class TempDirectory {
 public:
  TempDirectory()
      : previous_(std::filesystem::current_path()),
        dir_(std::filesystem::temp_directory_path() / ("hefesto-shell-pcell-" + std::to_string(::getpid()))) {
    std::filesystem::remove_all(dir_);
    std::filesystem::create_directories(dir_);
    std::filesystem::current_path(dir_);
  }
  ~TempDirectory() {
    std::error_code ec;
    std::filesystem::current_path(previous_, ec);
    std::filesystem::remove_all(dir_, ec);
  }
  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return dir_; }

 private:
  std::filesystem::path previous_;
  std::filesystem::path dir_;
};

/// The desktop's script path on a headless session: every command bound in
/// the project interpreter (BINDING's bind_registry), the test technology
/// and the test PDK library.
struct DesktopSession {
  TempDirectory temp;
  platform::PortableServices services{
      platform::PortableOptions{std::filesystem::temp_directory_path() / "hefesto-shell-pcell", {}, false, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession session{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  cmd::Dispatcher dispatcher{registry, services.view(), project, session};

  DesktopSession() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(&editor);
    script::SandboxPolicy policy;
    policy.granted_roots = {kData, temp.path()};
    (void)script::open_project_interpreter(editor, policy);
    REQUIRE(script::bind_registry(*editor.script.interp, registry, dispatcher).ok());
    REQUIRE(gui("tech load " + kData + "/pcell.tech").ok());
    REQUIRE(gui("load top").ok());
    (void)editor.windows.open_frame(editor.edit_cell);
    REQUIRE(gui("box values 0 0 0 0").ok());
    REQUIRE(gui("source " + kData + "/tpdk.tcl").ok());
  }
  ~DesktopSession() { script::close_project_interpreter(editor); }
  DesktopSession(const DesktopSession&) = delete;
  DesktopSession& operator=(const DesktopSession&) = delete;

  cmd::CommandResult gui(const std::string& script) {
    const cmd::CommandResult r = run_gui_script(editor, dispatcher, script);
    INFO(script << " -> " << gui_result_text(r));
    return r;
  }
  pcell::DialogModel& dialog() {
    REQUIRE(editor.script.gencell.dialog.has_value());
    return *editor.script.gencell.dialog;
  }
};

}  // namespace

TEST_CASE("Place device lists the PDK's device menus",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  const PlaceDeviceView view = place_device_view(sky130_menus(), "sky130A", false);
  CHECK(view.enabled);
  CHECK(view.title == "Place device · sky130A");
  REQUIRE(view.entries.size() == 6);
  CHECK(view.entries[0] == PlaceDeviceEntry{"Devices 1", {}, true, false, false});
  CHECK(view.entries[1].script == "magic::gencell sky130::sky130_fd_pr__nfet_01v8");
  CHECK_FALSE(view.entries[1].draws);
  CHECK(view.entries[3].label == "n-diode");  // separators are not listed
  CHECK(view.entries[4].heading);
  CHECK(view.entries[5].draws);  // a *_draw item paints at the box, no dialog
  // The filter keeps matching items under their menu.
  const PlaceDeviceView filtered = place_device_view(sky130_menus(), "sky130A", false, "MOS");
  REQUIRE(filtered.entries.size() == 3);
  CHECK(filtered.entries[2].label == "pmos (MOSFET)");
  // The last device used is listed last.
  const PlaceDeviceView recent =
      place_device_view(sky130_menus(), "sky130A", false, {}, "magic::gencell sky130::sky130_fd_pr__pfet_01v8");
  CHECK(recent.entries.back() == PlaceDeviceEntry{"Recent: pmos (MOSFET)",
                                                  "magic::gencell sky130::sky130_fd_pr__pfet_01v8", false, false, true});
}

TEST_CASE("Place device is disabled without PDK device scripts",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  const PlaceDeviceView none = place_device_view({}, "minimum", false);
  CHECK_FALSE(none.enabled);
  CHECK(none.tooltip == "No PDK device scripts");
  CHECK(none.entries.empty());
  const PlaceDeviceView declined = place_device_view(sky130_menus(), "minimum", true);
  CHECK_FALSE(declined.enabled);
  CHECK(declined.tooltip == "No PDK device scripts (the PDK scripts were not run)");
}

TEST_CASE("the dialog view renders DialogModel", "[cap:pcell-toolkit][req:Parameter dialog structure]") {
  const PCellDialogView view = pcell_dialog_view(nfet_model(), true);
  CHECK(view.mode == "New device:");
  CHECK(view.device == "sky130_fd_pr__nfet_01v8");
  CHECK(view.library == "sky130");
  CHECK(view.cell_name == "(default)");
  CHECK(view.instance == "(default)");
  CHECK(view.generator.label() == "● native generator");
  CHECK(pcell_dialog_view(nfet_model(), false).generator.label() == "◆ Tcl generator");
  REQUIRE(view.rows.size() == 6);
  CHECK(row(view, "gencell").choice == 0U);
  CHECK_FALSE(row(view, "poverlap").checked);
  CHECK(row(view, "doverlap").checked);
  CHECK(row(view, "info").color == "brown");
  // New device buttons: Reset, Close, Create, Create and Close (default).
  std::vector<std::string> labels;
  for (const PCellButton& b : view.buttons) labels.push_back(b.label + (b.primary ? "*" : ""));
  CHECK(labels == std::vector<std::string>{"Reset", "Close", "Create", "Create and Close*"});
  pcell::DialogModel edit = nfet_model();
  edit.mode = pcell::DialogMode::edit_device;
  labels.clear();
  for (const PCellButton& b : pcell_dialog_view(edit, true).buttons) labels.push_back(b.label + (b.primary ? "*" : ""));
  CHECK(labels == std::vector<std::string>{"Reset", "Close", "Apply", "Okay*"});
  // _check messages: a ▲ banner (shape and text).
  pcell::DialogModel invalid = nfet_model();
  invalid.messages = {"Mos width must be >= 0.42 um"};
  CHECK(pcell_dialog_view(invalid, true).banner == "▲ Mos width must be >= 0.42 um");
}

TEST_CASE("dialog edits and buttons are Tcl commands", "[cap:pcell-toolkit][req:Dialog builder API]") {
  const pcell::DialogModel m = nfet_model();
  CHECK(pcell_value_script(m.widgets[0], "1.0") == "set ::magic::w_val 1.0 ; magic::update_dialog {} w x sky130");
  CHECK(pcell_value_script(m.widgets[1], "a b") == "set ::magic::l_val {a b}");
  // Magic's `gencell <lib>::<type> {} k v` opens the dialog, so an unnamed
  // new device is created by gencell_create with the whole dictionary ...
  CHECK(pcell_gencell_command(m) ==
        "magic::gencell_create sky130_fd_pr__nfet_01v8 sky130 {w 0.420 l 0.150 gencell sky130_fd_pr__nfet_01v8 "
        "poverlap 0 doverlap 1}");
  // ... and a named one by gencell's create-with-name form.
  pcell::DialogModel named = m;
  named.instance = "xM2";
  CHECK(pcell_gencell_command(named) ==
        "magic::gencell sky130::sky130_fd_pr__nfet_01v8 xM2 w 0.420 l 0.150 gencell sky130_fd_pr__nfet_01v8 "
        "poverlap 0 doverlap 1");
  CHECK(pcell_button_script(m, PCellButtonRole::create) ==
        "magic::gencell_dialog [" + pcell_gencell_command(m) + "] sky130_fd_pr__nfet_01v8 sky130 {}");
  CHECK(pcell_button_script(m, PCellButtonRole::create_and_close) == pcell_gencell_command(m));
  CHECK(pcell_button_script(m, PCellButtonRole::reset) == "magic::gencell_dialog {} sky130_fd_pr__nfet_01v8 sky130 {}");
  CHECK(pcell_button_script(m, PCellButtonRole::close).empty());
  pcell::DialogModel edit = m;
  edit.mode = pcell::DialogMode::edit_device;
  edit.instance = "xM1";
  CHECK(pcell_button_script(edit, PCellButtonRole::apply).starts_with("magic::gencell sky130::sky130_fd_pr__nfet_01v8 xM1 w "));
  CHECK(tcl_true("1"));
  CHECK(tcl_true("true"));
  CHECK_FALSE(tcl_true("0"));
  CHECK_FALSE(tcl_true(""));
}

TEST_CASE("PCell dialog", "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DesktopSession s;
  // Choosing a device-menu item opens the new-device dialog.
  REQUIRE(s.gui("magic::gencell tpdk::res").ok());
  PCellEditState edits;
  PCellDialogView view = pcell_dialog_view(s.dialog(), false, edits);
  CHECK(view.mode == "New device:");
  CHECK(view.device == "res");
  // The generator's parameters with their defaults ...
  CHECK(row(view, "w").value == "1");
  CHECK(row(view, "l").value == "2");
  CHECK(row(view, "nf").value == "1");
  CHECK(row(view, "gencell").choice == 0U);
  // ... and their checks: a width below 1 is corrected by tpdk::res_check.
  pcell::DialogWidget* w = s.dialog().find("w");
  REQUIRE(w != nullptr);
  edits.begin_edit(s.dialog(), "w", "0.5");
  REQUIRE(s.gui(pcell_value_script(*w, "0.5")).ok());
  view = pcell_dialog_view(s.dialog(), false, edits);
  CHECK(row(view, "w").value == "1");
  CHECK(row(view, "w").corrected);
  CHECK(row(view, "w").typed == "0.5");
  // A dependency: nf changes l (tpdk::res_dep), announced politely.
  edits.begin_edit(s.dialog(), "nf", "3");
  REQUIRE(s.gui(pcell_value_script(*s.dialog().find("nf"), "3")).ok());
  view = pcell_dialog_view(s.dialog(), false, edits);
  CHECK(row(view, "l").value == "6");
  CHECK(row(view, "l").updated);
  CHECK(view.announcement == "Length (um) updated to 6");
  // The corrected width keeps its warning outline through another field's
  // edit, and loses it once the user types into it again (draft 12).
  CHECK(row(view, "w").corrected);
  CHECK(edits.begin_typing("w"));
  CHECK_FALSE(edits.begin_typing("w"));
  view = pcell_dialog_view(s.dialog(), false, edits);
  CHECK_FALSE(row(view, "w").corrected);
  // Create places the generated cell and keeps the dialog on the new instance.
  const std::string command = pcell_gencell_command(s.dialog());
  CHECK(command == "magic::gencell_create res tpdk {w 1 l 6 gencell res guard {} nf 3}");
  REQUIRE(s.gui(pcell_button_script(s.dialog(), PCellButtonRole::create)).ok());
  view = pcell_dialog_view(s.dialog(), false);
  CHECK(view.mode == "Edit device:");
  CHECK(view.cell_name != "(default)");
  CHECK(s.editor.cells.find(view.cell_name) != nullptr);
  const std::string instance = view.instance;
  CHECK_FALSE(instance.empty());
  // Apply changes that instance through the same logged command.
  REQUIRE(s.gui(pcell_value_script(*s.dialog().find("w"), "2")).ok());
  CHECK(pcell_button_script(s.dialog(), PCellButtonRole::apply)
            .starts_with("magic::gencell tpdk::res " + instance + " w 2 "));
  REQUIRE(s.gui(pcell_button_script(s.dialog(), PCellButtonRole::okay)).ok());
}

TEST_CASE("a failed gencell opens no dialog", "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DesktopSession s;
  // Ctrl-P with no generated cell selected (board tile 9): an error, no dialog.
  const cmd::CommandResult r = s.gui("magic::gencell {}");
  CHECK_FALSE(s.editor.script.gencell.dialog.has_value());
  if (!r.ok()) CHECK_FALSE(gui_result_text(r).empty());
}

TEST_CASE("Create with an instance name places that instance",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DesktopSession s;
  REQUIRE(s.gui("magic::gencell tpdk::res").ok());
  s.dialog().instance = "R7";  // typed in the dialog's Instance entry
  const std::string command = pcell_button_script(s.dialog(), PCellButtonRole::create_and_close);
  CHECK(command == "magic::gencell tpdk::res R7 w 1 l 2 gencell res guard {} nf 1");
  REQUIRE(s.gui(command).ok());
  const cmd::CommandResult cell = s.gui("instance list celldef R7");
  REQUIRE(cell.ok());
  CHECK_FALSE(gui_result_text(cell).empty());
}
