// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cifParseLayers ("Layer list syntax"): a comma list of tile types and
// earlier mask layers, with Magic's resolution order and messages.
//
// Ported from Magic VLSI 8.3.694 cif/CIFtech.c (cifParseLayers) and
// database/DBtechname.c (DBTechNameTypes), Copyright (C) 1985, 1990 Regents
// of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>

#include "mask/style/reader.hpp"
#include "mask/style/text.hpp"

namespace hefesto::mask::style_detail {

namespace {

constexpr int kAmbiguous = -1;
constexpr int kUnknown = -2;
constexpr int kGroup = -3;  // a parenthesised type-list group

/// DBTechNameTypes: {type, mask}; type -1 ambiguous, -2 unknown.
struct NameTypes {
  int type = kUnknown;
  tech::TypeMask mask;
};

std::optional<tech::TypeMask> alias_mask(const tech::Technology& technology, std::string_view name) {
  for (const auto& [alias, mask] : technology.aliases()) {
    if (alias == name) {
      return mask;
    }
  }
  return std::nullopt;
}

int first_user_type(const tech::Technology& technology, const tech::TypeMask& mask) {
  for (std::size_t t = tech::kFirstUserType; t < technology.user_type_end(); ++t) {
    if (mask.test(static_cast<tech::TileType>(t))) {
      return static_cast<int>(t);
    }
  }
  return kUnknown;
}

NameTypes base_name_types(const tech::Technology& technology, std::string_view name) {
  NameTypes out;
  const auto found = technology.find_type(name);
  if (found.ok()) {
    out.type = static_cast<int>(*found.id);
    out.mask = tech::TypeMask::of(*found.id);
    return out;
  }
  out.type = found.error == tech::LookupError::ambiguous ? kAmbiguous : kUnknown;
  if (const auto alias = alias_mask(technology, name)) {
    out.mask = *alias;
    out.type = first_user_type(technology, out.mask);
  }
  return out;
}

NameTypes name_types(const tech::Technology& technology, std::string_view name) {
  const std::size_t slash = name.find('/');
  NameTypes out = base_name_types(technology, name.substr(0, slash));
  if (slash == std::string_view::npos) {
    return out;
  }
  const auto plane = technology.find_plane(name.substr(slash + 1));
  if (!plane.ok()) {
    return NameTypes{kUnknown, out.mask};
  }
  out.mask &= technology.plane_types(*plane.id);
  if (out.type < 0 || !out.mask.test(static_cast<tech::TileType>(out.type))) {
    out.type = first_user_type(technology, out.mask);
  }
  return out;
}

/// The mask layers other than `exclude` whose name is exactly `name`.
std::vector<std::size_t> matching_layers(const LayerContext& ctx, std::string_view name) {
  std::vector<std::size_t> out;
  if (ctx.layers == nullptr) {
    return out;
  }
  for (std::size_t i = 0; i < ctx.layers->size(); ++i) {
    if ((!ctx.exclude || *ctx.exclude != i) && (*ctx.layers)[i].name == name) {
      out.push_back(i);
    }
  }
  return out;
}

/// One item of the list: the name, and whether it carried `*`.
struct Item {
  std::string name;
  bool all_residues = false;
};

/// The item loop of cifParseLayers: a `(` copies up to `)`, then `*` sets
/// the residue flag, then the rest up to the next comma; commas are skipped.
/// Appends list[i..] up to (not including) `stop` to `out`; returns the new position.
std::size_t copy_until(std::string_view list, std::size_t i, char stop, std::string& out) {
  const std::size_t end = std::min(list.find(stop, i), list.size());
  out.append(list.substr(i, end - i));
  return end;
}

std::vector<Item> split_items(std::string_view list) {
  std::vector<Item> items;
  std::size_t i = 0;
  while (i < list.size()) {
    Item item;
    if (list[i] == '(') {
      i = copy_until(list, i, ')', item.name);
    }
    if (i < list.size() && list[i] == '*') {
      item.all_residues = true;
      ++i;
    }
    i = copy_until(list, i, ',', item.name);
    i = std::min(list.find_first_not_of(',', i), list.size());
    items.push_back(std::move(item));
  }
  return items;
}

void report(const LayerContext& ctx, std::string text) {
  if (ctx.messages != nullptr && ctx.technology != nullptr) {
    ctx.messages->error(std::move(text));
  }
}

void add_residues(const tech::Technology& technology, int type, tech::TypeMask& mask) {
  for (std::size_t r = tech::kFirstUserType; r < technology.user_type_end(); ++r) {
    if (technology.full_residues(static_cast<tech::TileType>(r)).test(static_cast<tech::TileType>(type))) {
      mask.set(static_cast<tech::TileType>(r));
    }
  }
}

void merge_layers(std::vector<std::size_t>& into, const std::vector<std::size_t>& add) {
  for (const std::size_t layer : add) {
    if (std::find(into.begin(), into.end(), layer) == into.end()) {
      into.push_back(layer);
    }
  }
  std::sort(into.begin(), into.end());
}

/// The paint part of one item: the type (or group) and its mask.
NameTypes item_types(const LayerContext& ctx, Item& item) {
  if (ctx.technology == nullptr) {
    return NameTypes{};
  }
  if (!item.name.empty() && item.name.front() == '(') {
    item.all_residues = false;
    return NameTypes{kGroup, noisy_types(ctx, item.name)};
  }
  return name_types(*ctx.technology, item.name);
}

void add_paint(const LayerContext& ctx, const Item& item, const NameTypes& paint, bool space_ok, LayerSets& out) {
  if (paint.type == static_cast<int>(tech::kSpace) && !space_ok) {
    report(ctx, "\"Space\" layer not permitted in CIF rules.");
    return;
  }
  out.types |= paint.mask;
  if (item.all_residues && paint.type >= 0) {
    add_residues(*ctx.technology, paint.type, out.types);
  }
}

void add_item(const LayerContext& ctx, Item item, bool space_ok, LayerSets& out) {
  const NameTypes paint = item_types(ctx, item);
  const std::vector<std::size_t> cif = matching_layers(ctx, item.name);
  if (paint.type == kAmbiguous || (paint.type >= 0 && !cif.empty())) {
    report(ctx, "Ambiguous layer (type) \"" + item.name + "\".");
    return;
  }
  if (paint.type >= 0 || paint.type == kGroup) {
    add_paint(ctx, item, paint, space_ok, out);
  } else if (!cif.empty()) {
    merge_layers(out.layers, cif);
  } else if (const auto alias = ctx.technology != nullptr ? alias_mask(*ctx.technology, item.name) : std::nullopt) {
    out.types |= *alias;
  } else {
    report(ctx, "Unrecognized layer (type) \"" + item.name + "\".");
  }
}

}  // namespace

tech::TypeMask noisy_types(const LayerContext& ctx, std::string_view text) {
  if (ctx.technology == nullptr) {
    return {};
  }
  auto parsed = ctx.technology->parse_type_list(text);
  if (!parsed.ok()) {
    report(ctx, parsed.error().text);
    return {};
  }
  return parsed.value();
}

LayerSets parse_layers(const LayerContext& ctx, std::string_view list, bool space_ok) {
  LayerSets out;
  for (Item& item : split_items(list)) {
    add_item(ctx, std::move(item), space_ok, out);
  }
  return out;
}

}  // namespace hefesto::mask::style_detail
