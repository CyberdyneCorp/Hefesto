// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Format recognition on open (tasks.md 13.2): oasis::sniff_format and the
// generic open path app::open_command, which the desktop File > Open and
// drag and drop run (design D15, D16), and scripting access to the
// `oasis` command through the Tcl binding.
// Owner: OASCMD.

#include <catch2/catch_test_macros.hpp>

#include "../../script/binding/binding_fixture.hpp"
#include "command_fixture.hpp"
#include "hefesto/app/open_file.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/command/format.hpp"

using namespace oasis_command_test;

namespace {

oasis::FileFormat sniff(std::string_view text) {
  std::vector<std::byte> bytes(text.size());
  std::memcpy(bytes.data(), text.data(), text.size());
  hefesto::gds::MemorySource source{std::move(bytes)};
  return oasis::sniff_format(source);
}

oasis::FileFormat sniff(const Bytes& bytes) {
  hefesto::gds::MemorySource source{bytes};
  return oasis::sniff_format(source);
}

/// `bytes` written gzip-wrapped to `path` of `fs`.
void put_gzip(platform::FileSystem& fs, const std::string& path, const Bytes& bytes) {
  auto sink = hefesto::gds::open_sink(fs, path, 6);
  REQUIRE(sink.ok());
  REQUIRE(sink.value()->write(bytes).ok());
  REQUIRE(sink.value()->finish().ok());
}

/// A GDS library with one empty structure.
Bytes gds_bytes() {
  GdsFile g;
  g.library();
  g.begin("G").end().end_library();
  return g.bytes();
}

}  // namespace

TEST_CASE("sniff_format recognises OASIS, GDS and Magic cells by content",
          "[cap:oasis-command][req:Format recognition on open]") {
  CHECK(sniff(two_cells()) == oasis::FileFormat::oasis);
  CHECK(sniff(gds_bytes()) == oasis::FileFormat::gds);
  CHECK(sniff("magic\ntech scmos\n") == oasis::FileFormat::magic);
  CHECK(sniff("magic\r\ntech scmos\r\n") == oasis::FileFormat::magic);
  CHECK(sniff("magic") == oasis::FileFormat::magic);
  CHECK(sniff("magician\n") == oasis::FileFormat::unknown);
  CHECK(sniff("%SEMI-OASIS\r") == oasis::FileFormat::unknown);  // 12 of the 13 magic bytes
  CHECK(sniff("") == oasis::FileFormat::unknown);
  CHECK(sniff("VERSION 5.8 ;\n") == oasis::FileFormat::unknown);
}

TEST_CASE("the generic open path reads by content, whatever the extension",
          "[cap:oasis-command][req:Format recognition on open]") {
  SECTION("chip.gds holding OASIS is read as OASIS") {
    CommandSession s;
    s.put("chip.gds", two_cells());
    platform::FileSystem& fs = s.services.view().files;
    const auto open = hefesto::app::open_command(fs, "chip.gds");
    REQUIRE(open.ok());
    CHECK(open.value().format == oasis::FileFormat::oasis);
    CHECK(open.value().words == std::vector<std::string>{"oasis", "read", "chip.gds"});
    REQUIRE(s.run(open.value().line()).ok());
    CHECK(s.has_cell("A"));
    CHECK(s.editor.windows.current()->root()->name() == "TOP");
  }
  SECTION("a gzip-wrapped OASIS file is recognised after decompression") {
    CommandSession s;
    platform::FileSystem& fs = s.services.view().files;
    put_gzip(fs, "packed.bin", two_cells());
    const auto open = hefesto::app::open_command(fs, "packed.bin");
    REQUIRE(open.ok());
    CHECK(open.value().format == oasis::FileFormat::oasis);
    REQUIRE(s.run(open.value().line()).ok());
    CHECK(s.has_cell("A"));
  }
  SECTION("GDS and Magic cells keep their commands") {
    CommandSession s;
    platform::FileSystem& fs = s.services.view().files;
    s.put("lib.oas", gds_bytes());
    REQUIRE(fs.write_atomic("inv.mag", "magic\ntech minimum\ntimestamp 0\n<< end >>\n").ok());
    const auto gds = hefesto::app::open_command(fs, "lib.oas");
    REQUIRE(gds.ok());
    CHECK(gds.value().words == std::vector<std::string>{"gds", "read", "lib.oas"});
    const auto mag = hefesto::app::open_command(fs, "inv.mag");
    REQUIRE(mag.ok());
    CHECK(mag.value().words == std::vector<std::string>{"load", "inv.mag"});
  }
  SECTION("a path with blanks and Tcl characters is one word of the line") {
    hefesto::app::OpenCommand open;
    open.words = {"oasis", "read", "my dir/[a]$b.oas"};
    CHECK(open.line() == "oasis read {my dir/[a]$b.oas}");
  }
  SECTION("an unknown format is reported and nothing is read") {
    CommandSession s;
    platform::FileSystem& fs = s.services.view().files;
    REQUIRE(fs.write_atomic("notes.gds", "VERSION 5.8 ;\n").ok());
    const auto open = hefesto::app::open_command(fs, "notes.gds");
    REQUIRE_FALSE(open.ok());
    CHECK(open.error().code == "OASIS-CMD-005");
    CHECK(open.error().text == "Cannot open notes.gds: unknown format (not OASIS, GDS or a Magic cell file).");
  }
  SECTION("a missing file is the file system's error") {
    CommandSession s;
    CHECK_FALSE(hefesto::app::open_command(s.services.view().files, "missing.oas").ok());
  }
}

TEST_CASE("scripting access to the oasis command", "[cap:oasis-command][req:Scripting access]") {
  hefesto::script::test::BindingFixture f;
  CHECK(f.value("oasis compress") == "6");
  CHECK(f.value("magic::oasis compress") == "6");
  CHECK(f.value("oasis strict") == "1");
  CHECK(f.value("oasis validation") == "crc32");
  CHECK(f.value("gds readonly yes; oasis readonly") == "1");
  CHECK(f.value("oasis circlepoints 128; oasis circlepoints") == "128");
  // The same errors as typed, reported as for any command run from a
  // script (the binding's status rule); the setting is unchanged.
  (void)f.take_printed();
  (void)f.eval("oasis compress 12");
  CHECK(f.take_printed().find("Bad compression value 12.  Value must be in the range 0 to 9.") != std::string::npos);
  CHECK(f.value("oasis compress") == "6");
  (void)f.eval("oasis bogus");
  CHECK(f.take_printed().find("\"bogus\" isn't a valid oasis option.") != std::string::npos);
}
