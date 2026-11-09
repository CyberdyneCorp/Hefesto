// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M1 scaffolding: the shared editor state and the full command set
// (requirement-tagged app tests are in the other files of this directory).

#include <catch2/catch_test_macros.hpp>

#include "hefesto/app/project_file.hpp"
#include "hefesto/app/settings_stack.hpp"
#include "hefesto/app/startup.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace app = hefesto::app;

TEST_CASE("an editor state starts with the built-in technology", "[m1-scaffold]") {
  hefesto::editor::EditorState state;
  REQUIRE(state.technology != nullptr);
  CHECK(state.technology->name() == "minimum");
  CHECK(state.cells.size() == 0);
  CHECK(state.windows.current() == nullptr);
}

TEST_CASE("every M1 command registers without name clashes", "[m1-scaffold]") {
  hefesto::cmd::CommandRegistry registry;
  CHECK(hefesto::cmd::register_all_commands(registry));
  for (const char* name : {"help", "tech", "paint", "load", "expand", "pdk"}) {
    INFO(name);
    CHECK(registry.find(name) != nullptr);
  }
}
