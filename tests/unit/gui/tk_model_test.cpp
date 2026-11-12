// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-cli's headless widget model (tk_model.hpp; m4-scripting design D16:
// GUI cases replay through the view models and hefesto-cli).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "gui_fixture.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/gui/tk_model.hpp"
#include "hefesto/gui/wrapper.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/binding/window_commands.hpp"

namespace gui = hefesto::gui;
namespace db = hefesto::db;
namespace platform = hefesto::platform;
using gui::test::Session;

namespace {

// A session in wrapper mode: the model installed, then `.layout1` opened.
Session& wrapped(Session& s) {
  REQUIRE(hefesto::script::bind_registry(s.interp(), s.registry, *s.dispatcher).ok());
  const auto installed = gui::install_tk_model(s.interp());
  INFO((installed.diagnostics.empty() ? std::string{} : installed.diagnostics.front().text));
  REQUIRE(installed.ok());
  s.interp().define_command("::hefesto::tk::window_command", [&s](const hefesto::cmd::Invocation& inv) {
    return hefesto::script::register_window_commands(s.interp(), *s.dispatcher, inv.args);
  });
  REQUIRE(gui::import_wrapper_commands(s.interp()).ok());
  s.editor.display_running = true;
  REQUIRE(s.run("openwrapper").ok());
  return s;
}

// The Tcl result of `script`, or `error: <message>`.
std::string tcl(Session& s, std::string_view script) {
  const auto r = s.interp().evaluate(script);
  if (!r.ok()) {
    return "error: " + (r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text);
  }
  return r.value.to_string();
}

}  // namespace

TEST_CASE("the wrapper frame's widgets in openwrapper's order",
          "[cap:ui-gui-managers][req:Wrapper window construction]") {
  Session s;
  wrapped(s);
  CHECK(tcl(s, "winfo children .layout1.pane.top") ==
        ".layout1.pane.top.xscroll .layout1.pane.top.yscroll .layout1.pane.top.zb .layout1.pane.top.titlebar "
        ".layout1.pane.top.magic .layout1.pane.top.toolbar");
  CHECK(tcl(s, "winfo children .layout1") == ".layout1.pane");
  CHECK(tcl(s, "winfo toplevel .layout1.pane.top.magic") == ".layout1");
  CHECK(tcl(s, "winfo parent .layout1.pane.top.magic") == ".layout1.pane.top");
  CHECK(tcl(s, "windownames layout") == ".layout1.pane.top.magic");
  // Paths the frame does not have fail as in Tk.
  CHECK(tcl(s, "winfo children .layout1.titlebar") == "error: bad window path name \".layout1.titlebar\"");
  CHECK(tcl(s, ".layout1.titlebar.pos cget -text") == "error: invalid command name \".layout1.titlebar.pos\"");
  CHECK(tcl(s, "event generate .layout1.toolbar.b_poly <ButtonPress-1>") ==
        "error: bad window path name \".layout1.toolbar.b_poly\"");
  CHECK(tcl(s, "winfo exists .layout1.pane.bot") == "1");
  // openwrapper and closewrapper are global commands in wrapper mode.
  CHECK(tcl(s, "openwrapper") == ".layout2.pane.top.magic");
  CHECK(tcl(s, "windownames layout") == ".layout2.pane.top.magic .layout1.pane.top.magic");
  CHECK(tcl(s, "winfo exists .layout2.pane.top.titlebar.mbuttons.opts") == "1");
}

TEST_CASE("windownames gives window ids without a display", "[cap:scripting-api][req:Result formats of commands used by scripts]") {
  // Magic's windNamesCmd: the Tk path with a display, the id with NULL graphics.
  Session s;
  (void)s.interp();
  REQUIRE(s.run("openwrapper").ok());
  CHECK(s.run("windownames").value.to_string() != ".layout1.pane.top.magic");
  s.editor.display_running = true;
  CHECK(s.run("windownames").value.to_string() == ".layout1.pane.top.magic");
}

TEST_CASE("the wrapper menus are Tk menus of the menu model", "[cap:ui-gui-managers][req:Wrapper menus]") {
  Session s;
  wrapped(s);
  const std::string file = ".layout1.pane.top.titlebar.mbuttons.file";
  CHECK(tcl(s, file + " cget -text") == "File");
  CHECK(tcl(s, file + ".toolmenu cget -text") == "error: unknown option \"-text\"");
  CHECK(tcl(s, file + ".toolmenu index end") == "9");
  CHECK(tcl(s, file + ".toolmenu entrycget 0 -label") == "Open...");
  CHECK(tcl(s, file + ".toolmenu entrycget 0 -command") == "magic::promptload magic");
  CHECK(tcl(s, file + ".toolmenu type 2") == "separator");
  CHECK(tcl(s, file + ".toolmenu index Quit") == "9");
  // An item runs its command at the global level; a check item toggles its variable.
  const std::string win = ".layout1.pane.top.titlebar.mbuttons.win.toolmenu";
  CHECK(tcl(s, "set Opts(printunits) 0; " + win + " invoke {Display units}; set Opts(printunits)") == "1");
  CHECK(tcl(s, "menu .m -tearoff 0; .m index end") == "none");
  CHECK(tcl(s, "menu .t; .t index end") == "0");  // the tear-off entry
  CHECK(tcl(s, ".m add command -label A -command {set ::hit 1}; .m invoke 0; set ::hit") == "1");
}

TEST_CASE("maketoolbar builds the toolbar rows and fails on a frame without one", "[cap:ui-gui-managers][req:Layer toolbar]") {
  Session s;
  wrapped(s);
  // Regression (SCRIPT-GUI-06): Magic's `maketoolbar .layout1` fails on
  // `.layout1.toolbar` with Tk's error, as a Tcl error without console text.
  const auto bad = s.run("maketoolbar .layout1");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.raises);
  CHECK(bad.diagnostics.front().text == "bad window path name \".layout1.toolbar\"");
  s.ok("maketoolbar .layout1.pane.top");
  const std::string frame = ".layout1.pane.top.toolbar.canvas.frame";
  CHECK(tcl(s, "winfo children .layout1.pane.top.toolbar") ==
        ".layout1.pane.top.toolbar.canvas .layout1.pane.top.toolbar.vscroll");
  CHECK(tcl(s, "winfo children " + frame + ".fpoly") == frame + ".fpoly.l " + frame + ".fpoly.b");
  CHECK(tcl(s, frame + ".fmetal1.l cget -text") == "metal1");
  CHECK(tcl(s, "lindex [winfo children " + frame + "] 0") == frame + ".ferrors");
  // While the frame is suspended nothing is rebuilt.
  CHECK(tcl(s, "suspendall; destroy " + frame + "; maketoolbar .layout1.pane.top; winfo exists " + frame) == "0");
}

TEST_CASE("suspendall and resumeall count per wrapper frame", "[cap:ui-gui-managers][req:Wrapper option variables]") {
  Session s;
  wrapped(s);
  CHECK(tcl(s, "suspendall; suspendall; set Winopts(.layout1,suspend)") == "2");
  CHECK(tcl(s, "resumeall; resumeall; info exists Winopts(.layout1,suspend)") == "0");
  // Magic's resumeall reads the element it unset (tools.tcl:37-49).
  CHECK(tcl(s, "resumeall") == "error: can't read \"Winopts(.layout1,suspend)\": no such element in array");
}

TEST_CASE("widgets: entries, buttons, treeviews and toplevel titles", "[cap:ui-gui-managers][req:Cell manager]") {
  Session s;
  wrapped(s);
  CHECK(tcl(s, "toplevel .w; entry .w.e; .w.e insert 0 abcd; .w.e delete 1 3; .w.e get") == "ad");
  CHECK(tcl(s, ".w.e insert end X; .w.e get") == "adX");
  CHECK(tcl(s, "set ::v 7; entry .w.v -textvariable ::v; .w.v delete 0 end; .w.v insert 0 9; set ::v") == "9");
  CHECK(tcl(s, "button .w.b -command {set ::pressed yes}; .w.b invoke; set ::pressed") == "yes");
  CHECK(tcl(s, "checkbutton .w.c -variable ::flag; .w.c invoke; set ::flag") == "1");
  CHECK(tcl(s, "ttk::treeview .w.t; .w.t insert {} end -text a") == "I001");
  CHECK(tcl(s, ".w.t insert I001 end -id I001/b -text b; .w.t children I001") == "I001/b");
  CHECK(tcl(s, ".w.t item I001/b -text") == "b");
  CHECK(tcl(s, ".w.t children nope") == "error: Item nope not found");
  CHECK(tcl(s, "wm title .w") == "w");
  CHECK(tcl(s, "wm title .w Params; wm title .w") == "Params");
  CHECK(tcl(s, "wm title .w.b") == "error: window \".w.b\" isn't a top-level window");
  CHECK(tcl(s, "frame .w; list") == "error: window name \"w\" already exists in parent");
  CHECK(tcl(s, "destroy .w; list [winfo exists .w] [info commands .w.b]") == "0 {}");
  // Bindings run at the global level with %W substituted.
  CHECK(tcl(s, "frame .f; bind .f <Enter> {set ::entered %W}; event generate .f <Enter>; set ::entered") == ".f");
  // label and grid: a window path goes to Tk, anything else to Magic.
  CHECK(tcl(s, "label .f.l -text hi; .f.l cget -text") == "hi");
  CHECK(tcl(s, "grid .f.l -row 0 -column 0") == "");
}

TEST_CASE("the cell manager window lists the hierarchy", "[cap:ui-gui-managers][req:Cell manager]") {
  Session s;
  wrapped(s);
  db::CellDef* top = s.cell("top");
  db::CellDef* a = s.cell("a");
  db::PlaceOptions o;
  o.id = "a_0";
  REQUIRE(db::place(*top, *a, o).ok());
  CHECK(tcl(s, "cellmanager update; winfo exists .cellmgr") == "0");  // only `create` builds it
  s.ok("cellmanager create");
  CHECK(tcl(s, "wm state .cellmgr") == "withdrawn");
  CHECK(tcl(s, ".cellmgr.box.view children {}") == "(UNNAMED) top");  // the window's cell and top
  CHECK(tcl(s, ".cellmgr.box.view children top") == "top/a");
  CHECK(tcl(s, ".cellmgr.box.view item top/a -text") == "a");
  CHECK(tcl(s, "winfo children .cellmgr.target") == ".cellmgr.target.name .cellmgr.target.list");
}

TEST_CASE("the file prompts ask Tk's file dialogs", "[cap:ui-gui-managers][req:File prompts]") {
  CHECK(gui::tk_file_types({{"Magic", ".mag"}, {"All files", "*"}}) == R"({{Magic {.mag {.mag}}} {"All files" {*}}})");
  CHECK(gui::tk_file_types({{"GDS", ".gds .strm .cal"}, {"All files", "*"}}) ==
        R"({{GDS {.gds .strm .cal {.gds .strm .cal}}} {"All files" {*}}})");
  Session s;
  wrapped(s);
  gui::TkFilePicker picker(s.editor);
  platform::FilePickerRequest request;
  request.mode = platform::FilePickerRequest::Mode::save;
  request.title = "Save cell (UNNAMED) as:";
  request.file_types = {{"Magic", ".mag"}, {"All files", "*"}};
  CHECK_FALSE(picker.pick(request).has_value());  // the default dialog is cancelled
  REQUIRE(tcl(s, "proc tk_getSaveFile {args} {set ::asked $args; return /work/x.mag}") == "");
  CHECK(picker.pick(request) == platform::Path{"/work/x.mag"});
  CHECK(tcl(s, "set ::asked") == R"(-filetypes {{Magic {.mag {.mag}}} {"All files" {*}}} -title {Save cell (UNNAMED) as:})");
}

TEST_CASE("make_texthelper takes its manager path", "[cap:ui-gui-managers][req:Text helper]") {
  // Regression (SCRIPT-GUI-10): texthelper.tcl's `proc magic::make_texthelper { mgrpath }`.
  Session s;
  const auto r = s.run("make_texthelper");
  REQUIRE_FALSE(r.ok());
  CHECK(r.raises);
  CHECK(r.diagnostics.front().text == "wrong # args: should be \"magic::make_texthelper mgrpath\"");
  CHECK(s.run("make_texthelper .texthelper").ok());
}
