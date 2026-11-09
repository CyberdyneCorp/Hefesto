// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The MODEL contract: headers compile together and every command is
// registered.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/db/transform.hpp"

namespace db = hefesto::db;

TEST_CASE("MODEL commands are registered", "[m1-model]") {
  hefesto::cmd::CommandRegistry registry;
  CHECK(db::register_db_commands(registry));
  for (const char* name : {"box", "snap", "paint", "erase", "splitpaint", "label", "setlabel", "port", "property",
                           "cellname", "instance", "array"}) {
    CHECK(registry.find(name) != nullptr);
  }
  // getcell and flatten may read cells from files: the CELL area registers
  // them and calls db::getcell_command / db::flatten_command.
  CHECK(registry.find("getcell") == nullptr);
  CHECK(registry.find("flatten") == nullptr);
}
