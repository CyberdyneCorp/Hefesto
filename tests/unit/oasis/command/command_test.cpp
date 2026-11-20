// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `oasis` command (tasks.md 13.1): its forms, option syntax, the
// options shared with `gds` (one setting through MaskSession::stream_read,
// Q-OASIS-90), the options stored apart from `gds`, and the OASIS-only
// options.
// Owner: OASCMD.

#include <catch2/catch_test_macros.hpp>

#include "command_fixture.hpp"
#include "hefesto/oasis/command/command.hpp"
#include "hefesto/oasis/read/vendor.hpp"

using namespace oasis_command_test;

TEST_CASE("oasis command forms", "[cap:oasis-command][req:Command name and forms]") {
  SECTION("help lists every option") {
    CommandSession s;
    REQUIRE(s.run("oasis help").ok());
    const std::string text = s.take_printed();
    for (const std::string_view entry : oasis::oasis_options()) {
      const std::string name{entry.substr(0, entry.find_first_of(" \t\n"))};
      INFO(name);
      CHECK(text.find("    " + name) != std::string::npos);
    }
    CHECK(oasis::oasis_options().size() == 34);
  }
  SECTION("a bare oasis writes the root cell as <root>.oas") {
    CommandSession s;
    s.stub_write_host();
    REQUIRE(s.run("oasis").ok());
    CHECK(s.hosted_writes == std::vector<std::string>{"top.oas"});
  }
  SECTION("oasis write appends .oas only to a name without a dot") {
    CommandSession s;
    s.stub_write_host();
    REQUIRE(s.run("oasis write out/inv").ok());
    REQUIRE(s.run("oasis write chip.gds2").ok());
    CHECK(s.hosted_writes == std::vector<std::string>{"inv.oas", "chip.gds2"});
  }
  SECTION("write with two names or none is an argument error and writes nothing") {
    CommandSession s;
    s.stub_write_host();
    const auto two = s.run("oasis write a b");
    CHECK_FALSE(two.ok());
    CHECK(last_code(two) == "OASIS-CMD-001");
    CHECK(two.diagnostics.back().text == "Wrong number of arguments in \"oasis\" command.  Try \":oasis help\" for help.");
    CHECK_FALSE(s.run("oasis write").ok());
    CHECK(s.hosted_writes.empty());
  }
  SECTION("read and write need a window") {
    CommandSession s;
    REQUIRE(s.editor.windows.close(s.editor.windows.current()->id()).ok());
    CHECK(last_code(s.run("oasis write x")) == "VIEW-WINDOW-003");
    CHECK(last_code(s.run("oasis read x")) == "VIEW-WINDOW-003");
    CHECK(s.run("oasis compress").ok());
  }
  SECTION("oasis read finds <name>.oas and loads the top cell") {
    CommandSession s;
    s.put("cells.oas", two_cells());
    REQUIRE(s.run("oasis read cells").ok());
    REQUIRE(s.has_cell("TOP"));
    REQUIRE(s.has_cell("A"));
    REQUIRE(s.editor.windows.current() != nullptr);
    CHECK(s.editor.windows.current()->root()->name() == "TOP");
  }
  SECTION("oasis read of a missing file names the candidates") {
    CommandSession s;
    const auto r = s.run("oasis read nothing");
    CHECK_FALSE(r.ok());
    CHECK(last_code(r) == "OASIS-CMD-004");
    CHECK(r.diagnostics.back().text == "Cannot open nothing.oas, nothing.oasis or nothing to read OASIS input.");
  }
  SECTION("gds and calma never read OASIS") {
    CommandSession s;
    s.put("chip.gds", two_cells());
    (void)s.run("gds read chip");
    (void)s.run("calma read chip.gds");
    CHECK_FALSE(s.has_cell("A"));
  }
}

TEST_CASE("oasis option syntax", "[cap:oasis-command][req:Option syntax]") {
  SECTION("options and values match by unique prefix") {
    CommandSession s;
    REQUIRE(s.run("oasis nodup t").ok());
    CHECK(s.editor.mask.stream_read.noduplicates);
    CHECK(s.value("oasis nodup") == "1");
    REQUIRE(s.run("oasis valid check").ok());
    CHECK(s.value("oasis validation") == "checksum");
  }
  SECTION("an unknown or ambiguous option is an error and changes nothing") {
    CommandSession s;
    const auto before = s.editor.mask.oasis;
    for (const char* line : {"oasis bogus yes", "oasis re yes", "oasis p yes"}) {
      const auto r = s.run(line);
      INFO(line);
      CHECK_FALSE(r.ok());
      CHECK(last_code(r) == "CMD-USAGE-001");
    }
    CHECK(s.editor.mask.oasis == before);
  }
  SECTION("boolean and allow/disallow words") {
    CommandSession s;
    for (const char* word : {"yes", "true", "on", "1"}) {
      REQUIRE(s.run(std::string{"oasis labels no; oasis labels "} + word).ok());
      CHECK(s.editor.mask.oasis.labels);
    }
    for (const char* word : {"no", "false", "off", "0"}) {
      REQUIRE(s.run(std::string{"oasis arrays yes; oasis arrays "} + word).ok());
      CHECK_FALSE(s.editor.mask.oasis.arrays);
    }
    REQUIRE(s.run("oasis abstract allow").ok());
    CHECK(s.editor.mask.oasis.allow_abstract);
    REQUIRE(s.run("oasis undefined 1").ok());
    CHECK(s.editor.mask.oasis.allow_undefined);
    REQUIRE(s.run("oasis abstract disallow").ok());
    CHECK_FALSE(s.editor.mask.oasis.allow_abstract);
  }
  SECTION("a query reports the value and changes nothing") {
    CommandSession s;
    REQUIRE(s.run("oasis flatten 5").ok());
    const auto before = s.editor.mask.stream_read;
    CHECK(s.value("oasis flatten") == "1");
    CHECK(s.editor.mask.stream_read == before);
    CHECK(s.editor.mask.stream_read.flatten_limit == 5);
  }
  SECTION("an invalid value is an argument error and the setting is unchanged") {
    CommandSession s;
    const auto before = s.editor.mask.oasis;
    for (const char* line : {"oasis labels maybe", "oasis abstract perhaps", "oasis strict yes no",
                             "oasis validation md5", "oasis circlepoints many", "oasis datestamp soon"}) {
      const auto r = s.run(line);
      INFO(line);
      CHECK_FALSE(r.ok());
      CHECK(last_code(r) == "OASIS-CMD-001");
    }
    CHECK(s.editor.mask.oasis == before);
    const auto shared = s.run("oasis readonly maybe");
    CHECK_FALSE(shared.ok());
    CHECK(last_code(shared) == "OASIS-CMD-001");  // the gds handler's error, renumbered
  }
  SECTION("settings persist for the session") {
    CommandSession s;
    REQUIRE(s.run("oasis compress 3").ok());
    REQUIRE(s.run("oasis help").ok());
    CHECK(s.value("oasis compress") == "3");
  }
}

TEST_CASE("oasis options shared with the gds command", "[cap:oasis-command][req:Options shared with the gds command]") {
  SECTION("defaults") {
    CommandSession s;
    CHECK(s.value("oasis abstract") == "0");
    CHECK(s.value("oasis addendum") == "0");
    CHECK(s.value("oasis arrays") == "0");
    CHECK(s.value("oasis contacts") == "0");
    CHECK(s.value("oasis datestamp") == "default");
    CHECK(s.value("oasis nodatestamp") == "0");
    CHECK(s.value("oasis drccheck") == "1");
    CHECK(s.value("oasis flatten") == "0");
    CHECK(s.value("oasis flatglob") == "*_CDNS_*");
    CHECK(s.value("oasis labels") == "1");
    CHECK(s.value("oasis library") == "0");
    CHECK(s.value("oasis lower") == "1");
    CHECK(s.value("oasis magscale") == "1.0");
    CHECK(s.value("oasis maskhints") == "0");
    CHECK(s.value("oasis merge") == "0");
    CHECK(s.value("oasis noduplicates") == "0");
    CHECK(s.value("oasis ordering") == "0");
    CHECK(s.value("oasis polygon subcells") == "none");
    CHECK(s.value("oasis path subcells") == "0");
    CHECK(s.value("oasis readonly") == "0");
    CHECK(s.value("oasis rescale") == "1");
    CHECK(s.value("oasis savepaths") == "0");
    CHECK(s.value("oasis undefined") == "0");
    CHECK(s.value("oasis unique") == "0");
    CHECK(s.value("oasis compress") == "6");
  }
  SECTION("compression out of range is an error and the level is kept") {
    CommandSession s;
    REQUIRE(s.run("oasis compress 3").ok());
    const auto r = s.run("oasis compress 12");
    CHECK_FALSE(r.ok());
    CHECK(last_code(r) == "OASIS-CMD-002");
    CHECK(r.diagnostics.back().text == "Bad compression value 12.  Value must be in the range 0 to 9.");
    CHECK(s.value("oasis compress") == "3");
    CHECK_FALSE(s.run("oasis compress -1").ok());
    REQUIRE(s.run("oasis compress 0").ok());
    CHECK(s.editor.mask.oasis.compress == 0);
  }
  SECTION("rescale no implies warning limit, as for gds") {
    CommandSession s;
    REQUIRE(s.run("oasis warning default").ok());
    REQUIRE(s.run("oasis rescale no").ok());
    CHECK(s.editor.mask.warnings.level == hefesto::mask::WarningLevel::limit);
    CHECK_FALSE(s.editor.mask.rescale_allowed);
  }
  SECTION("an invalid warning option names the oasis command") {
    // Regression: the shared warning parser printed "gds" for `oasis` too.
    CommandSession s;
    REQUIRE(s.run("oasis warning loud").ok());
    const std::string printed = s.take_printed();
    CHECK(printed.find("\"loud\" isn't a valid oasis warning option.") != std::string::npos);
    CHECK(printed.find("valid gds warning") == std::string::npos);
  }
  SECTION("contacts yes is accepted and does nothing") {
    CommandSession s;
    REQUIRE(s.run("oasis contacts yes").ok());
    CHECK(s.take_printed().find("Unimplemented function") != std::string::npos);
    CHECK(s.value("oasis contacts") == "0");
  }
  SECTION("datestamp, nodatestamp and magscale") {
    CommandSession s;
    REQUIRE(s.run("oasis datestamp 1234").ok());
    CHECK(s.editor.mask.oasis.datestamp == 1234);
    CHECK(s.value("oasis nodatestamp") == "fixed");
    REQUIRE(s.run("oasis nodatestamp yes").ok());
    CHECK(s.editor.mask.oasis.datestamp == 0);
    REQUIRE(s.run("oasis datestamp yes").ok());
    CHECK_FALSE(s.editor.mask.oasis.datestamp.has_value());
    REQUIRE(s.run("oasis magscale 0.5").ok());
    CHECK(s.editor.mask.oasis.magscale == 0.5);
    REQUIRE(s.run("oasis magscale default").ok());
    CHECK(s.editor.mask.oasis.magscale == 1.0);
  }
}

TEST_CASE("OASIS-only options", "[cap:oasis-command][req:OASIS-only options]") {
  SECTION("defaults") {
    CommandSession s;
    CHECK(s.value("oasis strict") == "1");
    CHECK(s.value("oasis validation") == "crc32");
    CHECK(s.value("oasis repetitions") == "1");
    CHECK(s.value("oasis layernames") == "1");
    CHECK(s.value("oasis circlepoints") == "64");
  }
  SECTION("setting them") {
    CommandSession s;
    REQUIRE(s.run("oasis strict no; oasis repetitions no; oasis layernames no").ok());
    CHECK_FALSE(s.editor.mask.oasis.strict);
    CHECK_FALSE(s.editor.mask.oasis.repetitions);
    CHECK_FALSE(s.editor.mask.oasis.layernames);
    REQUIRE(s.run("oasis validation none").ok());
    CHECK(s.editor.mask.oasis.validation == oasis::ValidationScheme::none);
    REQUIRE(s.run("oasis validation checksum").ok());
    CHECK(s.editor.mask.oasis.validation == oasis::ValidationScheme::checksum32);
  }
  SECTION("circlepoints is bounded to 8-4096") {
    CommandSession s;
    const auto low = s.run("oasis circlepoints 4");
    CHECK_FALSE(low.ok());
    CHECK(last_code(low) == "OASIS-CMD-002");
    CHECK(s.value("oasis circlepoints") == "64");
    CHECK_FALSE(s.run("oasis circlepoints 4097").ok());
    REQUIRE(s.run("oasis circlepoints 8").ok());
    REQUIRE(s.run("oasis circlepoints 4096").ok());
    CHECK(s.editor.mask.oasis.circlepoints == 4096);
  }
  SECTION("circlepoints is the CIRCLE vertex count of a read") {
    CommandSession s;
    Oas f;
    f.cell("C").add(circle(68, 20, 0, 0, 1000));
    s.put("round.oas", f.bytes());
    REQUIRE(s.run("oasis circlepoints 8").ok());
    REQUIRE(s.run("oasis read round").ok());
    // An octagon of radius 1000: its area is 2 * sqrt(2) * r^2, about 2.83e6.
    CHECK(s.area("C", "metal1") < 2.9e6);
  }
}

TEST_CASE("oasis settings shared with the gds command", "[cap:oasis-command][req:Settings shared with the gds command]") {
  SECTION("gds readonly then oasis read marks the cells read-only (Q-OASIS-90)") {
    CommandSession s;
    REQUIRE(s.run("gds readonly true").ok());
    CHECK(s.value("oasis readonly") == "1");
    s.put("cells.oas", two_cells(1000.0));
    REQUIRE(s.run("oasis read cells").ok());
    for (const char* name : {"A", "TOP"}) {
      INFO(name);
      const db::CellDef& def = s.cell(name);
      CHECK(def.properties().find(oasis::kOasisFile) != nullptr);
      CHECK(def.properties().find(oasis::kOasisCell) != nullptr);
      CHECK(def.flags().vendor_gds);
    }
  }
  SECTION("every shared read setting is one setting with gds") {
    CommandSession s;
    REQUIRE(s.run("oasis flatten no").ok());
    REQUIRE(s.run("gds flatten yes").ok());
    CHECK(s.value("oasis flatten") == "1");
    REQUIRE(s.run("oasis drccheck no; oasis ordering on; oasis unique yes; oasis savepaths yes").ok());
    CHECK(s.value("gds drccheck") == "0");
    CHECK(s.value("gds ordering") == "1");
    CHECK(s.value("gds unique") == "1");
    CHECK(s.value("gds savepaths") == "1");
    REQUIRE(s.run("oasis polygon subcells keep; oasis path subcells yes; oasis flatglob none").ok());
    CHECK(s.value("gds polygon subcells") == "keep");
    CHECK(s.value("gds path subcells") == "1");
    CHECK(s.value("gds flatglob").empty());
    REQUIRE(s.run("gds noduplicates yes; gds maskhints yes").ok());
    CHECK(s.value("oasis noduplicates") == "1");
    CHECK(s.value("oasis maskhints") == "1");
    REQUIRE(s.run("oasis warning none").ok());
    CHECK(s.editor.mask.warnings.level == hefesto::mask::WarningLevel::none);
  }
  SECTION("gds flatten applies to an OASIS read") {
    CommandSession s;
    REQUIRE(s.run("oasis flatten no; gds flatten yes").ok());
    s.put("cells.oas", two_cells());
    REQUIRE(s.run("oasis read cells").ok());
    CHECK(uses(s.cell("TOP")).empty());  // `A` (one shape) is flattened into TOP
    CHECK_FALSE(s.painted("TOP", "metal1").empty());
  }
  SECTION("datestamp is independent: gds datestamp does not stamp OASIS cells") {
    CommandSession s;
    REQUIRE(s.run("gds datestamp 1700000000").ok());
    CHECK(s.value("oasis datestamp") == "default");
    s.put("cells.oas", two_cells());
    REQUIRE(s.run("oasis read cells").ok());
    CHECK(s.cell("A").timestamp() == 0);
    CHECK_FALSE(s.cell("A").flags().fixed_timestamp);
  }
  SECTION("compression and magscale are independent") {
    CommandSession s;
    REQUIRE(s.run("gds compress 9; gds magscale 2").ok());
    CHECK(s.value("oasis compress") == "6");
    CHECK(s.value("oasis magscale") == "1.0");
    REQUIRE(s.run("oasis compress 1").ok());
    CHECK(s.value("gds compress") == "9");
  }
}
