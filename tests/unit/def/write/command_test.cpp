// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `def write` arguments, output naming and preconditions (capability
// def-write "def command argument validation", "Output file naming",
// "Preconditions and diagnostics"; tasks.md 8.1; Q-LEFDEF-90, 104, 110).
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

#include "write_fixture.hpp"

namespace def_write_test {
namespace {

/// A file system whose writes to some names fail.
class FailingFiles final : public platform::FileSystem {
 public:
  FailingFiles(platform::FileSystem& inner, std::set<std::string> failing)
      : inner_(inner), failing_(std::move(failing)) {}
  diag::Outcome<std::string> read(const platform::Path& p) override { return inner_.read(p); }
  diag::Status write_atomic(const platform::Path& p, std::string_view data) override {
    if (failing_.contains(p.string())) {
      return diag::make("PLATFORM-FS-001", "Permission denied");
    }
    return inner_.write_atomic(p, data);
  }
  diag::Status append(const platform::Path& p, std::string_view data) override { return inner_.append(p, data); }
  bool exists(const platform::Path& p) override { return inner_.exists(p); }
  diag::Outcome<std::uint64_t> size(const platform::Path& p) override { return inner_.size(p); }
  diag::Outcome<std::vector<platform::Path>> list(const platform::Path& d) override { return inner_.list(d); }
  diag::Status create_directories(const platform::Path& d) override { return inner_.create_directories(d); }
  diag::Status rename(const platform::Path& from, const platform::Path& to) override {
    return inner_.rename(from, to);
  }
  diag::Status remove(const platform::Path& p) override { return inner_.remove(p); }

 private:
  platform::FileSystem& inner_;
  std::set<std::string> failing_;
};

TEST_CASE("a file name only right after write; a misplaced number prints the help",
          "[cap:def-write][req:def command argument validation]") {
  Session s;
  s.cell("top");
  REQUIRE(s.run("defw write -hide 5").ok());
  CHECK(s.said("The \"-hide\" option is only for lef write"));
  CHECK(std::any_of(s.messages.begin(), s.messages.end(),
                    [](const diag::Diagnostic& d) { return d.text.starts_with("The \"defw\" options are:\n"); }));
  CHECK_FALSE(s.exists("top.def"));
}

TEST_CASE("the lef write options are noted and ignored", "[cap:def-write][req:def command argument validation]") {
  Session s;
  s.cell("top");
  REQUIRE(s.run("defw write -tech -pinonly -toplayer -nomaster").ok());
  for (const char* opt : {"-tech", "-pinonly", "-toplayer", "-nomaster"}) {
    CHECK(s.said("The \"" + std::string{opt} + "\" option is only for lef write"));
  }
  CHECK(s.exists("top.def"));
}

TEST_CASE("def writeall writes nothing", "[cap:def-write][req:def writeall is not supported]") {
  Session s;
  s.cell("top");
  REQUIRE(s.run("def writeall").ok());
  CHECK(s.said("Sorry, can't write hierarchical DEF at this time."));
  CHECK(s.said("Try \"def write\""));
  CHECK_FALSE(s.exists("top.def"));
}

TEST_CASE("an unknown option prints the help and writes nothing",
          "[cap:def-write][req:def command argument validation]") {
  Session s;
  s.cell("top");
  REQUIRE(s.run("defw write -bogus").ok());
  CHECK_FALSE(s.exists("top.def"));
  REQUIRE(s.run("defw write a b").ok());
  CHECK_FALSE(s.exists("a.def"));
}

TEST_CASE("-allspecXYZ is -allspecial; -alls is not", "[cap:def-write][req:def write command]") {
  Session s;
  s.cell("top");
  REQUIRE(s.run("defw write x -allspecialXYZ").ok());
  CHECK(s.exists("x.def"));
  REQUIRE(s.run("defw write y -alls").ok());
  CHECK_FALSE(s.exists("y.def"));
}

TEST_CASE("-units: a value that is not an integer leaves 1000",
          "[cap:def-write][req:def command argument validation]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 10, 10});
  REQUIRE(s.run("defw write -units abc").ok());
  CHECK(s.said("The \"-units\" option requires an argument."));
  CHECK(s.read("top.def").find("UNITS DISTANCE MICRONS 1000 ;") != std::string::npos);
}

// Regression (Q-LEFDEF-104 fix): Magic reads the argument past the end of
// the list when -units is the last word (StrIsInt(NULL)).
TEST_CASE("-units with no value is reported, and the count sends it to the help",
          "[cap:def-write][req:def command argument validation]") {
  Session s;
  s.cell("top");
  REQUIRE(s.run("defw write -units").ok());
  CHECK(s.said("The \"-units\" option requires an argument."));
  CHECK_FALSE(s.exists("top.def"));
}

// Regression (Q-LEFDEF-104 fix): a value <= 0 made every coordinate 0 or
// negative.
TEST_CASE("-units of 0 or less is refused", "[cap:def-write][req:def command argument validation]") {
  Session s;
  s.cell("top");
  for (const char* value : {"0", "-5"}) {
    INFO(value);
    const auto result = s.run(std::string{"defw write -units "} + value);
    REQUIRE_FALSE(result.ok());
    CHECK(result.diagnostics.back().code == "DEF-WRITE-011");
    CHECK_FALSE(s.exists("top.def"));
  }
}

TEST_CASE("output file naming", "[cap:def-write][req:Output file naming]") {
  Session s;
  s.cell("top");
  REQUIRE(s.run("defw write result.txt").ok());
  CHECK(s.exists("result.txt"));
  REQUIRE(s.run("defw write chip").ok());
  CHECK(s.exists("chip.def"));
  REQUIRE(s.run("defw write /t/out.def").ok());
  CHECK(s.exists("/t/out.def"));
  CHECK_FALSE(s.exists("/t/out.def.def"));
}

TEST_CASE("a foreign extension that cannot be opened becomes .def", "[cap:def-write][req:Output file naming]") {
  Session s;
  db::CellDef& top = s.cell("top");
  FailingFiles files(s.services.view().files, {"r.txt"});
  def::DefWriteOptions o;
  o.files = &files;
  REQUIRE(def::write(s.editor, top, "r.txt", o).ok());
  CHECK(s.exists("r.def"));
}

TEST_CASE("an output file that cannot be opened writes nothing and changes nothing",
          "[cap:def-write][req:Preconditions and diagnostics]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "via1", db::Rect{10, 10, 36, 36});
  FailingFiles files(s.services.view().files, {"top.def"});
  def::DefWriteOptions o;
  o.files = &files;
  const auto result = def::write(s.editor, top, "top", o);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.diagnostics.size() == 2);
  CHECK(result.diagnostics[0].text == "Generating DEF output top.def for cell top:");
  CHECK(result.diagnostics[1].text == "Cannot open output file top.def (Permission denied).");
  CHECK(s.editor.lefdef.vias.size() == 0);  // no generated via key was left behind
}

TEST_CASE("an unnamed cell is refused", "[cap:def-write][req:Preconditions and diagnostics]") {
  Session s;
  db::CellDef& unnamed = s.cell("(UNNAMED)");
  const auto result = s.write(unnamed, "");
  REQUIRE_FALSE(result.ok());
  CHECK(result.diagnostics.back().text == "Please name the cell before generating DEF.");
}

TEST_CASE("no .ext file: the nets are left out with Magic's warning",
          "[cap:def-write][req:Preconditions and diagnostics]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 10, 3});
  s.label(top, Pin{"A", "metal1", db::Rect{0, 0, 10, 3}});
  REQUIRE(s.write(top, "").ok());
  CHECK(s.said("Warning:  Circuit has no .ext file;  no nets written."));
  CHECK(s.said("Run extract on this circuit if you want nets in the output."));
  const std::string text = s.read("top.def");
  CHECK(text.find("NETS") == std::string::npos);
  CHECK(text.find("PINS 1 ;") != std::string::npos);
  CHECK(text.ends_with("END PINS\n\nEND DESIGN\n\n"));
}

constexpr std::string_view kExtHeader =
    "timestamp 100\nversion 8.3\ntech lefdef\nstyle lefdef\nscale 1000 1000 1\n"
    "resistclasses 1000 500 100 50 30 20\n";

TEST_CASE("a stale .ext is used in both modes, and reported with magic-compat OFF (Q-LEFDEF-110)",
          "[cap:def-write][req:Preconditions and diagnostics]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    db::CellDef& top = s.cell("top");
    top.set_timestamp(200);
    s.paint(top, "metal1", db::Rect{0, 0, 100, 3});
    s.file("/t/top.ext", std::string{kExtHeader} + "node \"clk\" 1 6 0 0 m1 0 0 0 0 0 0 0 0 0 0 0 0\n");
    REQUIRE(s.write(top, "").ok());
    CHECK(s.read("top.def").find("   - clk\n") != std::string::npos);
    CHECK(s.said("top.ext is out of date; nets may be wrong. Run extract.") == !on);
  }
}

}  // namespace
}  // namespace def_write_test
