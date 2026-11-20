// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the oasis::write tests (tasks.md 12.1-12.7): the gds/write
// fixture (the spec's reference style: `scalefactor 10`, expander 1,
// nanometer units, layer M1 = metal1 on 49/0; cells built in memory; a
// memory file system), OASIS writes with the session's `oasis` settings,
// and a decoder of the written file with names resolved.
// Owner: OASW.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "../../gds/write/write_fixture.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/decoder.hpp"
#include "hefesto/oasis/codec/encoder.hpp"
#include "hefesto/oasis/write/write.hpp"

namespace oasis_write_test {

using namespace gds_write_test;  // NOLINT(google-build-using-namespace): the shared fixture
namespace oasis = hefesto::oasis;

/// The bytes of a string, as std::byte.
inline std::vector<std::byte> to_bytes(const std::string& text) {
  std::vector<std::byte> out(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    out[i] = static_cast<std::byte>(text[i]);
  }
  return out;
}

/// `needle` (hex bytes) occurs in `bytes`.
inline bool contains(const std::string& bytes, std::initializer_list<int> needle) {
  std::string n;
  for (const int b : needle) {
    n.push_back(static_cast<char>(b));
  }
  return bytes.find(n) != std::string::npos;
}

/// A decoded OASIS file: every record, and the name tables.
struct Decoded {
  std::vector<oasis::Record> records;
  oasis::NameTables names;

  /// The name of a CELL record or of a PLACEMENT/TEXT element.
  [[nodiscard]] std::string cell_name(const oasis::Record& r) const {
    return r.reference ? names.cellnames.at(*r.reference) : r.name;
  }
  [[nodiscard]] std::string element_name(const oasis::Element& e) const {
    if (!e.reference) {
      return e.name;
    }
    return e.kind == oasis::ElementKind::text ? names.textstrings.at(*e.reference) : names.cellnames.at(*e.reference);
  }
  /// The cell names in CELL record order.
  [[nodiscard]] std::vector<std::string> cells() const {
    std::vector<std::string> out;
    for (const oasis::Record& r : records) {
      if (r.id == oasis::RecordId::cell_by_name || r.id == oasis::RecordId::cell_by_reference) {
        out.push_back(cell_name(r));
      }
    }
    return out;
  }
  /// The elements of cell `name`, names resolved into `name`.
  [[nodiscard]] std::vector<oasis::Element> elements(std::string_view name) const {
    std::vector<oasis::Element> out;
    bool inside = false;
    for (const oasis::Record& r : records) {
      if (r.id == oasis::RecordId::cell_by_name || r.id == oasis::RecordId::cell_by_reference) {
        inside = cell_name(r) == name;
      } else if (inside && r.element) {
        oasis::Element e = *r.element;
        if (e.kind == oasis::ElementKind::placement || e.kind == oasis::ElementKind::text) {
          e.name = element_name(e);
          e.reference.reset();
        }
        out.push_back(std::move(e));
      } else if (r.id == oasis::RecordId::end || r.id == oasis::RecordId::cellname_implicit) {
        inside = false;
      }
    }
    return out;
  }
  [[nodiscard]] std::size_t count(oasis::RecordId id) const {
    std::size_t n = 0;
    for (const oasis::Record& r : records) {
      n += r.id == id ? 1U : 0U;
    }
    return n;
  }
  [[nodiscard]] bool any_in_cblock() const {
    for (const oasis::Record& r : records) {
      if (r.in_cblock) {
        return true;
      }
    }
    return false;
  }
};

inline Decoded decode_oasis(const std::string& bytes) {
  hefesto::gds::MemorySource source(to_bytes(bytes));
  oasis::Decoder decoder(source);
  Decoded out;
  for (;;) {
    auto next = decoder.next();
    INFO((next.ok() ? std::string{} : next.error().text));
    REQUIRE(next.ok());
    if (!next.value()) {
      break;
    }
    out.records.push_back(std::move(*next.value()));
  }
  out.names = decoder.names();
  return out;
}

/// The committed 419-byte reference file (tests/data/oasis/codec).
inline std::string reference_419() {
  std::ifstream in{std::string{HEFESTO_SOURCE_DIR} + "/tests/data/oasis/codec/reference_419.oas", std::ios::binary};
  REQUIRE(in.good());
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct OasFixture : Fixture {
  oasis::OasisSettings settings;

  explicit OasFixture(bool magic_compat = true) : Fixture(magic_compat) {}

  /// Writes `root` with `settings` to `path`; the bytes (empty when no
  /// file was made).
  std::string write_oas(db::CellDef& root, const std::string& path = "/out.oas",
                        const mask::Progress* progress = nullptr) {
    install();
    oasis::OasisWriteOptions o;
    o.settings = settings;
    o.prepare = oasis::to_gds_write_options(settings, {});
    o.prepare.files = &fs;
    o.prepare.time_zone = options.time_zone;
    o.prepare.now = options.now;
    o.prepare.typed_name = path;
    last = oasis::write(ed, root, path, o, progress);
    return file(path);
  }
};

/// A GDS library built record by record (vendor files).
class GdsLibrary {
 public:
  GdsLibrary() : writer_(sink_) {
    i2(gds::RecordType::header, {600});
    i2(gds::RecordType::bgnlib, std::vector<std::int16_t>(12, 1));
    str(gds::RecordType::libname, "vendor");
    const std::array<double, 2> u{0.001, 1e-9};
    REQUIRE(writer_.write_real8(gds::RecordType::units, u).ok());
  }
  void i2(gds::RecordType t, const std::vector<std::int16_t>& v) { REQUIRE(writer_.write_int2(t, v).ok()); }
  void i4(gds::RecordType t, const std::vector<std::int32_t>& v) { REQUIRE(writer_.write_int4(t, v).ok()); }
  void str(gds::RecordType t, std::string_view s) { REQUIRE(writer_.write_string(t, s).ok()); }
  void empty(gds::RecordType t) { REQUIRE(writer_.write_empty(t).ok()); }
  void real(gds::RecordType t, double v) {
    const std::array<double, 1> a{v};
    REQUIRE(writer_.write_real8(t, a).ok());
  }

  /// BGNSTR, STRNAME; returns the BGNSTR offset (start() is GDS_START).
  std::uint64_t begin(std::string_view name) {
    const std::uint64_t at = position();
    i2(gds::RecordType::bgnstr, {99, 2, 3, 4, 5, 6, 99, 2, 3, 4, 5, 6});
    str(gds::RecordType::strname, name);
    start_ = position();
    return at;
  }
  void boundary(std::int16_t layer, std::int16_t datatype, const std::vector<std::int32_t>& xy) {
    empty(gds::RecordType::boundary);
    i2(gds::RecordType::layer, {layer});
    i2(gds::RecordType::datatype, {datatype});
    i4(gds::RecordType::xy, xy);
    empty(gds::RecordType::endel);
  }
  void path(std::int16_t layer, std::int16_t datatype, std::int16_t type, std::int32_t width,
            const std::vector<std::int32_t>& xy) {
    empty(gds::RecordType::path);
    i2(gds::RecordType::layer, {layer});
    i2(gds::RecordType::datatype, {datatype});
    i2(gds::RecordType::pathtype, {type});
    i4(gds::RecordType::width, {width});
    i4(gds::RecordType::xy, xy);
    empty(gds::RecordType::endel);
  }
  void text(std::int16_t layer, std::int16_t type, std::int32_t x, std::int32_t y, std::string_view s) {
    empty(gds::RecordType::text);
    i2(gds::RecordType::layer, {layer});
    i2(gds::RecordType::texttype, {type});
    i4(gds::RecordType::xy, {x, y});
    str(gds::RecordType::string, s);
    empty(gds::RecordType::endel);
  }
  /// ENDSTR; returns GDS_END.
  std::uint64_t end() {
    empty(gds::RecordType::endstr);
    return position();
  }
  [[nodiscard]] std::uint64_t start() const { return start_; }
  std::string bytes() {
    empty(gds::RecordType::endlib);
    REQUIRE(writer_.flush().ok());
    std::string out;
    for (const std::byte b : sink_.bytes()) {
      out.push_back(static_cast<char>(b));
    }
    return out;
  }
  std::uint64_t position() {
    REQUIRE(writer_.flush().ok());
    return sink_.position();
  }

 private:
  gds::MemorySink sink_;
  gds::RecordWriter writer_;
  std::uint64_t start_ = 0;
};

/// An OASIS file written by oasis::Encoder (strict, uncompressed, unit 1000).
class OasLibrary {
 public:
  explicit OasLibrary(double unit = 1000.0)
      : encoder_(sink_, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, unit}) {
    REQUIRE(encoder_.begin().ok());
    REQUIRE(encoder_.standard_properties(std::nullopt).ok());
  }
  OasLibrary& cell(std::string_view name) {
    REQUIRE(encoder_.begin_cell(name).ok());
    return *this;
  }
  OasLibrary& add(const oasis::Element& e) {
    REQUIRE(encoder_.element(e).ok());
    return *this;
  }
  OasLibrary& property(const oasis::Property& p) {
    REQUIRE(encoder_.property(p).ok());
    return *this;
  }
  std::string bytes() {
    REQUIRE(encoder_.finish().ok());
    std::string out;
    for (const std::byte b : sink_.bytes()) {
      out.push_back(static_cast<char>(b));
    }
    return out;
  }

 private:
  gds::MemorySink sink_;
  oasis::Encoder encoder_;
};

inline oasis::Element rectangle(std::uint64_t layer, std::uint64_t datatype, std::int64_t x, std::int64_t y,
                                std::int64_t w, std::int64_t h) {
  oasis::Element e;
  e.kind = oasis::ElementKind::rectangle;
  e.layer = layer;
  e.datatype = datatype;
  e.position = {x, y};
  e.size = {w, h};
  return e;
}

inline db::Label plain_label(std::string text, tech::TileType type, db::Rect rect) {
  db::Label l;
  l.text = std::move(text);
  l.type = type;
  l.rect = rect;
  return l;
}

inline bool has(const cmd::CommandResult& result, std::string_view code) {
  for (const auto& d : result.diagnostics) {
    if (d.code == code) {
      return true;
    }
  }
  return false;
}

}  // namespace oasis_write_test
