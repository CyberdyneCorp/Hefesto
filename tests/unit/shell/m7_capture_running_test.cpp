// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// --capture-running for the Extract button's running capture
// (openspec/changes/m7-extraction task 16.1, design D19): with an --input
// script it arms once that script starts, so the Interrupt card of the
// --run script (`load big`, 8 s on the runner) is not the frame captured;
// without --input it arms at once, as before. Also: the Extract slot of the
// deferred actions no longer carries the M1 "arrives in M7" tooltip.
// Owner: SHELL.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "hefesto/shell/input_script.hpp"
#include "hefesto/shell/view_models.hpp"

namespace shell = hefesto::shell;

#define CAPTURE_RUNNING_TAGS "[cap:hefesto-app-ux][req:Main window layout]"

TEST_CASE("capture-running arms only once the --input script starts", CAPTURE_RUNNING_TAGS) {
  shell::InputPlayer player(shell::parse_input_script("wait 500; down 10,10; up 10,10; wait 500").steps);
  // The --run script (load) runs before the first step: no capture yet.
  CHECK_FALSE(player.started());
  CHECK_FALSE(shell::capture_running_armed(&player));
  // The first take_due plays the leading wait: the script has started.
  CHECK(player.take_due(0.0).empty());
  CHECK(player.started());
  CHECK(shell::capture_running_armed(&player));
  CHECK(player.take_due(600.0).size() == 2);  // the click
  CHECK(shell::capture_running_armed(&player));
}

TEST_CASE("capture-running without an input script arms at once", CAPTURE_RUNNING_TAGS) {
  CHECK(shell::capture_running_armed(nullptr));
}

TEST_CASE("the live Extract button has no deferred-milestone tooltip", CAPTURE_RUNNING_TAGS) {
  const auto& actions = shell::deferred_actions();
  const auto extract = std::find_if(actions.begin(), actions.end(), [](const auto& c) { return c.label == "Extract"; });
  REQUIRE(extract != actions.end());
  CHECK(extract->tooltip.empty());
}
