// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `irouter` style derived from the technology's `lef` section
// (capability router-maze-iroute "Derived irouter style"; design D13;
// Q-LEGACY-37 (b), J2). Hefesto-native: with magic-compat ON nothing is
// derived and `iroute` answers with Magic's missing-style error.
//
// The `lef` section lines are walked in file order (every occurrence of
// the section, as the LEF layer table reads them); the class of a line is
// that of the record its first LEF name has in the session's table
// (lef-tech-section "Keyword to LEF class mapping": `routing`, `route` and
// a `layer` of a non-contact type are route records; `cut`, `contact` and
// a `layer` of a contact type are cut records). `obstruction` and `ignore`
// lines never contribute.
//   - Route layers: the route records' magic types in order, the first
//     lowest. A type already taken, a contact type and a type on a plane
//     that already has a route layer are skipped (the `layer` statement's
//     one-layer-per-plane rule). Costs alternate scmos's `irouter` values:
//     hCost 32 / vCost 64 at even positions, 64 / 32 at odd ones; jogCost
//     256, hintCost 1, overCost 1.
//   - Route contacts: the cut records' contact types whose two residues are
//     both derived route layers, in order, cost 1024; layer1 is the lower of
//     the two layers.
//   - Everything else: the style defaults. Widths and spacings come from
//     DRC in derive_style, as for a written style.
//
// Owner: MZTECH.

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

#include "hefesto/cmd/keywords.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/route/tech/styles.hpp"
#include "hefesto/tech/technology.hpp"
#include "route/tech/detail.hpp"

namespace hefesto::route {

namespace {

constexpr int kEvenH = 32;
constexpr int kEvenV = 64;
constexpr int kJogCost = 256;
constexpr int kContactCost = 1024;

// LefTechLine's keywords (lef/tech/section.cpp), for the lines that never
// contribute.
constexpr std::array<std::string_view, 10> kLefKeywords = {"obstruction", "layer",   "route",   "routing", "masterslice",
                                                           "cut",         "contact", "overlap", "bound",   "ignore"};
constexpr std::size_t kObstruction = 0;
constexpr std::size_t kIgnore = 9;

// The route or cut record a `lef` line defines, or nullptr.
const lef::LayerRecord* line_record(const tech::RawLine& line, const lef::LayerTable& table) {
  const auto& t = line.tokens;
  if (t.size() < 3) {
    return nullptr;
  }
  const cmd::LookupResult keyword = cmd::lookup(t[0], kLefKeywords);
  if (keyword.match != cmd::Match::found || keyword.index == kObstruction || keyword.index == kIgnore) {
    return nullptr;
  }
  const lef::LayerRecord* record = table.find(t[2]);
  if (record == nullptr || record->type == tech::kSpace) {
    return nullptr;
  }
  const bool usable = record->layer_class == lef::LayerClass::route || record->layer_class == lef::LayerClass::cut;
  return usable ? record : nullptr;
}

class StyleBuilder {
 public:
  explicit StyleBuilder(const tech::Technology& tech) : tech_(tech) {
    style_.name = "irouter";
    style_.derived = true;
  }

  void add(const lef::LayerRecord& record) {
    if (record.layer_class == lef::LayerClass::route) {
      add_layer(record.type);
    } else {
      cuts_.push_back(record.type);
    }
  }

  // The contacts come after every layer is known (a cut line may precede
  // the routing lines it connects).
  MazeStyle finish() {
    for (const tech::TileType cut : cuts_) {
      add_contact(cut);
    }
    return std::move(style_);
  }

 private:
  [[nodiscard]] std::optional<std::size_t> layer_of(tech::TileType type) const {
    const auto& layers = style_.layers;
    const auto it =
        std::find_if(layers.begin(), layers.end(), [type](const RouteLayer& l) { return l.route.type == type; });
    return it == layers.end() ? std::nullopt : std::optional{static_cast<std::size_t>(it - layers.begin())};
  }

  [[nodiscard]] bool plane_taken(tech::TileType type) const {
    const auto plane = tech_.home_plane(type);
    return std::any_of(style_.layers.begin(), style_.layers.end(),
                       [&](const RouteLayer& l) { return tech_.home_plane(l.route.type) == plane; });
  }

  void add_layer(tech::TileType type) {
    if (tech_.is_contact(type) || !tech_.home_plane(type) || layer_of(type) || plane_taken(type)) {
      return;
    }
    const bool even = style_.layers.size() % 2 == 0;
    RouteLayer layer;
    layer.route.type = type;
    layer.h_cost = even ? kEvenH : kEvenV;
    layer.v_cost = even ? kEvenV : kEvenH;
    layer.jog_cost = kJogCost;
    style_.layers.push_back(layer);
    style_.types.push_back(RouteTypeRef{RouteTypeRef::Kind::layer, style_.layers.size() - 1});
  }

  void add_contact(tech::TileType type) {
    const tech::ContactInfo* info = tech_.contact(type);
    const bool known = std::any_of(style_.contacts.begin(), style_.contacts.end(),
                                   [type](const RouteContact& c) { return c.route.type == type; });
    if (info == nullptr || known || info->residues.count() != 2) {
      return;
    }
    std::array<std::size_t, 2> layers{};
    std::size_t found = 0;
    for (std::size_t t = 0; t < tech_.type_count() && found < 2; ++t) {
      if (!info->residues.test(static_cast<tech::TileType>(t))) {
        continue;
      }
      const std::optional<std::size_t> layer = layer_of(static_cast<tech::TileType>(t));
      if (!layer) {
        return;
      }
      layers[found++] = *layer;
    }
    RouteContact contact;
    contact.route.type = type;
    contact.layer1 = std::min(layers[0], layers[1]);
    contact.layer2 = std::max(layers[0], layers[1]);
    contact.cost = kContactCost;
    style_.contacts.push_back(contact);
    style_.types.push_back(RouteTypeRef{RouteTypeRef::Kind::contact, style_.contacts.size() - 1});
  }

  const tech::Technology& tech_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  MazeStyle style_;
  std::vector<tech::TileType> cuts_;
};

}  // namespace

bool maybe_derive_irouter(MazeStyles& styles, const tech::Technology& tech, const lef::LayerTable& lef_layers,
                          bool magic_compat) {
  if (magic_compat || find_style(styles, "irouter") != nullptr) {
    return false;  // Magic never derives; a written `irouter` style wins
  }
  StyleBuilder builder(tech);
  for (const tech::RawSection& section : tech.raw_sections()) {
    if (section.name != "lef") {
      continue;
    }
    for (const tech::RawLine& line : section.lines) {
      if (const lef::LayerRecord* record = line_record(line, lef_layers)) {
        builder.add(*record);
      }
    }
  }
  MazeStyle derived = builder.finish();
  if (derived.layers.empty()) {
    return false;  // no route layer: the missing-style error stays
  }
  styles.styles.push_back(std::move(derived));
  return true;
}

}  // namespace hefesto::route
