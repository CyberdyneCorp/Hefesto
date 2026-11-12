// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DRC manager and its report file (capability ui-gui-managers "DRC
// manager and DRC report file"; Q-SCRIPT-8, Q-SCRIPT-26).

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>
#include <vector>

#include "../drc/commands/drc_fixture.hpp"
#include "hefesto/cell/host.hpp"
#include "gui_fixture.hpp"
#include "hefesto/gui/drc_report.hpp"

namespace gui = hefesto::gui;
namespace db = hefesto::db;
using hefesto::drc::test::DrcFixture;
using hefesto::drc::test::FakeRecheck;

namespace {

const std::string kSep(40, '-');

/// `inv` shown in a window with one error tile of rule
/// `Metal1 spacing < 0.14um` at (0,0)-(28,40).
struct ReportSession {
  DrcFixture f;
  FakeRecheck fake;
  db::CellDef* inv = nullptr;

  ReportSession() {
    f.editor.cell_session.host = hefesto::cell::make_memory_file_host(f.services.files(), "/work");
    f.whys({"Metal1 spacing < 0.14um"});
    inv = f.cell("inv");
    f.paint(*inv, db::Rect{0, 0, 100, 100});
    f.show(*inv);
    f.box(*inv, db::Rect{0, 0, 100, 100});
    DrcFixture::error(*inv, db::Rect{0, 0, 28, 40});
    fake.errors = {{inv, db::Rect{0, 0, 28, 40}, DrcFixture::rule(6)}};
  }

  std::string report(const std::string& file = "/work/drc.out") {
    auto text = f.services.files().read(file);
    REQUIRE(text.ok());
    return text.value();
  }
};

}  // namespace

TEST_CASE("the report file format", "[cap:ui-gui-managers][req:DRC manager and DRC report file]") {
  // "Report file format": cif scale out 0.005.
  const std::vector<gui::DrcManagerError> errors = {{"Metal1 spacing < 0.14um", {"0 0 28 40"}}};
  CHECK(gui::format_drc_report("inv", "{inv 1}", errors, 0.005) ==
        "inv {inv 1}\n" + kSep + "\nMetal1 spacing < 0.14um\n" + kSep + "\n 0.000 0.000 0.140 0.200\n" + kSep +
            "\n\n");
  CHECK(gui::format_drc_report("top", "", {}, 0.01) == "top \n" + kSep + "\n\n");
}

TEST_CASE("drc_save_report writes the cell's errors in every unit setting",
          "[cap:ui-gui-managers][req:DRC manager and DRC report file]") {
  ReportSession s;
  const hefesto::drc::ScopedRecheck scoped(s.fake.fn());
  const auto saved = s.f.run("drc_save_report");
  REQUIRE(saved.ok());
  const std::string plain = s.report();
  INFO(plain);
  // The boxes are scaled by `cif scale out`.
  const auto scale = s.f.run("cif scale out");
  const double oscale = scale.ok() ? std::stod(scale.value.to_string()) : 1.0;
  char box[64];
  std::snprintf(box, sizeof box, " %.3f %.3f %.3f %.3f", 0.0, 0.0, 28 * oscale, 40 * oscale);
  CHECK(plain == "inv {inv 1}\n" + kSep + "\nMetal1 spacing < 0.14um\n" + kSep + "\n" + box + "\n" + kSep + "\n\n");
  CHECK(DrcFixture::texts(saved).find("done with drc.out\n") != std::string::npos);
  // "Report with micron units": identical file, units restored (Q-SCRIPT-26).
  REQUIRE(s.f.run("units microns print").ok());
  REQUIRE(s.f.run("drc_save_report inv /work/um.out").ok());
  CHECK(s.report("/work/um.out") == plain);
  CHECK(s.f.value("units") == "microns print");
  // Lambda units, magic-compat OFF: still the same file.
  s.f.compat(false);
  REQUIRE(s.f.run("units lambda").ok());
  REQUIRE(s.f.run("drc_save_report inv /work/l.out").ok());
  CHECK(s.report("/work/l.out") == plain);
  CHECK(s.f.run("drc_save_report a b c").raises);
}

TEST_CASE("drc_save_report loads the named cell and reloads the original",
          "[cap:ui-gui-managers][req:DRC manager and DRC report file]") {
  ReportSession s;
  const hefesto::drc::ScopedRecheck scoped(s.fake.fn());
  db::CellDef* other = s.f.cell("other");
  s.f.paint(*other, db::Rect{0, 0, 10, 10});
  REQUIRE(s.f.run("load other").ok());
  const auto r = s.f.run("drc_save_report inv");
  REQUIRE(r.ok());
  CHECK(DrcFixture::texts(r).find("loading inv\n") != std::string::npos);
  CHECK(s.report().starts_with("inv "));
  // `other` is loaded again, but `load` keeps the selection: inv's top cell, selected by
  // `select top cell`, is what `cellname list self` names, as in Magic (DBCellPrint SELF).
  CHECK(s.f.value("cellname window") == "other");
  CHECK(s.f.value("cellname list self") == "inv");
  REQUIRE(s.f.run("select clear").ok());
  CHECK(s.f.value("cellname list self") == "other");
}

TEST_CASE("drc_load_report adds feedback per box in microns",
          "[cap:ui-gui-managers][req:DRC manager and DRC report file]") {
  // Box lines: the first ` a b c d` match; other lines set the rule text.
  const auto box = gui::match_report_box("  1 2 3 4 5");
  REQUIRE(box.has_value());
  CHECK(box->llx == "1");  // the leftmost match starts at the first blank before a word
  CHECK(box->ury == "4");
  CHECK_FALSE(gui::match_report_box(" 1 2 3").has_value());
  const std::string text = "inv {inv 2}\n" + kSep + "\nrule A\n" + kSep + "\n 0.000 0.000 0.140 0.200\n 1 1 2 2\n" +
                           kSep + "\nrule B\n" + kSep + "\n 0 0 1\n" + kSep + "\n\n";
  const auto parsed = gui::parse_drc_report(text);
  REQUIRE(parsed.size() == 1);
  CHECK(parsed[0].rule == "rule A");
  CHECK(parsed[0].boxes == std::vector<std::string>{"0.000 0.000 0.140 0.200", "1 1 2 2"});

  hefesto::gui::test::Session s;
  s.load_cif_tech();
  s.editor.edit_cell = s.cell("top");
  REQUIRE(s.run("openwindow top").ok());
  s.editor.styles.emplace();
  s.editor.styles->table.styles.resize(61);
  s.editor.styles->table.styles[60].long_name = "vert_highlights";
  s.write("/work/drc.out", text);
  const auto r = s.run("drc_load_report");
  INFO(hefesto::gui::test::all_text(r));
  REQUIRE(r.ok());
  CHECK(hefesto::gui::test::all_text(r).find("Reading drc.out\n") != std::string::npos);
  // (llx,lly) (llx,ury) (urx,ury) (urx,lly) in µm, with style vert_highlights and the rule text.
  const auto& areas = s.editor.feedback.areas();
  REQUIRE(areas.size() == 2);
  CHECK(areas[0].style.index == 60);
  CHECK(areas[0].message() == "rule A");
  CHECK(areas[1].area == db::Rect{10, 10, 20, 20});
  CHECK(s.run("drc_load_report /work/none.out").raises);
}

TEST_CASE("the DRC manager lists drc listall why and zooms in internal units",
          "[cap:ui-gui-managers][req:DRC manager and DRC report file]") {
  ReportSession s;
  const hefesto::drc::ScopedRecheck scoped(s.fake.fn());
  REQUIRE(s.f.run("units microns").ok());
  // The model is built by a command handler's context; run it through a probe command.
  gui::DrcManagerModel model;
  hefesto::cmd::CommandRegistry& registry = s.f.registry;
  REQUIRE_FALSE(registry
                    .add({"probe_drcmanager", "", "",
                          [&](hefesto::cmd::CommandContext& ctx, const hefesto::cmd::Invocation&) {
                            model = gui::drc_manager_model(ctx);
                            return gui::drc_manager_zoom(ctx, "0 0 28 40");
                          }})
                    .has_value());
  REQUIRE(s.f.run("probe_drcmanager").ok());
  CHECK(model.cell == "inv");
  REQUIRE(model.errors.size() == 1);
  CHECK(model.errors[0] == gui::DrcManagerError{"Metal1 spacing < 0.14um", {"0 0 28 40"}});
  CHECK(s.f.editor.box == db::Rect{0, 0, 28, 40});
  CHECK(s.f.value("units") == "microns noprint");
}
