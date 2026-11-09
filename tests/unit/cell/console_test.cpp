// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Console messages and laziness of `load`, `save` and `getcell` as Magic
// 8.3.694 prints them in the golden CELL and MODEL cases (regressions of
// the golden replay fixes).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "command_session.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/view/commands.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
using cell::test::CommandSession;
using cell::test::has_text;

namespace {

constexpr const char* kLeaf = "magic\ntech tiny\ntimestamp 3\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n";
constexpr const char* kTop =
    "magic\ntech tiny\ntimestamp 9\nuse leaf leaf_0\ntimestamp 3\ntransform 1 0 0 0 1 0\nbox 0 0 4 4\n<< end >>\n";

std::vector<std::string> texts(const hefesto::cmd::CommandResult& r) {
  std::vector<std::string> out;
  for (const auto& d : r.diagnostics) {
    out.push_back(d.text);
  }
  return out;
}

}  // namespace

TEST_CASE("load of a missing cell prints strerror and creates the cell",
          "[cap:cell-library-resolution][req:Missing cell handling]") {
  CommandSession s;
  const auto r = s.run("load", {"a"});
  REQUIRE(r.ok());
  CHECK(texts(r) == std::vector<std::string>{"Cell a couldn't be read", "No such file or directory",
                                             "Creating new cell"});
}

TEST_CASE("load reads only the named cell and reports its directory",
          "[cap:cell-library-resolution][req:Cell file lookup order]") {
  CommandSession s;
  s.write("/work/in/leaf.mag", kLeaf);
  s.write("/work/in/top.mag", kTop);
  const auto r = s.run("load", {"/work/in/top"});
  REQUIRE(r.ok());
  CHECK(texts(r) == std::vector<std::string>{"Cell top read from path /work/in"});
  db::CellDef* leaf = s.editor.cells.find("leaf");
  REQUIRE(leaf != nullptr);
  CHECK_FALSE(leaf->flags().available);  // read when needed, as Magic's load does not expand
  CHECK(leaf->bbox() == db::Rect{0, 0, 4, 4});
}

TEST_CASE("save writes use paths relative to the file being written",
          "[cap:cell-file-format][req:Writer use blocks]") {
  CommandSession s;
  s.write("/work/in/leaf.mag", kLeaf);
  s.write("/work/in/top.mag", kTop);
  REQUIRE(s.run("load", {"/work/in/top"}).ok());
  REQUIRE(s.run("save", {"/work/top_out"}).ok());
  const std::string out = s.files().read("/work/top_out.mag").value();
  CHECK(out.find("use leaf  leaf_0 in\n") != std::string::npos);
}

TEST_CASE("a format error is reported before the post-read steps and the load creates the cell",
          "[cap:cell-file-format][req:Format errors and partial reads]") {
  CommandSession s;
  s.write("/work/a.mag", "MAGIC\ntech tiny\ntimestamp 0\n<< end >>\n");
  const auto r = s.run("load", {"a"});
  const auto t = texts(r);
  const auto pos = [&](std::string_view needle) {
    for (std::size_t i = 0; i < t.size(); ++i) {
      if (t[i].starts_with(needle)) {
        return i;
      }
    }
    return t.size();
  };
  REQUIRE(pos("File a contained format error") < t.size());
  CHECK(pos("File a contained format error") < pos("\"a\" has a zero timestamp"));
  CHECK(has_text(r, "Creating new cell"));
}

TEST_CASE("a technology mismatch on load leaves an empty new cell",
          "[cap:cell-file-format][req:Technology check on read]") {
  CommandSession s;
  s.write("/work/s.mag", "magic\ntech scmos\ntimestamp 3\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  const auto first = s.run("load", {"s"});
  REQUIRE(first.ok());
  CHECK(has_text(first, "Creating new cell"));
  const auto forced = s.run("load", {"s", "-force"});
  REQUIRE(forced.ok());
  CHECK(forced.diagnostics.empty());  // already available: nothing is read again
}

TEST_CASE("a cell read from its compressed file alone is not writeable",
          "[cap:cell-io-safety][req:Read-only cell detection]") {
  CommandSession s;
  // gzip of "magic\ntech tiny\ntimestamp 7\n<< end >>\n"
  constexpr unsigned char kGzip[] = {
      0x1f, 0x8b, 0x8,  0x0,  0x0,  0x0,  0x0,  0x0,  0x2,  0xff, 0xcb, 0x4d, 0x4c, 0xcf, 0x4c,
      0xe6, 0x2a, 0x49, 0x4d, 0xce, 0x50, 0x28, 0xc9, 0xcc, 0xab, 0xe4, 0x2a, 0xc9, 0xcc, 0x4d,
      0x2d, 0x2e, 0x49, 0xcc, 0x2d, 0x50, 0x30, 0xe7, 0xb2, 0xb1, 0x51, 0x48, 0xcd, 0x4b, 0x51,
      0xb0, 0xb3, 0xe3, 0x2,  0x0,  0xe3, 0x59, 0x6e, 0xc5, 0x26, 0x0,  0x0,  0x0};
  std::string data;
  for (const unsigned char b : kGzip) {
    data.push_back(static_cast<char>(b));
  }
  s.write("/work/inv.mag.gz", data);
  const auto r = s.run("load", {"inv"});
  REQUIRE(r.ok());
  CHECK(has_text(r, "Warning: cell <inv> from file /work/inv.mag is not writeable"));
  CHECK(s.editor.cells.find("inv")->flags().no_edit);
}

TEST_CASE("getcell reads the cell it places and the box covers the instance",
          "[cap:layout-hierarchy][req:Cell instances]") {
  CommandSession s;
  s.write("/work/lib/c.mag", kLeaf);
  REQUIRE(s.run("load", {"top"}).ok());
  s.editor.box = db::Rect{10, 0, 10, 0};
  REQUIRE(s.registry.find("getcell") != nullptr);
  const auto r = s.run("getcell", {"/work/lib/c"});
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "c_0");
  CHECK(has_text(r, "Cell c read from path /work/lib"));
  CHECK(*s.editor.box == db::Rect{10, 0, 14, 4});
}

TEST_CASE("expanding an instance reads its unavailable cell through the CELL reader (UI-G-037)",
          "[cap:ui-windows-and-views][req:Per-window expansion state]") {
  CommandSession s;
  REQUIRE(hefesto::view::register_view_commands(s.registry));
  s.write("/work/in/leaf.mag", kLeaf);
  s.write("/work/in/top.mag", kTop);
  REQUIRE(s.editor.windows.open_frame(nullptr).ok());
  REQUIRE(s.run("load", {"/work/in/top"}).ok());
  s.editor.box = db::Rect{0, 0, 10, 10};
  const auto r = s.run("expand");
  REQUIRE(r.ok());
  CHECK(has_text(r, "Cell leaf read from path /work/in"));
  CHECK(s.editor.cells.find("leaf")->flags().available);
}
