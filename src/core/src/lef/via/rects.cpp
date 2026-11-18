// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The RECT lines of a VIAS record in DEF write: the residue rectangles and
// the cut array of the output style (defWriteVias, lef/defWrite.c:2099-2256;
// CIFGetContactSize, cif/CIFtech.c:2327-2382; capability def-write "VIAS
// section content", Q-LEFDEF-107, 115). Magic is (c) the Regents of the
// University of California, BSD-style licence (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lef/via/regions.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

/// The cut operator in output units (`size *= oscale; size /= cscale`,
/// each step truncated to int as Magic's int assignments do).
struct CutArray {
  std::int32_t size = 0;
  std::int32_t sep = 0;
  std::int32_t border = 0;
  std::int32_t pitch() const { return size + sep; }
};

std::int32_t to_output(std::int32_t v, float scale, float cscale) {
  const auto scaled = static_cast<std::int32_t>(static_cast<float>(v) * scale);
  return static_cast<std::int32_t>(static_cast<float>(scaled) / cscale);
}

/// `v *= oscale; v /= 2` on an int.
std::int32_t half_output(db::Coord v, float scale) {
  return static_cast<std::int32_t>(static_cast<float>(v) * scale) / 2;
}

float scaled_half(db::Coord v, float scale) {
  return static_cast<float>(v) * scale / 2;
}

ViaRect float_rect(std::string layer, const db::Rect& r, float scale) {
  return ViaRect{std::move(layer), scaled_half(r.x0, scale), scaled_half(r.y0, scale), scaled_half(r.x1, scale),
                 scaled_half(r.y1, scale)};
}

std::optional<std::string> name_or_warn(tech::TileType type, std::string_view via, const DefViaContext& context) {
  auto name = context.lef_name ? context.lef_name(type) : std::nullopt;
  if (!name) {
    context.sink.emit(diag::make_native("LEF-VIA-011", "No LEF layer corresponding to magic type \"" +
                                                           std::string{context.tech.type_name(type)} + "\" of via \"" +
                                                           std::string{via} + "\"; RECT skipped."));
  }
  return name;
}

/// The residue types of `type` in type order (DBResidueMask): a contact's
/// residues, else the type itself.
std::vector<tech::TileType> residues(tech::TileType type, const tech::Technology& tech) {
  const tech::ContactInfo* info = tech.contact(type);
  std::vector<tech::TileType> out;
  for (std::size_t t = tech::kFirstUserType; t < tech.user_type_end(); ++t) {
    const auto r = static_cast<tech::TileType>(t);
    if (info != nullptr ? info->residues.test(r) : r == type) {
      out.push_back(r);
    }
  }
  return out;
}

/// The record's rectangle for residue `r`: the last extra rectangle of
/// that type, else the area.
const db::Rect& residue_area(const ViaDefinition& via, tech::TileType r) {
  const db::Rect* area = &via.area;
  for (const ViaShape& s : via.shapes) {
    if (s.type == r) {
      area = &s.area;
    }
  }
  return *area;
}

/// The cut count and first cut position along one axis; `single` when no
/// whole cut fits and one is centred (the case of the size warning).
struct Axis {
  std::int32_t count = 0;
  std::int32_t start = 0;
  bool single = false;
};

Axis axis(std::int32_t lo, std::int32_t hi, const CutArray& cut) {
  Axis a;
  a.count = (hi - lo + cut.sep - 2 * cut.border) / cut.pitch();
  if (a.count == 0) {
    a.count = 1;
    a.single = true;
    a.start = (lo + hi - cut.size) / 2;
  } else {
    a.start = (lo + hi + cut.sep - a.count * cut.pitch()) / 2;
  }
  return a;
}

void warn_size(std::string_view code, std::string_view what, const CutArray& cut, std::int32_t extent,
               const DefViaContext& context) {
  context.sink.emit(diag::make(code, "Warning: via size is " + std::to_string(cut.size) + " but area " +
                                         std::string{what} + " is " + std::to_string(extent) + "!"));
}

void add_cut_array(const ViaDefinition& via, const std::string& layer, const CutArray& cut,
                   const DefViaContext& context, std::vector<ViaRect>& out) {
  const std::int32_t x0 = half_output(via.area.x0, context.scale);
  const std::int32_t y0 = half_output(via.area.y0, context.scale);
  const std::int32_t x1 = half_output(via.area.x1, context.scale);
  const std::int32_t y1 = half_output(via.area.y1, context.scale);
  const Axis ax = axis(x0, x1, cut);
  const Axis ay = axis(y0, y1, cut);
  if (ax.single && ax.start < x0) {
    warn_size("LEF-VIA-009", "width", cut, x1 - x0, context);
  }
  const bool y_fires = context.compat.magic_compat ? ay.start >= y0 : ay.start < y0;  // Q-LEFDEF-107
  if (ay.single && y_fires) {
    warn_size("LEF-VIA-010", "height", cut, y1 - y0, context);
  }
  for (std::int32_t i = 0; i < ay.count; ++i) {
    const std::int32_t b = ay.start + i * cut.pitch();
    for (std::int32_t j = 0; j < ax.count; ++j) {
      const std::int32_t l = ax.start + j * cut.pitch();
      out.push_back(ViaRect{layer, static_cast<float>(l), static_cast<float>(b), static_cast<float>(l + cut.size),
                            static_cast<float>(b + cut.size)});
    }
  }
}

/// The cut operator of `type` in output units; nullopt when the style has
/// none (CIFGetContactSize answers 0) or its pitch is not positive (Magic
/// divides by it).
std::optional<CutArray> cut_array(tech::TileType type, const mask::OutputStyle* style, float scale) {
  if (style == nullptr) {
    return std::nullopt;
  }
  const auto cut = style->contact_cut(type);
  if (!cut || cut->size() == 0) {
    return std::nullopt;
  }
  // CIFGetOutputScale(1): CIF units per internal unit.
  const float cscale = static_cast<float>(10 * style->scale.scale_factor) / static_cast<float>(style->scale.expander);
  CutArray out{to_output(cut->edge, scale, cscale), to_output(cut->spacing, scale, cscale),
               to_output(cut->border, scale, cscale)};
  if (out.pitch() <= 0) {
    return std::nullopt;
  }
  return out;
}

}  // namespace

std::vector<ViaRect> via_rects(const ViaDefinition& via, std::string_view via_name, const mask::OutputStyle* style,
                               const DefViaContext& context) {
  std::vector<ViaRect> out;
  if (!via.type) {
    return out;
  }
  for (const tech::TileType r : residues(*via.type, context.tech)) {
    if (auto name = name_or_warn(r, via_name, context)) {
      out.push_back(float_rect(*name, residue_area(via, r), context.scale));
    }
  }
  const auto cut_name = name_or_warn(*via.type, via_name, context);
  if (!cut_name) {
    return out;
  }
  if (const auto cut = cut_array(*via.type, style, context.scale)) {
    add_cut_array(via, *cut_name, *cut, context, out);
  } else {
    out.push_back(float_rect(*cut_name, via.area, context.scale));
  }
  return out;
}

}  // namespace hefesto::lef
