// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M4 manager panels' view models (docs/design/mockups/M4-Managers.dc.html;
// openspec/changes/m4-scripting task 10.4).

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/manager_views.hpp"
#include "hefesto/shell/text_helper_view.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

gui::CellManagerModel ota_model() {
  gui::CellManagerModel model;
  model.window = ".layout1";
  model.rows = {{"ota_5t", "ota_5t", "", 0},
                {"ota_5t/nfet", "nfet", "xM1", 1},
                {"ota_5t/pfet", "pfet", "xM3", 1},
                {"ota_5t/inv", "inv", "x1", 1},
                {"bias_gen", "bias_gen", "", 0}};
  return model;
}

// Children of the cells below the model's first level (`cellname list children`).
std::vector<gui::CellManagerRow> children_of(const gui::CellManagerRow& row) {
  if (row.cell == "inv") return {{row.id + "/nfet", "nfet", "xN", row.depth + 1}, {row.id + "/pfet", "pfet", "xP", row.depth + 1}};
  return {};
}

}  // namespace

TEST_CASE("the Cells tab lists top cells and children as virtual table rows",
          "[cap:ui-gui-managers][req:Cell manager]") {
  CellsView view(children_of);
  view.reset(ota_model());
  REQUIRE(view.visible_count() == 5);
  CHECK(view.target() == ".layout1");
  CHECK(view.cell_text(0, 0) == "▾ ota_5t");
  CHECK(view.cell_text(0, 1) == "(top cell)");
  CHECK(view.cell_text(1, 0) == "  ▸ nfet");
  CHECK(view.cell_text(1, 1) == "xM1");
  CHECK(view.cell_text(4, 0) == "  bias_gen");  // a top cell without children is a leaf

  // Opening `inv` asks for its children once; a cell without children becomes a leaf.
  CHECK(view.toggle(3));
  REQUIRE(view.visible_count() == 7);
  CHECK(view.cell_text(4, 0) == "    ▸ nfet");
  CHECK(view.cell_text(4, 1) == "xN");
  CHECK_FALSE(view.toggle(1));
  CHECK(view.cell_text(1, 0) == "    nfet");
  CHECK(view.toggle(0));  // closing the top hides its subtree
  CHECK(view.visible_count() == 2);
}

TEST_CASE("Cells buttons issue Magic's cell manager commands on the selected row",
          "[cap:ui-gui-managers][req:Cell manager]") {
  CellsView view(children_of);
  view.reset(ota_model());
  CHECK(view.commands(gui::CellManagerAction::load).empty());  // nothing selected
  view.toggle(3);
  view.select(4);  // ota_5t/inv/nfet, instance x1/xN
  CHECK(view.instance_path(4) == "x1/xN");
  CHECK(view.commands(gui::CellManagerAction::load) == std::vector<std::string>{"load nfet"});
  CHECK(view.commands(gui::CellManagerAction::edit) ==
        std::vector<std::string>{"select cell x1/xN", "expand", "edit"});
  CHECK(view.commands(gui::CellManagerAction::expand) ==
        std::vector<std::string>{"select cell x1/xN", "expand toggle"});
  CHECK(view.commands(gui::CellManagerAction::place) == std::vector<std::string>{"getcell nfet"});
  CHECK(view.commands(gui::CellManagerAction::zoom) ==
        std::vector<std::string>{"select cell x1/xN", "findbox zoom"});
  view.select(0);  // a top cell: `view`, `select top cell`
  CHECK(view.commands(gui::CellManagerAction::zoom) == std::vector<std::string>{"view"});
  CHECK(view.commands(gui::CellManagerAction::expand) == std::vector<std::string>{"select top cell", "expand"});
}

TEST_CASE("a refresh keeps the open nodes and the selection",
          "[cap:ui-gui-managers][req:Cell manager]") {
  CellsView view(children_of);
  view.reset(ota_model());
  view.toggle(3);
  view.select(5);  // ota_5t/inv/pfet
  view.reset(ota_model());  // `cellmanager update` after a `getcell`
  REQUIRE(view.visible_count() == 7);
  REQUIRE(view.selected());
  CHECK(view.visible_row(*view.selected()).id == "ota_5t/inv/pfet");
}

TEST_CASE("the select tag highlights the selected instance (mgrselect)",
          "[cap:ui-gui-managers][req:Cell manager]") {
  CellsView view(children_of);
  view.reset(ota_model());
  const auto message = view.select_instance("ota_5t", {{"x1", "inv"}, {"xP", "pfet"}}, "x1/xP");
  REQUIRE(message);
  CHECK(*message == "Selected cell is ota_5t/inv/pfet (x1/xP)");
  REQUIRE(view.selected());
  CHECK(view.visible_row(*view.selected()).id == "ota_5t/inv/pfet");
  // A top cell (Magic's 5-element select result): the window's root row.
  CHECK(view.select_instance("ota_5t", {}, "ota_5t 0 0 10 10"));
  CHECK(view.visible_row(*view.selected()).id == "ota_5t");
  CHECK_FALSE(view.select_instance("ota_5t", {{"xZ", "nothere"}}, "xZ"));
}

TEST_CASE("Cells rows are reachable by keyboard and named for screen readers",
          "[cap:hefesto-app-ux][req:Accessibility and localization]") {
  CellsView view(children_of);
  view.reset(ota_model());
  CHECK(view.accessible_text(0) == "ota_5t, top cell, level 1, open");
  CHECK(view.accessible_text(1) == "nfet, instance xM1, level 2, closed");
  // Left on an open node closes it; Right opens it, then moves to the first child.
  CHECK(view.key_left(0) == std::nullopt);
  CHECK(view.visible_count() == 2);
  view.key_right(0);
  CHECK(view.visible_count() == 5);
  view.select(0);
  CHECK(view.key_right(0) == std::optional<std::size_t>{1});
  CHECK(view.key_left(1) == std::optional<std::size_t>{0});  // a closed child: to its parent
}

TEST_CASE("Left and Right in the Cells table open, close and move along the tree (G-1)",
          "[cap:hefesto-app-ux][req:Accessibility and localization]") {
  CellsView view(children_of);
  view.reset(ota_model());
  view.select(0);
  // Unmodified Left / Right are the tree's keys; the table keeps the others.
  CHECK(view.tree_key(raven_key::kLeft, 0, 0));  // closes the open top cell
  CHECK(view.visible_count() == 2);
  CHECK(view.tree_key(raven_key::kRight, 0, 0));  // opens it again
  CHECK(view.visible_count() == 5);
  CHECK(view.selected() == std::optional<std::size_t>{0});
  CHECK(view.tree_key(raven_key::kRight, 0, 0));  // open: moves to the first child
  CHECK(view.selected() == std::optional<std::size_t>{1});
  CHECK(view.tree_key(raven_key::kLeft, 0, 1));  // a closed child: back to its parent
  CHECK(view.selected() == std::optional<std::size_t>{0});
  CHECK_FALSE(view.tree_key(raven_key::kDown, 0, 0));
  CHECK_FALSE(view.tree_key(raven_key::kEnter, 0, 0));
  CHECK_FALSE(view.tree_key(raven_key::kLeft, kModShift, 0));
  CHECK(view.visible_count() == 5);  // nothing changed by the keys left to the table
}

TEST_CASE("the manager tables are virtual: rows are text on demand",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  gui::LibraryManagerModel model;
  model.directories = {"/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag/"};
  for (int i = 0; i < 446; ++i) {
    model.rows.push_back({model.directories[0], "cell" + std::to_string(i), "sky130A"});
  }
  LibrariesView view;
  view.reset(model);
  CHECK(view.visible_count() == 447);  // one row per cell, no widget per row
  CHECK(view.cell_count() == 446);
  CHECK(view.cell_text(446, 0) == "    cell445");
}

TEST_CASE("the Libraries tab shows one node per directory, only the first open",
          "[cap:ui-gui-managers][req:Library manager]") {
  gui::LibraryManagerModel model;
  model.directories = {"/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag/", "/pdk/sky130A/libs.ref/sky130_fd_pr/mag/",
                       "./"};
  model.rows = {{model.directories[0], "sky130_fd_sc_hd__inv_1", "sky130A"},
                {model.directories[0], "sky130_fd_sc_hd__nand2_1", "sky130A"},
                {model.directories[1], "sky130_fd_pr__nfet_01v8", "sky130A"}};
  LibrariesView view;
  view.reset(model, {{"PDKPATH", "/pdk/sky130A/"}});
  REQUIRE(view.visible_count() == 5);
  CHECK(view.cell_text(0, 0) == "▾ $PDKPATH/libs.ref/sky130_fd_sc_hd/mag/");
  CHECK(view.cell_text(1, 0) == "    sky130_fd_sc_hd__inv_1");
  CHECK(view.cell_text(1, 1) == "sky130A");
  CHECK(view.cell_text(3, 0) == "▸ $PDKPATH/libs.ref/sky130_fd_pr/mag/");
  CHECK(view.accessible_text(4) == "Directory ./, 0 cells, closed");
  CHECK(view.is_directory(0));
  CHECK(view.toggle(3));
  CHECK(view.visible_count() == 6);

  view.select(1);
  CHECK(view.commands(gui::LibraryManagerAction::load) == std::vector<std::string>{"load sky130_fd_sc_hd__inv_1"});
  CHECK(view.commands(gui::LibraryManagerAction::place) ==
        std::vector<std::string>{"getcell sky130_fd_sc_hd__inv_1 child 0 0"});
  CHECK(view.commands(gui::LibraryManagerAction::pick) ==
        std::vector<std::string>{"tool pick", "getcell sky130_fd_sc_hd__inv_1 child 0 0", "startselect pick"});
  view.select(0);
  CHECK(view.commands(gui::LibraryManagerAction::load).empty());  // a directory node
}

TEST_CASE("a directory shows its longest path variable unexpanded",
          "[cap:ui-gui-managers][req:Library manager]") {
  const std::vector<std::pair<std::string, std::string>> vars = {{"PDK_ROOT", "/pdk"}, {"PDKPATH", "/pdk/sky130A"}};
  CHECK(display_directory("/pdk/sky130A/libs.ref/x/", vars) == "$PDKPATH/libs.ref/x/");
  CHECK(display_directory("/pdk/gf180mcuD/x/", vars) == "$PDK_ROOT/gf180mcuD/x/");
  CHECK(display_directory("/pdkother/x/", vars) == "/pdkother/x/");  // not a path boundary
  CHECK(display_directory("./", vars) == "./");
}

TEST_CASE("the technology manager panel renders the model and issues its commands",
          "[cap:ui-gui-managers][req:Technology manager]") {
  gui::TechManagerModel model;
  model.technology = "sky130A";
  model.version = "(version 1.0.588-126-g5740c4a)";
  model.description = "SkyWater SKY130";
  model.microns_per_lambda = "0.01";
  model.lambda_numerator = "1";
  model.lambda_denominator = "2";
  model.cif_input_styles = {"sky130", "rdlimport"};
  model.cif_input_style = "sky130";
  model.drc_styles = {"drc(full)", "drc(fast)"};
  model.drc_style = "drc(full)";
  model.tech_menu = {{"minimum", "tech load minimum"},
                     {"sky130A", "source /pdk/sky130A/libs.tech/magic/sky130A.magicrc"}};
  TechView view;
  view.reset(model);
  CHECK(view.title() == "sky130A (version 1.0.588-126-g5740c4a)");
  const auto fields = view.fields();
  REQUIRE(fields.size() == 6);
  CHECK(fields[0] == TechField{"Microns per lambda (CIF)", "0.01"});
  CHECK(fields[1] == TechField{"Internal units per lambda", "1 / 2"});
  CHECK(fields[4] == TechField{"Extract", "(none)"});
  CHECK(view.styles(TechStyle::cif_input).size() == 2);
  CHECK(view.current(TechStyle::drc) == "drc(full)");
  CHECK(TechView::style_command(TechStyle::cif_input, "rdlimport") == "cif istyle rdlimport");
  CHECK(TechView::style_command(TechStyle::cif_output, "gdsii") == "cif ostyle gdsii");
  CHECK(TechView::style_command(TechStyle::extract, "ngspice()") == "extract style ngspice()");
  CHECK(TechView::style_command(TechStyle::drc, "drc(fast)") == "drc style drc(fast)");
  CHECK(view.tech_menu_command(1) == std::optional<std::string>{"source /pdk/sky130A/libs.tech/magic/sky130A.magicrc"});
  CHECK_FALSE(view.tech_menu_command(2));
  // Editing the units entries: scalegrid (lval0·λnum) (lval1·λden).
  CHECK(view.scalegrid_command("2", "3") == std::optional<std::string>{"scalegrid 2 6"});
  CHECK_FALSE(view.scalegrid_command("x", "3"));
  CHECK_FALSE(view.scalegrid_command("0", "3"));
}

TEST_CASE("text helper Apply for a new port label",
          "[cap:ui-gui-managers][req:Text helper]") {
  TextHelperView helper;
  helper.show_new_label(2);
  CHECK(helper.title() == "New label");
  CHECK(helper.model().port_index == "3");
  CHECK(helper.model().font == "FreeSans");
  CHECK(helper.justification_is("center"));
  gui::TextHelperModel& m = helper.model();
  m.text = "VDD";
  m.size = "0.5";
  m.layer = "m1";
  m.default_layer = false;
  m.port = true;
  // Scenario "New port label".
  CHECK(helper.apply_lines(false, "lambda") ==
        std::vector<std::string>{"label VDD FreeSans 0.5um 0 {0 0} center m1", "port make 3"});
  CHECK(helper.preview() == "label VDD FreeSans 0.5um 0 {0 0} center m1 · port make 3");
  helper.after_apply(3);  // `port last` is now 3
  CHECK(helper.model().port_index == "4");
}

TEST_CASE("text helper justification grid and selected labels",
          "[cap:ui-gui-managers][req:Text helper]") {
  TextHelperView helper;
  helper.show_new_label(0);
  helper.set_justification("NE");
  CHECK(helper.justification_is("NE"));
  helper.set_justification("bogus");  // not a choice: unchanged
  CHECK(helper.justification_is("NE"));
  // The grid marks the chosen square only (primary button style, draft 13).
  CHECK(helper.justification_marks() == std::array<bool, 11>{false, false, true, false, false, false, false, false,
                                                             false, false, false});
  helper.set_justification("center");  // the middle square and the "center" button
  CHECK(helper.justification_marks() == std::array<bool, 11>{false, false, false, false, true, false, false, false,
                                                             false, true, false});
  helper.set_justification("default");
  helper.model().text = "A";
  helper.model().font = "default";
  // Q-SCRIPT-31: the default font with justification `default` (replicated).
  CHECK(helper.apply_lines(false, "lambda") == std::vector<std::string>{"label A default"});

  gui::TextHelperModel fields;
  fields.text = "OUT";
  fields.font = "default";
  fields.justification = "N";
  fields.size = "0.3";
  fields.offset = "0 0";
  fields.rotation = "90";
  helper.show_selection(fields, 2);
  CHECK(helper.title() == "Selected labels");
  CHECK(helper.apply_lines(true, "microns") ==
        std::vector<std::string>{"units microns", "setlabel text OUT", "setlabel font -1", "setlabel justify N",
                                 "setlabel size 0.3", "setlabel offset {0 0}", "setlabel rotate 90",
                                 "setlabel sticky 0", "port remove", "units microns"});
}
