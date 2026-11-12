// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The --input script: pointer and key events injected through Raven for
// captures of pointer- and key-driven states (openspec/changes/m4-scripting
// tasks 11.2-11.7).

#include "hefesto/shell/input_script.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include <vector>

#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/options.hpp"

using namespace hefesto::shell;

TEST_CASE("an input script parses pointer, key, text and wait steps",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  const InputScriptParse parsed =
      parse_input_script("move 10,20; down 30.5,40 right ; up 50,60;key ctrl+shift+z; key Escape; text M1 ; wait 600");
  REQUIRE(parsed.error.empty());
  REQUIRE(parsed.steps.size() == 7);
  CHECK(parsed.steps[0].kind == ScriptKind::move);
  CHECK(parsed.steps[0].x == 10.0F);
  CHECK(parsed.steps[0].y == 20.0F);
  CHECK(parsed.steps[1].kind == ScriptKind::down);
  CHECK(parsed.steps[1].x == 30.5F);
  CHECK(parsed.steps[1].button == ScriptButton::right);
  CHECK(parsed.steps[2].kind == ScriptKind::up);
  CHECK(parsed.steps[2].button == ScriptButton::left);
  CHECK(parsed.steps[3].key == 'Z');
  CHECK(parsed.steps[3].modifiers == (kModCtrl | kModShift));
  CHECK(parsed.steps[4].key == raven_key::kEscape);
  CHECK(parsed.steps[4].modifiers == 0);
  CHECK(parsed.steps[5].text == "M1");
  CHECK(parsed.steps[6].wait_ms == 600);
}

TEST_CASE("key names map to Raven key codes", "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  CHECK(input_key_code("Return") == raven_key::kEnter);
  CHECK(input_key_code("esc") == raven_key::kEscape);
  CHECK(input_key_code("F1") == raven_key::kF1);
  CHECK(input_key_code("f12") == raven_key::kF12);
  CHECK(input_key_code("a") == 'A');
  CHECK(input_key_code("7") == '7');
  CHECK_FALSE(input_key_code("F13"));
  CHECK_FALSE(input_key_code("F1.5"));
  CHECK_FALSE(input_key_code("nosuchkey"));
  CHECK_FALSE(input_key_code("#"));
}

TEST_CASE("a bad input step is refused with its text",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  for (const std::string_view bad :
       {"jump 1,2", "move 1", "move a,b", "down 1,2 sideways", "key", "key hyper+a", "text", "wait -1", "wait x"}) {
    const InputScriptParse parsed = parse_input_script(bad);
    CHECK_FALSE(parsed.error.empty());
    CHECK(parsed.steps.empty());
  }
  CHECK(parse_input_script("move 1,2; jump").error == "bad input step: jump");
  const std::vector<std::string_view> args{"--input", "move 1,2; wait 5"};
  const ParseResult ok = parse_shell_options(args);
  REQUIRE(ok.options);
  CHECK(ok.options->input == "move 1,2; wait 5");
  const std::vector<std::string_view> wrong{"--input", "move 1"};
  CHECK_FALSE(parse_shell_options(wrong).options);
}

TEST_CASE("the input player runs the steps up to each wait, then waits",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  InputPlayer player(parse_input_script("down 1,1; wait 500; up 1,1; key Escape; wait 100").steps);
  std::vector<InputStep> due = player.take_due(1000.0);
  REQUIRE(due.size() == 1);
  CHECK(due[0].kind == ScriptKind::down);
  CHECK_FALSE(player.done());
  CHECK(player.take_due(1499.0).empty());  // the hold is not over
  due = player.take_due(1500.0);
  REQUIRE(due.size() == 2);
  CHECK(due[0].kind == ScriptKind::up);
  CHECK(due[1].kind == ScriptKind::key);
  CHECK_FALSE(player.done());  // the trailing wait still runs
  CHECK(player.take_due(1550.0).empty());
  CHECK_FALSE(player.done());
  CHECK(player.take_due(1600.0).empty());
  CHECK(player.done());
}
