// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// app-startup: command-line options, file arguments, display selection.

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/startup.hpp"

namespace app = hefesto::app;
using Words = std::vector<std::string_view>;

namespace {

hefesto::diag::Outcome<app::StartupOptions> parse(Words words) {
  return app::parse_startup_options(words);
}

}  // namespace

TEST_CASE("unknown option prints Magic's message and usage line", "[cap:app-startup][req:Command-line options]") {
  const auto r = parse({"-x"});
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == app::codes::kOptionUnknown);
  CHECK(r.error().text.starts_with("Unknown option: '-x'\n"));
  CHECK(r.error().text.find(std::string{app::startup_usage_line()}) != std::string::npos);
  CHECK(app::startup_usage_line().starts_with("Usage:  magic [-g gPort]"));
}

TEST_CASE("ambiguous -n and the -no... options", "[cap:app-startup][req:Command-line options]") {
  const auto ambiguous = parse({"-no"});
  REQUIRE_FALSE(ambiguous.ok());
  CHECK(ambiguous.error().code == app::codes::kOptionAmbiguous);
  CHECK(ambiguous.error().text == "Ambiguous option -no:  use -nowindow, -noconsole, or -norcfile");
  const auto ok = parse({"-noconsole", "-nowindow", "-norcfile"});
  REQUIRE(ok.ok());
  CHECK(ok.value().no_console);
  CHECK(ok.value().no_window);
  CHECK(ok.value().no_rc);
  const auto other = parse({"-noxyz"});
  REQUIRE_FALSE(other.ok());
  CHECK(other.error().text == "Unknown option: '-noxyz'");  // no usage line for -n...
}

TEST_CASE("option arguments attached or as the next word", "[cap:app-startup][req:Command-line options]") {
  const auto attached = parse({"-dNULL", "-Tsky130A", "-m", "pale"});
  REQUIRE(attached.ok());
  CHECK(attached.value().display_type == "NULL");
  CHECK(attached.value().technology == "sky130A");
  CHECK(attached.value().monitor_type == "pale");
  const auto separate = parse({"-d", "NULL", "-T", "scmos"});
  REQUIRE(separate.ok());
  CHECK(separate.value().display_type == "NULL");
  CHECK(separate.value().technology == "scmos");
}

TEST_CASE("an option argument starting with '-' is rejected", "[cap:app-startup][req:Command-line options]") {
  const auto r = parse({"-T", "-d"});
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == app::codes::kOptionBadName);
  CHECK(r.error().text == "Bad name after '-T' option: '-d'");
  const auto missing = parse({"-T"});
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == app::codes::kOptionMissing);
  CHECK(missing.error().text == "-T requires a following technology");
}

TEST_CASE("option g sets the tablet port unless -i was given", "[cap:app-startup][req:Command-line options]") {
  const auto g_only = parse({"-g", "tty1"});
  REQUIRE(g_only.ok());
  CHECK(g_only.value().tablet_port == "tty1");
  const auto both = parse({"-i", "tty2", "-g", "tty1"});
  REQUIRE(both.ok());
  CHECK(both.value().graphics_port == "tty1");
  CHECK(both.value().tablet_port == "tty2");
}

TEST_CASE("option r recovers, option rcfile names the startup file", "[cap:app-startup][req:Command-line options]") {
  CHECK(parse({"-r"}).value().recover);
  CHECK(parse({"-recover"}).value().recover);
  const auto rc = parse({"-rcfile", "my.rc"});
  REQUIRE(rc.ok());
  CHECK(rc.value().rc_file == "my.rc");
  CHECK(rc.value().rc_name() == "my.rc");
  CHECK(parse({}).value().rc_name() == ".magicrc");
  const auto last = parse({"-rcfile"});  // no word follows: Magic's "Unknown option"
  REQUIRE_FALSE(last.ok());
  CHECK(last.error().text == "Unknown option: '-rcfile'");
  CHECK(parse({"-D"}).value().debug);
}

// Regression (SCRIPT-PDK-02): `-norc -rcfile FILE` skipped FILE; Magic's
// RCFileName is set by whichever of the two options comes last.
TEST_CASE("the last of -rcfile and -norcfile decides the startup file", "[cap:app-startup][req:Command-line options]") {
  const auto rc_last = parse({"-norc", "-rcfile", "bad.magicrc"});
  REQUIRE(rc_last.ok());
  CHECK_FALSE(rc_last.value().no_rc);
  CHECK(rc_last.value().rc_name() == "bad.magicrc");
  const auto norc_last = parse({"-rcfile", "bad.magicrc", "-norc"});
  REQUIRE(norc_last.ok());
  CHECK(norc_last.value().no_rc);
}

TEST_CASE("file arguments: .mag stripped, exact extensions, .tcl ends processing",
          "[cap:app-startup][req:Command-line file arguments]") {
  using K = app::FileKind;
  CHECK(app::classify_file_argument("inv.mag") == app::FileArgument{"inv", K::cell});
  CHECK(app::classify_file_argument("lib/top") == app::FileArgument{"lib/top", K::cell});
  CHECK(app::classify_file_argument("a.LEF") == app::FileArgument{"a.LEF", K::lef});
  CHECK(app::classify_file_argument("a.def") == app::FileArgument{"a.def", K::def});
  CHECK(app::classify_file_argument("a.cif") == app::FileArgument{"a.cif", K::cif});
  CHECK(app::classify_file_argument("a.GDS") == app::FileArgument{"a.GDS", K::gds});
  CHECK(app::classify_file_argument("a.gdsii").kind == K::cell);  // Q-UI-25: exact extensions only
  CHECK(app::classify_file_argument("a.gds.mag") == app::FileArgument{"a.gds", K::gds});

  const auto r = parse({"a.mag", "b", "run.tcl", "-x", "c.mag"});
  REQUIRE(r.ok());
  REQUIRE(r.value().files.size() == 3);
  CHECK(r.value().files[0] == app::FileArgument{"a", K::cell});
  CHECK(r.value().files[1] == app::FileArgument{"b", K::cell});
  CHECK(r.value().files[2] == app::FileArgument{"run.tcl", K::script});
  CHECK(r.value().script_args == std::vector<std::string>{"-x", "c.mag"});
}

TEST_CASE("file arguments are kept with -nowindow (Q-UI-25)", "[cap:app-startup][req:Command-line file arguments]") {
  const auto r = parse({"-nowindow", "inv.mag"});
  REQUIRE(r.ok());
  CHECK(r.value().no_window);
  REQUIRE(r.value().files.size() == 1);
  CHECK(r.value().files[0].name == "inv");
}

TEST_CASE("display type prefix rule and defaults", "[cap:app-startup][req:Display type selection]") {
  const auto nullx = app::resolve_display(std::string{"nullx"}, true);
  REQUIRE(nullx.ok());
  CHECK(nullx.value().name == "NULL");
  CHECK(nullx.value().headless);
  const auto ogl = app::resolve_display(std::string{" ogl"}, false);
  REQUIRE(ogl.ok());
  CHECK(ogl.value().name == "OGL");
  CHECK_FALSE(ogl.value().headless);
  CHECK(app::resolve_display(std::nullopt, true).value().name == "XWIND");
  CHECK(app::resolve_display(std::nullopt, false).value().headless);
}

TEST_CASE("unknown display lists the known types", "[cap:app-startup][req:Display type selection]") {
  const auto r = app::resolve_display(std::string{"FOO"}, false);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == app::codes::kDisplayUnknown);
  CHECK(r.error().text ==
        "Unknown display type:  FOO\n"
        "These display types are available in this version of Magic:\n"
        "        XWIND\n        X11\n        8BIT\n        16BIT\n        24BIT\n        OPEN_GL\n"
        "        OGL\n        OPENGL\n        CAIRO\n        XR\n        NULL\n"
        "Use '-d NULL' if you don't need graphics.");
}
