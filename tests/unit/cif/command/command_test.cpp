// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `cif` command through the dispatcher (capability cif-format-io "cif
// command dispatch", "cif read, write and flat options", "cif limit and
// rescale options", "cif output-format toggles", "cif drccheck option",
// "cif warning option", "cif polygon and unfracture options", "cif see,
// paint, coverage and statistics options"; mask-hierarchical-generation
// "Hierarchy and array output switches").
// Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../cif_fixture.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace {

using namespace cif_test;  // NOLINT(google-build-using-namespace): the fixture's aliases
namespace view = hefesto::view;

// A command session with masktest loaded and, unless `window` is false, a
// window showing `top` (metal1 (0,0)-(4,2)).
struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  db::CellDef* top = nullptr;

  explicit Session(bool magic_compat = true, bool window = true, const std::string& name = "top",
                   const std::string& technology = masktest_text()) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
    REQUIRE(files().write_atomic("/t/masktest.tech", technology).ok());
    REQUIRE(run("tech load /t/masktest.tech -noprompt").ok());
    auto made = editor.cells.create(name, editor.technology->plane_count());
    REQUIRE(made.ok());
    top = made.value();
    top->flags().available = true;
    paint(*top, "metal1", db::Rect{0, 0, 4, 2});
    if (window) {
      REQUIRE(editor.windows.open(top, view::ScreenRect{0, 0, 999, 999}).ok());
    }
    (void)printed.take();
  }

  platform::FileSystem& files() { return services.view().files; }

  void paint(db::CellDef& def, std::string_view type, const db::Rect& r) {
    const auto found = editor.technology->find_type(type);
    REQUIRE(found.ok());
    REQUIRE(db::paint_type(def, *editor.technology, r, *found.id).ok());
    db::recompute_bbox(def);
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  [[nodiscard]] std::string file(const std::string& path) {
    auto read = files().read(path);
    REQUIRE(read.ok());
    return read.value();
  }

  /// The texts printed since the last call.
  std::vector<std::string> take() {
    std::vector<std::string> out;
    for (const auto& d : printed.take()) {
      out.push_back(d.text);
    }
    return out;
  }

  [[nodiscard]] std::string value(std::string_view line) { return run(line).value.to_string(); }
};

bool has(const std::vector<std::string>& lines, std::string_view text) {
  for (const std::string& l : lines) {
    if (l == text) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("a bare cif writes the root cell to the name after its last slash",
          "[cap:cif-format-io][req:cif command dispatch]") {
  Session s(true, true, "lib/inv");
  REQUIRE(s.run("cif").ok());
  REQUIRE(s.files().exists("inv.cif"));
  CHECK(body(s.file("inv.cif")).at(1) == "9 lib/inv;");
}

TEST_CASE("options other than the settings need a window", "[cap:cif-format-io][req:cif command dispatch]") {
  Session s(true, false);
  for (const char* line : {"cif write x", "cif flat x", "cif read x", "cif see CMF", "cif statistics", "cif"}) {
    const cmd::CommandResult r = s.run(line);
    REQUIRE_FALSE(r.ok());
    CHECK(r.diagnostics.front().text == "Point to a window first");
  }
  for (const char* line : {"cif help", "cif prefix /x", "cif arealabels no", "cif warning", "cif limit",
                           "cif polygon subcells", "cif ostyle", "cif istyle"}) {
    CHECK(s.run(line).ok());
  }
}

TEST_CASE("an unknown option prints the error and the help", "[cap:cif-format-io][req:cif command dispatch]") {
  Session s;
  REQUIRE(s.run("cif xyz").ok());
  const std::vector<std::string> printed = s.take();
  REQUIRE(printed.size() >= 2);
  CHECK(printed[0] == "\"xyz\" isn't a valid cif option.");
  CHECK(printed[1].starts_with("CIF commands have the form"));
}

TEST_CASE("cif write and cif flat name the file (Q-MASK-320)", "[cap:cif-format-io][req:cif read, write and flat options]") {
  for (const bool on : {true, false}) {
    Session s(on);
    REQUIRE(s.run("cif write chip").ok());
    CHECK(s.files().exists("chip.cif"));
    REQUIRE(s.run("cif write other.cif").ok());
    CHECK(s.files().exists(on ? "other.cif.cif" : "other.cif"));
    REQUIRE(s.run("cif flat flat").ok());
    CHECK(body(s.file("flat.cif")).at(1) == (on ? "9 __CIF2__;" : "9 top;"));
  }
}

TEST_CASE("cif read reads a file along the search path into new cells",
          "[cap:cif-format-io][req:cif read, write and flat options][req:cif drccheck option]") {
  for (const bool check : {true, false}) {
    Session s;
    REQUIRE(s.files().write_atomic("in.cif", "DS 1 1 1;\n9 leaf;\nL CMF;\nB 40 20 20 10;\n94 a 10 10 CMF;\nDF;\n"
                                             "91 u1;\nC 1 T 100 0;\nE\n").ok());
    s.editor.edit_cell = s.top;
    if (!check) {
      REQUIRE(s.run("cif drccheck no").ok());
    }
    CHECK(s.value("cif drccheck") == (check ? "1" : "0"));
    REQUIRE(s.run("cif read in").ok());
    CHECK(has(s.take(), "Warning: CIF reading is not undoable!  I hope that's OK."));
    const db::CellDef* leaf = s.editor.cells.find("leaf");
    REQUIRE(leaf != nullptr);
    CHECK(leaf->bbox() == db::Rect{0, 0, 4, 2});
    REQUIRE(leaf->labels().size() == 1);
    CHECK(leaf->labels().front().text == "a");
    CHECK(leaf->labels().front().rect == db::Rect{1, 1, 1, 1});
    CHECK(s.editor.drc.pending.contains(*leaf) == check);
    REQUIRE(s.top->uses().size() == 1);
    CHECK(s.top->uses().front()->id == "u1");
    CHECK(s.top->uses().front()->transform.c == 10);
  }
}

TEST_CASE("cif read of a missing file", "[cap:cif-format-io][req:cif read, write and flat options]") {
  Session s;
  REQUIRE(s.run("cif read nothere").ok());
  CHECK(has(s.take(), "Cannot open nothere.cif to read CIF."));
  REQUIRE(s.run("cif read").ok());  // wrong arguments
  CHECK(s.take().front() == "Wrong arguments in \"cif read\" command:");
}

TEST_CASE("cif limit and cif rescale", "[cap:cif-format-io][req:cif limit and rescale options]") {
  Session s;
  CHECK(s.value("cif limit") == "100");
  REQUIRE(s.run("cif limit 500").ok());
  CHECK(has(s.take(), "Warning:  rescale limit set at maximum value of 100"));
  CHECK(s.value("cif limit") == "100");
  REQUIRE(s.run("cif limit 0").ok());
  CHECK(has(s.take(), "Warning:  rescale limit set at minimum value of 1"));
  CHECK(s.value("cif limit") == "1");
  REQUIRE(s.run("cif limit 7").ok());
  CHECK(s.value("cif limit") == "7");
  REQUIRE(s.run("cif limit seven").ok());
  CHECK(s.take().front() == "Wrong arguments in \"cif limit\" command:");
  CHECK(s.value("cif limit") == "7");
  CHECK(s.value("cif rescale") == "1");
  REQUIRE(s.run("cif rescale no").ok());
  CHECK(s.value("cif rescale") == "0");
  CHECK(s.editor.mask.warnings.level == mask::WarningLevel::limit);
}

TEST_CASE("cif arealabels, idcell and prefix", "[cap:cif-format-io][req:cif output-format toggles]") {
  Session s;
  REQUIRE(s.run("cif arealabels yes").ok());
  CHECK(has(s.take(), "Output of CIF area labels is now enabled"));
  CHECK(s.editor.mask.cif.arealabels);
  REQUIRE(s.run("cif idcell").ok());
  CHECK(has(s.take(), "Output of CIF cell ID labels is now enabled"));  // the default
  REQUIRE(s.run("cif idcell no").ok());
  CHECK(has(s.take(), "Output of CIF cell ID labels is now disabled"));
  REQUIRE(s.run("cif prefix /lib").ok());
  REQUIRE(s.run("cif write pre").ok());
  CHECK(body(s.file("pre.cif")).at(1) == "9 /lib/top;");
  REQUIRE(s.run("cif prefix").ok());
  CHECK(s.editor.mask.cif.prefix.empty());
}

TEST_CASE("cif warning levels and the redirect file", "[cap:cif-format-io][req:cif warning option]") {
  Session s;
  REQUIRE(s.run("cif warning").ok());
  CHECK(has(s.take(), "Warning display options: default"));
  REQUIRE(s.run("cif warning none").ok());
  CHECK(s.editor.mask.warnings.level == mask::WarningLevel::none);
  REQUIRE(s.run("cif warning bogus").ok());
  const std::vector<std::string> printed = s.take();
  CHECK(has(printed, "\"bogus\" isn't a valid cif warning option."));
  CHECK(has(printed, "Valid options are: default, align, limit, redirect, and none"));
  CHECK(s.editor.mask.warnings.level == mask::WarningLevel::none);
  REQUIRE(s.run("cif warning redirect msgs.txt").ok());
  CHECK(s.editor.mask.warnings.level == mask::WarningLevel::redirect);
  CHECK(s.editor.mask.warnings.redirect_file == "msgs.txt");
  REQUIRE(s.run("cif warning redirect").ok());
  CHECK(s.editor.mask.warnings.redirect_file.empty());
}

TEST_CASE("cif polygon subcells", "[cap:cif-format-io][req:cif polygon and unfracture options]") {
  Session s;
  CHECK(s.value("cif polygon subcells") == "0");
  REQUIRE(s.run("cif polygon subcells yes").ok());
  CHECK(s.value("cif polygon subcells") == "1");
  REQUIRE(s.run("cif polygon").ok());
  CHECK(s.take().front() == "Wrong arguments in \"cif polygon\" command:");
}

TEST_CASE("cif unfracture reads the fourth word with magic-compat ON (Q-MASK-413)",
          "[cap:cif-format-io][req:cif polygon and unfracture options]") {
  for (const bool on : {true, false}) {
    Session s(on);
    const cmd::CommandResult third = s.run("cif unfracture yes");
    REQUIRE(third.ok());
    if (on) {
      CHECK(third.value.to_string() == "0");
      CHECK_FALSE(s.editor.mask.cif.unfracture);
      REQUIRE_FALSE(third.diagnostics.empty());
      CHECK(third.diagnostics.front().code == "CIF-CMD-001");
      CHECK(third.diagnostics.front().native_only);
    } else {
      CHECK(s.editor.mask.cif.unfracture);
    }
    REQUIRE(s.run("cif unfracture x no").ok());
    CHECK_FALSE(s.editor.mask.cif.unfracture);
    REQUIRE(s.run("cif unfracture x yes").ok());
    CHECK(s.editor.mask.cif.unfracture);
    CHECK(s.value("cif unfracture x") == "1");
    (void)s.take();
    REQUIRE(s.run("cif unfracture").ok());
    CHECK(s.take().front() == "Wrong arguments in \"cif unfracture\" command:");
  }
}

TEST_CASE("cif *hier and *array write switches",
          "[cap:mask-hierarchical-generation][req:Hierarchy and array output switches]") {
  Session s;
  REQUIRE(s.run("cif *hier write disable").ok());
  CHECK_FALSE(s.editor.mask.cif.hier);
  REQUIRE(s.run("cif *hier write report").ok());
  CHECK(has(s.take(), "Subcell interaction output disabled"));
  REQUIRE(s.run("cif *hier write en").ok());
  CHECK(s.editor.mask.cif.hier);
  REQUIRE(s.run("cif *array write dis").ok());
  CHECK_FALSE(s.editor.mask.cif.array);
  REQUIRE(s.run("cif *array read dis").ok());
  CHECK(s.take().front() == "Wrong arguments in \"cif *array\" command:");
}

TEST_CASE("cif see shows the generated tiles as feedback",
          "[cap:cif-format-io][req:cif see, paint, coverage and statistics options]") {
  Session s;
  REQUIRE(s.run("cif see CMF").ok());
  CHECK(has(s.take(), "Use the box to select the area in which you want to see CIF."));
  REQUIRE(s.run("box 0 0 10 10").ok());
  REQUIRE(s.run("cif see CMF").ok());
  REQUIRE(s.editor.feedback.size() == 1);
  CHECK(s.editor.feedback.areas().front().message() == "CIF layer \"CMF\"");
  CHECK(s.editor.feedback.areas().front().area == db::Rect{0, 0, 4, 2});
  REQUIRE(s.run("cif see NOPE").ok());
  CHECK(s.editor.feedback.size() == 1);
}

TEST_CASE("cif paint, coverage and statistics", "[cap:cif-format-io][req:cif see, paint, coverage and statistics options]") {
  Session s;
  REQUIRE(s.run("box 0 0 4 4").ok());
  REQUIRE(s.run("cif paint CMF metal2 newcell").ok());
  const db::CellDef* made = s.editor.cells.find("newcell");
  REQUIRE(made != nullptr);
  CHECK(made->bbox() == db::Rect{0, 0, 4, 2});
  CHECK(s.editor.drc.pending.contains(*made));
  CHECK(s.editor.drc.pending.contains(*s.top));
  // The box (0,0)-(4,4) is half covered by metal1 (0,0)-(4,2).
  CHECK(s.value("cif list coverage CMF box") == "0.5");
  REQUIRE(s.run("cif coverage CMF").ok());
  std::string report;
  for (const std::string& line : s.take()) {
    report += line + "\n";
  }
  CHECK(report.find("Cell Area = 800 CIF units^2\n") != std::string::npos);
  CHECK(report.find("Coverage in cell = 100.0%\n") != std::string::npos);
  REQUIRE(s.run("cif statistics").ok());
  const std::vector<std::string> stats = s.take();
  REQUIRE_FALSE(stats.empty());
  CHECK(stats.front().starts_with("CIF statistics (recent/total):"));
}

TEST_CASE("the CIF writer ignores CIFhier with magic-compat ON and honours it OFF (Q-MASK-410)",
          "[cap:mask-hierarchical-generation][req:Hierarchy and array output switches]") {
  // CCA closes gaps up to 2 units: grow 10, shrink 10 in output units.
  std::string text = masktest_text();
  const std::string squares = "    layer CCA pdc\n        squares 10 20 20\n";
  const std::size_t at = text.find(squares);
  REQUIRE(at != std::string::npos);
  text.replace(at, squares.size(), "    layer CCA pdc\n        grow 10\n        shrink 10\n");
  for (const bool on : {true, false}) {
    Session s(on, true, "top", text);
    auto made = s.editor.cells.create("a", s.editor.technology->plane_count());
    REQUIRE(made.ok());
    db::CellDef& a = *made.value();
    a.flags().available = true;
    s.paint(a, "pdc", db::Rect{0, 0, 3, 6});
    for (const db::Coord x : {10, 14}) {
      db::PlaceOptions o;
      o.transform = db::Transform{1, 0, x, 0, 1, 0};
      REQUIRE(db::place(*s.top, a, o).ok());
    }
    db::recompute_bbox(*s.top);
    REQUIRE(s.run("cif write hier").ok());
    const std::vector<std::string> with_hier = body(s.file("hier.cif"));
    REQUIRE(s.run("cif *hier write disable").ok());
    REQUIRE(s.run("cif write nohier").ok());
    const std::vector<std::string> without = body(s.file("nohier.cif"));
    db::set_property(*s.top, "CIFhier", db::PropertyValue{std::string{"1"}});
    REQUIRE(s.run("cif write forced").ok());
    const std::vector<std::string> forced = body(s.file("forced.cif"));
    REQUIRE(with_hier != without);  // the gap between the uses is closed in the parent only
    CHECK(forced == (on ? without : with_hier));
  }
}
