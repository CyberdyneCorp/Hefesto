// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only vendor views (tasks.md 9.6): the read-only units check, the
// GDS_FILE value (Q-MASK-128), the stamp of a cell from its GDS_START, the
// assignment at the end of a read, and the recomputation from the file
// named by GDS_FILE when timestamps are updated (cell/timestamps.cpp's
// vendor-stamp hook). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../cell/cell_fixture.hpp"
#include "../stream/stream_fixture.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/cell/timestamps.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/read/stamps.hpp"
#include "hefesto/gds/read/vendor.hpp"
#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/platform/file_system.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace gds = hefesto::gds;
namespace platform = hefesto::platform;
using gds_test::Bytes;
using gds_test::Gds;
using gds_test::hex;

namespace {

constexpr std::size_t kPlanes = 4;
constexpr std::uint32_t kLeafStamp = 143581310;
constexpr std::uint32_t kEmptyStamp = 1595775465;

const Bytes kLeafBody = hex(
    "00 04 08 00  00 06 0d 02 00 01  00 06 0e 02 00 00  00 2c 10 03"
    "00000000 00000000  00000000 0000000a  0000000a 0000000a  0000000a 00000000  00000000 00000000"
    "00 04 11 00  00 04 07 00");

/// A library with structures A (the spec's leaf) and E (empty), and the
/// GDS_START offsets of both.
struct VendorFile {
  Bytes bytes;
  std::uint64_t a_start = 0;
  std::uint64_t e_start = 0;
};

VendorFile vendor_file() {
  Gds g;
  g.library("lib");
  g.i2(0x05, {124, 1, 1, 0, 0, 0, 124, 1, 1, 0, 0, 0}).str(0x06, "A");
  VendorFile f;
  f.a_start = g.size();
  g.raw(kLeafBody);
  g.i2(0x05, {124, 1, 1, 0, 0, 0, 124, 1, 1, 0, 0, 0}).str(0x06, "E");
  f.e_start = g.size();
  g.empty(0x07).empty(0x04);
  f.bytes = g.bytes();
  return f;
}

void make_vendor(db::CellDef& def, const std::string& file, std::uint64_t start) {
  db::set_property(def, "GDS_FILE", db::PropertyValue{file});
  db::set_property(def, "GDS_START", db::PropertyValue{db::DoubleList{{static_cast<std::int64_t>(start)}}});
  db::set_property(def, "GDS_END", db::PropertyValue{db::DoubleList{{static_cast<std::int64_t>(start + 4)}}});
}

std::string as_string(const Bytes& b) {
  return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

Bytes gzip(const Bytes& plain) {
  auto inner = std::make_unique<gds::MemorySink>();
  gds::MemorySink* memory = inner.get();
  auto sink = gds::make_gzip_sink(std::move(inner), 6);
  REQUIRE(sink.ok());
  REQUIRE(sink.value()->write(plain).ok());
  REQUIRE(sink.value()->finish().ok());
  return memory->bytes();
}

/// A file system with the vendor file at `path`, and a lookup context
/// whose cell search path is "lib".
struct Lookup {
  platform::MemoryFileSystem fs;
  cell::SearchPaths paths{{"lib"}, {}, {}};
  cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext ctx{fs, paths, vars, sink};

  void put(const platform::Path& path, const Bytes& bytes) {
    if (path.has_parent_path()) {
      REQUIRE(fs.create_directories(path.parent_path()).ok());
    }
    REQUIRE(fs.write_atomic(path, as_string(bytes)).ok());
  }
};

}  // namespace

TEST_CASE("a read-only read needs 1 nm units, or 1 angstrom for an angstrom style",
          "[cap:gds-read][req:Read-only vendor views]") {
  CHECK_FALSE(gds::readonly_units_error(1e-9, false).has_value());
  CHECK_FALSE(gds::readonly_units_error(1.004e-9, false).has_value());
  const auto wrong = gds::readonly_units_error(1e-8, false);
  REQUIRE(wrong.has_value());
  CHECK(wrong->code == "GDS-READ-003");
  CHECK(wrong->text == "Incompatible scale factor of 1e-08, must be 1e-9.");
  CHECK_FALSE(gds::readonly_units_error(1e-10, true).has_value());
  const auto angstrom = gds::readonly_units_error(1e-9, true);
  REQUIRE(angstrom.has_value());
  CHECK(angstrom->text == "Incompatible scale factor of 1e-09, must be 1e-10.");
  CHECK(gds::readonly_units_error(std::numeric_limits<double>::quiet_NaN(), false).has_value());
  CHECK(gds::readonly_units_error(1e300, false).has_value());
}

TEST_CASE("GDS_FILE substitutes the PDK variable or the home directory", "[cap:gds-read][req:Read-only vendor views]") {
  cell::PathVariables vars;
  vars.variable = [](std::string_view name) -> std::optional<std::string> {
    if (name == "PDK_ROOT") {
      return "/opt/pdk";
    }
    return std::nullopt;
  };
  vars.home_of = [](std::string_view) -> std::optional<std::string> { return "/home/u"; };
  CHECK(gds::gds_file_value("/opt/pdk/sky130A/libs.ref/gds/cells.gds", vars) ==
        "$PDK_ROOT/sky130A/libs.ref/gds/cells.gds");
  // The home rule tests the GDS path itself (Q-MASK-128), in both settings.
  CHECK(gds::gds_file_value("/home/u/ip/cells.gds", vars) == "~/ip/cells.gds");
  CHECK(gds::gds_file_value("/home/user2/cells.gds", vars) == "/home/user2/cells.gds");
  CHECK(gds::gds_file_value("cells.gds", vars) == "cells.gds");
  // PDK_PATH is checked before PDK_ROOT.
  vars.variable = [](std::string_view name) -> std::optional<std::string> {
    if (name == "PDK_PATH") {
      return "/opt/pdk/sky130A";
    }
    if (name == "PDK_ROOT") {
      return "/opt/pdk";
    }
    return std::nullopt;
  };
  CHECK(gds::gds_file_value("/opt/pdk/sky130A/libs.ref/x.gds", vars) == "$PDK_PATH/libs.ref/x.gds");
}

TEST_CASE("GDS_START and GDS_END read as Magic's DBPropGetDouble", "[cap:gds-read][req:Read-only vendor views]") {
  db::CellDef def{"A", kPlanes};
  CHECK_FALSE(gds::property_integer(def, "GDS_START").has_value());
  db::set_property(def, "GDS_START", db::PropertyValue{db::DoubleList{{134}}});
  CHECK(gds::property_integer(def, "GDS_START") == 134);
  db::set_property(def, "GDS_START", db::PropertyValue{std::string{" 404 bytes"}});
  CHECK(gds::property_integer(def, "GDS_START") == 404);
  db::set_property(def, "GDS_START", db::PropertyValue{std::string{"x"}});
  CHECK(gds::property_integer(def, "GDS_START") == 0);
  db::set_property(def, "GDS_START", db::PropertyValue{db::IntegerList{{5}}});
  CHECK_FALSE(gds::property_integer(def, "GDS_START").has_value());
  // The most negative value negates without overflow (UBSan on the
  // linux-asan leg).
  db::set_property(def, "GDS_START", db::PropertyValue{std::string{"-9223372036854775808"}});
  CHECK(gds::property_integer(def, "GDS_START") == std::numeric_limits<std::int64_t>::min());
  db::set_property(def, "GDS_END", db::PropertyValue{std::string{"-12"}});
  CHECK(gds::property_integer(def, "GDS_END") == -12);
}

TEST_CASE("a read-only cell's stamp is the stamp at its GDS_START",
          "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  const VendorFile f = vendor_file();
  const gds::StampTable table = gds::content_stamps(f.bytes);
  db::CellDef a{"A", kPlanes};
  make_vendor(a, "cells.gds", f.a_start);
  CHECK(gds::cell_content_stamp(table, a) == kLeafStamp);
  db::CellDef lef{"PAD", kPlanes};  // a LEF abstract annotated with the same data
  make_vendor(lef, "other/cells.gds", f.a_start);
  CHECK(gds::cell_content_stamp(table, lef) == kLeafStamp);
  db::set_property(a, "GDS_END", std::nullopt);  // a whole-file dump has no single structure
  CHECK(gds::cell_content_stamp(table, a) == 0);
  db::CellDef off{"B", kPlanes};
  make_vendor(off, "cells.gds", f.a_start + 2);
  CHECK(gds::cell_content_stamp(table, off) == 0);
}

TEST_CASE("the read assigns content stamps to its read-only cells",
          "[cap:gds-read][req:When content-hash timestamps are assigned]") {
  const VendorFile f = vendor_file();
  const gds::StampTable table = gds::content_stamps(f.bytes);
  db::CellDef a{"A", kPlanes};
  db::CellDef e{"E", kPlanes};
  db::CellDef fixed{"F", kPlanes};
  db::CellDef plain{"P", kPlanes};
  make_vendor(a, "cells.gds", f.a_start);
  make_vendor(e, "cells.gds", f.e_start);
  make_vendor(fixed, "cells.gds", f.a_start);
  fixed.flags().fixed_timestamp = true;  // `gds datestamp N` overrides the hash
  fixed.set_timestamp(1700000000);
  db::set_property(plain, "GDS_START", db::PropertyValue{db::DoubleList{{static_cast<std::int64_t>(f.a_start)}}});
  db::set_property(plain, "GDS_END", db::PropertyValue{db::DoubleList{{1}}});
  for (db::CellDef* d : {&a, &e, &fixed, &plain}) {
    d->flags().needs_new_timestamp = true;
  }
  const std::vector<db::CellDef*> cells{&a, &e, &fixed, &plain, nullptr};
  gds::assign_content_stamps(table, cells);
  CHECK(a.timestamp() == kLeafStamp);
  CHECK_FALSE(a.flags().needs_new_timestamp);
  CHECK(e.timestamp() == kEmptyStamp);
  CHECK(fixed.timestamp() == 1700000000);
  CHECK(plain.timestamp() == 0);  // not read-only
  CHECK(plain.flags().needs_new_timestamp);
}

TEST_CASE("the vendor stamper finds GDS_FILE on the search path, compressed, or beside the .mag file",
          "[cap:gds-read][req:When content-hash timestamps are assigned]") {
  const VendorFile f = vendor_file();
  Lookup look;
  look.put("lib/cells.gds", f.bytes);
  look.put("lib/packed.gds.gz", gzip(f.bytes));
  look.put("/proj/sub/local.gds", f.bytes);
  gds::VendorStamper stamper{look.ctx};

  db::CellDef a{"A", kPlanes};
  make_vendor(a, "cells.gds", f.a_start);
  CHECK(stamper.stamp(a) == kLeafStamp);
  db::CellDef packed{"A2", kPlanes};
  make_vendor(packed, "packed.gds", f.e_start);
  CHECK(stamper.stamp(packed) == kEmptyStamp);
  db::CellDef local{"A3", kPlanes};
  make_vendor(local, "local.gds", f.a_start);
  CHECK(stamper.stamp(local) == 0);  // not on the search path
  local.set_file_path("/proj/sub/A3.mag");
  CHECK(stamper.stamp(local) == kLeafStamp);
  db::CellDef absolute{"A4", kPlanes};
  make_vendor(absolute, "/proj/sub/local.gds", f.a_start);
  CHECK(stamper.stamp(absolute) == kLeafStamp);
  db::CellDef missing{"A5", kPlanes};
  make_vendor(missing, "nowhere.gds", f.a_start);
  missing.set_file_path("/proj/sub/A5.mag");
  CHECK(stamper.stamp(missing) == 0);
  db::CellDef none{"A6", kPlanes};
  CHECK(stamper.stamp(none) == 0);
}

TEST_CASE("each GDS file is scanned once per update pass", "[cap:gds-read][req:When content-hash timestamps are assigned]") {
  const VendorFile f = vendor_file();
  Lookup look;
  look.put("lib/cells.gds", f.bytes);
  gds::VendorStamper stamper{look.ctx};
  db::CellDef a{"A", kPlanes};
  make_vendor(a, "cells.gds", f.a_start);
  CHECK(stamper.stamp(a) == kLeafStamp);
  // Changing the file during the pass does not change the answer...
  Gds other;
  other.library("lib");
  look.put("lib/cells.gds", other.bytes());
  CHECK(stamper.stamp(a) == kLeafStamp);
  // ...until the cache is flushed at the end of the pass.
  stamper.flush();
  CHECK(stamper.stamp(a) == 0);
  // An unreadable file gives 0.
  look.put("lib/broken.gds", gds_test::hex("1f 8b 00 01 02"));
  db::CellDef b{"B", kPlanes};
  make_vendor(b, "broken.gds", f.a_start);
  CHECK(stamper.stamp(b) == 0);
}

TEST_CASE("editing a vendor cell keeps its stamp; pointing it to new data changes it and its parents",
          "[cap:gds-read][req:When content-hash timestamps are assigned]") {
  const VendorFile f = vendor_file();
  Lookup look;
  look.put("lib/cells.gds", f.bytes);
  db::CellLibrary library;
  db::CellDef& top = *library.create("top", kPlanes).value();
  db::CellDef& pad = *library.create("pad", kPlanes).value();
  REQUIRE(db::place(top, pad, db::PlaceOptions{}).ok());
  make_vendor(pad, "cells.gds", f.a_start);
  pad.set_timestamp(kLeafStamp);
  top.set_timestamp(100);
  for (db::CellDef* d : {&top, &pad}) {
    d->flags().needs_new_timestamp = false;
    d->flags().child_timestamps_changed = false;
  }
  gds::VendorStamper stamper{look.ctx};
  const cell::VendorStampFn hook = [&](const db::CellDef& def) { return stamper.stamp(def); };

  pad.flags().needs_new_timestamp = true;  // the Magic view was edited
  cell::update_timestamps(library, nullptr, 500, hook);
  stamper.flush();
  CHECK(pad.timestamp() == kLeafStamp);
  CHECK_FALSE(pad.flags().needs_new_timestamp);
  CHECK(top.timestamp() == 100);
  CHECK_FALSE(top.flags().child_timestamps_changed);

  db::set_property(pad, "GDS_START", db::PropertyValue{db::DoubleList{{static_cast<std::int64_t>(f.e_start)}}});
  pad.flags().needs_new_timestamp = true;
  cell::update_timestamps(library, nullptr, 600, hook);
  stamper.flush();
  CHECK(pad.timestamp() == kEmptyStamp);
  CHECK(top.timestamp() == 600);
  CHECK(top.flags().child_timestamps_changed);

  // Without the hook (or with a missing file) the stamp is unchanged.
  db::set_property(pad, "GDS_START", db::PropertyValue{db::DoubleList{{static_cast<std::int64_t>(f.a_start)}}});
  pad.flags().needs_new_timestamp = true;
  cell::update_timestamps(library, &pad, 700);
  CHECK(pad.timestamp() == kEmptyStamp);
}

TEST_CASE("the save-path timestamp update restamps vendor cells through the search path",
          "[cap:gds-read][req:When content-hash timestamps are assigned]") {
  const VendorFile f = vendor_file();
  Lookup look;
  look.put("lib/cells.gds", f.bytes);
  db::CellLibrary library;
  db::CellDef& top = *library.create("top", kPlanes).value();
  db::CellDef& pad = *library.create("pad", kPlanes).value();
  REQUIRE(db::place(top, pad, db::PlaceOptions{}).ok());
  make_vendor(pad, "cells.gds", f.e_start);
  pad.set_timestamp(kLeafStamp);
  top.set_timestamp(100);
  top.flags().needs_new_timestamp = false;
  pad.flags().needs_new_timestamp = true;

  cell::update_timestamps(library, &pad, 600, look.ctx);
  CHECK(pad.timestamp() == kEmptyStamp);
  CHECK(top.timestamp() == 600);

  // Each pass drops its scan cache, so the next pass reads the file again.
  Gds other;
  other.library("lib");
  look.put("lib/cells.gds", other.bytes());
  db::set_property(pad, "GDS_START", db::PropertyValue{db::DoubleList{{static_cast<std::int64_t>(f.a_start)}}});
  pad.flags().needs_new_timestamp = true;
  cell::update_timestamps(library, nullptr, 700, look.ctx);
  CHECK(pad.timestamp() == kEmptyStamp);  // no structure starts there now: unchanged
  CHECK(top.timestamp() == 600);
  look.put("lib/cells.gds", f.bytes);
  pad.flags().needs_new_timestamp = true;
  cell::update_timestamps(library, nullptr, 800, look.ctx);
  CHECK(pad.timestamp() == kLeafStamp);
  CHECK(top.timestamp() == 800);
}

TEST_CASE("the save paths restamp vendor cells through the cell search path",
          "[cap:gds-read][req:When content-hash timestamps are assigned]") {
  // Regression (round 4 integration): save, writeall and the stamp pass of
  // `save` go through cell::update_save_stamps, which restamps read-only
  // cells from their GDS_FILE (DBUpdateStamps with CalmaVendorStamp).
  const VendorFile f = vendor_file();
  Lookup look;
  look.put("lib/cells.gds", f.bytes);
  cell::test::CellIo io{cell::test::tiny_technology(), &look.fs};
  io.paths.path = {"lib"};
  db::CellDef& top = *io.library.create("top", kPlanes).value();
  db::CellDef& pad = *io.library.create("pad", kPlanes).value();
  REQUIRE(db::place(top, pad, db::PlaceOptions{}).ok());
  make_vendor(pad, "cells.gds", f.e_start);
  pad.set_timestamp(kLeafStamp);
  top.set_timestamp(100);
  top.flags().needs_new_timestamp = false;
  pad.flags().needs_new_timestamp = true;

  cell::update_save_stamps(io.ctx, nullptr, 600);
  CHECK(pad.timestamp() == kEmptyStamp);
  CHECK(top.timestamp() == 600);
  CHECK(io.sink.snapshot().empty());  // the lookup is quiet
}
