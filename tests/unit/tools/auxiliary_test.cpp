// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The auxiliary scripting tools (capability scripting-auxiliary-tools;
// Q-SCRIPT-12, Q-SCRIPT-30).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/tools/auxiliary.hpp"
#include "hefesto/tools/diag_codes.hpp"
#include "tools_fixture.hpp"

namespace tools = hefesto::tools;
using tools::test::Session;

TEST_CASE("the stripper removes GDS_START and GDS_END lines of every .mag file",
          "[cap:scripting-auxiliary-tools][req:Reference-library stripper]") {
  Session s;
  auto& files = s.services.files();
  // "Strip offsets".
  s.write("/lib/inv.mag",
          "magic\ntech scmos\nstring GDS_FILE cells.gds\nstring GDS_START 1024\nstring GDS_END 4096\n<< end >>\n");
  s.write("/lib/nand.mag", "magic\r\nstring GDS_START 1\r\nstring GDS_ENDX\r\nstring GDS_STARTED\n<< end >>");
  s.write("/lib/notes.txt", "string GDS_START 1\n");
  std::vector<std::string> stripped;
  const auto diagnostics = tools::strip_reflibs(files, "/lib", &stripped);
  CHECK(diagnostics.empty());
  CHECK(stripped == std::vector<std::string>{"inv.mag", "nand.mag"});
  CHECK(files.read("/lib/inv.mag").value() == "magic\ntech scmos\nstring GDS_FILE cells.gds\n<< end >>\n");
  // Prefix matches (`^string GDS_START`), CR LF read as line ends, LF written.
  CHECK(files.read("/lib/nand.mag").value() == "magic\n<< end >>\n");
  CHECK(files.read("/lib/notes.txt").value() == "string GDS_START 1\n");  // not a .mag file
  CHECK(tools::strip_reflib_lines("") == "");
  // A directory that cannot be listed matches nothing.
  CHECK(tools::strip_reflibs(files, "/none").empty());
}

TEST_CASE("the stripper reports a file it cannot rewrite and continues",
          "[cap:scripting-auxiliary-tools][req:Reference-library stripper]") {
  // A file system whose writes fail for one file.
  class FailingWrites final : public hefesto::platform::FileSystem {
   public:
    hefesto::platform::MemoryFileSystem inner;
    hefesto::diag::Outcome<std::string> read(const hefesto::platform::Path& p) override { return inner.read(p); }
    hefesto::diag::Status write_atomic(const hefesto::platform::Path& p, std::string_view d) override {
      if (p.filename() == "bad.mag") {
        return hefesto::diag::make("PLATFORM-IO-001", "denied");
      }
      return inner.write_atomic(p, d);
    }
    hefesto::diag::Status append(const hefesto::platform::Path& p, std::string_view d) override {
      return inner.append(p, d);
    }
    bool exists(const hefesto::platform::Path& p) override { return inner.exists(p); }
    hefesto::diag::Outcome<std::uint64_t> size(const hefesto::platform::Path& p) override { return inner.size(p); }
    hefesto::diag::Outcome<std::vector<hefesto::platform::Path>> list(const hefesto::platform::Path& p) override {
      return inner.list(p);
    }
    hefesto::diag::Status create_directories(const hefesto::platform::Path& p) override {
      return inner.create_directories(p);
    }
    hefesto::diag::Status rename(const hefesto::platform::Path& a, const hefesto::platform::Path& b) override {
      return inner.rename(a, b);
    }
    hefesto::diag::Status remove(const hefesto::platform::Path& p) override { return inner.remove(p); }
  };
  FailingWrites files;
  REQUIRE(files.inner.write_atomic("/d/bad.mag", "string GDS_END 1\n").ok());
  REQUIRE(files.inner.write_atomic("/d/good.mag", "string GDS_END 1\nx\n").ok());
  const auto diagnostics = tools::strip_reflibs(files, "/d");
  REQUIRE(diagnostics.size() == 1);
  CHECK(diagnostics[0].code == tools::codes::kStripOpen);
  CHECK(diagnostics[0].severity == hefesto::diag::Severity::info);  // `puts stdout`
  CHECK(diagnostics[0].text == "Error: can't open file bad.mag");
  CHECK(files.inner.read("/d/good.mag").value() == "x\n");
}

TEST_CASE("mazeroute is registered and says it arrives with the maze router (M10)",
          "[cap:scripting-auxiliary-tools]") {
  // Q-SCRIPT-30: the "Netlist maze-routing script" requirement is tested
  // with router-maze-iroute in M10; until then the command names it.
  Session s;
  const auto r = s.run("mazeroute f.net");
  REQUIRE_FALSE(r.ok());
  REQUIRE_FALSE(r.diagnostics.empty());
  CHECK(r.diagnostics.front().code == tools::codes::kMazeRoute);
  CHECK(r.diagnostics.front().text.find("router-maze-iroute") != std::string::npos);
  CHECK(r.diagnostics.front().text.find("M10") != std::string::npos);
  CHECK(s.run("magic::mazeroute").diagnostics.front().code == tools::codes::kMazeRoute);
}

TEST_CASE("the socket server, BSI helpers, rev-0 toolkit and tech builder are not loaded",
          "[cap:scripting-auxiliary-tools]") {
  // Informative requirements "Socket command server is opt-in", "BSI menu
  // helpers", "Legacy gencell toolkit revision 0", "Technology builder
  // prototype": none of their commands exists.
  Session s;
  for (const std::string_view name : tools::not_loaded_commands()) {
    INFO(name);
    CHECK(s.registry.find(name) == nullptr);
    CHECK(s.run("magic::" + std::string{name}).diagnostics.front().text ==
          "invalid command name \"magic::" + std::string{name} + "\"");
  }
  // "Legacy naming": the rev-0 toolkit names the first free `<type>_<n>`.
  const std::vector<std::string> existing = {"pfet_1", "pfet_2", "nfet_1"};
  CHECK(tools::rev0_cell_name("pfet", existing) == "pfet_3");
  CHECK(tools::rev0_cell_name("nfet", existing) == "nfet_2");
  CHECK(tools::rev0_cell_name("cap", existing) == "cap_1");
}

TEST_CASE("console prompts and the launcher batch mode", "[cap:scripting-auxiliary-tools]") {
  // Informative "Console integration" and "Interpreter launchers".
  CHECK(tools::console_prompt(false, false) == '%');
  CHECK(tools::console_prompt(true, false) == ']');
  CHECK(tools::console_prompt(true, true) == ':');
  // "Batch mode flag": `magic -dnull -noconsole`.
  CHECK(tools::launcher_batch_mode(true, true));
  CHECK_FALSE(tools::launcher_batch_mode(true, false));
  CHECK_FALSE(tools::launcher_batch_mode(false, true));
}

// Regression (SCRIPT-AUX-03, SCRIPT-PDK-03): `--magic -dnull -noconsole`
// left `batch_mode` and `argv` undefined.
TEST_CASE("the launcher's interpreter globals", "[cap:scripting-auxiliary-tools][req:Interpreter launchers]") {
  const std::vector<std::string> batch = {"-dnull", "-noconsole", "-norc", "s.tcl", "a.gds", "-foo"};
  const tools::LauncherVariables dnull = tools::launcher_variables(batch);
  CHECK(dnull.batch_mode);
  CHECK(dnull.argv ==
        std::vector<std::string>{"-nowrapper", "-dnull", "-noconsole", "-norc", "s.tcl", "a.gds", "-foo"});
  // -noconsole alone: magicexec -- <args>.
  const std::vector<std::string> wish = {"-noconsole", "s.tcl"};
  CHECK_FALSE(tools::launcher_variables(wish).batch_mode);
  CHECK(tools::launcher_variables(wish).argv == wish);
  // The console: tkcon gets the words without -dnull and -noc*.
  const std::vector<std::string> console = {"-dnull", "-norc", "s.tcl"};
  CHECK_FALSE(tools::launcher_variables(console).batch_mode);
  CHECK(tools::launcher_variables(console).argv == std::vector<std::string>{"-norc", "s.tcl"});
  // --version runs magicdnull.
  const std::vector<std::string> version = {"--version"};
  CHECK(tools::launcher_variables(version).batch_mode);
}
