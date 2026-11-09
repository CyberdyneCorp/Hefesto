// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The command-script save variant with the Q-CELL-14 fixes: instance
// transforms, arrays, locks and paths, and port use/class/shape lines.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "cell_fixture.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
using cell::test::CellIo;

namespace {

bool contains(const std::string& text, std::string_view part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE("instances keep their transform, array, lock and path in the script",
          "[cap:cell-file-format][req:Command-script content]") {
  CellIo io;
  db::CellDef& top = io.read("top",
                             "magic\ntech tiny\ntimestamp 9\n"
                             "use inv inv_0 /lib\ntimestamp 3\ntransform 0 1 100 -1 0 50\nbox 0 0 4 4\n"
                             "use inv *inv_1\narray 0 2 10 0 1 20\ntimestamp 3\ntransform 1 0 7 0 -1 8\nbox 0 0 4 4\n"
                             "<< end >>\n");
  REQUIRE(top.uses().size() == 2);
  const std::string script = cell::write_command_script(io.ctx, top);
  CHECK(contains(script, "getcell /lib/inv child 0 0 parent 100 50 90 0 0\nidentify inv_0\n"));
  CHECK(contains(script, "getcell /lib/inv child 0 0 parent 7 8 v 0 0\nidentify inv_1\n"
                         "box values 0 0 10 20\narray 0 2 0 1\ninstance lock inv_1\n"));
  CHECK_FALSE(contains(script, "box position"));
}

TEST_CASE("a rotated array gives its indices and spacing in the parent frame",
          "[cap:cell-file-format][req:Command-script content]") {
  CellIo io;
  db::CellDef& top = io.read("top",
                             "magic\ntech tiny\ntimestamp 9\n"
                             "use inv inv_0\narray 0 3 10 0 1 20\ntimestamp 3\ntransform 0 1 0 1 0 0\nbox 0 0 4 4\n"
                             "<< end >>\n");
  const std::string script = cell::write_command_script(io.ctx, top);
  // Parent-frame spacing (20, 10); DBMakeArray swaps the indices back for a.
  CHECK(contains(script, "getcell inv child 0 0 parent 0 0 90v 0 0\nidentify inv_0\nbox values 0 0 20 10\n"
                         "array 0 1 0 3\n"));
}

TEST_CASE("ports write their use, class and shape", "[cap:cell-file-format][req:Command-script content]") {
  CellIo io;
  db::CellDef& c = io.read("c",
                           "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\nrect 0 0 4 4\n"
                           "<< labels >>\nrlabel metal1 s 0 0 4 4 0 A\nport 1 nsew reset input abutment\n"
                           "rlabel metal1 0 0 4 4 0 B\nport 2 n\n<< end >>\n");
  const std::string script = cell::write_command_script(io.ctx, c);
  CHECK(contains(script, "port make 1\nport connections n s e w\nport 1 use reset\nport 1 class input\n"
                         "port 1 shape abutment\n"));
  CHECK(contains(script, "select area label\nport make 2\nport connections n\nselect clear\n"));
}
