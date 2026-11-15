// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the gds/write unit tests: a small technology (metal1,
// metal2), an editor session with a hand-built output style (the spec's
// reference style: `scalefactor 10`, expander 1, nanometer units, layer M1
// = metal1 on 49/0), cells built in memory, writes into a memory file
// system under TZ "UTC0" with a fixed current time, and a decoder of the
// written records.
// Owner: GDSW.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/gds/stream/values.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/technology.hpp"

namespace gds_write_test {

namespace gds = hefesto::gds;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace mask = hefesto::mask;
namespace diag = hefesto::diag;
namespace compat = hefesto::compat;
namespace platform = hefesto::platform;
namespace cmd = hefesto::cmd;

using Bytes = std::vector<std::uint8_t>;

/// The current time the tests write as the ON modification date.
inline constexpr std::int64_t kNow = 1700000000;

/// One decoded record.
struct Rec {
  std::uint8_t type = 0;
  std::vector<std::byte> data;

  [[nodiscard]] gds::RecordType record() const { return static_cast<gds::RecordType>(type); }
  [[nodiscard]] std::string text() const { return gds::string_value(data); }
  [[nodiscard]] std::vector<std::int32_t> i4() const { return gds::i4_values(data); }
  [[nodiscard]] std::vector<std::int16_t> i2() const { return gds::i2_values(data); }
};

/// The records of a GDS stream.
inline std::vector<Rec> decode(const std::string& bytes) {
  std::vector<std::byte> raw(bytes.size());
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    raw[i] = static_cast<std::byte>(bytes[i]);
  }
  gds::MemorySource source{std::move(raw)};
  gds::RecordReader reader{source};
  std::vector<Rec> out;
  for (;;) {
    auto next = reader.next();
    REQUIRE(next.ok());
    if (!next.value()) {
      break;
    }
    out.push_back(Rec{next.value()->type, next.value()->data});
  }
  return out;
}

/// The bytes of `text` as unsigned values.
inline Bytes bytes_of(const std::string& text) {
  return Bytes(text.begin(), text.end());
}

struct Fixture {
  hefesto::editor::EditorState ed;
  std::shared_ptr<const tech::Technology> technology;
  compat::CompatSettings compat;
  tech::TileType m1 = 0;
  tech::TileType m2 = 0;
  mask::OutputStyle style;
  platform::MemoryFileSystem fs;
  gds::GdsWriteOptions options;
  cmd::CommandResult last = cmd::CommandResult::success();

  explicit Fixture(bool magic_compat = true) {
    tech::TechnologyBuilder b;
    b.set_name("gdsw");
    REQUIRE(b.add_plane("metal1").ok());
    REQUIRE(b.add_plane("metal2").ok());
    m1 = b.add_type("metal1", "m1").value();
    m2 = b.add_type("metal2", "m2").value();
    technology = b.finalize().value();
    ed.technology = technology;
    compat.magic_compat = magic_compat;
    ed.drc.compat = &compat;
    style.name = "gdsii";
    style.scale.scale_factor = 10;
    style.scale.expander = 1;
    add_layer("M1", m1, mask::GdsLayer{49, 0});
    install();
    options.files = &fs;
    options.time_zone = "UTC0";
    options.now = kNow;
    options.settings.datestamp = 0;
  }

  void set_compat(bool on) { compat.magic_compat = on; }

  /// Appends an output layer `or <type>` on `gds` (nullopt: no calma).
  void add_layer(std::string name, tech::TileType type, std::optional<mask::GdsLayer> gds) {
    mask::OutputLayer l;
    l.name = std::move(name);
    mask::LayerOp op;
    op.kind = mask::OpKind::op_or;
    op.types.set(type);
    l.ops.push_back(op);
    l.gds = gds;
    style.layers.push_back(std::move(l));
    style.radius = style.compute_halo();
  }

  /// Makes `style` the session's only, current output style.
  void install() {
    auto shared = std::make_shared<const mask::OutputStyle>(style);
    ed.mask.output_styles.styles = {shared};
    ed.mask.output_styles.base = {shared};
    ed.mask.output_styles.current = 0;
  }

  db::CellDef& cell(const std::string& name) {
    auto made = ed.cells.create(name, technology->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    return *made.value();
  }

  void paint(db::CellDef& def, tech::TileType type, const db::Rect& r) {
    REQUIRE(db::paint_type(def, *technology, r, type).ok());
    db::recompute_bbox(def);
  }

  db::CellUse& place(db::CellDef& parent, db::CellDef& child, db::Transform t = {}, db::ArrayInfo array = {},
                     std::optional<std::string> id = {}) {
    db::PlaceOptions o;
    o.transform = t;
    o.array = array;
    o.id = std::move(id);
    auto u = db::place(parent, child, o);
    REQUIRE(u.ok());
    db::recompute_bbox(parent);
    return *u.value();
  }

  /// Writes `root` to /out.gds; the bytes (empty when no file was made).
  std::string write(db::CellDef& root, const std::string& path = "/out.gds") {
    install();
    last = gds::write(ed, root, path, options);
    return file(path);
  }

  [[nodiscard]] std::string file(const std::string& path = "/out.gds") {
    if (!fs.exists(path)) {
      return {};
    }
    auto read = fs.read(path);
    REQUIRE(read.ok());
    return read.value();
  }

  [[nodiscard]] std::vector<std::string> codes() const {
    std::vector<std::string> out;
    for (const auto& d : last.diagnostics) {
      out.push_back(d.code);
    }
    return out;
  }
  [[nodiscard]] bool has_code(std::string_view code) const {
    for (const auto& d : last.diagnostics) {
      if (d.code == code) {
        return true;
      }
    }
    return false;
  }
};

/// The STRNAME of every structure, in order.
inline std::vector<std::string> structure_names(const std::vector<Rec>& recs) {
  std::vector<std::string> out;
  for (const Rec& r : recs) {
    if (r.record() == gds::RecordType::strname) {
      out.push_back(r.text());
    }
  }
  return out;
}

/// The records of structure `name` (BGNSTR to ENDSTR, both included).
inline std::vector<Rec> structure(const std::vector<Rec>& recs, std::string_view name) {
  std::vector<Rec> out;
  bool inside = false;
  for (std::size_t i = 0; i < recs.size(); ++i) {
    if (recs[i].record() == gds::RecordType::bgnstr && i + 1 < recs.size() && recs[i + 1].text() == name) {
      inside = true;
    }
    if (inside) {
      out.push_back(recs[i]);
      if (recs[i].record() == gds::RecordType::endstr) {
        break;
      }
    }
  }
  return out;
}

/// The instance names (PROPATTR 61 values) of the references of `recs`.
inline std::vector<std::string> use_names(const std::vector<Rec>& recs) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i + 1 < recs.size(); ++i) {
    if (recs[i].record() == gds::RecordType::propattr && recs[i].i2().front() == 61) {
      out.push_back(recs[i + 1].text());
    }
  }
  return out;
}

/// The record types of `recs`.
inline std::vector<gds::RecordType> types(const std::vector<Rec>& recs) {
  std::vector<gds::RecordType> out;
  for (const Rec& r : recs) {
    out.push_back(r.record());
  }
  return out;
}

}  // namespace gds_write_test
