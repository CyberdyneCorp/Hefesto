// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `writeall` over the cells a GDS or CIF read leaves behind: the modified
// "(UNNAMED)" start-up cell (no name to write under) and a used but
// undefined cell, which is not available (cmdSaveCell, cmdWriteallFunc,
// DBCellWrite; golden MASK-GR-001, MASK-CF-012, MASK-GR-045).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "hefesto/cell/diag_codes.hpp"
#include "hier_session.hpp"

namespace cell = hefesto::cell;
namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
using Session = cell::test::HierSession;

namespace {

std::vector<std::string> texts(const cmd::CommandResult& r) {
  std::vector<std::string> out;
  for (const auto& d : r.diagnostics) {
    out.push_back(d.text);
  }
  return out;
}

db::CellDef* modified_unnamed(Session& s) {
  db::CellDef* unnamed = s.cell("(UNNAMED)");
  db::mark_modified(*unnamed);
  return unnamed;
}

/// `top` places `u`, which was used but never defined (not available).
db::CellDef* undefined_child(Session& s) {
  auto made = s.editor.cells.create("u", s.editor.technology->plane_count());
  REQUIRE(made.ok());
  made.value()->flags().needs_new_timestamp = true;  // as the read leaves it
  s.place(*s.top, *made.value(), db::kIdentity, "u_0");
  return made.value();
}

}  // namespace

TEST_CASE("writeall force refuses the modified (UNNAMED) cell, which has no name",
          "[cap:cmd-hierarchy][req:Writeall command][golden:MASK-GR-001]") {
  Session s;
  db::CellDef* unnamed = modified_unnamed(s);
  const auto r = s.run("writeall force");
  REQUIRE(r.ok());
  const std::vector<std::string> expected{"Writing '(UNNAMED)'", "Must specify name for cell (UNNAMED).",
                                          "Can't write file named '(UNNAMED)'"};
  const std::vector<std::string> got = texts(r);
  const auto at = std::search(got.begin(), got.end(), expected.begin(), expected.end());
  CHECK(at != got.end());
  CHECK(r.diagnostics.back().code == cell::codes::kWriteRefused);
  CHECK_FALSE(s.services.files().exists("(UNNAMED).mag"));
  CHECK(unnamed->flags().modified);
}

TEST_CASE("writeall of a named (UNNAMED) asks for a file name and is abandoned without an answer",
          "[cap:cmd-hierarchy][req:Writeall command]") {
  Session s;
  db::CellDef* unnamed = modified_unnamed(s);
  const auto r = s.run("writeall", {"force", "(UNNAMED)"});
  REQUIRE(r.ok());
  CHECK(texts(r) == std::vector<std::string>{"Must specify name for cell (UNNAMED).", "Cell not saved."});
  CHECK_FALSE(s.services.files().exists("(UNNAMED).mag"));
  CHECK(unnamed->flags().modified);
}

TEST_CASE("writeall force writes an undefined cell, which reads its own empty file, with magic-compat ON",
          "[cap:cmd-hierarchy][req:Writeall command][golden:MASK-CF-012]") {
  Session s{true};
  db::CellDef* u = undefined_child(s);
  const auto r = s.run("writeall force");
  REQUIRE(r.ok());
  CHECK(cell::test::mentions(r, "Writing 'u'"));
  CHECK(cell::test::mentions(r, "File u contained format error"));
  CHECK(cell::test::mentions(r, "\"u\" has a zero timestamp; it should be written out"));
  CHECK(u->flags().available);
  CHECK(u->timestamp() == 0);  // the read of the empty file
  const auto child = s.services.files().read("u.mag");
  REQUIRE(child.ok());
  CHECK(child.value().starts_with("magic\ntech tiny\ntimestamp "));
  CHECK(child.value().find("timestamp 0\n") == std::string::npos);  // the header has the new stamp
  const auto parent = s.services.files().read("top.mag");
  REQUIRE(parent.ok());
  CHECK(parent.value().find("use u  u_0\ntimestamp 0\n") != std::string::npos);
}

TEST_CASE("writeall force leaves an undefined cell unwritten with magic-compat OFF",
          "[cap:cmd-hierarchy][req:Writeall command]") {
  Session s{false};
  db::CellDef* u = undefined_child(s);
  const auto r = s.run("writeall force");
  REQUIRE(r.ok());
  CHECK_FALSE(cell::test::mentions(r, "Writing 'u'"));
  CHECK_FALSE(s.services.files().exists("u.mag"));
  CHECK_FALSE(u->flags().available);
}
