// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The command-script save variant (cell-file-format "Command-script save
// variant", "Command-script content", with the Q-CELL-14 fixes: each split
// side with its own direction, instance transforms, arrays, locks and
// paths, label offsets converted like sizes, quoted texts, port use, class
// and shape lines, and dimension properties in internal units). Follows
// Magic 8.3.694 database/DBio.c (DBCellWriteCommandFile).

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "hefesto/cell/mag_io.hpp"
#include "hefesto/db/labels.hpp"
#include "mag_writer_util.hpp"

namespace hefesto::cell {

namespace {

constexpr std::array<std::string_view, 4> kCorner = {"nw", "sw", "se", "ne"};
constexpr std::array<std::string_view, 9> kPosition = {"c", "n", "ne", "e", "se", "s", "sw", "w", "nw"};

std::string rect4(const db::Rect& r) {
  return std::to_string(r.x0) + " " + std::to_string(r.y0) + " " + std::to_string(r.x1) + " " + std::to_string(r.y1);
}

std::string tcl_quoted(std::string_view text) {
  std::string out = "\"";
  for (const char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out + "\"";
}

void script_paint(std::string& out, const tech::Technology& tech, const db::CellDef& def) {
  tech::TypeMask all;
  for (std::size_t t = tech::kFirstUserType; t < tech.type_count(); ++t) {
    all.set(static_cast<tech::TileType>(t));
  }
  for (std::size_t p = 0; p < def.plane_count() && p < tech.plane_count(); ++p) {
    def.plane(static_cast<tech::PlaneId>(p)).enumerate(db::whole_plane(), all, [&](const db::TileView& v) {
      out += "box values " + rect4(v.area) + "\n";
      if (!v.split) {
        out += "paint " + std::string{tech.type_name(v.type)} + "\n";
      } else {
        const db::TriangleCorner corner = db::triangle_corner(v.diagonal, v.right_side);
        out += "splitpaint " + std::string{kCorner[static_cast<std::size_t>(corner)]} + " " +
               std::string{tech.type_name(v.type)} + "\n";
      }
      return true;
    });
  }
}

/// The getcell orientation keyword of a transform's rotation part
/// (cmd-hierarchy "Placement reference syntax").
std::string_view orientation_word(const db::Transform& t) {
  struct Entry {
    std::int32_t a, b, d, e;
    std::string_view word;
  };
  static constexpr std::array<Entry, 8> kTable = {{{1, 0, 0, 1, "0"},
                                                   {0, 1, -1, 0, "90"},
                                                   {-1, 0, 0, -1, "180"},
                                                   {0, -1, 1, 0, "270"},
                                                   {1, 0, 0, -1, "v"},
                                                   {-1, 0, 0, 1, "h"},
                                                   {0, 1, 1, 0, "90v"},
                                                   {0, -1, -1, 0, "270v"}}};
  for (const Entry& e : kTable) {
    if (e.a == t.a && e.b == t.b && e.d == t.d && e.e == t.e) {
      return e.word;
    }
  }
  return "0";
}

/// `array` in the parent frame: the indices as DBMakeArray swaps them for
/// a rotated use, the spacing as the box size. A spacing that is negative
/// in the parent frame cannot be given as a box size; its magnitude is used.
void script_array(std::string& out, const db::CellUse& u) {
  const db::ArrayInfo& a = u.array;
  const db::Transform& t = u.transform;
  const std::int64_t sx = a.xsep;  // 64-bit: no overflow on negation
  const std::int64_t sy = a.ysep;
  const std::int64_t xsep = t.a * sx + t.b * sy;
  const std::int64_t ysep = t.d * sx + t.e * sy;
  const bool swapped = t.a == 0;
  const std::int32_t xlo = swapped ? a.ylo : a.xlo;
  const std::int32_t xhi = swapped ? a.yhi : a.xhi;
  const std::int32_t ylo = swapped ? a.xlo : a.ylo;
  const std::int32_t yhi = swapped ? a.xhi : a.yhi;
  out += "box values 0 0 " + std::to_string(xsep < 0 ? -xsep : xsep) + " " + std::to_string(ysep < 0 ? -ysep : ysep) +
         "\n";
  out += "array " + std::to_string(xlo) + " " + std::to_string(xhi) + " " + std::to_string(ylo) + " " +
         std::to_string(yhi) + "\n";
}

/// Each instance with its exact transform: the child's origin lands on the
/// translation, after the orientation about the origin. The cell is named
/// by its recorded path when that has a directory, so getcell reads the
/// same file.
void script_uses(std::string& out, const db::CellDef& def) {
  for (const auto& u : def.uses()) {
    std::string recorded = u->def->file_path().generic_string();
    if (recorded.size() > 4 && recorded.ends_with(".mag")) {
      recorded.resize(recorded.size() - 4);
    }
    const std::string cell = recorded.find('/') != std::string::npos ? recorded : u->def->name();
    out += "getcell " + cell + " child 0 0 parent " + std::to_string(u->transform.c) + " " +
           std::to_string(u->transform.f);
    if (const std::string_view orient = orientation_word(u->transform); orient != "0") {
      out += " " + std::string{orient} + " 0 0";
    }
    out += "\nidentify " + u->id + "\n";
    if (u->array.is_array()) {
      script_array(out, *u);
    }
    if (u->locked) {
      out += "instance lock " + u->id + "\n";
    }
  }
}

void script_port(std::string& out, const db::Label& lab) {
  if (!lab.sticky) {
    out += "select area label\n";
  }
  const std::string index = std::to_string(lab.port_index);
  out += "port make " + index + "\n";
  out += "port connections";
  for (const auto& [bit, word] : {std::pair{db::kPortNorth, " n"}, std::pair{db::kPortSouth, " s"},
                                  std::pair{db::kPortEast, " e"}, std::pair{db::kPortWest, " w"}}) {
    if ((lab.port_directions & bit) != 0) {
      out += word;
    }
  }
  out += "\n";
  for (const auto& [word, attribute] : {std::pair{detail::use_word(lab.port_use), "use"},
                                        std::pair{detail::class_word(lab.port_class), "class"},
                                        std::pair{detail::shape_word(lab.port_shape), "shape"}}) {
    if (!word.empty() && word != " default") {
      out += "port " + index + " " + attribute + std::string{word} + "\n";
    }
  }
}

void script_labels(std::string& out, const tech::Technology& tech, const db::CellDef& def,
                   const display::FontRegistry* fonts) {
  for (const db::Label& lab : def.labels()) {
    out += "box values " + rect4(lab.rect) + "\n";
    const std::string_view pos =
        lab.justification >= 0 && lab.justification < 9 ? kPosition[static_cast<std::size_t>(lab.justification)] : "c";
    const display::OutlineFont* font = (fonts != nullptr && lab.font >= 0) ? fonts->font_of(lab) : nullptr;
    if (font != nullptr) {
      out += "label " + tcl_quoted(lab.text) + " " + font->name + " " + std::to_string(lab.size / 8) + " " +
             std::to_string(lab.rotation) + " " + std::to_string(lab.offset.x / 8) + " " +
             std::to_string(lab.offset.y / 8) + " " + std::string{pos} + " " + (lab.sticky ? "-" : "") +
             std::string{tech.type_name(lab.type)} + "\n";
    } else {
      out += "label " + tcl_quoted(lab.text) + " " + std::string{pos} + " " + std::string{tech.type_name(lab.type)} +
             "\n";
    }
    if (lab.port_directions != 0) {
      script_port(out, lab);
    }
  }
}

std::string property_line(std::string_view key, const db::PropertyValue& value) {
  const std::string k{key};
  if (const auto* s = std::get_if<std::string>(&value)) {
    return "property string " + k + " " + tcl_quoted(*s) + "\n";
  }
  std::string out;
  if (const auto* i = std::get_if<db::IntegerList>(&value)) {
    out = "property integer " + k;
    for (const auto v : i->values) {
      out += " " + std::to_string(v);
    }
  } else if (const auto* d = std::get_if<db::DimensionList>(&value)) {
    out = "property dimension " + k;
    for (const auto v : d->values) {
      out += " " + std::to_string(v) + "i";
    }
  } else if (const auto* dd = std::get_if<db::DoubleList>(&value)) {
    out = "property double " + k;
    for (const auto v : dd->values) {
      out += " " + std::to_string(v);
    }
  } else if (const auto* p = std::get_if<db::PlaneValue>(&value)) {
    out = "property " + k;
    for (const db::Rect& r : detail::hint_tiles(*p)) {
      out += " " + rect4(r);
    }
  }
  return out + "\n";
}

}  // namespace

std::string write_command_script(const CellIoContext& ctx, const db::CellDef& def) {
  std::string name = def.name();
  if (name.ends_with(".tcl")) {
    name.resize(name.size() - 4);
  }
  std::string out = "# Command script for generating cell " + name + "\n\nsuspendall\ntech unlock *\n" +
                    "set curunits [units]\nunits internal\nload " + name + " -silent\nbox values 0 0 0 0\n";
  script_paint(out, *ctx.tech, def);
  script_uses(out, def);
  script_labels(out, *ctx.tech, def, ctx.fonts);
  def.properties().for_each(
      [&](std::string_view key, const db::PropertyValue& value) { out += property_line(key, value); });
  out += "select clear\nview\ntech revert\nunits {*}$curunits\nresumeall\n";
  return out;
}

}  // namespace hefesto::cell
