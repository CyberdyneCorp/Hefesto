// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// oasis::read: the read command and file lookup, two-pass decoding and
// name resolution, units, coordinate conversion, cell processing and
// timestamps, top cell selection, the read option set and the read
// diagnostics (tasks.md 11.1-11.3). Owner: OASR.

#include <catch2/catch_test_macros.hpp>

#include "../codec/codec_fixture.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/oasis/read/find_file.hpp"
#include "oasis_read_fixture.hpp"

using namespace oasis_read_test;

namespace {

/// A cell `name` with a 10 x 10 metal1 rectangle at the origin.
void unit_cell(Oas& f, std::string_view name) {
  f.cell(name).add(rect(68, 20, 0, 0, 10, 10));
}

/// The same as a GDS structure.
void unit_structure(GdsFile& f, std::string_view name) {
  f.begin(name).boundary(68, 20, {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}}).end();
}

}  // namespace

TEST_CASE("read command and file lookup", "[cap:oasis-read][req:Read command and file lookup]") {
  SECTION("the extensions are tried in order, the given name last") {
    hefesto::platform::MemoryFileSystem fs;
    REQUIRE(fs.create_directories("/w").ok());
    REQUIRE(fs.write_atomic("/w/chip.oas", "x").ok());
    REQUIRE(fs.write_atomic("/w/lib.oasis", "x").ok());
    REQUIRE(fs.write_atomic("/w/pads.oas.gz", "x").ok());
    cell::SearchPaths paths;
    paths.path = {"/w"};
    cell::PathVariables vars;
    diag::CollectingSink sink;
    cell::LookupContext ctx{fs, paths, vars, sink};
    const auto chip = oasis::find_oasis_file(ctx, "chip");
    REQUIRE(chip);
    CHECK(chip->path.filename() == "chip.oas");
    const auto by_full_name = oasis::find_oasis_file(ctx, "chip.oas");
    REQUIRE(by_full_name);
    CHECK(by_full_name->path.filename() == "chip.oas");
    const auto lib = oasis::find_oasis_file(ctx, "lib");
    REQUIRE(lib);
    CHECK(lib->path.filename() == "lib.oasis");
    const auto pads = oasis::find_oasis_file(ctx, "pads");
    REQUIRE(pads);
    CHECK(pads->path.filename() == "pads.oas.gz");
    CHECK_FALSE(oasis::find_oasis_file(ctx, "missing"));
  }
  SECTION("no input style: nothing is read") {
    OasSession s;
    s.editor.mask.input_styles = decltype(s.editor.mask.input_styles){};
    Oas f;
    unit_cell(f, "chip");
    const auto result = s.read_oasis(f.bytes());
    CHECK(result.status == hefesto::cmd::Status::failed);
    CHECK(s.said("Nothing in \"cifinput\" section of tech file."));
    CHECK_FALSE(s.has_cell("chip"));
  }
  SECTION("a file that cannot be opened") {
    OasSession s;
    s.last = oasis::read(s.editor, "/data/none.oas", s.oasis_options);
    CHECK(s.last.status == hefesto::cmd::Status::failed);
    CHECK(s.said("Cannot open /data/none.oas to read OASIS input."));
  }
  SECTION("the read records no undo events and paints nothing into the edit cell") {
    OasSession s;
    auto edit = s.editor.cells.create("edit", s.editor.technology->plane_count());
    REQUIRE(edit.ok());
    s.editor.edit_cell = edit.value();
    Oas f;
    unit_cell(f, "chip");
    (void)s.read_oasis(f.bytes());
    CHECK(s.painted("edit", "metal1").empty());
    CHECK(s.painted("chip", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
}

TEST_CASE("two-pass decoding and name resolution", "[cap:oasis-read][req:Two-pass decoding and name resolution]") {
  SECTION("a CELLNAME table after the cells") {
    OasSession s;
    Oas f(/*strict=*/true);
    f.cell("TOP").add(place("A", 0, 0));
    unit_cell(f, "A");
    (void)s.read_oasis(f.bytes());
    CHECK(s.has_cell("TOP"));
    CHECK(s.has_cell("A"));
    bool placeholder = false;
    s.editor.cells.for_each([&](const db::CellDef& d) { placeholder = placeholder || d.name().starts_with("#"); });
    CHECK_FALSE(placeholder);
    CHECK(uses(s.cell("TOP")).size() == 1);
  }
  SECTION("a cell reference number with no CELLNAME is named #<n> (Q-OASIS-34)") {
    OasSession s;
    oasis_test::File raw;
    raw.add("0D 07");                    // CELL refnum 7
    raw.add("14 7B 44 14 64 64 00 00");  // RECTANGLE 68/20, 100 x 100 nm at (0, 0)
    (void)s.read_oasis(raw.end());
    CHECK(s.has_cell("#7"));
    CHECK(s.said("undefined CELLNAME refnum 7"));
    CHECK(s.painted("#7", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
  SECTION("a truncated file reads the cells complete before the error") {
    Oas one;
    unit_cell(one, "A");
    const std::size_t first_cell_end = one.bytes().size() - 256;
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("B").add(rect(68, 20, 0, 0, 20, 20));
    unit_cell(f, "C");
    Bytes bytes = f.bytes();
    bytes.resize(first_cell_end + 5);  // inside B's RECTANGLE
    (void)s.read_oasis(bytes);
    CHECK(s.said("unexpected end of file"));
    CHECK(s.said("(byte position " + std::to_string(first_cell_end + 5) + ")"));
    CHECK(s.has_cell("A"));
    CHECK_FALSE(s.has_cell("B"));
    CHECK_FALSE(s.has_cell("C"));
  }
  SECTION("an error before any complete cell creates no cell") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    Bytes bytes = f.bytes();
    bytes.resize(bytes.size() - 256 - 2);
    const auto result = s.read_oasis(bytes);
    CHECK(result.status == hefesto::cmd::Status::failed);
    CHECK_FALSE(s.has_cell("A"));
  }
  SECTION("not an OASIS file") {
    OasSession s;
    const auto result = s.read_oasis(Bytes(64, std::byte{0x41}));
    CHECK(result.status == hefesto::cmd::Status::failed);
    CHECK(s.said("not an OASIS file"));
  }
}

TEST_CASE("units and input scale", "[cap:oasis-read][req:Units and input scale]") {
  const auto read_both = [](double unit, double mpd, std::int64_t size) {
    OasSession o;
    Oas f(false, 0, unit);
    f.cell("c").add(rect(68, 20, 0, 0, size, size));
    (void)o.read_oasis(f.bytes());
    OasSession g;
    GdsFile gf;
    gf.library("lib", mpd);
    gf.begin("c").boundary(68, 20, {{0, 0}, {static_cast<std::int32_t>(size), 0},
                                    {static_cast<std::int32_t>(size), static_cast<std::int32_t>(size)},
                                    {0, static_cast<std::int32_t>(size)}, {0, 0}})
        .end()
        .end_library();
    g.read(gf);
    CHECK(o.painted("c", "metal1") == g.painted("c", "metal1"));
    return o.painted("c", "metal1");
  };
  SECTION("nanometer OASIS with a centimicron style: num 1, den 10") {
    CHECK(read_both(1000.0, 1e-9, 100) == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
  SECTION("angstrom OASIS: num 1, den 100") {
    CHECK(read_both(10000.0, 1e-10, 1000) == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
}

TEST_CASE("coordinate conversion", "[cap:oasis-read][req:Coordinate conversion]") {
  SECTION("an off-grid coordinate rescales as the GDS vertex does") {
    OasSession o;
    Oas f(false, 0, 1000.0);
    f.cell("c").add(rect(68, 20, 15, 0, 100, 100));
    (void)o.read_oasis(f.bytes());
    OasSession g;
    GdsFile gf;
    gf.library("lib", 1e-9);
    gf.begin("c").boundary(68, 20, {{15, 0}, {115, 0}, {115, 100}, {15, 100}, {15, 0}}).end().end_library();
    g.read(gf);
    CHECK(o.said("units rescaled by factor of 2"));
    CHECK(o.mag("c") == g.mag("c"));
    CHECK(o.extent("c", "metal1").x0 == 3);
  }
  SECTION("a coordinate outside 32 bits drops the instance with coordinate overflow") {
    OasSession o;
    Oas f;
    f.cell("c").add(rect(68, 20, 3'000'000'000LL, 0, 10, 10)).add(rect(68, 20, 0, 0, 10, 10));
    (void)o.read_oasis(f.bytes());
    CHECK(o.said("coordinate overflow"));
    CHECK(o.painted("c", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
}

TEST_CASE("cell processing", "[cap:oasis-read][req:Cell processing]") {
  SECTION("a forward-referenced cell reads as the GDS file does") {
    OasSession o;
    Oas f;
    f.cell("TOP").add(place("A", 100, 0));
    unit_cell(f, "A");
    (void)o.read_oasis(f.bytes());
    OasSession g;
    GdsFile gf;
    gf.library();
    gf.begin("TOP").sref("A", {100, 0}).end();
    unit_structure(gf, "A");
    gf.end_library();
    g.read(gf);
    CHECK(o.mag("TOP") == g.mag("TOP"));
    CHECK(o.mag("A") == g.mag("A"));
  }
  SECTION("noduplicates keeps a pre-loaded cell") {
    OasSession s;
    Oas first;
    unit_cell(first, "INV");
    (void)s.read_oasis(first.bytes());
    s.oasis_options.stream_read.noduplicates = true;
    Oas second;
    second.cell("INV").add(rect(69, 20, 0, 0, 30, 30));
    (void)s.read_oasis(second.bytes(), "/data/second.oas");
    CHECK(s.painted("INV", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
    CHECK(s.painted("INV", "metal2").empty());
  }
  SECTION("a cell defined twice follows the gds-read duplicate rule") {
    OasSession s;
    oasis_test::File raw;  // the encoder refuses a second CELL "A"
    raw.add("0E 01 41 14 7B 44 14 64 64 00 00");  // CELL "A", RECTANGLE 68/20 100 x 100 nm
    raw.add("0E 01 41 14 7B 45 14 32 32 00 00");  // CELL "A", RECTANGLE 69/20 50 x 50 nm
    (void)s.read_oasis(raw.end());
    CHECK(s.painted("A", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
    CHECK(s.said("Cell \"A\" was already defined in this file."));
    CHECK(s.said("Ignoring duplicate definition"));
    CHECK(s.painted("A", "metal2").empty());
  }
  SECTION("a placed cell that is never defined") {
    OasSession s;
    Oas f;
    f.cell("TOP").add(place("ghost", 0, 0));
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("cell ghost was used but not defined."));
  }
}

TEST_CASE("cell timestamps", "[cap:oasis-read][req:Cell timestamps]") {
  SECTION("default: timestamp 0") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    (void)s.read_oasis(f.bytes());
    CHECK(s.cell("A").timestamp() == 0);
    CHECK_FALSE(s.cell("A").flags().fixed_timestamp);
  }
  SECTION("nodatestamp: timestamp 0") {
    OasSession s;
    s.oasis_options.settings.datestamp = 0;
    Oas f;
    unit_cell(f, "A");
    (void)s.read_oasis(f.bytes());
    CHECK(s.cell("A").timestamp() == 0);
  }
  SECTION("a fixed datestamp") {
    OasSession s;
    s.oasis_options.settings.datestamp = 1700000000;
    Oas f;
    unit_cell(f, "A");
    (void)s.read_oasis(f.bytes());
    CHECK(s.cell("A").timestamp() == 1700000000);
    CHECK(s.cell("A").flags().fixed_timestamp);
  }
}

TEST_CASE("top cell selection", "[cap:oasis-read][req:Top cell selection]") {
  SECTION("S_TOP_CELL chooses the cell") {
    OasSession s;
    Oas f;
    f.top("chip");
    unit_cell(f, "pad");
    unit_cell(f, "chip");
    const auto result = s.read_oasis(f.bytes());
    CHECK(result.value.to_string() == "chip");
  }
  SECTION("an S_TOP_CELL naming no cell of the file is skipped") {
    OasSession s;
    Oas f;
    f.top("nowhere");
    f.cell("top").add(place("leaf", 0, 0));
    unit_cell(f, "leaf");
    const auto result = s.read_oasis(f.bytes());
    CHECK(result.value.to_string() == "top");
  }
  SECTION("two unplaced cells and no S_TOP_CELL: none") {
    OasSession s;
    Oas f;
    unit_cell(f, "a");
    unit_cell(f, "b");
    const auto result = s.read_oasis(f.bytes());
    CHECK(result.value.to_string().empty());
  }
  SECTION("`oasis library` has no effect") {
    OasSession s;
    s.oasis_options.settings.library = true;
    Oas f;
    f.cell("top").add(place("leaf", 0, 0));
    unit_cell(f, "leaf");
    const auto result = s.read_oasis(f.bytes());
    CHECK(result.value.to_string() == "top");
    CHECK_FALSE(s.said("Library name"));
  }
}

TEST_CASE("read option set", "[cap:oasis-read][req:Read option set]") {
  constexpr std::string_view kS10 = "style test\nscalefactor 10\nlayer m1 M1\ncalma M1 68 20\n";
  SECTION("rescale disallowed snaps placements") {
    OasSession s(kS10);
    s.oasis_options.rescale_allowed = false;
    Oas f;
    f.cell("A").add(rect(68, 20, 0, 0, 100, 100));
    f.cell("top").add(place("A", 25, 0));
    (void)s.read_oasis(f.bytes());
    CHECK(uses(s.cell("top")).front()->transform.c == 2);
  }
  SECTION("magscale is accepted and has no effect") {
    OasSession a;
    OasSession b;
    b.oasis_options.settings.magscale = 3.0;
    Oas f;
    f.cell("A").add(text(68, 5, 5, 5, "IN"));
    const Bytes bytes = f.bytes();
    (void)a.read_oasis(bytes);
    (void)b.read_oasis(bytes);
    CHECK(a.mag("A") == b.mag("A"));
  }
}

TEST_CASE("read diagnostics", "[cap:oasis-read][req:Read diagnostics]") {
  SECTION("warning limit: 99 errors and the limit notice") {
    OasSession s;
    s.oasis_options.warnings.level = mask::WarningLevel::limit;
    Oas f;
    f.cell("A");
    for (std::uint64_t layer = 100; layer < 250; ++layer) {
      f.add(rect(layer, 0, 0, 0, 10, 10));
    }
    (void)s.read_oasis(f.bytes());
    CHECK(s.count("Error while reading cell") == 99);
    CHECK(s.said("Error limit set:  Remaining errors will not be reported."));
  }
  SECTION("warning none: no read errors") {
    OasSession s;
    s.oasis_options.warnings.level = mask::WarningLevel::none;
    Oas f;
    f.cell("A").add(rect(100, 0, 0, 0, 10, 10));
    (void)s.read_oasis(f.bytes());
    CHECK(s.count("Error while reading cell") == 0);
  }
  SECTION("warning redirect: the read errors go to the file") {
    OasSession s;
    s.oasis_options.warnings.level = mask::WarningLevel::redirect;
    s.oasis_options.warnings.redirect_file = "/data/errors.txt";
    Oas f;
    f.cell("A").add(rect(100, 0, 0, 0, 10, 10));
    (void)s.read_oasis(f.bytes());
    CHECK(s.count("Error while reading cell") == 0);
    const auto text = s.fs.read("/data/errors.txt");
    REQUIRE(text.ok());
    CHECK(text.value().find("Unknown layer/datatype in box, layer=100 type=0") != std::string::npos);
  }
  SECTION("positions are the OASIS record's, a CBLOCK's as <offset>+<inner>") {
    for (const std::int32_t compress : {0, 6}) {
      OasSession s;
      Oas f(false, compress);
      f.cell("A");
      for (std::int64_t x = 0; x < 400; x += 20) {
        f.add(rect(68, 20, x, 0, 10, 10));  // compressible
      }
      f.add(rect(100, 0, 0, 0, 10, 10));
      const Bytes bytes = f.bytes();
      const auto decoded = oasis_test::decode(bytes);
      std::string expected;
      int rectangles = 0;
      for (const auto& r : decoded.records) {
        if (r.id == oasis::RecordId::rectangle && ++rectangles == 21) {
          expected = std::to_string(r.offset) + (r.in_cblock ? "+" + std::to_string(r.cblock_position) : "");
        }
      }
      (void)s.read_oasis(bytes);
      INFO("compress " << compress << ": " << expected);
      CHECK(s.said("Error while reading cell \"A\" (byte position " + expected + "): Unknown layer/datatype in box"));
      CHECK((expected.find('+') != std::string::npos) == (compress != 0));
    }
  }
  SECTION("the GDS-equivalent stream's own messages are not shown") {
    OasSession s;
    Oas f;
    f.cell("TOP").add(place("A", 0, 0));
    unit_cell(f, "A");
    (void)s.read_oasis(f.bytes());
    CHECK_FALSE(s.said("GDS-II Release"));
    CHECK_FALSE(s.said("Library name"));
  }
}
