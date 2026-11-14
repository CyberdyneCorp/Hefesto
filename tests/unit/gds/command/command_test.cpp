// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `gds`/`calma` command's write side: gds-write "Write command and
// output file naming" (Q-MASK-210) and "Write option settings and
// defaults".
// Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "../write/write_fixture.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/gds/command/command.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace gds_command_test {
namespace {

using namespace gds_write_test;  // NOLINT(google-build-using-namespace): the fixture's aliases
namespace view = hefesto::view;

// A command session over the fixture's editor, with in-memory files and a
// window showing `top` (metal1 (0,0)-(4,2)).
struct Session {
  Fixture f;
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  db::CellDef* top = nullptr;

  explicit Session(bool magic_compat = true) : f(magic_compat) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&f.ed);
    dispatcher->add_sink(printed);
    top = &f.cell("top");
    f.paint(*top, f.m1, db::Rect{0, 0, 4, 2});
    REQUIRE(f.ed.windows.open(top, view::ScreenRect{0, 0, 999, 999}).ok());
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  [[nodiscard]] bool exists(const std::string& path) { return services.view().files.exists(path); }
  [[nodiscard]] std::string file(const std::string& path) {
    auto read = services.view().files.read(path);
    REQUIRE(read.ok());
    return read.value();
  }
};

TEST_CASE("output file names", "[cap:gds-write][req:Write command and output file naming]") {
  const compat::CompatSettings on{true};
  const compat::CompatSettings off{false};
  CHECK(gds::output_file_name("out/inv", 0, on) == "out/inv.gds");
  CHECK(gds::output_file_name("chip", 6, on) == "chip.gds.gz");
  CHECK(gds::output_file_name("chip.strm", 6, on) == "chip.strm");
  CHECK(gds::output_file_name("./build/chip", 0, on) == "./build/chip");
  CHECK(gds::output_file_name("./build/chip", 0, off) == "./build/chip.gds");
  CHECK(gds::output_file_name("./build/chip.gds", 0, off) == "./build/chip.gds");
}

TEST_CASE("gds write adds the extension and gds compress writes gzip",
          "[cap:gds-write][req:Write command and output file naming]") {
  Session s;
  REQUIRE(s.services.view().files.create_directories("out").ok());
  REQUIRE(s.run("gds write out/inv").ok());
  CHECK(s.exists("out/inv.gds"));
  REQUIRE(s.run("gds compress 6").ok());
  REQUIRE(s.run("gds write chip").ok());
  REQUIRE(s.exists("chip.gds.gz"));
  const std::string packed = s.file("chip.gds.gz");
  REQUIRE(packed.size() > 2);
  CHECK(static_cast<unsigned char>(packed[0]) == 0x1f);
  CHECK(static_cast<unsigned char>(packed[1]) == 0x8b);
}

TEST_CASE("a dot in a directory name suppresses the extension with magic-compat ON",
          "[cap:gds-write][req:Write command and output file naming]") {
  for (const bool on : {true, false}) {
    Session s(on);
    REQUIRE(s.services.view().files.create_directories("./build").ok());
    REQUIRE(s.run("gds write ./build/chip").ok());
    CHECK(s.exists(on ? "./build/chip" : "./build/chip.gds"));
    CHECK_FALSE(s.exists(on ? "./build/chip.gds" : "./build/chip"));
  }
}

TEST_CASE("a bare gds writes the root cell", "[cap:gds-write][req:Write command and output file naming]") {
  Session s;
  REQUIRE(s.run("gds").ok());
  REQUIRE(s.exists("top.gds"));
  CHECK(structure_names(decode(s.file("top.gds"))) == std::vector<std::string>{"top"});
  REQUIRE(s.run("calma write other").ok());
  CHECK(s.exists("other.gds"));
}

TEST_CASE("gds write takes exactly one file name", "[cap:gds-write][req:Write command and output file naming]") {
  Session s;
  const auto none = s.run("gds write");
  CHECK_FALSE(none.ok());
  CHECK(none.diagnostics.back().text ==
        "Wrong number of arguments in \"gds\" command.  Try \":gds help\" for help.");
  CHECK_FALSE(s.run("gds write a b").ok());
  CHECK_FALSE(s.exists("a.gds"));
}

TEST_CASE("a failed write still reports and leaves the terminated file",
          "[cap:gds-write][req:Write command and output file naming]") {
  Session s;
  db::CellDef& macro = s.f.cell("macro");
  macro.properties().put("LEFview", std::string{"TRUE"});
  REQUIRE(db::place(*s.top, macro, db::PlaceOptions{}).ok());
  db::recompute_bbox(*s.top);
  const auto result = s.run("gds write bad");
  CHECK_FALSE(result.ok());
  CHECK(result.diagnostics.back().text == "I/O error in writing file bad.\nFile may be incompletely written.");
  REQUIRE(s.exists("bad.gds"));
  CHECK(decode(s.file("bad.gds")).back().record() == gds::RecordType::endlib);
}

TEST_CASE("the write options and their defaults", "[cap:gds-write][req:Write option settings and defaults]") {
  Session s;
  const auto value = [&s](std::string_view line) { return s.run(line).value.to_string(); };
  CHECK(value("gds labels") == "1");
  CHECK(value("gds lower") == "1");
  CHECK(value("gds library") == "0");
  CHECK(value("gds addendum") == "0");
  CHECK(value("gds arrays") == "0");
  CHECK(value("gds merge") == "0");
  CHECK(value("gds compress") == "0");
  CHECK(value("gds datestamp") == "default");
  CHECK(value("gds nodatestamp") == "0");
  CHECK(value("gds undefined") == "0");
  CHECK(value("gds abstract") == "0");
  CHECK(value("gds contacts") == "0");

  for (const auto* word : {"no", "false", "off", "0"}) {
    REQUIRE(s.run(std::string{"gds labels "} + word).ok());
    CHECK_FALSE(s.f.ed.mask.gds.labels);
    REQUIRE(s.run("gds labels yes").ok());
    CHECK(s.f.ed.mask.gds.labels);
  }
  REQUIRE(s.run("gds merge true").ok());
  CHECK(s.f.ed.mask.gds.merge);
  REQUIRE(s.run("gds undefined allow").ok());
  CHECK(s.f.ed.mask.gds.allow_undefined);
  REQUIRE(s.run("gds abstract 1").ok());
  CHECK(s.f.ed.mask.gds.allow_abstract);

  // An unrecognized argument is an argument error and changes nothing.
  CHECK_FALSE(s.run("gds library maybe").ok());
  CHECK_FALSE(s.f.ed.mask.gds.library);
  CHECK_FALSE(s.run("gds undefined yes").ok());
  CHECK(s.f.ed.mask.gds.allow_undefined);
}

TEST_CASE("the date stamp options", "[cap:gds-write][req:Write option settings and defaults]") {
  Session s;
  REQUIRE(s.run("gds datestamp 1700000000").ok());
  CHECK(s.f.ed.mask.gds.datestamp == 1700000000);
  CHECK(s.run("gds nodatestamp").value.to_string() == "fixed");
  REQUIRE(s.run("gds datestamp no").ok());
  CHECK(s.f.ed.mask.gds.datestamp == 0);
  CHECK(s.run("gds nodatestamp").value.to_string() == "1");
  REQUIRE(s.run("gds datestamp yes").ok());
  CHECK_FALSE(s.f.ed.mask.gds.datestamp.has_value());
  REQUIRE(s.run("gds nodatestamp yes").ok());
  CHECK(s.f.ed.mask.gds.datestamp == 0);
  REQUIRE(s.run("gds nodatestamp no").ok());
  CHECK_FALSE(s.f.ed.mask.gds.datestamp.has_value());
  const auto bad = s.run("gds datestamp soon");
  CHECK_FALSE(bad.ok());
  CHECK(bad.diagnostics.back().text.starts_with("Unrecognizable date stamp \"soon\".\nWrong number of arguments"));
  CHECK_FALSE(s.f.ed.mask.gds.datestamp.has_value());
}

TEST_CASE("a compression value out of range keeps the setting",
          "[cap:gds-write][req:Write option settings and defaults]") {
  Session s;
  REQUIRE(s.run("gds compress 3").ok());
  const auto bad = s.run("gds compress 12");
  CHECK_FALSE(bad.ok());
  CHECK(bad.diagnostics.back().text == "Bad compression value 12.  Value must be in the range 0 to 9.");
  CHECK(s.f.ed.mask.gds.compress == 3);
}

TEST_CASE("the contacts option is a no-op", "[cap:gds-write][req:Write option settings and defaults]") {
  Session s;
  REQUIRE(s.run("gds datestamp 0").ok());
  REQUIRE(s.run("gds write before").ok());
  const auto result = s.run("gds contacts yes");
  REQUIRE(result.ok());
  bool unimplemented = false;
  for (const auto& d : s.printed.take()) {
    unimplemented = unimplemented ||
                    d.text == "Unimplemented function:  Contacts will be output as individual cuts.";
  }
  CHECK(unimplemented);
  REQUIRE(s.run("gds write after").ok());
  // Equal apart from the 24 date bytes of BGNLIB and BGNSTR (the ON
  // modification date is the clock's).
  std::string before = s.file("before.gds");
  std::string after = s.file("after.gds");
  REQUIRE(before.size() == after.size());
  for (const std::size_t at : {std::size_t{22}, std::size_t{62 + 4 + 12}}) {
    for (std::size_t i = 0; i < 12; ++i) {
      before[at + i] = after[at + i] = 0;
    }
  }
  CHECK(before == after);
}

TEST_CASE("gds lower no upper-cases the names", "[cap:gds-write][req:Write option settings and defaults]") {
  Session s;
  db::CellDef& inv = s.f.cell("inv");
  s.f.ed.windows.current()->set_root(&inv);
  REQUIRE(s.run("gds lower no").ok());
  REQUIRE(s.run("gds write inv").ok());
  const std::vector<Rec> recs = decode(s.file("inv.gds"));
  CHECK(structure_names(recs) == std::vector<std::string>{"INV"});
}

// --- the read side (task 10.8) -----------------------------------------------

TEST_CASE("the read options and their defaults", "[cap:gds-read][req:Read option syntax and defaults]") {
  Session s;
  const auto value = [&s](std::string_view line) { return s.run(line).value.to_string(); };
  CHECK(value("gds drccheck") == "1");
  CHECK(value("gds flatten") == "0");
  CHECK(value("gds flatglob") == "*_CDNS_*");
  CHECK(value("gds magscale") == "1.0");
  CHECK(value("gds maskhints") == "0");
  CHECK(value("gds noduplicates") == "0");
  CHECK(value("gds ordering") == "0");
  CHECK(value("gds polygon subcells") == "none");
  CHECK(value("gds path subcells") == "0");
  CHECK(value("gds readonly") == "0");
  CHECK(value("gds rescale") == "1");
  CHECK(value("gds savepaths") == "0");
  CHECK(value("gds unique") == "0");
  (void)s.printed.take();
  REQUIRE(s.run("gds warning").ok());
  const auto printed = s.printed.take();
  REQUIRE_FALSE(printed.empty());
  CHECK(printed.back().text == "Warning display options: default");

  for (const auto* option : {"drccheck", "noduplicates", "ordering", "readonly", "savepaths", "unique"}) {
    INFO(option);
    CHECK_FALSE(s.run(std::string{"gds "} + option + " maybe").ok());
  }
  REQUIRE(s.run("gds readonly yes").ok());
  CHECK(s.f.ed.mask.gds.readonly);
  REQUIRE(s.run("gds drccheck off").ok());
  CHECK_FALSE(s.f.ed.mask.gds.drccheck);
  REQUIRE(s.run("gds path subcells true").ok());
  CHECK(s.f.ed.mask.gds.path_subcells);
}

TEST_CASE("read options match by unique prefix", "[cap:gds-read][req:Read option syntax and defaults]") {
  Session s;
  REQUIRE(s.run("gds noduplicates t").ok());
  CHECK(s.f.ed.mask.gds.noduplicates);
  REQUIRE(s.run("gds ord on").ok());
  CHECK(s.f.ed.mask.gds.ordering);
}

TEST_CASE("flatten takes a boolean or a limit", "[cap:gds-read][req:Read option syntax and defaults]") {
  Session s;
  REQUIRE(s.run("gds flatten 5").ok());
  CHECK(s.f.ed.mask.gds.flatten);
  CHECK(s.f.ed.mask.gds.flatten_limit == 5);
  REQUIRE(s.run("gds flatten 0").ok());  // "0" is the boolean no: the limit stays
  CHECK_FALSE(s.f.ed.mask.gds.flatten);
  CHECK(s.f.ed.mask.gds.flatten_limit == 5);
  REQUIRE(s.run("gds flatten -2").ok());
  CHECK_FALSE(s.f.ed.mask.gds.flatten);
  CHECK(s.f.ed.mask.gds.flatten_limit == -2);
  REQUIRE(s.run("gds flatten yes").ok());
  CHECK(s.f.ed.mask.gds.flatten);
  CHECK_FALSE(s.run("gds flatten some").ok());
  CHECK(s.f.ed.mask.gds.flatten);
}

TEST_CASE("flatglob adds patterns and none clears them", "[cap:gds-read][req:Read option syntax and defaults]") {
  Session s;
  REQUIRE(s.run("gds flatglob via*").ok());
  CHECK(s.run("gds flatglob").value.to_string() == "*_CDNS_* via*");
  REQUIRE(s.run("gds flatglob NONE").ok());
  CHECK(s.f.ed.mask.gds.flatglob.empty());
  CHECK(s.run("gds flatglob").value.to_string().empty());
}

TEST_CASE("polygon subcells takes a type or a boolean", "[cap:gds-read][req:Read option syntax and defaults]") {
  Session s;
  REQUIRE(s.run("gds polygon subcells keep").ok());
  CHECK(s.run("gds polygon subcells").value.to_string() == "keep");
  REQUIRE(s.run("gds polygon subcells yes").ok());
  CHECK(s.f.ed.mask.gds.polygon_subcells == gds::PolygonSubcells::temporary);
  REQUIRE(s.run("gds polygon subcells no").ok());
  CHECK(s.f.ed.mask.gds.polygon_subcells == gds::PolygonSubcells::none);
  CHECK_FALSE(s.run("gds polygon subcells maybe").ok());
  CHECK_FALSE(s.run("gds polygon").ok());
}

TEST_CASE("maskhints takes a boolean or a list of input layers",
          "[cap:gds-read][req:Read option syntax and defaults]") {
  Session s;
  REQUIRE(s.run("gds maskhints yes").ok());
  CHECK(s.run("gds maskhints").value.to_string() == "1");
  REQUIRE(s.run("gds maskhints no").ok());
  CHECK(s.f.ed.mask.gds.maskhints.empty());
  (void)s.printed.take();
  REQUIRE(s.run("gds maskhints CMF,CPG").ok());  // no input style: both unknown
  CHECK(s.f.ed.mask.gds.maskhints.empty());
  const auto printed = s.printed.take();
  REQUIRE(printed.size() == 2);
  CHECK(printed[0].text == "Error:  CIF layer \"CMF\" is unknown.");
}

TEST_CASE("magscale default resets the factor", "[cap:gds-read][req:Text magnification scale option]") {
  Session s;
  REQUIRE(s.run("gds magscale 0.1").ok());
  CHECK(s.f.ed.mask.gds.magscale == static_cast<double>(0.1F));
  REQUIRE(s.run("gds magscale default").ok());
  CHECK(s.f.ed.mask.gds.magscale == 1.0);
  CHECK_FALSE(s.run("gds magscale big").ok());
  CHECK(s.f.ed.mask.gds.magscale == 1.0);
}

TEST_CASE("rescale no sets warning level limit with magic-compat ON", "[cap:gds-read][req:Grid subdivision option]") {
  for (const bool on : {true, false}) {
    Session s(on);
    REQUIRE(s.run("gds warning default").ok());
    REQUIRE(s.run("gds rescale no").ok());
    CHECK_FALSE(s.f.ed.mask.gds.rescale);
    CHECK(s.f.ed.mask.warnings.level == (on ? hefesto::mask::WarningLevel::limit
                                              : hefesto::mask::WarningLevel::warn_default));
  }
}

TEST_CASE("gds warning levels and redirect", "[cap:gds-read][req:Read diagnostics]") {
  Session s;
  REQUIRE(s.run("gds warning redirect errs.txt").ok());
  CHECK(s.f.ed.mask.warnings.level == hefesto::mask::WarningLevel::redirect);
  CHECK(s.f.ed.mask.warnings.redirect_file == "errs.txt");
  REQUIRE(s.run("gds warning lim").ok());
  CHECK(s.f.ed.mask.warnings.level == hefesto::mask::WarningLevel::limit);
  (void)s.printed.take();
  REQUIRE(s.run("gds warning loud").ok());
  const auto printed = s.printed.take();
  REQUIRE(printed.size() == 2);
  CHECK(printed[0].text == "\"loud\" isn't a valid gds warning option.");
  CHECK(printed[1].text == "Valid options are: default, align, limit, redirect, and none.");
  CHECK(s.f.ed.mask.warnings.level == hefesto::mask::WarningLevel::limit);
}

TEST_CASE("gds read searches the extensions and reports a missing file",
          "[cap:gds-read][req:Read command and file lookup]") {
  Session s;
  const auto missing = s.run("gds read chip");
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.diagnostics.back().text == "Cannot open chip.gds, chip.strm or chip to read GDS-II stream input.");
  CHECK_FALSE(s.run("gds read").ok());

  REQUIRE(s.run("gds compress 6").ok());
  REQUIRE(s.run("gds write chip").ok());
  REQUIRE(s.exists("chip.gds.gz"));
  // The file opens (chip.gds.gz); without a cifinput style nothing is read.
  const auto read = s.run("gds read chip");
  CHECK_FALSE(read.ok());
  bool no_style = false;
  for (const auto& d : read.diagnostics) {
    no_style = no_style || d.code == "GDS-READ-009";
  }
  CHECK(no_style);
}

}  // namespace
}  // namespace gds_command_test
