// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `tag` and the tag callback (tasks.md 5.2; capability scripting-api
// "Command tag callbacks", "Tag callback execution and substitution";
// Q-SCRIPT-3, fix in both magic-compat settings).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array>
#include <string>

#include "binding_fixture.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/binding/window_commands.hpp"

namespace script = hefesto::script;
using script::test::BindingFixture;

TEST_CASE("Appending tags", "[cap:scripting-api][req:Command tag callbacks]") {
  BindingFixture f;
  CHECK(f.value("tag select {magic::gencell_update %1}") == "");
  CHECK(f.value("magic::tag add select \"magic::mgrselect %r\"") == "");
  CHECK(f.value("magic::tag select") == "magic::gencell_update %1 ; magic::mgrselect %r");
  // `add` with an empty script is a no-op; `add` without a tag replaces.
  CHECK(f.value("tag add select {}; tag select") == "magic::gencell_update %1 ; magic::mgrselect %r");
  CHECK(f.value("tag ad box {set ::x 1}; tag box") == "set ::x 1");
  CHECK(f.value("tag replace box {set ::y 2}; tag box") == "set ::y 2");
}

TEST_CASE("Removing a tag", "[cap:scripting-api][req:Command tag callbacks]") {
  BindingFixture f;
  CHECK(f.value("tag box {set ::ran 1}; magic::tag box {}; magic::tag box") == "");
  CHECK(f.value("box values; info exists ::ran") == "0");
}

TEST_CASE("tag argument errors", "[cap:scripting-api][req:Command tag callbacks]") {
  BindingFixture f;
  CHECK(f.error("tag") == "");
  CHECK(f.error("tag a b c d e") == "");
  CHECK(f.error("tag frob box {x}") == "bad tag options \"frob\": must be add or replace");
  CHECK(f.error("tag Add box {x}") == "bad tag options \"Add\": must be add or replace");
  // A query of a command without a tag is empty.
  CHECK(f.value("tag nosuch") == "");
}

TEST_CASE("Argument substitution with placeholder", "[cap:scripting-api][req:Tag callback execution and substitution]") {
  BindingFixture f;
  f.value("proc magic::boxview {args} {set ::seen $args}");
  f.value("tag box {magic::boxview %W %1}");
  f.value("box");
  CHECK(f.value("set ::seen") == ".layout1.pane.top.magic {}");
  // The tag of `box` runs for `magic::box` and `::magic::box` too.
  f.value("magic::box values");
  CHECK(f.value("set ::seen") == ".layout1.pane.top.magic values");
  f.value("::magic::box size");
  CHECK(f.value("set ::seen") == ".layout1.pane.top.magic size");
}

TEST_CASE("Result passthrough", "[cap:scripting-api][req:Tag callback execution and substitution]") {
  BindingFixture f;
  f.value("proc magic::mgrupdate {args} {return ignored}");
  f.value("tag box {magic::mgrupdate %W %1}");
  CHECK(f.value("box values") == "0 0 10 20");
}

TEST_CASE("Result with spaces stays one word", "[cap:scripting-api][req:Tag callback execution and substitution]") {
  BindingFixture f;
  f.value("tag box {set ::r %r}");
  CHECK(f.value("box values") == "0 0 10 20");
  CHECK(f.value("set ::r") == "0 0 10 20");
  // Special characters are not evaluated.
  REQUIRE(f.editor.cells.create("a$b[1]", f.editor.technology->plane_count()).ok());
  f.value("tag cellname {set ::c %r}");
  CHECK(f.value("cellname list exists {a$b[1]}") == "a$b[1]");
  CHECK(f.value("set ::c") == "a$b[1]");
}

TEST_CASE("Tag result replaces the command result with %R",
          "[cap:scripting-api][req:Tag callback execution and substitution]") {
  BindingFixture f;
  f.value("tag box {string length %R}");
  CHECK(f.value("box values") == "9");
}

TEST_CASE("Literal percent in a tag script", "[cap:scripting-api][req:Tag callback execution and substitution]") {
  BindingFixture f;
  f.value("tag box {set ::p 50%%}");
  f.value("box");
  CHECK(f.value("set ::p") == "50%");
  // Any other escape and a final percent are copied unchanged.
  f.value("tag box {set ::q {%x 100%}}");
  f.value("box");
  CHECK(f.value("set ::q") == "%x 100%");
}

TEST_CASE("Tag error propagates", "[cap:scripting-api][req:Tag callback execution and substitution]") {
  BindingFixture f;
  f.value("tag paint {error boom}");
  CHECK(f.error("paint m1") == "boom");
  CHECK_FALSE(f.top->bbox().is_null());  // the paint stays applied
}

TEST_CASE("The tag keeps the command number", "[cap:scripting-api][req:Tag callback execution and substitution]") {
  BindingFixture f;
  f.value("tag box {units; units}");
  const std::uint64_t before = f.editor.command_count;
  f.value("box");
  CHECK(f.editor.command_count == before + 1);
}

TEST_CASE("Window-path tags name the path and number from the command",
          "[cap:scripting-api][req:Tag callback execution and substitution]") {
  BindingFixture f;
  const std::array<std::string, 1> paths{".layout1.pane.top.magic"};
  REQUIRE(script::register_window_commands(*f.interp, *f.dispatcher, paths).ok());
  f.value("tag box {set ::w [list %W %0 %1]}");
  f.value(".layout1.pane.top.magic box values");
  CHECK(f.value("set ::w") == ".layout1.pane.top.magic box values");
}

TEST_CASE("Substitution is a single pass with list-quoted values",
          "[cap:scripting-api][req:Tag callback execution and substitution]") {
  script::TagContext context;
  context.window = "";
  context.result = "a [b] $c";
  context.args = {"box", "x y"};
  CHECK(script::substitute_tag("f %W %r %1 %2 %5 %%W", context) == "f {} {a [b] $c} {x y} {} {} %W");
  CHECK(script::substitute_tag("%", context) == "%");
  CHECK(script::substitute_tag("%%%%", context) == "%%");
  // A substituted value that contains an escape is not rescanned.
  context.result = "%W";
  CHECK(script::substitute_tag("%r", context) == "%W");
  CHECK_FALSE(script::tag_resets_result("x %%R y"));
  CHECK(script::tag_resets_result("set a %R"));
}
