// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `drc` command: option matching, help, the extra-argument rule and
// the options that control the checker or print the rule database
// (drc-commands; openspec/changes/m3-drc tasks.md 7.1, 7.2, 7.4, 7.5).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "drc_fixture.hpp"
#include "hefesto/drc/commands/command.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/ui/macros.hpp"

namespace drc = hefesto::drc;
namespace db = hefesto::db;
namespace cmd = hefesto::cmd;
namespace ui = hefesto::ui;
using drc::test::DrcFixture;

namespace {

// CmdDrc's help, verbatim (MAGIC_WRAPPER header).
const std::string kHelpHeader =
    "DRC commands have the form \":drc [list|listall] option\", where option is one of:";
const std::string kHelpLines =
    "\n    catchup                run checker and wait for it to complete"
    "\n    check                  recheck area under box in all cells"
    "\n    count [total]          count error tiles in each cell under box"
    "\n    euclidean on|off\tenable/disable Euclidean geometry checking"
    "\n    find [nth]     \tlocate next (or nth) error in the layout"
    "\n    help                   print this help information"
    "\n    ignore\t[<text>|none]   do not report on rules with this text"
    "\n    off                    turn off background checker"
    "\n    on                     reenable background checker"
    "\n    status\t\t\treport if the drc checker is on or off"
    "\n    style\t\t\tset the DRC style"
    "\n    printrules [file]      print out design rules in file or on tty"
    "\n    rulestats              print out stats about design rule database"
    "\n    statistics             print out statistics gathered by checker"
    "\n    why                    print out reasons for errors under box\n";
const std::string kWizardLines =
    "\n    *flatcheck             check box area by flattening"
    "\n    *halo [d]\t\tlimit error checking to areas of d units"
    "\n    *showint radius        show interaction area under box"
    "\n    *stepsize [d]\t\tchange DRC step size to d units";
const std::string kHelp = kHelpHeader + kHelpLines;

/// A rule set with halo `halo`, step size `step` and scale n/d.
void style(DrcFixture& f, std::int32_t halo, std::int32_t step = 64, drc::ScaleFactor scale = {}) {
  drc::RuleSetBuilder b;
  b.halo(drc::Halo{halo, halo, 0});
  b.step_size(step);
  b.scale(scale);
  f.editor.drc.rules = b.build();
}

}  // namespace

// --- Command syntax and option matching ------------------------------------------

TEST_CASE("drc options are Magic's table and are found by Lookup", "[cap:drc-commands][req:Command syntax and option matching]") {
  std::vector<std::string_view> names;
  for (const drc::DrcOption& o : drc::drc_options()) {
    names.push_back(o.name);
    CHECK(o.help.starts_with(o.name));
    CHECK(o.wizard == o.name.starts_with("*"));
  }
  CHECK(names == std::vector<std::string_view>{"*flatcheck", "*halo", "*showint", "*stepsize", "catchup", "check",
                                               "count", "euclidean", "find", "help", "ignore", "off", "on", "status",
                                               "style", "printrules", "rulestats", "statistics", "why"});
  CHECK(drc::lookup_option("why") == 18);
  CHECK(drc::lookup_option("WHY") == 18);
  CHECK(drc::lookup_option("cat") == 4);
  CHECK(drc::lookup_option("::magic::why") == 18);
  CHECK(drc::lookup_option("magic::cat") == 4);
  CHECK(drc::lookup_option("statu") == 13);
  CHECK(drc::lookup_option("*h") == 1);
  CHECK(drc::lookup_option("c") == -1);      // catchup, check, count
  CHECK(drc::lookup_option("stat") == -1);   // status, statistics
  CHECK(drc::lookup_option("") == -1);
  CHECK(drc::lookup_option("halo") == -2);   // wizard names keep the asterisk
  CHECK(drc::lookup_option("whyx") == -2);
  CHECK(drc::lookup_option("on") == 12);     // exact beats the prefix of nothing else
  CHECK(drc::lookup_option("o") == -1);      // off, on
}

TEST_CASE("Ambiguous prefix", "[cap:drc-commands][req:Command syntax and option matching]") {
  DrcFixture f;
  CHECK(f.out("drc c") == "c isn't a valid drc option.\n" + kHelp);
  CHECK(f.out("drc list c") == "c isn't a valid drc option.\n" + kHelp);
}

TEST_CASE("Wizard option requires asterisk", "[cap:drc-commands][req:Command syntax and option matching]") {
  DrcFixture f;
  CHECK(f.out("drc halo") == "halo isn't a valid drc option.\n" + kHelp);
  style(f, 20);
  CHECK(f.out("drc *halo") == "DRC halo is 20 internal units (20 DRC units)\n");
  CHECK(f.out("drc *ha") == "DRC halo is 20 internal units (20 DRC units)\n");
}

TEST_CASE("drc without an option prints help, also after list (Q-DRC-22, both modes)",
          "[cap:drc-commands][req:Command syntax and option matching]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    DrcFixture f;
    f.compat(compat);
    const std::string expected = "No option given in \":drc\" command.\n" + kHelp;
    CHECK(f.out("drc") == expected);
    CHECK(f.out("drc list") == expected);
    CHECK(f.out("drc listall") == expected);
    CHECK(f.run("drc list").ok());
  }
}

TEST_CASE("drc help and drc help wizard", "[cap:drc-commands][req:Command syntax and option matching]") {
  DrcFixture f;
  CHECK(f.out("drc help") == kHelp);
  CHECK(f.out("drc help wizard") == kHelpHeader + kWizardLines + kHelpLines);
  CHECK(f.out("drc help Wizard") == kHelp);         // strcmp
  CHECK(f.out("drc help wizard extra") == kHelp);   // only as the one argument
  CHECK(f.out("drc listall help") == kHelp);
}

TEST_CASE("extra arguments to an option without arguments do nothing", "[cap:drc-commands][req:Command syntax and option matching]") {
  DrcFixture f;
  const auto r = f.run("drc why extra");
  CHECK(r.ok());
  CHECK(DrcFixture::texts(r) ==
        "Wrong arguments in \"drc why\" command:\n"
        "    drc why                    print out reasons for errors under box\n"
        "Try \"drc help\" for more help.\n");
  f.run("drc off");
  CHECK(f.out("drc on now") ==
        "Wrong arguments in \"drc on\" command:\n"
        "    drc on                     reenable background checker\n"
        "Try \"drc help\" for more help.\n");
  CHECK(f.value("drc status") == "0");  // `on now` did nothing
  CHECK(f.out("drc list sta extra").find("isn't a valid drc option") != std::string::npos);
  CHECK(f.out("drc printrules a b").starts_with("Wrong arguments in \"drc printrules\" command:\n"));
  CHECK(f.out("drc *showint").starts_with("Wrong arguments in \"drc *showint\" command:\n"));
  CHECK(f.out("drc ignore a b").starts_with("Wrong arguments in \"drc ignore\" command:\n"));
}

TEST_CASE("options on the box do nothing without a window holding the box",
          "[cap:drc-commands][req:Command syntax and option matching]") {
  DrcFixture f;
  db::CellDef* top = f.cell("top");
  f.box(*top, db::Rect{0, 0, 10, 10});  // no window shows `top`
  for (const char* line : {"drc why", "drc listall why", "drc check", "drc *flatcheck", "drc *showint 2",
                           "drc count", "drc find"}) {
    INFO(line);
    const auto r = f.run(line);
    CHECK(r.ok());
    CHECK(r.diagnostics.empty());
    CHECK(r.value.is_none());
  }
}

// --- Background checker control --------------------------------------------------

TEST_CASE("Status in scripting mode", "[cap:drc-commands][req:Background checker control]") {
  DrcFixture f;
  CHECK(f.value("drc status") == "1");
  CHECK(f.run("drc off").diagnostics.empty());
  CHECK(f.value("drc status") == "0");
  CHECK(f.editor.drc.background == drc::BackgroundMode::off);
  f.run("drc on");
  CHECK(f.value("drc status") == "1");
  CHECK(f.editor.drc.background == drc::BackgroundMode::on);
  CHECK(f.value("drc list status") == "1");
}

TEST_CASE("drc status treats the not-set state as on and drc on leaves it", "[cap:drc-commands][req:Background checker control]") {
  DrcFixture f;
  f.editor.drc.background = drc::BackgroundMode::not_set;
  CHECK(f.value("drc status") == "1");
}

// --- drc catchup and drc check -----------------------------------------------------

TEST_CASE("drc catchup restores the background state", "[cap:drc-commands][req:drc catchup and drc check]") {
  DrcFixture f;
  f.run("drc off");
  CHECK(f.run("drc catchup").ok());
  CHECK(f.value("drc status") == "0");
  f.run("drc on");
  CHECK(f.run("drc catchup").ok());
  CHECK(f.value("drc status") == "1");
}

TEST_CASE("drc check marks the box area of the root cell", "[cap:drc-commands][req:drc catchup and drc check]") {
  DrcFixture f;
  db::CellDef* top = f.cell("top");
  f.paint(*top, db::Rect{0, 0, 40, 40});
  f.show(*top);
  f.box(*top, db::Rect{0, 0, 20, 20});
  CHECK(f.run("drc check").ok());
  CHECK_FALSE(top->plane(hefesto::tech::kPlaneDrcCheck).empty());
  CHECK(f.editor.drc.pending.contains(*top));
}

// --- drc euclidean --------------------------------------------------------------------

TEST_CASE("Query metric", "[cap:drc-commands][req:drc euclidean]") {
  DrcFixture f;
  CHECK(f.out("drc euclidean") == "DRC measurements are Manhattan.\n");
  CHECK(f.out("drc euclidean on").empty());
  CHECK(f.editor.drc.euclidean);
  CHECK(f.out("drc euclidean") == "DRC measurements are Euclidean.\n");
  f.run("drc euclidean yes");  // anything but "on" disables
  CHECK_FALSE(f.editor.drc.euclidean);
  f.run("drc euclidean On");  // strcmp
  CHECK_FALSE(f.editor.drc.euclidean);
  f.run("drc euclidean on extra");
  CHECK(f.editor.drc.euclidean);
}

// --- drc style --------------------------------------------------------------------------

TEST_CASE("List styles", "[cap:drc-commands][req:drc style]") {
  DrcFixture f;
  f.editor.drc.styles = drc::DrcStyleList{{"drc(full)", "drc(fast)"}};
  f.editor.drc.style = "drc(full)";
  using L = cmd::Value::List;
  CHECK(f.run("drc listall style").value == cmd::Value{L{"drc(full)", "drc(fast)"}});
  CHECK(f.value("drc listall style") == "drc(full) drc(fast)");  // Tcl quotes no parentheses
  CHECK(f.value("drc list style") == "drc(full)");
  CHECK(f.out("drc style") == "The current style is \"drc(full)\".\n");
}

TEST_CASE("drc style selects a style and resets the halo and step size", "[cap:drc-commands][req:drc style]") {
  DrcFixture f;
  f.editor.drc.styles = drc::DrcStyleList{{"drc(full)", "drc(fast)"}};
  f.editor.drc.style = "drc(full)";
  f.editor.drc.halo_override = 1;
  f.editor.drc.step_size = 7;
  const auto r = f.run("drc style drc(fast)");
  CHECK(r.ok());
  CHECK(DrcFixture::texts(r).ends_with("DRC style is now \"drc(fast)\"\n"));
  CHECK(f.editor.drc.style == "drc(fast)");
  CHECK_FALSE(f.editor.drc.halo_override);
  CHECK_FALSE(f.editor.drc.step_size);

  // An unknown name keeps the style and lists the styles (DRCPrintStyle).
  const std::string out = f.out("drc style nosuch");
  CHECK(out.find("The current style is \"drc(fast)\".\nThe DRC styles are: drc(full), drc(fast).\n") !=
        std::string::npos);
  CHECK(f.editor.drc.style == "drc(fast)");
}

// --- drc printrules / rulestats / statistics -------------------------------------------

TEST_CASE("drc printrules writes the table to a file or reports it cannot", "[cap:drc-commands][req:drc printrules]") {
  DrcFixture f;
  CHECK(f.out("drc printrules rules.txt").empty());
  CHECK(f.services.view().files.exists("rules.txt"));

  DrcFixture disk(false);
  const std::string bad = (std::filesystem::temp_directory_path() / "no-such-dir-hefesto" / "x" / "rules.txt").string();
  CHECK(disk.out("drc printrules " + bad) == "Cannot write file " + bad + "\n");
}

TEST_CASE("Statistics header", "[cap:drc-commands][req:drc rulestats and statistics]") {
  DrcFixture f;
  (void)f.run("drc statistics");  // the totals are process-wide, as in Magic
  f.editor.drc.statistics.squares = 3;
  f.editor.drc.statistics.tiles = 5;
  const std::string out = f.out("drc statistics");
  CHECK(out.starts_with("Design-rule checker statistics (recent/total):\n    Squares processed: 3/"));
  for (const char* line : {"    Tiles processed: 5/", "    Edges pieces processed: 0/", "    Constraint areas checked: 0/",
                           "    Multi-tile constraints: 0/", "    Interaction areas processed: 0/",
                           "    Tiles processed for interactions: 0/", "    Tiles processed for arrays: 0/"}) {
    CHECK(out.find(line) != std::string::npos);
  }
  CHECK(f.editor.drc.statistics == drc::CheckStatistics{});  // the recent counters restart
  CHECK(f.out("drc statistics").find("    Squares processed: 0/") != std::string::npos);
}

TEST_CASE("drc rulestats prints the rule database statistics", "[cap:drc-commands][req:drc rulestats and statistics]") {
  DrcFixture f;
  CHECK(f.run("drc rulestats").ok());
  CHECK(f.out("drc rulestats x").starts_with("Wrong arguments in \"drc rulestats\" command:"));
}

// --- drc *halo and drc *stepsize ------------------------------------------------------------

TEST_CASE("Reduce halo", "[cap:drc-commands][req:drc *halo and drc *stepsize]") {
  DrcFixture f;
  style(f, 20);
  CHECK(f.out("drc *halo 5") == "Warning: rulechecking limited to halo of 5 internal units (5 DRC units).\n");
  CHECK(f.editor.drc.halo_override == 5);
  CHECK(f.out("drc *halo") ==
        "DRC halo is 5 internal units (5 DRC units)\nMaximum rule distance is 20 internal units (20 DRC units)\n");
  // Never above the style halo.
  CHECK(f.out("drc *halo 50") == "DRC checks all rules (halo of 20 internal units, or 20 DRC units)\n");
  CHECK(f.out("drc *halo") == "DRC halo is 20 internal units (20 DRC units)\n");
}

TEST_CASE("drc *halo reports DRC units through the style scale factor", "[cap:drc-commands][req:drc *halo and drc *stepsize]") {
  DrcFixture f;
  style(f, 20, 64, drc::ScaleFactor{3, 2});
  CHECK(f.out("drc *halo 5") == "Warning: rulechecking limited to halo of 5 internal units (7 DRC units).\n");
}

TEST_CASE("drc *stepsize sets and reports the step size", "[cap:drc-commands][req:drc *halo and drc *stepsize]") {
  DrcFixture f;
  style(f, 20, 320);
  CHECK(f.out("drc *stepsize") == "DRC step size is 320 internal units (320 DRC units)\n");
  CHECK(f.out("drc *stepsize 100") == "DRC step size is now 100 internal units (100 DRC units))\n");
  CHECK(f.editor.drc.step_size == 100);
  CHECK(f.out("drc *stepsize") ==
        "DRC step size is 100 internal units (100 DRC units)\n"
        "Recommended step size is 320 internal units (320 DRC units)\n");
}

TEST_CASE("drc *stepsize rejects a step size that is not positive (Q-DRC-24, both modes)",
          "[cap:drc-commands][req:drc *halo and drc *stepsize]") {
  // Regression: Magic stores 0 or a negative value and then divides by it
  // (SIGFPE) or loops forever; Hefesto kept it and silently checked with a
  // step of 1. The value is rejected and the current step size kept.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    DrcFixture f;
    f.compat(compat);
    style(f, 20, 320);
    CHECK(f.out("drc *stepsize 100") == "DRC step size is now 100 internal units (100 DRC units))\n");
    for (const char* word : {"0", "-5", "abc", "-0"}) {
      INFO(word);
      const auto r = f.run(std::string{"drc *stepsize "} + word);
      CHECK(r.ok());
      CHECK(DrcFixture::texts(r) == "Step size must be a positive integer.\n");
      CHECK(f.editor.drc.step_size == 100);
      CHECK(drc::current_step_size(f.editor) == 100);
    }
    CHECK(f.out("drc *stepsize") ==
          "DRC step size is 100 internal units (100 DRC units)\n"
          "Recommended step size is 320 internal units (320 DRC units)\n");
    // Without an earlier override the style's step size stays in effect.
    f.editor.drc.step_size.reset();
    CHECK(f.out("drc *stepsize 0") == "Step size must be a positive integer.\n");
    CHECK_FALSE(f.editor.drc.step_size.has_value());
    CHECK(f.out("drc *stepsize") == "DRC step size is 320 internal units (320 DRC units)\n");
  }
}

// --- drc *flatcheck and drc *showint ---------------------------------------------------------

TEST_CASE("No interactions", "[cap:drc-commands][req:drc *flatcheck and drc *showint]") {
  DrcFixture f;
  db::CellDef* top = f.cell("top");
  f.paint(*top, db::Rect{0, 0, 40, 40});
  f.show(*top);
  f.box(*top, db::Rect{0, 0, 40, 40});
  CHECK(f.out("drc *showint 10") == "No interactions in this area for that radius.\n");
  CHECK(f.editor.box == db::Rect{0, 0, 40, 40});
  CHECK(f.out("drc *showint -1") == "Radius must not be negative\n");
}

TEST_CASE("drc *flatcheck prints the error count", "[cap:drc-commands][req:drc *flatcheck and drc *showint]") {
  DrcFixture f;
  db::CellDef* top = f.cell("top");
  f.paint(*top, db::Rect{0, 0, 40, 40});
  f.show(*top);
  f.box(*top, db::Rect{0, 0, 40, 40});
  CHECK(f.out("drc *flatcheck") == "0 total errors found.\n");  // no rules
}

// --- Scripting results: the ? and ! macros run drc why and drc find -----------------------------

TEST_CASE("the ? and ! macros run drc why and drc find end to end",
          "[cap:drc-commands][req:Scripting results on iPadOS and non-Tcl hosts]") {
  DrcFixture f;
  f.whys({"m1 width"});
  db::CellDef* top = f.cell("top");
  f.paint(*top, db::Rect{0, 0, 40, 40});
  DrcFixture::error(*top, db::Rect{0, 0, 2, 2});
  DrcFixture::error(*top, db::Rect{10, 0, 12, 2});
  f.show(*top);
  f.box(*top, db::Rect{0, 0, 40, 40});
  drc::test::FakeRecheck fake;
  fake.errors = {{top, db::Rect{0, 0, 2, 2}, DrcFixture::rule(6)}, {top, db::Rect{10, 0, 12, 2}, DrcFixture::rule(6)}};
  const drc::ScopedRecheck scoped(fake.fn());
  ui::install_defaults(f.editor);
  const auto key = [](std::string_view name) { return ui::parse_key_name(name).value(); };

  const ui::KeyAction why = ui::handle_key(f.editor, key("?"));
  REQUIRE(why.kind == ui::KeyActionKind::run);
  auto results = ui::run_command_line(*f.dispatcher, why.text, cmd::Origin::key);
  REQUIRE(results.size() == 1);
  CHECK(DrcFixture::texts(results.front()) == "m1 width\n");

  const ui::KeyAction find = ui::handle_key(f.editor, key("!"));
  REQUIRE(find.kind == ui::KeyActionKind::run);
  results = ui::run_command_line(*f.dispatcher, find.text, cmd::Origin::key);
  REQUIRE_FALSE(results.empty());
  CHECK(DrcFixture::texts(results.front()) == "Error area #2:\nm1 width\n");  // Q-DRC-15 (magic-compat ON)
  for (const auto& r : results) {
    CHECK(r.ok());
  }
  CHECK(f.editor.box == drc::error_tiles(*top)[1]);
}
