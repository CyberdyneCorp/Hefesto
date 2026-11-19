// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only OASIS vendor views, content-hash timestamps and the host
// mechanisms of the OASIS reader (tasks.md 11.4, 11.5). Owner: OASR.

#include <catch2/catch_test_macros.hpp>

#include <array>

#include "hefesto/db/property.hpp"
#include "hefesto/gds/read/vendor.hpp"
#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/oasis/read/stamps.hpp"
#include "hefesto/oasis/read/vendor.hpp"
#include "oasis_read_fixture.hpp"

using namespace oasis_read_test;

namespace {

/// A library of nanometer cells: `leaf` (many rectangles, so a CBLOCK
/// pays off) placed by `top`, written with `compress`.
Bytes library(std::int32_t compress, std::int64_t leaf_width = 100) {
  Oas f(false, compress, 1000.0);
  f.cell("leaf");
  for (std::int64_t x = 0; x < 4000; x += 200) {
    f.add(rect(68, 20, x, 0, leaf_width, 100));
  }
  f.cell("top").add(place("leaf", 0, 0)).add(rect(69, 20, 0, 0, 500, 500));
  return f.bytes();
}

gds::StampTable stamps_of(const Bytes& bytes) {
  gds::MemorySource source{bytes};
  return oasis::content_stamps(source);
}

std::uint32_t stamp_named(const gds::StampTable& t, std::string_view name) {
  return oasis::lookup_stamp(t, name);
}

const std::string* string_property(const db::CellDef& def, std::string_view name) {
  return std::get_if<std::string>(def.properties().find(name));
}

/// `oasis readonly yes`.
void readonly(OasSession& s) {
  s.oasis_options.stream_read.readonly = true;
}

}  // namespace

TEST_CASE("read-only OASIS vendor views", "[cap:oasis-read][req:Read-only OASIS vendor views]") {
  SECTION("vendor cell properties") {
    OasSession s;
    readonly(s);
    s.oasis_options.oasis_file = "$PDK_ROOT/lib/cells.oas";
    Oas f(false, 0, 1000.0);
    f.cell("sc_inv").add(rect(68, 20, 0, 0, 100, 100));
    (void)s.read_oasis(f.bytes(), "/data/cells.oas");
    const db::CellDef& inv = s.cell("sc_inv");
    CHECK(inv.flags().vendor_gds);
    REQUIRE(string_property(inv, "OASIS_FILE") != nullptr);
    CHECK(*string_property(inv, "OASIS_FILE") == "$PDK_ROOT/lib/cells.oas");
    REQUIRE(string_property(inv, "OASIS_CELL") != nullptr);
    CHECK(*string_property(inv, "OASIS_CELL") == "sc_inv");
    CHECK(inv.properties().find("GDS_FILE") == nullptr);
    CHECK(inv.properties().find("GDS_START") == nullptr);
    CHECK(inv.properties().find("GDS_END") == nullptr);
    CHECK(s.painted("sc_inv", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
  SECTION("wrong unit refused") {
    OasSession s;
    readonly(s);
    Oas f(false, 0, 100.0);
    f.cell("sc_inv").add(rect(68, 20, 0, 0, 10, 10));
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("Incompatible scale factor of 1e-08, must be 1e-9."));
    CHECK(s.said("Cannot read this file in read-only mode."));
    CHECK_FALSE(s.has_cell("sc_inv"));
  }
  SECTION("OASIS_CELL is the name in the file, before a cell label renames the cell") {
    OasSession s("style test\nscalefactor 1\nlayer m1 M1\ncalma M1 68 20\nlayer m2 CID\nlabels CID cell\ncalma CID 63 0\n");
    readonly(s);
    Oas f(false, 0, 1000.0);
    f.cell("XYZ").add(text(63, 0, 0, 0, "real_name"));
    (void)s.read_oasis(f.bytes());
    REQUIRE(s.has_cell("real_name"));
    REQUIRE(string_property(s.cell("real_name"), "OASIS_CELL") != nullptr);
    CHECK(*string_property(s.cell("real_name"), "OASIS_CELL") == "XYZ");
  }
  SECTION("magnified copies are not read-only") {
    OasSession s;
    readonly(s);
    Oas f(false, 0, 1000.0);
    f.cell("C").add(rect(68, 20, 0, 0, 100, 100));
    f.cell("top").add(place("C", 0, 0, 0.0, false, 2.0));
    (void)s.read_oasis(f.bytes());
    REQUIRE(s.has_cell("C_mag2"));
    CHECK_FALSE(s.cell("C_mag2").flags().vendor_gds);
    CHECK(s.cell("C_mag2").properties().find("OASIS_FILE") == nullptr);
  }
  SECTION("setting OASIS_FILE makes a cell read-only, removing it clears that") {
    hefesto::db::CellLibrary cells;
    auto def = cells.create("v", 4);
    REQUIRE(def.ok());
    db::set_property(*def.value(), "OASIS_FILE", db::PropertyValue{std::string{"/x.oas"}});
    CHECK(def.value()->flags().vendor_gds);
    CHECK(oasis::is_oasis_vendor(*def.value()));
    db::set_property(*def.value(), "OASIS_FILE", std::nullopt);
    CHECK_FALSE(def.value()->flags().vendor_gds);
    CHECK(db::coerce_reserved_property("OASIS_FILE", db::PropertyValue{db::IntegerList{{5}}}) ==
          db::PropertyValue{std::string{"5"}});
  }
  SECTION("renaming a read-only cell with force drops its OASIS properties") {
    hefesto::db::CellLibrary cells;
    auto def = cells.create("v", 4);
    REQUIRE(def.ok());
    db::set_property(*def.value(), "OASIS_FILE", db::PropertyValue{std::string{"/x.oas"}});
    db::set_property(*def.value(), "OASIS_CELL", db::PropertyValue{std::string{"v"}});
    CHECK_FALSE(cells.rename(*def.value(), "w", false).ok());
    REQUIRE(cells.rename(*def.value(), "w", true).ok());
    CHECK(def.value()->properties().find("OASIS_FILE") == nullptr);
    CHECK(def.value()->properties().find("OASIS_CELL") == nullptr);
    CHECK_FALSE(def.value()->flags().vendor_gds);
  }
  SECTION("unknown layers are not reported in read-only cells") {
    OasSession s;
    readonly(s);
    Oas f(false, 0, 1000.0);
    f.cell("sc_inv").add(rect(1000, 0, 0, 0, 100, 100));
    (void)s.read_oasis(f.bytes());
    CHECK_FALSE(s.said("Unknown layer/datatype"));
  }
  SECTION("noduplicates annotates a pre-existing abstract view") {
    OasSession s;
    s.oasis_options.stream_read.noduplicates = true;
    auto abstract = s.editor.cells.create("INV", s.editor.technology->plane_count());
    REQUIRE(abstract.ok());
    db::set_property(*abstract.value(), "LEFview", db::PropertyValue{std::string{"TRUE"}});
    Oas f(false, 0, 1000.0);
    f.cell("INV").add(rect(68, 20, 0, 0, 100, 100));
    (void)s.read_oasis(f.bytes());
    REQUIRE(string_property(s.cell("INV"), "OASIS_FILE") != nullptr);
    CHECK(*string_property(s.cell("INV"), "OASIS_FILE") == "/data/test.oas");
    REQUIRE(string_property(s.cell("INV"), "OASIS_CELL") != nullptr);
    CHECK(*string_property(s.cell("INV"), "OASIS_CELL") == "INV");
    CHECK(s.cell("INV").properties().find("GDS_FILE") == nullptr);
    CHECK(s.painted("INV", "metal1").empty());
  }
}

TEST_CASE("content-hash timestamps for read-only OASIS cells",
          "[cap:oasis-read][req:Content-hash timestamps for read-only OASIS cells]") {
  SECTION("recompression does not change the stamp") {
    const Bytes plain = library(0);
    const Bytes packed = library(6);
    REQUIRE(packed.size() < plain.size());  // a CBLOCK holds the cell bodies
    const auto a = stamps_of(plain);
    const auto b = stamps_of(packed);
    REQUIRE(a.size() == 2);
    REQUIRE(b.size() == 2);
    for (const std::string_view name : {"leaf", "top"}) {
      CHECK(stamp_named(a, name) != 0);
      CHECK(stamp_named(a, name) == stamp_named(b, name));
    }
    OasSession one;
    readonly(one);
    (void)one.read_oasis(plain);
    OasSession two;
    readonly(two);
    (void)two.read_oasis(packed);
    CHECK(one.cell("leaf").timestamp() == stamp_named(a, "leaf"));
    CHECK(one.cell("top").timestamp() == stamp_named(a, "top"));
    CHECK(two.cell("leaf").timestamp() == one.cell("leaf").timestamp());
    CHECK(two.cell("top").timestamp() == one.cell("top").timestamp());
  }
  SECTION("a child's content changes its parent's stamp, not the other way round") {
    const auto a = stamps_of(library(0, 100));
    const auto b = stamps_of(library(0, 120));
    CHECK(stamp_named(a, "leaf") != stamp_named(b, "leaf"));
    CHECK(stamp_named(a, "top") != stamp_named(b, "top"));
  }
  SECTION("a fixed datestamp takes precedence") {
    OasSession s;
    readonly(s);
    s.oasis_options.settings.datestamp = 1700000000;
    (void)s.read_oasis(library(0));
    CHECK(s.cell("leaf").timestamp() == 1700000000);
  }
  SECTION("an edit does not change a vendor stamp") {
    OasSession s;
    readonly(s);
    const Bytes bytes = library(6);
    (void)s.read_oasis(bytes);
    const std::int64_t before = s.cell("leaf").timestamp();
    REQUIRE(before != 0);
    db::CellDef& leaf = *s.editor.cells.find("leaf");
    leaf.set_timestamp(5);  // as if the view were edited and restamped
    db::mark_modified(leaf);
    cell::SearchPaths paths;
    paths.path = {"/data"};
    cell::PathVariables vars;
    diag::CollectingSink sink;
    cell::LookupContext lookup{s.fs, paths, vars, sink};
    std::array<db::CellDef*, 1> cells{&leaf};
    cell::update_timestamps(s.editor.cells, cells, 999999, lookup);
    CHECK(leaf.timestamp() == before);
  }
  SECTION("the vendor stamp lookup") {
    OasSession s;
    readonly(s);
    (void)s.read_oasis(library(0));
    cell::SearchPaths paths;
    cell::PathVariables vars;
    diag::CollectingSink sink;
    cell::LookupContext lookup{s.fs, paths, vars, sink};
    oasis::VendorStamper stamper{lookup};
    const auto table = stamps_of(library(0));
    CHECK(stamper.stamp(s.cell("leaf")) == stamp_named(table, "leaf"));
    db::CellDef& top = *s.editor.cells.find("top");
    db::set_property(top, "OASIS_CELL", db::PropertyValue{std::string{"nowhere"}});
    CHECK(stamper.stamp(top) == 0);
    db::set_property(top, "OASIS_CELL", std::nullopt);
    CHECK(stamper.stamp(top) == 0);
  }
}

TEST_CASE("host mechanisms used by the reader", "[cap:oasis-read][req:Host mechanisms used by the reader]") {
  SECTION("gzip-wrapped input") {
    auto inner = std::make_unique<gds::MemorySink>();
    gds::MemorySink* raw = inner.get();
    auto gz = gds::make_gzip_sink(std::move(inner), 6);
    REQUIRE(gz.ok());
    const Bytes plain = library(6);
    REQUIRE(gz.value()->write(plain).ok());
    REQUIRE(gz.value()->finish().ok());
    OasSession s;
    (void)s.read_oasis(raw->bytes(), "/data/cells.oas.gz");
    CHECK(s.has_cell("leaf"));
    CHECK(s.painted("leaf", "metal1").size() == 20);
  }
  SECTION("CBLOCK-compressed input") {
    OasSession s;
    (void)s.read_oasis(library(9));
    CHECK(s.painted("leaf", "metal1").size() == 20);
  }
  SECTION("PDK path-variable substitution for OASIS_FILE") {
    cell::PathVariables vars;
    vars.variable = [](std::string_view name) -> std::optional<std::string> {
      return name == "PDK_ROOT" ? std::optional<std::string>{"/pdk"} : std::nullopt;
    };
    const std::string value = gds::gds_file_value("/pdk/sky130/cells.oas", vars);
    CHECK(value == "$PDK_ROOT/sky130/cells.oas");
    OasSession s;
    readonly(s);
    s.oasis_options.oasis_file = value;
    (void)s.read_oasis(library(0), "/data/cells.oas");
    REQUIRE(string_property(s.cell("leaf"), "OASIS_FILE") != nullptr);
    CHECK(*string_property(s.cell("leaf"), "OASIS_FILE") == "$PDK_ROOT/sky130/cells.oas");
  }
}
