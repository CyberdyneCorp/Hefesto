// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The .mag writer (cell-file-format "Writer header and scale reduction",
// "Writer paint sections", "Writer use blocks", "Writer labels and ports",
// "Writer elements", "Writer properties", "Natural ordering of names"; cell-timestamps
// "Timestamps in written files"). Ported from Magic 8.3.694
// database/DBio.c (DBCellFindScale, dbFindGCFFunc, dbFindCellGCFFunc,
// dbFindPropGCFFunc, DBCellWriteFile, dbWritePaintFunc, dbWriteCellFunc,
// dbWritePropFunc) and utils/fraction.c (FindGCF); Magic is (c) the
// Regents of the University of California, distributed under its
// permissive license (THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <cstdlib>
#include <set>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/mag_io.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/util/record_order.hpp"
#include "mag_elements.hpp"
#include "mag_writer_util.hpp"

namespace hefesto::cell {

namespace detail {

std::vector<db::Rect> hint_tiles(const db::PlaneValue& value) {
  db::Plane plane;
  db::PaintRow row{};
  row.fill(1);
  for (const db::Rect& r : value.rects) {
    if (r.x0 < r.x1 && r.y0 < r.y1) {
      (void)plane.paint(r, row);
    }
  }
  std::vector<db::Rect> out;
  plane.enumerate(db::whole_plane(), tech::TypeMask::of(1), [&](const db::TileView& t) {
    out.push_back(t.area);
    return true;
  });
  return out;
}

tech::TypeMask residues_of(const tech::Technology& tech, tech::TileType t) {
  if (const tech::ContactInfo* ci = tech.contact(t)) {
    return ci->residues;
  }
  return tech::TypeMask::of(t);
}

}  // namespace detail

namespace {

using detail::hint_tiles;
using detail::residues_of;

/// utils/fraction.c FindGCF.
std::int64_t find_gcf(std::int64_t a, std::int64_t b) {
  while (true) {
    const std::int64_t bp = std::llabs(b);
    const std::int64_t m = std::llabs(a) % bp;
    if (m == 0) {
      return bp;
    }
    a = bp;
    b = m;
  }
}

class Gcf {
 public:
  explicit Gcf(std::int64_t start) : g_(start) {}
  /// Folds v in; true once the factor reaches 1.
  bool fold(std::int64_t v) {
    if (v % g_ != 0) {
      g_ = find_gcf(v, g_);
    }
    return g_ == 1;
  }
  bool fold(const db::Rect& r) { return fold(r.x1) || fold(r.x0) || fold(r.y1) || fold(r.y0); }
  [[nodiscard]] std::int64_t value() const noexcept { return g_; }
  [[nodiscard]] bool done() const noexcept { return g_ == 1; }

 private:
  std::int64_t g_;
};

void fold_plane_types(Gcf& g, const db::CellDef& def, const tech::Technology& tech, std::size_t first,
                      std::size_t end) {
  for (std::size_t t = first; t < end && !g.done(); ++t) {
    const auto type = static_cast<tech::TileType>(t);
    const auto home = tech.home_plane(type);
    if (!home || *home >= def.plane_count()) {
      continue;
    }
    def.plane(*home).enumerate(db::whole_plane(), tech::TypeMask::of(type), [&](const db::TileView& v) {
      return !g.fold(v.area);
    });
  }
}

void fold_labels(Gcf& g, const db::CellDef& def, bool font_values) {
  for (const db::Label& lab : def.labels()) {
    if (g.fold(lab.rect)) {
      return;
    }
    if (font_values && lab.font >= 0 && (g.fold(lab.size) || g.fold(lab.offset.x) || g.fold(lab.offset.y))) {
      return;
    }
  }
}

/// Q-CELL-9 (magic-compat off): element coordinates are folded into r.
void fold_elements(Gcf& g, const db::CellDef& def) {
  for (const db::Element& e : def.elements()) {
    if (e.persistent && g.fold(e.area)) {
      return;
    }
  }
}

void fold_properties(Gcf& g, const db::CellDef& def) {
  def.properties().for_each([&](std::string_view, const db::PropertyValue& value) {
    if (g.done()) {
      return;
    }
    if (const auto* dim = std::get_if<db::DimensionList>(&value)) {
      for (const std::int32_t v : dim->values) {
        if (g.fold(v)) {
          return;
        }
      }
    } else if (const auto* plane = std::get_if<db::PlaneValue>(&value)) {
      for (const db::Rect& r : hint_tiles(*plane)) {
        if (g.fold(r)) {
          return;
        }
      }
    }
  });
}

void fold_uses(Gcf& g, const db::CellDef& def) {
  for (const auto& u : def.uses()) {
    if (g.fold(u->transform.c) || g.fold(u->transform.f) || g.fold(u->def->bbox())) {
      return;
    }
    if (u->array.is_array() && (g.fold(u->array.xsep) || g.fold(u->array.ysep))) {
      return;
    }
  }
}

// --- rendering -------------------------------------------------------------

struct Writer {
  const CellIoContext& ctx;
  const db::CellDef& def;
  std::int64_t r = 1;
  std::string out;

  [[nodiscard]] const tech::Technology& tech() const { return *ctx.tech; }
  [[nodiscard]] std::string num(std::int64_t v) const { return std::to_string(v / r); }
  [[nodiscard]] std::string rect4(const db::Rect& x) const {
    return num(x.x0) + " " + num(x.y0) + " " + num(x.x1) + " " + num(x.y1);
  }
};

void write_header(Writer& w) {
  const std::int64_t l0 = w.ctx.grid.lambda;
  const std::int64_t l1 = w.ctx.grid.internal;
  w.out += "magic\ntech " + w.tech().name() + "\n";
  if (l0 != l1 / w.r) {
    w.out += "magscale " + std::to_string(l0) + " " + std::to_string(l1 / w.r) + "\n";
  }
  w.out += "timestamp " + std::to_string(stamp_as_written(w.def.timestamp())) + "\n";
}

bool side_matches(const Writer& w, tech::TileType side, tech::TileType t) {
  return side == t || (side >= w.tech().user_type_end() && residues_of(w.tech(), side).test(t));
}

/// dbWritePaintFunc: the record for tile `v` written under type `t`, or ""
/// when the tile is not written under `t`.
std::string paint_record(const Writer& w, const db::TileView& v, tech::TileType t) {
  if (!v.split) {
    if (v.type != t && (v.type < w.tech().user_type_end() || !residues_of(w.tech(), v.type).test(t))) {
      return {};
    }
    return "rect " + w.rect4(v.area) + "\n";
  }
  bool right = false;
  if (w.ctx.magic_compat) {  // Q-CELL-15: the left record when the left side matches
    if (side_matches(w, v.left_type, t)) {
      right = false;
    } else if (side_matches(w, v.right_type, t)) {
      right = true;
    } else {
      return {};
    }
  } else {
    right = v.right_side;
  }
  static constexpr const char* kCorner[] = {"nw", "sw", "se", "ne"};
  const db::TriangleCorner corner = db::triangle_corner(v.diagonal, right);
  return "tri " + w.rect4(v.area) + " " + kCorner[static_cast<int>(corner)] + "\n";
}

/// `t` and every stacked type that has `t` among its residues.
tech::TypeMask types_written_as(const tech::Technology& tech, tech::TileType t) {
  tech::TypeMask mask = tech::TypeMask::of(t);
  for (std::size_t s = tech.user_type_end(); s < tech.type_count(); ++s) {
    const auto st = static_cast<tech::TileType>(s);
    if (residues_of(tech, st).test(t)) {
      mask.set(st);
    }
  }
  return mask;
}

void write_paint(Writer& w) {
  const tech::Technology& tech = w.tech();
  const std::size_t user_end = tech.user_type_end();
  for (std::size_t ti = 1; ti < user_end; ++ti) {
    const auto t = static_cast<tech::TileType>(ti);
    const auto home = tech.home_plane(t);
    if (!home || *home >= w.def.plane_count()) {
      continue;
    }
    const tech::TypeMask mask = types_written_as(tech, t);
    bool found = false;
    w.def.plane(*home).enumerate(db::whole_plane(), mask, [&](const db::TileView& v) {
      const std::string rec = paint_record(w, v, t);
      if (!rec.empty()) {
        if (!found) {
          w.out += "<< " + std::string{tech.type_name(t)} + " >>\n";
          found = true;
        }
        w.out += rec;
      }
      return true;
    });
  }
}

std::string parent_file_for_write(const db::CellDef& def) {
  return def.file_path().empty() ? def.name() : def.file_path().generic_string();
}

void write_uses(Writer& w) {
  std::vector<const db::CellUse*> uses;
  for (const auto& u : w.def.uses()) {
    uses.push_back(u.get());
  }
  std::stable_sort(uses.begin(), uses.end(), [](const db::CellUse* a, const db::CellUse* b) {
    return util::compare_natural(a->id, b->id) < 0;
  });
  std::set<const db::CellDef*> written;
  const std::string pf = parent_file_for_write(w.def);
  for (const db::CellUse* u : uses) {
    const std::string cf = u->def->file_path().generic_string();
    const UsePathRequest req{cf, pf, written.contains(u->def), w.ctx.magic_compat};
    const auto path = use_path_for_write(req, w.ctx.vars, w.ctx.host);
    written.insert(u->def);
    w.out += "use " + u->def->name() + " " + (u->locked ? "*" : " ") + u->id + (path ? " " + *path : "") + "\n";
    const db::ArrayInfo& a = u->array;
    if (a.is_array()) {
      w.out += "array " + std::to_string(a.xlo) + " " + std::to_string(a.xhi) + " " + w.num(a.xsep) + " " +
               std::to_string(a.ylo) + " " + std::to_string(a.yhi) + " " + w.num(a.ysep) + "\n";
    }
    w.out += "timestamp " + std::to_string(stamp_as_written(u->def->timestamp())) + "\n";
    const db::Transform& t = u->transform;
    w.out += "transform " + std::to_string(t.a) + " " + std::to_string(t.b) + " " + w.num(t.c) + " " +
             std::to_string(t.d) + " " + std::to_string(t.e) + " " + w.num(t.f) + "\n";
    w.out += "box " + w.rect4(u->def->bbox()) + "\n";
  }
}

std::string_view use_word(db::PortUse u) {
  switch (u) {
    case db::PortUse::signal:
      return " signal";
    case db::PortUse::analog:
      return " analog";
    case db::PortUse::power:
      return " power";
    case db::PortUse::ground:
      return " ground";
    case db::PortUse::clock:
      return " clock";
    case db::PortUse::reset:
      return " reset";  // Q-CELL-6
    case db::PortUse::scan:
      return " scan";
    case db::PortUse::tieoff:
      return " tieoff";
    case db::PortUse::default_use:
      return " default";
  }
  return {};
}

std::string_view class_word(db::PortClass c) {
  switch (c) {
    case db::PortClass::input:
      return " input";
    case db::PortClass::output:
      return " output";
    case db::PortClass::tristate:
      return " tristate";
    case db::PortClass::bidirectional:
      return " bidirectional";
    case db::PortClass::feedthrough:
      return " feedthrough";
    case db::PortClass::default_class:
      return " default";
  }
  return {};
}

std::string_view shape_word(db::PortShape s) {
  switch (s) {
    case db::PortShape::abutment:
      return " abutment";
    case db::PortShape::ring:
      return " ring";
    case db::PortShape::feedthrough:
      return " feedthrough";
    case db::PortShape::default_shape:
      return {};
  }
  return {};
}

std::string port_record(const db::Label& lab) {
  std::string dirs;
  if ((lab.port_directions & db::kPortNorth) != 0) {
    dirs += 'n';
  }
  if ((lab.port_directions & db::kPortSouth) != 0) {
    dirs += 's';
  }
  if ((lab.port_directions & db::kPortEast) != 0) {
    dirs += 'e';
  }
  if ((lab.port_directions & db::kPortWest) != 0) {
    dirs += 'w';
  }
  std::string rec = "port " + std::to_string(lab.port_index) + " " + dirs;
  if (lab.port_use != db::PortUse::default_use || lab.port_class != db::PortClass::default_class ||
      lab.port_shape != db::PortShape::default_shape) {
    rec += use_word(lab.port_use);
    rec += class_word(lab.port_class);
    rec += shape_word(lab.port_shape);
  }
  return rec + "\n";
}

const display::OutlineFont* label_font(const Writer& w, const db::Label& lab) {
  if (lab.font < 0 || w.ctx.fonts == nullptr) {
    return nullptr;
  }
  return w.ctx.fonts->font_of(lab);
}

void write_labels(Writer& w) {
  if (w.def.labels().empty()) {
    return;
  }
  w.out += "<< labels >>\n";
  for (const db::Label& lab : w.def.labels()) {
    if (lab.text.empty()) {
      continue;
    }
    const std::string head = std::string{w.tech().type_name(lab.type)} + " " + (lab.sticky ? "s " : "") +
                             w.rect4(lab.rect) + " " + std::to_string(lab.justification);
    if (const display::OutlineFont* font = label_font(w, lab)) {
      w.out += "flabel " + head + " " + font->name + " " + w.num(lab.size) + " " + std::to_string(lab.rotation) +
               " " + w.num(lab.offset.x) + " " + w.num(lab.offset.y) + " " + lab.text + "\n";
    } else {
      w.out += "rlabel " + head + " " + lab.text + "\n";
    }
    if (lab.port_directions != 0) {
      w.out += port_record(lab);
    }
  }
}

std::string_view type_word(const db::PropertyValue& v) {
  switch (v.index()) {
    case 1:
      return "integer ";
    case 2:
    case 4:
      return "dimension ";
    case 3:
      return "double ";
    default:
      return "string ";
  }
}

std::string property_values(const Writer& w, const db::PropertyValue& v) {
  std::string out;
  if (const auto* s = std::get_if<std::string>(&v)) {
    return " " + *s;
  }
  if (const auto* i = std::get_if<db::IntegerList>(&v)) {
    for (const auto x : i->values) {
      out += " " + std::to_string(x);
    }
  } else if (const auto* d = std::get_if<db::DimensionList>(&v)) {
    for (const auto x : d->values) {
      out += " " + w.num(x);
    }
  } else if (const auto* dd = std::get_if<db::DoubleList>(&v)) {
    for (const auto x : dd->values) {
      out += " " + std::to_string(x);
    }
  } else if (const auto* p = std::get_if<db::PlaneValue>(&v)) {
    for (const db::Rect& r : hint_tiles(*p)) {
      out += " " + w.rect4(r);
    }
  }
  return out;
}

void write_properties(Writer& w) {
  std::vector<std::pair<std::string, const db::PropertyValue*>> props;
  w.def.properties().for_each(
      [&](std::string_view key, const db::PropertyValue& value) { props.emplace_back(std::string{key}, &value); });
  if (props.empty()) {
    return;
  }
  std::stable_sort(props.begin(), props.end(),
                   [](const auto& a, const auto& b) { return util::compare_natural(a.first, b.first) < 0; });
  const bool all_string = w.ctx.property_compat && w.ctx.magic_compat;  // Q-CELL-7
  w.out += "<< properties >>\n";
  for (const auto& [key, value] : props) {
    w.out += all_string ? std::string_view{"string "} : type_word(*value);
    w.out += key + property_values(w, *value) + "\n";
  }
}

}  // namespace

std::int64_t find_write_scale(const db::CellDef& def, const tech::Technology& tech, const tech::GridScale& grid,
                              bool magic_compat) {
  if (grid.internal <= grid.lambda) {
    return 1;
  }
  Gcf g{grid.internal};
  if (!magic_compat) {  // Q-CELL-9: built-in types and font values folded too
    fold_plane_types(g, def, tech, 1, tech::kFirstUserType);
  }
  fold_plane_types(g, def, tech, tech::kFirstUserType, tech.user_type_end());
  if (!g.done()) {
    fold_labels(g, def, !magic_compat);
  }
  if (!magic_compat && !g.done()) {
    fold_elements(g, def);
  }
  if (!g.done()) {
    fold_properties(g, def);
  }
  if (!g.done()) {
    fold_uses(g, def);
  }
  return g.value();
}

std::string write_mag(const CellIoContext& ctx, const db::CellDef& def, const WriteOptions& options) {
  Writer w{ctx, def, find_write_scale(def, *ctx.tech, ctx.grid, ctx.magic_compat), {}};
  if (def.flags().needs_new_timestamp && !options.backup) {
    ctx.sink.emit(diag::make(codes::kOutOfDateStamp, "Magic error: writing out-of-date timestamp for " +
                                                         def.name() + "."));
  }
  write_header(w);
  write_paint(w);
  write_uses(w);
  write_labels(w);
  w.out += detail::element_section(def, w.r);
  write_properties(w);
  w.out += "<< end >>\n";
  return std::move(w.out);
}

}  // namespace hefesto::cell
