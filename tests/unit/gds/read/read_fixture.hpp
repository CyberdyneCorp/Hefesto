// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the gds::read tests: tests/data/gds/read/read.tech with a
// test's own cifinput (and cifoutput) section, a session with the styles
// installed, a GDSII builder for libraries, structures and elements, and
// queries on the cells read. The default library units are 1e-8 m per DB
// unit, so with `scalefactor 1` one DB unit is one read unit and one
// internal unit (num = den = S = 1).
// Owner: GDSR.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../stream/stream_fixture.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/read/read.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/gds/stream/encode.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace gds_read_test {

namespace gds = hefesto::gds;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace mask = hefesto::mask;
namespace diag = hefesto::diag;
namespace compat = hefesto::compat;
namespace platform = hefesto::platform;
using gds_test::Bytes;

/// The cifinput of most tests: S = 1, labels on metal1 (68/5 text, 68/16
/// pin as port), metal2 69/20, poly 66/20.
inline constexpr std::string_view kStyle =
    "style test\n"
    "scalefactor 1\n"
    "layer m1 M1\n"
    "labels M1\n"
    "calma M1 68 20\n"
    "layer m1 M1PIN\n"
    "labels M1PIN port\n"
    "calma M1PIN 68 16\n"
    "calma M1PIN 68 5\n"
    "layer m2 M2\n"
    "calma M2 69 20\n"
    "layer poly PO\n"
    "calma PO 66 20\n";

inline std::string read_tech_text() {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "read.tech");
  REQUIRE(in.good());
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

inline std::shared_ptr<const tech::Technology> tech_with(std::string_view cifinput, std::string_view cifoutput) {
  std::string text = read_tech_text();
  text += "\ncifoutput\n";
  text += cifoutput;
  text += "\nend\n\ncifinput\n";
  text += cifinput;
  text += "\nend\n\ndrc\nend\n\nextract\nend\n";
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/read.tech", fs, tech::LoadOptions{}, sink);
  if (!loaded.ok()) {
    for (const auto& d : sink.take()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

/// A point of an XY record.
struct P {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

struct PathSpec {
  std::optional<int> type;
  std::optional<std::int32_t> width;
  std::optional<std::int32_t> begin;
  std::optional<std::int32_t> end;
};

struct TextSpec {
  std::optional<int> presentation;
  std::optional<std::int32_t> width;
  bool strans = false;
  std::optional<double> mag;
  std::optional<double> angle;
};

struct RefSpec {
  int flags = 0;
  std::optional<double> mag;
  std::optional<double> angle;
  std::vector<std::pair<int, std::string>> properties;
};

/// GDSII built record by record (record numbers as calmaInt.h).
class GdsFile {
 public:
  using PathSpec = gds_read_test::PathSpec;
  using TextSpec = gds_read_test::TextSpec;
  using RefSpec = gds_read_test::RefSpec;

  GdsFile& library(std::string_view name = "lib", double mpd = 1e-8) {
    g_.i2(0x00, {600});
    g_.i2(0x01, {124, 1, 1, 0, 0, 0, 124, 1, 1, 0, 0, 0});
    g_.str(0x02, name);
    return units(mpd);
  }
  GdsFile& units(double mpd) {
    Bytes p;
    for (const double v : {0.001, mpd}) {
      const auto r = gds::encode_real8(v);
      p.insert(p.end(), r.begin(), r.end());
    }
    g_.rec(0x03, 5, p);
    return *this;
  }
  GdsFile& begin(std::string_view name, std::initializer_list<int> date = {124, 1, 15, 10, 30, 0, 124, 1, 15, 10, 30, 0}) {
    g_.i2(0x05, date);
    g_.str(0x06, name);
    return *this;
  }
  GdsFile& end() {
    g_.empty(0x07);
    return *this;
  }
  GdsFile& end_library() {
    g_.empty(0x04);
    return *this;
  }
  GdsFile& xy(std::initializer_list<P> points) {
    std::vector<std::int32_t> v;
    for (const P& p : points) {
      v.push_back(p.x);
      v.push_back(p.y);
    }
    return i4s(0x10, v);
  }
  GdsFile& boundary(int layer, int datatype, std::initializer_list<P> points) {
    g_.empty(0x08);
    g_.i2(0x0d, {layer});
    g_.i2(0x0e, {datatype});
    xy(points);
    g_.empty(0x11);
    return *this;
  }
  GdsFile& box(int layer, int boxtype, std::initializer_list<P> points) {
    g_.empty(0x2d);
    g_.i2(0x0d, {layer});
    g_.i2(0x2e, {boxtype});
    xy(points);
    g_.empty(0x11);
    return *this;
  }
  GdsFile& path(int layer, int datatype, const PathSpec& spec, std::initializer_list<P> points) {
    g_.empty(0x09);
    g_.i2(0x0d, {layer});
    g_.i2(0x0e, {datatype});
    if (spec.type) {
      g_.i2(0x21, {*spec.type});
    }
    if (spec.width) {
      g_.i4(0x0f, {*spec.width});
    }
    if (spec.begin) {
      g_.i4(0x30, {*spec.begin});
    }
    if (spec.end) {
      g_.i4(0x31, {*spec.end});
    }
    xy(points);
    g_.empty(0x11);
    return *this;
  }
  GdsFile& text(int layer, int texttype, P at, std::string_view s, const TextSpec& spec = {}) {
    g_.empty(0x0c);
    g_.i2(0x0d, {layer});
    g_.i2(0x16, {texttype});
    if (spec.presentation) {
      g_.i2(0x17, {*spec.presentation});
    }
    if (spec.width) {
      g_.i4(0x0f, {*spec.width});
    }
    if (spec.strans || spec.mag || spec.angle) {
      strans(0, spec.mag, spec.angle);
    }
    xy({at});
    g_.str(0x19, s);
    g_.empty(0x11);
    return *this;
  }
  GdsFile& sref(std::string_view name, P at, const RefSpec& spec = {}) {
    g_.empty(0x0a);
    g_.str(0x12, name);
    if (spec.flags != 0 || spec.mag || spec.angle) {
      strans(spec.flags, spec.mag, spec.angle);
    }
    xy({at});
    properties(spec.properties);
    g_.empty(0x11);
    return *this;
  }
  GdsFile& aref(std::string_view name, int cols, int rows, std::initializer_list<P> points, const RefSpec& spec = {}) {
    g_.empty(0x0b);
    g_.str(0x12, name);
    if (spec.flags != 0 || spec.mag || spec.angle) {
      strans(spec.flags, spec.mag, spec.angle);
    }
    g_.i2(0x13, {cols, rows});
    xy(points);
    properties(spec.properties);
    g_.empty(0x11);
    return *this;
  }
  GdsFile& raw(std::uint8_t type, std::uint8_t data_type, const Bytes& payload) {
    g_.rec(type, data_type, payload);
    return *this;
  }
  GdsFile& empty(std::uint8_t type) {
    g_.empty(type);
    return *this;
  }
  GdsFile& i2(std::uint8_t type, std::initializer_list<int> values) {
    g_.i2(type, values);
    return *this;
  }
  GdsFile& i4(std::uint8_t type, std::initializer_list<std::int32_t> values) {
    g_.i4(type, values);
    return *this;
  }
  GdsFile& str(std::uint8_t type, std::string_view text) {
    g_.str(type, text);
    return *this;
  }

  [[nodiscard]] const Bytes& bytes() const { return g_.bytes(); }
  [[nodiscard]] std::size_t size() const { return g_.size(); }

 private:
  GdsFile& i4s(std::uint8_t type, const std::vector<std::int32_t>& values) {
    Bytes p;
    for (const std::int32_t v : values) {
      const auto u = static_cast<std::uint32_t>(v);
      for (const unsigned shift : {24U, 16U, 8U, 0U}) {
        p.push_back(static_cast<std::byte>((u >> shift) & 0xffU));
      }
    }
    g_.rec(type, 3, p);
    return *this;
  }
  void real(std::uint8_t type, double v) {
    const auto r = gds::encode_real8(v);
    g_.rec(type, 5, Bytes{r.begin(), r.end()});
  }
  void strans(int flags, std::optional<double> mag, std::optional<double> angle) {
    g_.i2(0x1a, {flags});
    if (mag) {
      real(0x1b, *mag);
    }
    if (angle) {
      real(0x1c, *angle);
    }
  }
  void properties(const std::vector<std::pair<int, std::string>>& props) {
    for (const auto& [attr, value] : props) {
      g_.i2(0x2b, {attr});
      g_.str(0x2c, value);
    }
  }

  gds_test::Gds g_;
};

inline compat::CompatSettings mode(bool magic_compat) {
  compat::CompatSettings c;
  c.magic_compat = magic_compat;
  return c;
}

/// A session with the technology and both style sets installed.
struct Session {
  compat::CompatSettings compat;
  hefesto::editor::EditorState editor;
  gds::GdsReadOptions options;

  explicit Session(std::string_view cifinput = kStyle, bool magic_compat = true, std::string_view cifoutput = {}) {
    compat = mode(magic_compat);
    editor.technology = tech_with(cifinput, cifoutput);
    editor.drc.compat = &compat;
    diag::CollectingSink quiet;
    editor.mask.output_styles = mask::read_output_styles(*editor.technology, quiet, compat);
    editor.mask.input_styles = mask::read_input_styles(*editor.technology, compat, quiet);
    REQUIRE(editor.mask.input_styles.current_style() != nullptr);
    options.time_zone = "UTC0";
    options.gds_file = "/data/test.gds";
  }

  /// Loads `count` copies of a one-glyph test font, so GDS fonts 0..count-1
  /// exist (Magic's default session has FreeSans, FreeSerif and FreeMono).
  void add_fonts(int count) {
    constexpr std::string_view kFont = R"(%!PS-AdobeFont-1.0: Test
/FontBBox {0 0 1000 1000 }readonly def
/CharProcs 128 dict def
CharProcs begin
  /.notdef { 500 0 0 0 400 700 setcachedevice
	} bind def
currentdict end
/Test.Regular exch definefont
)";
    for (int i = 0; i < count; ++i) {
      auto font = hefesto::display::parse_outline_font(kFont, "Test.pt3");
      REQUIRE(font.ok());
      font.value().name = "Test" + std::to_string(i);
      editor.fonts.fonts.push_back(std::make_shared<const hefesto::display::OutlineFont>(std::move(font).value()));
    }
  }

  hefesto::cmd::CommandResult read(const GdsFile& file) {
    gds::MemorySource source{file.bytes()};
    last = gds::read(editor, source, options);
    return last;
  }

  [[nodiscard]] const db::CellDef& cell(std::string_view name) const {
    const db::CellDef* def = editor.cells.find(name);
    INFO("cell " << name);
    REQUIRE(def != nullptr);
    return *def;
  }
  [[nodiscard]] bool has_cell(std::string_view name) const { return editor.cells.find(name) != nullptr; }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  /// The tiles of `type` in `cell` (split tiles by bounding box), merged
  /// rectangles in enumeration order.
  [[nodiscard]] std::vector<db::Rect> painted(std::string_view cell_name, std::string_view type_name) const {
    const db::CellDef& def = cell(cell_name);
    const tech::TileType t = type(type_name);
    tech::TypeMask m;
    m.set(t);
    std::vector<db::Rect> out;
    def.plane(*editor.technology->home_plane(t)).enumerate(db::whole_plane(), m, [&](const db::TileView& v) {
      out.push_back(v.area);
      return true;
    });
    return out;
  }
  /// The area of `type` in `cell` (a split tile counts half).
  [[nodiscard]] double area(std::string_view cell_name, std::string_view type_name) const {
    const db::CellDef& def = cell(cell_name);
    const tech::TileType t = type(type_name);
    tech::TypeMask m;
    m.set(t);
    double total = 0;
    def.plane(*editor.technology->home_plane(t)).enumerate(db::whole_plane(), m, [&](const db::TileView& v) {
      const double a = static_cast<double>(v.area.x1 - v.area.x0) * static_cast<double>(v.area.y1 - v.area.y0);
      total += v.split ? a / 2 : a;
      return true;
    });
    return total;
  }
  /// The bounding box of `type` in `cell`.
  [[nodiscard]] db::Rect extent(std::string_view cell_name, std::string_view type_name) const {
    const auto rects = painted(cell_name, type_name);
    REQUIRE_FALSE(rects.empty());
    db::Rect r = rects.front();
    for (const db::Rect& q : rects) {
      r = db::Rect{std::min(r.x0, q.x0), std::min(r.y0, q.y0), std::max(r.x1, q.x1), std::max(r.y1, q.y1)};
    }
    return r;
  }

  [[nodiscard]] std::vector<std::string> texts() const {
    std::vector<std::string> out;
    for (const auto& d : last.diagnostics) {
      out.push_back(d.text);
    }
    return out;
  }
  /// Whether a message contains `fragment`.
  [[nodiscard]] bool said(std::string_view fragment) const {
    return std::any_of(last.diagnostics.begin(), last.diagnostics.end(),
                       [&](const diag::Diagnostic& d) { return d.text.find(fragment) != std::string::npos; });
  }
  [[nodiscard]] std::size_t count(std::string_view fragment) const {
    return static_cast<std::size_t>(std::count_if(last.diagnostics.begin(), last.diagnostics.end(), [&](const diag::Diagnostic& d) {
      return d.text.find(fragment) != std::string::npos;
    }));
  }

  hefesto::cmd::CommandResult last;
};

/// The uses of `parent`, in creation order.
inline std::vector<const db::CellUse*> uses(const db::CellDef& parent) {
  std::vector<const db::CellUse*> out;
  for (const auto& u : parent.uses()) {
    out.push_back(u.get());
  }
  return out;
}

inline const db::Label* label(const db::CellDef& def, std::string_view text) {
  for (const db::Label& l : def.labels()) {
    if (l.text == text) {
      return &l;
    }
  }
  return nullptr;
}

}  // namespace gds_read_test
