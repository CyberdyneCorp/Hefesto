// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the oasis::read tests (tasks.md 11.1-11.6): the gds-read
// session and GDSII builder (tests/unit/gds/read/read_fixture.hpp, with the
// same test technology copied to tests/data/oasis/read/read.tech), OASIS
// files written by the codec's Encoder (non-strict, uncompressed, unit 100
// so one DB unit is one input unit with `scalefactor 1`, as the GDS
// fixture's 1e-8 m), element builders, and the .mag text of the cells
// read, for the GDS-equivalence comparisons.
// Owner: OASR.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../gds/read/read_fixture.hpp"
#include "hefesto/cell/host.hpp"
#include "hefesto/cell/mag_io.hpp"
#include "hefesto/cell/timestamps.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/encoder.hpp"
#include "hefesto/oasis/read/read.hpp"
#include "hefesto/platform/clock.hpp"

namespace oasis_read_test {

using namespace gds_read_test;  // NOLINT(google-build-using-namespace): the shared fixture
namespace oasis = hefesto::oasis;
namespace cell = hefesto::cell;
using Bytes = std::vector<std::byte>;

/// An OASIS file written by oasis::Encoder.
class Oas {
 public:
  explicit Oas(bool strict = false, std::int32_t compress = 0, double unit = 100.0)
      : encoder_(sink_, oasis::EncoderOptions{strict, oasis::ValidationScheme::crc32, compress, unit}) {
    REQUIRE(encoder_.begin().ok());
  }
  Oas& top(std::string_view name) {
    REQUIRE(encoder_.standard_properties(name).ok());
    return *this;
  }
  Oas& file_property(const oasis::Property& p) {
    REQUIRE(encoder_.file_property(p).ok());
    return *this;
  }
  Oas& layer_name(std::string_view name, std::uint32_t layer, std::uint32_t datatype) {
    REQUIRE(encoder_.layer_name(name, layer, datatype).ok());
    return *this;
  }
  Oas& cell(std::string_view name) {
    REQUIRE(encoder_.begin_cell(name).ok());
    return *this;
  }
  Oas& add(const oasis::Element& e) {
    const auto done = encoder_.element(e);
    INFO((done.ok() ? std::string{} : done.error().text));
    REQUIRE(done.ok());
    return *this;
  }
  Oas& property(const oasis::Property& p) {
    REQUIRE(encoder_.property(p).ok());
    return *this;
  }
  Bytes bytes() {
    REQUIRE(encoder_.finish().ok());
    return sink_.bytes();
  }

 private:
  hefesto::gds::MemorySink sink_;
  oasis::Encoder encoder_;
};

inline oasis::Element rect(std::uint64_t layer, std::uint64_t datatype, std::int64_t x, std::int64_t y, std::int64_t w,
                           std::int64_t h) {
  oasis::Element e;
  e.kind = oasis::ElementKind::rectangle;
  e.layer = layer;
  e.datatype = datatype;
  e.position = {x, y};
  e.size = {w, h};
  return e;
}

inline oasis::Element polygon(std::uint64_t layer, std::uint64_t datatype, std::int64_t x, std::int64_t y,
                              std::vector<oasis::Point64> rest) {
  oasis::Element e;
  e.kind = oasis::ElementKind::polygon;
  e.layer = layer;
  e.datatype = datatype;
  e.position = {x, y};
  e.points = std::move(rest);
  return e;
}

inline oasis::Element path(std::uint64_t layer, std::uint64_t datatype, std::int64_t hw, std::int64_t es,
                           std::int64_t ee, std::int64_t x, std::int64_t y, std::vector<oasis::Point64> rest) {
  oasis::Element e;
  e.kind = oasis::ElementKind::path;
  e.layer = layer;
  e.datatype = datatype;
  e.half_width = hw;
  e.start_extension = es;
  e.end_extension = ee;
  e.position = {x, y};
  e.points = std::move(rest);
  return e;
}

inline oasis::Element text(std::uint64_t layer, std::uint64_t type, std::int64_t x, std::int64_t y,
                           std::string_view s) {
  oasis::Element e;
  e.kind = oasis::ElementKind::text;
  e.layer = layer;
  e.datatype = type;
  e.position = {x, y};
  e.name = std::string{s};
  return e;
}

inline oasis::Element place(std::string_view cell_name, std::int64_t x, std::int64_t y, double angle = 0.0,
                            bool flip = false, double mag = 1.0) {
  oasis::Element e;
  e.kind = oasis::ElementKind::placement;
  e.name = std::string{cell_name};
  e.position = {x, y};
  e.angle = angle;
  e.flip = flip;
  e.magnification = mag;
  return e;
}

inline oasis::Element circle(std::uint64_t layer, std::uint64_t datatype, std::int64_t x, std::int64_t y,
                             std::int64_t r) {
  oasis::Element e;
  e.kind = oasis::ElementKind::circle;
  e.layer = layer;
  e.datatype = datatype;
  e.position = {x, y};
  e.radius = r;
  return e;
}

/// A grid repetition (types 1-3 and 8-9 by their vectors).
inline oasis::Repetition grid(std::uint8_t type, std::uint64_t columns, oasis::Point64 column_step,
                              std::uint64_t rows = 1, oasis::Point64 row_step = {}) {
  oasis::Repetition r;
  r.type = type;
  r.columns = columns;
  r.rows = rows;
  r.column_step = column_step;
  r.row_step = row_step;
  return r;
}

/// A list repetition (types 4-7, 10, 11) with the displacements after the
/// first.
inline oasis::Repetition list(std::uint8_t type, std::vector<oasis::Point64> offsets) {
  oasis::Repetition r;
  r.type = type;
  r.offsets = std::move(offsets);
  return r;
}

inline oasis::Element with(oasis::Element e, oasis::Repetition r) {
  e.repetition = std::move(r);
  return e;
}

/// An S_GDS_PROPERTY with (attribute, string value).
inline oasis::Property gds_property(std::uint64_t attribute, std::string value) {
  oasis::Property p;
  p.name = "S_GDS_PROPERTY";
  p.standard = true;
  p.values = {oasis::PropertyValue::of_unsigned(attribute),
              oasis::PropertyValue::of_string(oasis::PropertyValue::Kind::b_string, std::move(value))};
  return p;
}

/// A gds-read session that also reads OASIS from an in-memory file system.
struct OasSession : Session {
  explicit OasSession(std::string_view cifinput = kStyle, bool magic_compat = true) : Session(cifinput, magic_compat) {
    REQUIRE(fs.create_directories("/data").ok());
    oasis_options.files = &fs;
    oasis_options.oasis_file = "/data/test.oas";
  }

  hefesto::cmd::CommandResult read_oasis(const Bytes& bytes, std::string_view file = "/data/test.oas") {
    REQUIRE(fs.write_atomic(std::string{file}, std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()})
                .ok());
    last = oasis::read(editor, std::string{file}, oasis_options);
    return last;
  }

  /// The .mag text of `name` without its timestamp line.
  [[nodiscard]] std::string mag(std::string_view name) {
    cell::SearchPaths paths;
    cell::PathVariables vars;
    diag::CollectingSink sink;
    cell::TimestampLog stamps;
    hefesto::platform::FixedClock clock{std::chrono::system_clock::time_point{std::chrono::seconds{1000}}};
    auto host = cell::make_memory_file_host(fs, "/work");
    cell::CellIoContext ctx{fs,     clock,   editor.cells, editor.technology.get(), editor.grid, paths, vars, sink,
                            *host,  stamps,  nullptr,      nullptr,                 true,        true,  nullptr,
                            {},     {},      {}};
    std::string text = cell::write_mag(ctx, cell(name));
    std::string out;
    std::size_t from = 0;
    while (from < text.size()) {
      std::size_t to = text.find('\n', from);
      to = to == std::string::npos ? text.size() : to + 1;
      const std::string_view line{text.data() + from, to - from};
      if (!line.starts_with("timestamp ")) {
        out += line;
      }
      from = to;
    }
    return out;
  }

  hefesto::platform::MemoryFileSystem fs;
  oasis::OasisReadOptions oasis_options;
};

/// The .mag text of every cell of a session, by name, without timestamps.
inline std::vector<std::pair<std::string, std::string>> all_mags(OasSession& s) {
  std::vector<std::string> names;
  s.editor.cells.for_each([&](const db::CellDef& def) {
    if (!def.flags().internal) {
      names.push_back(def.name());
    }
  });
  std::sort(names.begin(), names.end());
  std::vector<std::pair<std::string, std::string>> out;
  for (const std::string& n : names) {
    out.emplace_back(n, s.mag(n));
  }
  return out;
}

}  // namespace oasis_read_test
