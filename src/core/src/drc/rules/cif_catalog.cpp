// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::CifCatalog: the cifoutput styles as DRC needs them (see
// cif_catalog.hpp). Each style is read with the cifoutput style machine
// (CIFTechLine), keeping layer names, the paint masks of `or` operators
// and every operator's numeric operands; CIFTechFinal adds stacked
// contacts to the paint masks, runs CIFTechOutputScale(1, 1) and computes
// the reducer; CIFGetContactSize finds the squares/slots border of a
// contact.
//
// Ported from Magic VLSI 8.3.694 cif/CIFtech.c (CIFTechLine,
// cifParseLayers, CIFParseScale, CIFTechFinal, CIFGetContactSize,
// CIFTechOutputScale) and drc/DRCtech.c (drcGetContactBorder), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Deviations (docs/decisions/cross-cutting.md X-2 tier 1): the reducer
// skips the slots and interaction operators, which CIFTechFinal reads
// through the wrong client type (undefined behavior); `slots` with six
// arguments is a wrong-argument-count line instead of a NULL read.
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/rules/cif_catalog.hpp"

#include <algorithm>
#include <array>
#include <string>

#include "drc/rules/style_machine.hpp"
#include "drc/rules/text_util.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::drc {

namespace {

using rules_detail::StyleMachine;
using rules_detail::Tokens;
using Kind = CifOpValues::Kind;

constexpr std::size_t kMaxCifLayers = 255;  // MAXCIFLAYERS

// --- cifParseLayers (the parts the catalog needs) ---------------------------

struct LayerMasks {
  tech::TypeMask paint;
  bool cif = false;  // some name was a CIF layer
};

/// DBTechNameTypes for one name: the paint mask, or nothing (unknown or ambiguous).
std::optional<tech::TypeMask> paint_name(const tech::Technology& tech, std::string_view name, bool all_residues) {
  if (!name.empty() && name.front() == '(') {
    auto mask = tech.parse_type_list(name);
    return mask.ok() ? std::optional{mask.value()} : std::nullopt;
  }
  const auto type = tech.find_type(name);
  if (!type.ok()) {
    return std::nullopt;
  }
  tech::TypeMask mask = tech::TypeMask::of(*type.id);
  if (all_residues) {
    for (std::size_t r = tech::kFirstUserType; r < tech.user_type_end(); ++r) {
      if (tech.full_residues(static_cast<tech::TileType>(r)).test(*type.id)) {
        mask.set(static_cast<tech::TileType>(r));
      }
    }
  }
  return mask;
}

std::optional<tech::TypeMask> alias_mask(const tech::Technology& tech, std::string_view name) {
  for (const auto& [alias, mask] : tech.aliases()) {
    if (alias == name) {
      return mask;
    }
  }
  return std::nullopt;
}

/// Whether `name` is a CIF layer other than the one being defined.
bool other_cif_layer(const std::vector<std::string>& layers, std::size_t current_layer, std::string_view name) {
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (i != current_layer && layers[i] == name) {
      return true;
    }
  }
  return false;
}

LayerMasks parse_layers(const tech::Technology& tech, std::string_view list, const std::vector<std::string>& layers,
                        std::size_t current_layer) {
  LayerMasks out;
  for (const std::string& piece : rules_detail::split_comma_list(list)) {
    std::string_view name = piece;
    const bool all_residues = !name.empty() && name.front() == '*';
    if (all_residues) {
      name.remove_prefix(1);
    }
    if (name.empty()) {
      continue;
    }
    const auto paint = paint_name(tech, name, all_residues);
    const bool cif = other_cif_layer(layers, current_layer, name);
    if (paint && !cif) {
      out.paint |= *paint;
    } else if (!paint && cif) {
      out.cif = true;
    } else if (!paint) {
      out.paint |= alias_mask(tech, name).value_or(tech::TypeMask{});
    }
  }
  return out;
}

// --- operator parsing ----------------------------------------------------------

enum class Opcode : std::uint8_t {
  and_op, and_not, or_op, grow, grow_min, grow_grid, shrink, bloat_or, bloat_max, bloat_min, bloat_all, squares,
  squares_grid, slots, bbox, net, tagged, maxrect, boundary, mask_hints, close, orthogonal, not_square, bridge,
  bridge_lim, interact,
};

struct OpName {
  std::string_view name;
  Opcode code;
};

constexpr std::array kOpNames = {
    OpName{"and", Opcode::and_op},          OpName{"and-not", Opcode::and_not},
    OpName{"or", Opcode::or_op},            OpName{"grow", Opcode::grow},
    OpName{"grow-min", Opcode::grow_min},   OpName{"grow-grid", Opcode::grow_grid},
    OpName{"shrink", Opcode::shrink},       OpName{"bloat-or", Opcode::bloat_or},
    OpName{"bloat-max", Opcode::bloat_max}, OpName{"bloat-min", Opcode::bloat_min},
    OpName{"bloat-all", Opcode::bloat_all}, OpName{"squares", Opcode::squares},
    OpName{"squares-grid", Opcode::squares_grid}, OpName{"slots", Opcode::slots},
    OpName{"bbox", Opcode::bbox},           OpName{"net", Opcode::net},
    OpName{"tagged", Opcode::tagged},       OpName{"maxrect", Opcode::maxrect},
    OpName{"boundary", Opcode::boundary},   OpName{"mask-hints", Opcode::mask_hints},
    OpName{"close", Opcode::close},         OpName{"orthogonal", Opcode::orthogonal},
    OpName{"not-square", Opcode::not_square}, OpName{"bridge", Opcode::bridge},
    OpName{"bridge-lim", Opcode::bridge_lim}, OpName{"overlapping", Opcode::interact},
    OpName{"nonoverlapping", Opcode::interact}, OpName{"interacting", Opcode::interact},
    OpName{"noninteracting", Opcode::interact},
};

std::optional<Opcode> opcode_of(std::string_view word) {
  for (const OpName& op : kOpNames) {
    if (op.name == word) {
      return op.code;
    }
  }
  return std::nullopt;
}

/// Reads one cifoutput style (one pass over the section).
class StyleReader {
 public:
  StyleReader(const tech::Technology& tech, StyleMachine machine) : tech_(tech), machine_(std::move(machine)) {}

  void line(Tokens t);
  [[nodiscard]] CifStyleInfo finish();

 private:
  bool settings_line(Tokens t);
  void scalefactor_line(Tokens t);
  void layer_line(Tokens t);
  void op_line(Tokens t);
  [[nodiscard]] std::optional<CifOpValues> parse_op(Opcode code, Tokens t) const;
  [[nodiscard]] std::optional<CifOpValues> parse_squares(Opcode code, Tokens t) const;
  [[nodiscard]] static std::optional<CifOpValues> parse_slots(Tokens t);
  [[nodiscard]] std::optional<CifOpValues> parse_bloat(Tokens t) const;
  [[nodiscard]] std::optional<CifOpValues> parse_bloat_all(Tokens t) const;
  [[nodiscard]] LayerMasks layers_of(std::string_view list) const {
    return parse_layers(tech_, list, info_.layers, current_layer_.value_or(info_.layers.size()));
  }

  const tech::Technology& tech_;
  StyleMachine machine_;
  CifStyleInfo info_;
  std::int32_t grid_limit_ = 0;
  std::optional<std::size_t> current_layer_;
};

/// CIFParseScale: {scale factor, expander} of a `scalefactor` value; a
/// decimal value is the reduced fraction n / 10^places.
std::pair<std::int32_t, std::int32_t> parse_scale(const std::string& scale) {
  const std::size_t dot = scale.find('.');
  if (dot == std::string::npos) {
    return {text::c_atoi(scale), 1};
  }
  const std::string decimals = scale.substr(dot + 1);
  std::int64_t d = 1;
  for (std::size_t i = 0; i < decimals.size() && d < 1000000000; ++i) {
    d *= 10;
  }
  std::int64_t n = text::c_atoi(scale.substr(0, dot)) * d + text::c_atoi(decimals);
  const std::int64_t g = text::find_gcf(n, d);
  if (g != 0) {
    n /= g;
    d /= g;
  }
  return {text::clamp_int(n), text::clamp_int(d)};
}

void StyleReader::scalefactor_line(Tokens t) {
  if (t.size() < 2 || t.size() > 4) {
    return;
  }
  const auto [scale, expander] = parse_scale(t[1]);
  info_.scale_factor = scale;
  info_.expander = expander;
  // "nanometers" and "angstroms" multiply the expander.
  if (t.size() >= 3 && text::starts_with(t.back(), "nanom")) {
    info_.expander *= 10;
  } else if (t.size() >= 3 && text::starts_with(t.back(), "angstr")) {
    info_.expander *= 100;
  }
  info_.reducer = 1;
  info_.scale_factor = std::max(info_.scale_factor, 0);  // "Scalefactor must be a strictly positive value."
}

bool StyleReader::settings_line(Tokens t) {
  const std::string& word = t[0];
  if (word == "scalefactor") {
    scalefactor_line(t);
    return true;
  }
  if (word == "units" || word == "stepsize") {
    return true;
  }
  if (!text::starts_with(word, "grid")) {
    return false;
  }
  const auto limit = text::parse_leading_int(t.size() > 1 ? std::string_view{t[1]} : std::string_view{});
  if (t.size() > 1 && limit.digits && limit.end == t[1].size()) {
    grid_limit_ = std::max(text::clamp_int(limit.value), 0);
  }
  return true;
}

void StyleReader::layer_line(Tokens t) {
  if (info_.layers.size() == kMaxCifLayers || (t.size() != 2 && t.size() != 3)) {
    current_layer_.reset();
    return;
  }
  info_.layers.push_back(t[1]);
  info_.ops.emplace_back();
  current_layer_ = info_.layers.size() - 1;
  if (t.size() == 3) {
    const LayerMasks masks = layers_of(t[2]);
    CifOpValues op;
    op.keeps_cuts = true;
    if (!masks.cif) {
      op.or_paint = masks.paint;
    }
    info_.ops.back().push_back(std::move(op));
  }
}

void StyleReader::line(Tokens t) {
  if (t.empty()) {
    return;
  }
  if (t[0] == "style") {
    (void)machine_.style_line(t);
    return;
  }
  if (!machine_.active() || settings_line(t)) {
    return;
  }
  if (text::starts_with(t[0], "variant")) {
    (void)machine_.variant_line(t);
    return;
  }
  if (!machine_.loading()) {
    return;
  }
  const std::string& word = t[0];
  if (word == "templayer" || word == "layer" || word == "labellayer") {
    layer_line(t);
  } else if (word != "labels" && word != "calma" && !text::starts_with(word, "gds") && word != "min-width" &&
             word != "render" && word != "options") {
    op_line(t);
  }
}

void StyleReader::op_line(Tokens t) {
  const auto code = opcode_of(t[0]);
  if (!current_layer_ || !code) {
    return;  // "Must define layer before specifying operation." / unknown statement
  }
  if (auto op = parse_op(*code, t)) {
    info_.ops[*current_layer_].push_back(std::move(*op));
  }
}

std::optional<CifOpValues> positive_distance(Tokens t, std::size_t index) {
  CifOpValues op;
  op.distance = text::c_atoi(t[index]);
  return op.distance > 0 ? std::optional{op} : std::nullopt;
}

std::optional<CifOpValues> plain_if(bool ok) {
  return ok ? std::optional{CifOpValues{}} : std::nullopt;
}

std::optional<CifOpValues> parse_bridge(Opcode code, Tokens t) {
  if (t.size() != (code == Opcode::bridge ? 3U : 4U)) {
    return std::nullopt;
  }
  auto op = positive_distance(t, 1);
  const std::int32_t width = text::c_atoi(t[2]);
  if (!op || width <= 0) {
    return std::nullopt;
  }
  op->kind = Kind::bridge;
  op->values = {width};
  return op;
}

/// The operators without client data and their argument counts.
std::optional<CifOpValues> parse_simple(Opcode code, Tokens t) {
  const std::size_t argc = t.size();
  switch (code) {
    case Opcode::and_op:
    case Opcode::interact:
    case Opcode::mask_hints: return plain_if(argc == 2);
    case Opcode::grow:
    case Opcode::grow_min:
    case Opcode::grow_grid:
    case Opcode::shrink: return argc == 2 ? positive_distance(t, 1) : std::nullopt;
    case Opcode::close: return argc == 1 ? plain_if(true) : (argc == 2 ? positive_distance(t, 1) : std::nullopt);
    case Opcode::net:
    case Opcode::tagged: return plain_if(argc == 2 || argc == 3);
    case Opcode::boundary: return plain_if(argc == 1);
    case Opcode::maxrect:
    case Opcode::orthogonal:
    case Opcode::bbox: return plain_if(argc <= 2);
    default: return plain_if(true);  // not-square: no argument check
  }
}

std::optional<CifOpValues> StyleReader::parse_op(Opcode code, Tokens t) const {
  switch (code) {
    case Opcode::or_op:
    case Opcode::and_not: {
      if (t.size() != 2) {
        return std::nullopt;
      }
      CifOpValues op;
      op.keeps_cuts = true;
      if (code == Opcode::or_op && !layers_of(t[1]).cif) {
        op.or_paint = layers_of(t[1]).paint;
      }
      return op;
    }
    case Opcode::bridge:
    case Opcode::bridge_lim: return parse_bridge(code, t);
    case Opcode::bloat_all: return parse_bloat_all(t);
    case Opcode::bloat_or:
    case Opcode::bloat_min:
    case Opcode::bloat_max: return parse_bloat(t);
    case Opcode::squares:
    case Opcode::squares_grid: return parse_squares(code, t);
    case Opcode::slots: return parse_slots(t);
    default: return parse_simple(code, t);
  }
}

/// The squares operands {border, size, sep, grid x, grid y} as written.
std::optional<std::array<std::int32_t, 5>> squares_operands(Opcode code, Tokens t) {
  const std::size_t argc = t.size();
  if (argc == 2) {
    const std::int32_t size = text::c_atoi(t[1]);
    if (size <= 0 || (size & 1) != 0) {
      return std::nullopt;  // "Squares must have positive even sizes."
    }
    return std::array<std::int32_t, 5>{size / 2, size, size, 1, 1};
  }
  if (argc != 4 && !((argc == 5 || argc == 6) && code == Opcode::squares_grid)) {
    return std::nullopt;
  }
  std::array<std::int32_t, 5> v{text::c_atoi(t[1]), text::c_atoi(t[2]), text::c_atoi(t[3]), 1, 1};
  if (argc >= 5) {
    v[3] = v[4] = text::c_atoi(t[4]);
  }
  if (argc == 6) {
    v[4] = text::c_atoi(t[5]);
  }
  // border >= 0, size > 0, separation > 0, grid > 0
  if (v[0] < 0 || v[1] <= 0 || v[2] <= 0 || v[3] <= 0 || v[4] <= 0) {
    return std::nullopt;
  }
  return v;
}

std::optional<CifOpValues> StyleReader::parse_squares(Opcode code, Tokens t) const {
  auto v = squares_operands(code, t);
  if (!v) {
    return std::nullopt;
  }
  // Squares are never placed below the minimum mask resolution: a coarser
  // grid limit turns `squares` into `squares-grid`.
  bool grid = code == Opcode::squares_grid || (*v)[3] < grid_limit_ || (*v)[4] < grid_limit_;
  (*v)[3] = std::max((*v)[3], grid_limit_);
  (*v)[4] = std::max((*v)[4], grid_limit_);
  CifOpValues op;
  op.kind = grid ? Kind::squares_grid : Kind::squares;
  op.values.assign(v->begin(), v->begin() + (grid ? 5 : 3));
  return op;
}

std::optional<CifOpValues> StyleReader::parse_slots(Tokens t) {
  const std::size_t argc = t.size();
  if (argc < 4 || argc == 6 || argc > 9) {
    return std::nullopt;
  }
  std::array<std::int32_t, 8> v{};
  for (std::size_t i = 1; i < argc; ++i) {
    v[i - 1] = text::c_atoi(t[i]);
  }
  // sborder >= 0, ssize > 0, ssep > 0, lborder >= 0, lsize >= 0, lsep >= 0
  // (> 0 with a long side), offset >= 0, start >= 0.
  const bool bad = v[0] < 0 || v[1] <= 0 || v[2] <= 0 || v[3] < 0 || v[4] < 0 || v[5] < 0 ||
                   (argc >= 7 && v[5] == 0 && v[4] > 0) || v[6] < 0 || v[7] < 0;
  if (bad) {
    return std::nullopt;
  }
  CifOpValues op;
  op.kind = Kind::slots;
  op.values.assign(v.begin(), v.end());
  return op;
}

std::vector<std::int32_t> distinct_nonzero(const std::array<std::int32_t, tech::kMaxTileTypes>& distances) {
  std::vector<std::int32_t> values;
  for (const std::int32_t d : distances) {
    if (d != 0 && std::find(values.begin(), values.end(), d) == values.end()) {
      values.push_back(d);
    }
  }
  return values;
}

std::optional<CifOpValues> StyleReader::parse_bloat(Tokens t) const {
  if (t.size() < 4 || t.size() % 2 != 0) {
    return std::nullopt;
  }
  const tech::TypeMask own = layers_of(t[1]).paint;
  std::array<std::int32_t, tech::kMaxTileTypes> distances{};
  const bool bloat_or = t[0] == "bloat-or";
  const auto assign = [&distances](const tech::TypeMask& mask, std::int32_t distance) {
    for (std::size_t i = 0; i < tech::kMaxTileTypes; ++i) {
      if (mask.test(static_cast<tech::TileType>(i))) {
        distances[i] = distance;
      }
    }
  };
  for (std::size_t k = 2; k + 1 < t.size(); k += 2) {
    const tech::TypeMask mask = t[k] == "*" ? tech::TypeMask::all() : layers_of(t[k]).paint;
    const std::int32_t distance = text::c_atoi(t[k + 1]);
    // bloat-or: "Bloat-or distances must not be negative."
    assign(mask, distance < 0 && bloat_or ? 0 : distance);
  }
  assign(own, 0);  // no bloating between tiles of the bloated types
  CifOpValues op;
  op.kind = Kind::bloat;
  op.values = distinct_nonzero(distances);
  return op;
}

std::optional<CifOpValues> StyleReader::parse_bloat_all(Tokens t) const {
  if (t.size() != 3 && t.size() != 4) {
    return std::nullopt;
  }
  CifOpValues op;
  op.kind = Kind::bloat_all;
  if (t.size() == 4) {
    op.distance = text::c_atoi(t[3]);
    if (op.distance <= 0) {
      return std::nullopt;  // "Bloat distance must be greater than zero."
    }
  }
  const tech::TypeMask own = layers_of(t[1]).paint;
  const LayerMasks bloat = layers_of(t[2]);
  // Paint layers: distance 1 for each, cleared for the op's own types;
  // CIF layers: distance 1 for each layer (never cleared).
  const bool any = !bloat.paint.empty() ? !(bloat.paint & ~own).empty() : bloat.cif;
  if (any) {
    op.values = {1};
  }
  return op;
}

/// CIFTechFinal: stacked contacts join the paint masks of their components.
void join_stacked_contacts(const tech::Technology& tech, tech::TypeMask& paint) {
  for (const tech::ContactInfo& contact : tech.contacts()) {
    if (contact.stacked && (paint.test(contact.component_a) || paint.test(contact.component_b))) {
      paint.set(contact.type);
    }
  }
}

void add_stacked_contacts(const tech::Technology& tech, CifStyleInfo& style) {
  for (auto& layer_ops : style.ops) {
    for (CifOpValues& op : layer_ops) {
      if (op.or_paint) {
        join_stacked_contacts(tech, *op.or_paint);
      }
    }
  }
}

/// The values of one operator that CIFTechFinal's reducer loop considers.
std::vector<std::int64_t> reducer_operands(const CifOpValues& op) {
  std::vector<std::int64_t> out;
  if (op.distance > 0) {
    out.push_back(op.distance);
  }
  if (op.kind == Kind::slots) {
    return out;  // slots: undefined behavior in Magic (header note)
  }
  for (std::size_t j = 0; j < op.values.size(); ++j) {
    const bool halved = op.kind == Kind::squares && (j == 1 || j == 2);
    out.push_back(halved ? op.values[j] >> 1 : op.values[j]);
  }
  return out;
}

/// The cs_reducer loop of CIFTechFinal.
std::int32_t reducer_of(const CifStyleInfo& style) {
  std::int64_t min_reduce = style.scale_factor;
  for (const auto& layer_ops : style.ops) {
    for (const CifOpValues& op : layer_ops) {
      for (const std::int64_t value : reducer_operands(op)) {
        if (value != 0) {
          min_reduce = std::min(min_reduce, text::find_gcf(style.scale_factor, value));
        }
      }
    }
  }
  return static_cast<std::int32_t>(min_reduce);
}

/// The squares or slots operator after operator `i` of a layer, through
/// `or` and `and-not` only (the operators that keep the relation between
/// layers and cuts); nullptr when another operator comes first.
const CifOpValues* cut_operator(const std::vector<CifOpValues>& layer_ops, std::size_t i) {
  for (std::size_t k = i + 1; k < layer_ops.size(); ++k) {
    const CifOpValues& sop = layer_ops[k];
    if (sop.kind == Kind::squares || sop.kind == Kind::squares_grid || sop.kind == Kind::slots) {
      return &sop;
    }
    if (!sop.keeps_cuts) {
      return nullptr;
    }
  }
  return nullptr;
}

std::optional<ContactBorder> find_contact_border(const CifStyleInfo& style, tech::TileType type) {
  for (const auto& layer_ops : style.ops) {
    // The leading run of `or` operators without CIF layers.
    for (std::size_t i = 0; i < layer_ops.size() && layer_ops[i].or_paint; ++i) {
      const CifOpValues* cut = layer_ops[i].or_paint->test(type) ? cut_operator(layer_ops, i) : nullptr;
      if (cut == nullptr) {
        continue;
      }
      const std::int32_t border = cut->values[0];
      return border > 0 ? std::optional{ContactBorder{border, style.scale_factor}} : std::nullopt;
    }
  }
  return std::nullopt;
}

void compute_contact_borders(const tech::Technology& tech, CifStyleInfo& style) {
  style.contact_borders.assign(tech.type_count(), std::nullopt);
  for (std::size_t t = 0; t < tech.type_count(); ++t) {
    if (tech.is_contact(static_cast<tech::TileType>(t))) {
      style.contact_borders[t] = find_contact_border(style, static_cast<tech::TileType>(t));
    }
  }
}

}  // namespace

// --- CIFTechOutputScale ----------------------------------------------------------

namespace {

bool odd_squares(const CifStyleInfo& style) {
  for (const auto& layer_ops : style.ops) {
    for (const CifOpValues& op : layer_ops) {
      if (op.kind == Kind::squares && ((op.values[1] & 1) != 0 || (op.values[2] & 1) != 0)) {
        return true;
      }
      if (op.kind == Kind::slots &&
          ((op.values[4] & 1) != 0 || (op.values[5] & 1) != 0 || (op.values[1] & 1) != 0 || (op.values[2] & 1) != 0)) {
        return true;
      }
    }
  }
  return false;
}

bool halved_value(Kind kind, std::size_t j) {
  return (kind == Kind::squares && (j == 1 || j == 2)) || (kind == Kind::slots && (j == 1 || j == 2 || j == 4 || j == 5));
}

/// Multiplies the non-zero client value j of `op` by d; returns the value
/// CIFTechOutputScale folds into lexpand, or nothing for a zero operand.
std::optional<std::int64_t> scale_value(CifOpValues& op, std::size_t j, std::int64_t d, bool odd_space) {
  if (op.values[j] == 0) {
    return std::nullopt;
  }
  op.values[j] = text::clamp_int(op.values[j] * d);
  std::int64_t value = op.values[j] < 0 ? -std::int64_t{op.values[j]} : op.values[j];
  if (halved_value(op.kind, j)) {
    value >>= 1;
  }
  if (odd_space && (op.kind == Kind::squares || op.kind == Kind::slots)) {
    value >>= 1;
  }
  return value;
}

/// Multiplies every operand of `op` by d; returns the values folded into lexpand.
std::vector<std::int64_t> scale_op(CifOpValues& op, std::int64_t d, bool odd_space) {
  std::vector<std::int64_t> folded;
  if (op.distance != 0) {
    op.distance = text::clamp_int(op.distance * d);
    folded.push_back(op.distance);
  }
  for (std::size_t j = 0; j < op.values.size(); ++j) {
    if (const auto value = scale_value(op, j, d, odd_space)) {
      folded.push_back(*value);
    }
  }
  return folded;
}

/// Multiplies every operand by d and returns the expander reduction lexpand.
std::int64_t scale_operands(CifStyleInfo& style, std::int64_t d, bool odd_space) {
  std::int64_t lexpand = style.expander;
  for (auto& layer_ops : style.ops) {
    for (CifOpValues& op : layer_ops) {
      for (const std::int64_t value : scale_op(op, d, odd_space)) {
        lexpand = text::find_gcf(lexpand, text::find_gcf(value, style.expander));
      }
    }
  }
  return lexpand;
}

void divide_operands(CifStyleInfo& style, std::int64_t lexpand) {
  for (auto& layer_ops : style.ops) {
    for (CifOpValues& op : layer_ops) {
      op.distance = static_cast<std::int32_t>(op.distance / lexpand);
      if (op.kind == Kind::bloat_all) {
        continue;  // bloat-all flags are not divided
      }
      for (std::int32_t& value : op.values) {
        value = static_cast<std::int32_t>(value / lexpand);
      }
    }
  }
}

/// CIFTechOutputScale for one style; returns {d', lexpand} for drcCifScale.
std::pair<std::int32_t, std::int32_t> output_scale(CifStyleInfo& style, std::int64_t n, std::int64_t d) {
  bool odd_space = false;
  if ((style.scale_factor & 1) != 0) {
    n *= 2;
    d *= 2;
    odd_space = true;
  } else if ((d & 1) != 0 && odd_squares(style)) {
    n *= 2;
    d *= 2;
  }
  style.scale_factor = text::clamp_int(style.scale_factor * n);
  style.expander = text::clamp_int(style.expander * d);
  std::int64_t lexpand = scale_operands(style, d, odd_space);
  const std::int64_t lgcf = text::find_gcf(style.scale_factor, style.expander);
  lexpand = std::min(lexpand, lgcf);
  if (lexpand <= 1) {
    return {static_cast<std::int32_t>(d), 1};
  }
  style.scale_factor = static_cast<std::int32_t>(style.scale_factor / lexpand);
  style.expander = static_cast<std::int32_t>(style.expander / lexpand);
  divide_operands(style, lexpand);
  return {static_cast<std::int32_t>(d), static_cast<std::int32_t>(lexpand)};
}

CifStyleInfo StyleReader::finish() {
  info_.name = machine_.current().value_or(std::string{});
  return std::move(info_);
}

CifStyleInfo read_style(const tech::Technology& tech, const tech::RawSection& section, const StyleMachine& machine) {
  StyleReader reader(tech, machine);
  for (const tech::RawLine& line : section.lines) {
    reader.line(line.tokens);
  }
  CifStyleInfo style = reader.finish();
  (void)output_scale(style, 1, 1);
  if (style.scale_factor <= 0) {
    style.scale_factor = 1;  // "No valid scale factor was given for <style> CIF."
  } else {
    add_stacked_contacts(tech, style);
    style.reducer = reducer_of(style);
  }
  compute_contact_borders(tech, style);
  return style;
}

/// The style list of the cifoutput section (all `style` lines).
std::vector<std::string> cif_style_list(const tech::RawSection& section) {
  StyleMachine machine(true);
  for (const tech::RawLine& line : section.lines) {
    if (!line.tokens.empty() && line.tokens[0] == "style") {
      (void)machine.style_line(line.tokens);
    }
  }
  return machine.list();
}

}  // namespace

CifCatalog CifCatalog::from_technology(const tech::Technology& technology, diag::DiagnosticSink& /*sink*/) {
  CifCatalog catalog;
  for (const tech::RawSection& section : technology.raw_sections()) {
    if (section.name != "cifoutput") {
      continue;
    }
    // A repeated section restarts the list (CIFTechInit), as Magic does.
    catalog.styles_.clear();
    const std::vector<std::string> names = cif_style_list(section);
    for (std::size_t i = 0; i < names.size(); ++i) {
      StyleMachine machine(true);
      if (i > 0) {
        machine.begin_reload(names, names[i]);
      }
      CifStyleInfo style = read_style(technology, section, machine);
      style.name = names[i];
      catalog.styles_.push_back(std::move(style));
    }
  }
  if (!catalog.styles_.empty()) {
    catalog.current_ = 0;
  }
  return catalog;
}

const CifStyleInfo* CifCatalog::current() const noexcept {
  return current_ && *current_ < styles_.size() ? &styles_[*current_] : nullptr;
}

diag::Status CifCatalog::select(std::string_view name) {
  const auto it = std::find_if(styles_.begin(), styles_.end(), [&](const CifStyleInfo& s) { return s.name == name; });
  if (it == styles_.end()) {
    return diag::make("DRC-TECH-003", "\"" + std::string{name} + "\" is not one of the CIF output styles.");
  }
  current_ = static_cast<std::size_t>(it - styles_.begin());
  return diag::Done{};
}

std::optional<ContactBorder> CifCatalog::contact_border(tech::TileType type) const noexcept {
  const CifStyleInfo* style = current();
  if (style == nullptr || type >= style->contact_borders.size()) {
    return std::nullopt;
  }
  return style->contact_borders[type];
}

std::int32_t CifCatalog::default_step_size() const noexcept {
  const CifStyleInfo* style = current();
  if (style == nullptr || style->scale_factor <= 0) {
    return 64;
  }
  // Magic's 6400 / cs_scaleFactor is 0 above 6400, and the checker divides
  // by the step size: never below 1 (Q-DRC-25, fix in both modes).
  return std::max(1, 6400 / style->scale_factor);
}

std::pair<std::int32_t, std::int32_t> CifCatalog::rescale(std::int32_t n, std::int32_t d) {
  std::pair<std::int32_t, std::int32_t> factor{1, 1};
  for (std::size_t i = 0; i < styles_.size(); ++i) {
    CifStyleInfo& style = styles_[i];
    const auto applied = output_scale(style, n, d);
    for (std::size_t t = 0; t < style.contact_borders.size(); ++t) {
      if (style.contact_borders[t]) {
        style.contact_borders[t] = find_contact_border(style, static_cast<tech::TileType>(t));
      }
    }
    if (current_ && *current_ == i) {
      factor = applied;
    }
  }
  return factor;
}

}  // namespace hefesto::drc
