// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds::read at the library level (tasks.md 9.2, 9.6, 9.7): the header and
// library cell, element dispatch, read-only views and their content stamps,
// mask hints on input, the read diagnostics and warning levels, the file
// lookup of `gds read`, the read option defaults and the cifinput style
// requirement. Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/gds/read/stamps.hpp"
#include "hefesto/hints/hints.hpp"
#include "read_fixture.hpp"

using namespace gds_read_test;

namespace {

void square(GdsFile& f, int layer, int datatype = 20) {
  f.boundary(layer, datatype, {{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}});
}

const hefesto::hints::HintRegion* hint(const db::CellDef& def, std::string_view layer) {
  const db::PropertyValue* value = def.properties().find("MASKHINTS_" + std::string{layer});
  return value != nullptr ? std::get_if<hefesto::hints::HintRegion>(value) : nullptr;
}

}  // namespace

TEST_CASE("library header and library cell", "[cap:gds-read][req:Library grammar and header processing]") {
  SECTION("library name sanitizing") {
    Session s;
    GdsFile f;
    f.library("lib/my top").begin("my_top").end().end_library();
    const auto result = s.read(f);
    REQUIRE(result.ok());
    CHECK(s.said("Library name modified to make legal cell name syntax."));
    CHECK(s.said("Library name: my_top"));
    CHECK(result.value.to_string() == "my_top");
  }
  SECTION("`gds library yes` discards the name") {
    Session s;
    s.options.settings.library = true;
    GdsFile f;
    f.library("my_top").begin("my_top").end().end_library();
    const auto result = s.read(f);
    CHECK_FALSE(s.said("Library name:"));
    CHECK(result.value.is_none());
  }
  SECTION("missing UNITS aborts") {
    Session s;
    GdsFile f;
    f.i2(0x00, {600}).i2(0x01, {124, 1, 1, 0, 0, 0, 124, 1, 1, 0, 0, 0}).str(0x02, "lib");
    f.begin("A").end().end_library();
    s.read(f);
    CHECK(s.said("Unexpected record type in input: "));
    CHECK(s.said("    Expected UNITS record but got BGNSTR."));
    CHECK_FALSE(s.has_cell("A"));
  }
  SECTION("a missing ENDLIB is reported") {
    Session s;
    GdsFile f;
    f.library().begin("A").end().i2(0x0d, {1});
    s.read(f);
    CHECK(s.said("    Expected ENDLIB record but got LAYER."));
    CHECK(s.has_cell("A"));
  }
}

TEST_CASE("a GDS read reports the fraction of the file read",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  Session s;
  GdsFile f;
  f.library("lib");
  for (int i = 0; i < 4; ++i) {
    f.begin("cell" + std::to_string(i));
    square(f, 1);
    f.end();
  }
  f.end_library();
  s.options.source_bytes = f.bytes().size();
  std::vector<double> fractions;
  hefesto::mask::Progress progress;
  progress.report = [&fractions](double fraction) { fractions.push_back(fraction); };
  gds::MemorySource source{f.bytes()};
  REQUIRE(gds::read(s.editor, source, s.options, &progress).ok());
  REQUIRE(fractions.size() >= 4);
  CHECK(std::is_sorted(fractions.begin(), fractions.end()));
  CHECK(fractions.front() > 0.0);
  CHECK(fractions.back() <= 1.0);

  SECTION("an unknown size reports nothing") {
    Session unknown;
    fractions.clear();
    gds::MemorySource again{f.bytes()};
    REQUIRE(gds::read(unknown.editor, again, unknown.options, &progress).ok());
    CHECK(fractions.empty());
  }
}

TEST_CASE("element dispatch", "[cap:gds-read][req:Element dispatch and unsupported content]") {
  Session s;
  GdsFile f;
  f.library().begin("top");
  f.empty(0x08).i2(0x0d, {68}).i2(0x0e, {20}).xy({{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}});
  f.i2(0x2b, {1}).str(0x2c, "x").empty(0x11);
  f.empty(0x15).i2(0x0d, {1}).i2(0x2a, {0}).xy({{0, 0}}).empty(0x11);
  f.end().end_library();
  s.read(f);
  CHECK(s.painted("top", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  CHECK(s.said("NODE elements not supported: skipping."));
}

TEST_CASE("read-only vendor views", "[cap:gds-read][req:Read-only vendor views]") {
  SECTION("offsets, file and flag") {
    Session s;
    s.options.stream_read.readonly = true;
    GdsFile f;
    f.library("lib", 1e-9);
    f.begin("A");
    const std::size_t start = f.size();
    square(f, 68);
    f.end();
    const std::size_t end = f.size();
    f.end_library();
    s.read(f);
    const db::CellDef& a = s.cell("A");
    CHECK(a.flags().vendor_gds);
    const auto* gds_start = std::get_if<db::DoubleList>(a.properties().find("GDS_START"));
    const auto* gds_end = std::get_if<db::DoubleList>(a.properties().find("GDS_END"));
    const auto* gds_file = std::get_if<std::string>(a.properties().find("GDS_FILE"));
    REQUIRE(gds_start != nullptr);
    REQUIRE(gds_end != nullptr);
    REQUIRE(gds_file != nullptr);
    CHECK(gds_start->values.front() == static_cast<std::int64_t>(start));
    CHECK(gds_end->values.front() == static_cast<std::int64_t>(end));
    CHECK(*gds_file == "/data/test.gds");
    // The end of the read queues every cell unless `drccheck no`, read-only
    // ones included (CIFReadCellCleanup); the background checker skips them.
    CHECK(s.editor.drc.pending.contains(a));
    CHECK_FALSE(s.said("Unknown layer"));
  }
  SECTION("wrong units refused") {
    Session s;
    s.options.stream_read.readonly = true;
    GdsFile f;
    f.library("lib", 1e-8).begin("A").end().end_library();
    s.read(f);
    CHECK(s.said("Incompatible scale factor of 1e-08, must be 1e-9."));
    CHECK(s.said("Cannot read this file in read-only mode."));
    CHECK_FALSE(s.has_cell("A"));
  }
  SECTION("unknown layers are not reported in read-only cells") {
    Session s;
    s.options.stream_read.readonly = true;
    GdsFile f;
    f.library("lib", 1e-9).begin("A");
    square(f, 99);
    f.end().end_library();
    s.read(f);
    CHECK_FALSE(s.said("Unknown layer"));
  }
}

TEST_CASE("content-hash stamps at read time", "[cap:gds-read][req:When content-hash timestamps are assigned]") {
  GdsFile f;
  f.library("lib", 1e-9).begin("A");
  square(f, 68);
  f.end().begin("B").sref("A", {0, 0}).end().end_library();
  const gds::StampTable table = gds::content_stamps(f.bytes());
  REQUIRE(table.size() == 2);
  SECTION("readonly with the default datestamp") {
    Session s;
    s.options.stream_read.readonly = true;
    s.read(f);
    CHECK(s.cell("A").timestamp() == table[0].stamp);
    CHECK(s.cell("B").timestamp() == table[1].stamp);
    CHECK_FALSE(s.cell("A").flags().needs_new_timestamp);
  }
  SECTION("a fixed datestamp overrides the hash") {
    Session s;
    s.options.stream_read.readonly = true;
    s.options.settings.datestamp = 1234;
    s.read(f);
    CHECK(s.cell("A").timestamp() == 1234);
  }
}

TEST_CASE("mask hints on input", "[cap:gds-read][req:Mask hints on input]") {
  constexpr std::string_view kOutput = "style out\nscalefactor 1\nlayer M1 m1\nmask-hints M1\n";
  // M1 generated from m2 only: the read m1 square is not regenerated.
  constexpr std::string_view kOtherSource = "style out\nscalefactor 1\nlayer M1 m2\nmask-hints M1\n";
  SECTION("hint for input the output does not regenerate") {
    Session s(kStyle, true, kOtherSource);
    s.options.stream_read.maskhints = "yes";
    GdsFile f;
    f.library().begin("top");
    square(f, 68);
    f.end().end_library();
    s.read(f);
    const auto* region = hint(s.cell("top"), "M1");
    REQUIRE(region != nullptr);
    CHECK(region->strips() == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
  SECTION("no hint for input the output regenerates") {
    Session s(kStyle, true, kOutput);
    s.options.stream_read.maskhints = "yes";
    GdsFile f;
    f.library().begin("top");
    square(f, 68);
    f.end().end_library();
    s.read(f);
    CHECK(hint(s.cell("top"), "M1") == nullptr);
  }
  SECTION("only the named layers") {
    Session s(kStyle, true, kOutput);
    s.options.stream_read.maskhints = "M2,PO";
    GdsFile f;
    f.library().begin("top");
    square(f, 68);
    f.end().end_library();
    s.read(f);
    CHECK(hint(s.cell("top"), "M1") == nullptr);
  }
  SECTION("off by default") {
    Session s(kStyle, true, kOutput);
    GdsFile f;
    f.library().begin("top");
    square(f, 68);
    f.end().end_library();
    s.read(f);
    CHECK(hint(s.cell("top"), "M1") == nullptr);
  }
}

TEST_CASE("read diagnostics and warning levels", "[cap:gds-read][req:Read diagnostics]") {
  const auto many_errors = [] {
    GdsFile f;
    f.library().begin("top");
    for (int dt = 0; dt < 150; ++dt) {
      square(f, 99, dt);
    }
    f.end().end_library();
    return f;
  };
  SECTION("limit") {
    Session s;
    s.options.warnings.level = mask::WarningLevel::limit;
    s.read(many_errors());
    CHECK(s.count("Unknown layer/datatype in boundary") == 99);
    CHECK(s.said("Error limit set:  Remaining errors will not be reported."));
  }
  SECTION("default") {
    Session s;
    s.read(many_errors());
    CHECK(s.count("Unknown layer/datatype in boundary") == 150);
    CHECK(s.said("Error while reading cell \"top\" (byte position "));
  }
  SECTION("none") {
    Session s;
    s.options.warnings.level = mask::WarningLevel::none;
    s.read(many_errors());
    CHECK(s.count("Unknown layer/datatype") == 0);
  }
  SECTION("redirect") {
    Session s;
    platform::MemoryFileSystem files;
    s.options.files = &files;
    s.options.warnings.level = mask::WarningLevel::redirect;
    s.options.warnings.redirect_file = "/out/err.txt";
    s.read(many_errors());
    CHECK(s.count("Unknown layer/datatype") == 0);
    const auto written = files.read("/out/err.txt");
    REQUIRE(written.ok());
    CHECK(written.value().find("Unknown layer/datatype in boundary, layer=99 type=149") != std::string::npos);
  }
}

TEST_CASE("the redirect file holds whole messages in both modes (Q-MASK-135)", "[cap:gds-read][req:Read diagnostics]") {
  // Magic's Tcl build writes only the prefix of each message to the file and
  // prints the body on the console; Hefesto writes prefix and body together
  // to the file and prints nothing, with magic-compat ON and OFF.
  for (const bool on : {true, false}) {
    Session s(kStyle, on);
    platform::MemoryFileSystem files;
    s.options.files = &files;
    s.options.warnings.level = mask::WarningLevel::redirect;
    s.options.warnings.redirect_file = "/out/err.txt";
    GdsFile f;
    f.library().begin("top");
    square(f, 99, 7);
    f.end().end_library();
    s.read(f);
    CHECK(s.count("Unknown layer/datatype") == 0);
    CHECK_FALSE(s.said("Error while reading cell"));
    const auto written = files.read("/out/err.txt");
    REQUIRE(written.ok());
    const std::string& text = written.value();
    const std::size_t prefix = text.find("Error while reading cell \"top\" (byte position ");
    REQUIRE(prefix != std::string::npos);
    const std::size_t body = text.find("Unknown layer/datatype in boundary, layer=99 type=7", prefix);
    REQUIRE(body != std::string::npos);
    CHECK(text.find('\n', prefix) > body);  // the body is on the prefix's line
  }
}

TEST_CASE("read command file lookup", "[cap:gds-read][req:Read command and file lookup]") {
  platform::MemoryFileSystem files;
  REQUIRE(files.write_atomic("/w/chip.gds.gz", "x").ok());
  REQUIRE(files.write_atomic("/w/plain.strm", "x").ok());
  hefesto::cell::SearchPaths paths;
  paths.path = {"/w"};
  hefesto::cell::PathVariables vars;
  diag::CollectingSink sink;
  hefesto::cell::LookupContext ctx{files, paths, vars, sink};
  const auto chip = gds::find_gds_file(ctx, "chip");
  REQUIRE(chip.has_value());
  CHECK(chip->path == platform::Path{"/w/chip.gds.gz"});
  CHECK_FALSE(gds::find_gds_file(ctx, "chip.gds").has_value());  // no implicit .gz
  const auto plain = gds::find_gds_file(ctx, "plain");
  REQUIRE(plain.has_value());
  CHECK(plain->path == platform::Path{"/w/plain.strm"});
  CHECK_FALSE(gds::find_gds_file(ctx, "missing").has_value());

  SECTION("a missing file") {
    Session s;
    const auto result = gds::read(s.editor, platform::Path{"/nowhere/x.gds"}, s.options);
    CHECK_FALSE(result.ok());
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.front().code == "GDS-READ-010");
  }
  SECTION("a gzip file is inflated") {
    Session s;
    GdsFile f;
    f.library().begin("A").end().end_library();
    platform::MemoryFileSystem gz_files;
    auto gz_sink = gds::open_sink(gz_files, "/w/c.gds.gz", 6);
    REQUIRE(gz_sink.ok());
    REQUIRE(gz_sink.value()->write(f.bytes()).ok());
    REQUIRE(gz_sink.value()->finish().ok());
    s.options.files = &gz_files;
    const auto result = gds::read(s.editor, platform::Path{"/w/c.gds.gz"}, s.options);
    CHECK(result.ok());
    CHECK(s.has_cell("A"));
  }
}

TEST_CASE("read option defaults", "[cap:gds-read][req:Read option syntax and defaults]") {
  const hefesto::stream::StreamReadSettings defaults;  // shared with `oasis` (MaskSession::stream_read)
  const gds::GdsSettings gds_only;
  CHECK_FALSE(defaults.flatten);
  CHECK(defaults.flatten_limit == 10);
  CHECK(defaults.flatglob == std::vector<std::string>{"*_CDNS_*"});
  CHECK(gds_only.magscale == 1.0);
  CHECK(defaults.drccheck);
  CHECK_FALSE(defaults.ordering);
  CHECK_FALSE(defaults.noduplicates);
  CHECK_FALSE(defaults.unique);
  CHECK_FALSE(defaults.readonly);
  CHECK_FALSE(defaults.savepaths);
  CHECK(defaults.maskhints.empty());
  CHECK(defaults.polygon_subcells == gds::PolygonSubcells::none);
  CHECK_FALSE(defaults.path_subcells);
  CHECK(hefesto::mask::MaskSession{}.rescale_allowed);  // shared with `cif rescale`
  CHECK_FALSE(gds_only.datestamp.has_value());
}

TEST_CASE("a read needs a cifinput style", "[cap:gds-read][req:Read command and file lookup]") {
  hefesto::editor::EditorState editor;
  gds::MemorySource source{GdsFile{}.library().begin("A").end().end_library().bytes()};
  const auto result = gds::read(editor, source, gds::GdsReadOptions{});
  CHECK_FALSE(result.ok());
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.front().text ==
        "Don't know how to read GDS-II:\nNothing in \"cifinput\" section of tech file.");
  CHECK(editor.cells.find("A") == nullptr);
}

TEST_CASE("the read style keeps its rescale", "[cap:gds-read][req:Host mechanisms used by the reader]") {
  Session s;
  s.options.time_zone = "<-03>3";
  GdsFile f;
  f.library().begin("A").end().end_library();
  s.read(f);
  CHECK(s.cell("A").timestamp() == 1705314600 + 3 * 3600);  // 10:30 at UTC-3
}

TEST_CASE("the end-of-read clean-up acts on the library cell loaded into the window",
          "[cap:gds-read][req:End-of-read cleanup]") {
  // CalmaReadFile loads the library cell (DBWloadWindow) before
  // CIFReadCellCleanup, so the edit cell the clean-up marks modified is that
  // cell, not the "(UNNAMED)" one (MASK-GR-002).
  Session s;
  auto unnamed = s.editor.cells.create("(UNNAMED)", s.editor.technology->plane_count());
  REQUIRE(unnamed.ok());
  unnamed.value()->flags().available = true;
  unnamed.value()->flags().modified = false;
  s.editor.edit_cell = unnamed.value();
  GdsFile f;
  f.library("top").begin("top").boundary(68, 20, {{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}}).end().end_library();
  s.read(f);
  CHECK_FALSE(s.cell("(UNNAMED)").flags().modified);
  CHECK(s.cell("top").flags().modified);

  SECTION("without a library cell the edit cell is cleaned up and its moved labels reported") {
    Session t;
    auto edit = t.editor.cells.create("edit", t.editor.technology->plane_count());
    REQUIRE(edit.ok());
    db::CellDef& e = *edit.value();
    e.flags().available = true;
    REQUIRE(db::paint_type(e, *t.editor.technology, db::Rect{0, 0, 10, 10}, t.type("m1")).ok());
    db::Label l;
    l.text = "x";
    l.rect = db::Rect{5, 5, 5, 5};
    REQUIRE(db::put_label(e, std::move(l)).ok());
    t.editor.edit_cell = &e;
    GdsFile g;
    g.library("lib").begin("top").end().end_library();
    t.read(g);
    CHECK(t.said("Moving label \"x\" from space to metal1 in cell edit."));
    CHECK(e.flags().modified);
  }
}
