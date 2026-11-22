// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF reader's view of the LEF layer table and LEF layer name
// resolution (capability lef-read "LEF layer name resolution", "LEF LAYER
// section"; design D6):
// - LayerEntry and LayerAccess: the fields of a table entry the reader
//   uses (lefLayer) and the changes a LEF LAYER section makes to the table.
//   The reader works through LayerAccess so that it does not depend on how
//   lef::LayerTable stores its records (LEFTECH); the adapter over
//   lef::LayerTable is added when the table offers mutation;
// - resolve_layer: LefReadLayers, with Q-LEFDEF-3 per magic-compat mode and
//   Q-LEFDEF-4 fixed (a name without an entry is treated as absent).
//
// Owner: LEFR (design D2).

#pragma once

#include <optional>
#include <string_view>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/lef/lex/statements.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::lef {

/// The fields of one LEF layer table entry that the reader uses. Types
/// are 0 when there is none (Magic's -1).
struct LayerEntry {
  LayerClass layer_class = LayerClass::ignore;
  tech::TileType type = 0;             // the normal type (lefLayer::type)
  tech::TileType obstruction = 0;      // lefLayer::obsType
  tech::TileType via_obstruction = 0;  // VIA class: info.via.obsType, the secondary obstruction type
  std::optional<db::Rect> contact_area;  // VIA class: info.via.area when not null

  friend bool operator==(const LayerEntry&, const LayerEntry&) = default;
};

/// The route values a LEF LAYER section sets; nullopt leaves a value as it
/// is.
struct RouteUpdate {
  std::optional<db::Coord> width;
  std::optional<db::Coord> spacing;
  std::optional<db::Coord> pitch;
  std::optional<RouteDirection> direction;

  [[nodiscard]] bool empty() const noexcept { return !width && !spacing && !pitch && !direction; }
  friend bool operator==(const RouteUpdate&, const RouteUpdate&) = default;
};

/// What the LEF reader needs of the session's LEF layer table.
class LayerAccess {
 public:
  LayerAccess() = default;
  LayerAccess(const LayerAccess&) = delete;
  LayerAccess& operator=(const LayerAccess&) = delete;
  virtual ~LayerAccess() = default;

  /// The entry named `name` (an alias or a canonical name); nullopt when
  /// the name is absent or has no entry (Q-LEFDEF-4).
  [[nodiscard]] virtual std::optional<LayerEntry> find(std::string_view name) const = 0;

  /// Adds the ROUTE entry `name` (canonical name `name`) for magic type
  /// `type`, from a LEF LAYER section whose name is not in the table. Its
  /// width, spacing, pitch and direction start as a tech `routing` line's
  /// (Q-LEFDEF-5: DRC default width and spacing, pitch 0, horizontal).
  virtual void add_route(std::string_view name, tech::TileType type) = 0;

  /// Applies `update` to the ROUTE entry `name` (shared by its aliases).
  virtual void update_route(std::string_view name, const RouteUpdate& update) = 0;

  /// HashFind's side effect on a name looked up without an entry: the name
  /// takes its place in Magic's hash order with no entry, so an entry added
  /// later under it keeps that place (NONDEFAULTRULE LAYER and USEVIA).
  virtual void reserve(std::string_view name) = 0;

  /// True when `name` is a key of Magic's LefInfo table with no record (a
  /// released generated via, a noted via or rule name): find gives nullopt
  /// although Magic's HashLookOnly finds the key. False by default.
  [[nodiscard]] virtual bool keyed_without_entry(std::string_view /*name*/) const { return false; }
};

/// LefReadLayers' `obstruct` argument.
enum class LayerLookup : std::uint8_t {
  normal,       // pin, route and via geometry
  obstruction,  // OBS geometry
};

/// Which of Magic's lookups resolves the name: LefReadLayers with result
/// pointers (MACRO PORT and OBS geometry, where a key with no record
/// crashes Magic: Q-LEFDEF-4, treated as absent), or LefReadLayer without
/// them (LEF VIA and LAYER section geometry, DEF geometry), where such a
/// key gives no type and the unknown-layer error (Q-LEFDEF-151).
enum class LayerSource : std::uint8_t {
  geometry,
  read_layer,
};

/// The layer state of one geometry block: the current type (curlayer), the
/// secondary obstruction type (otherlayer) and the contact area
/// (*rreturn). nullopt is Magic's -1 or null.
struct LayerState {
  std::optional<tech::TileType> type;
  std::optional<tech::TileType> secondary;
  std::optional<db::Rect> contact_area;

  friend bool operator==(const LayerState&, const LayerState&) = default;
};

/// What resolve_layer needs besides the name.
struct LayerContext {
  const LayerAccess& layers;
  const tech::Technology& tech;
  compat::CompatSettings compat;
  const MessageTarget& messages;
};

/// LefReadLayers for the layer name `token` of a `LAYER <name>` statement:
/// sets `state.type` (nullopt when the name does not resolve) and, as
/// lef-read "LEF layer name resolution" says, the secondary obstruction
/// type and the contact area. With magic-compat OFF both are reset first
/// (Q-LEFDEF-3). A `;` reports "Bad Layer statement"; a name that does not
/// resolve and is not an IGNORE entry reports the two "Don't know how to
/// parse layer" errors (LEF-READ-001). A name with no entry falls back to
/// the magic type names, except that with magic-compat ON a LefReadLayer
/// lookup (`source` read_layer) of a key with no record resolves to nothing,
/// as in Magic (Q-LEFDEF-151).
void resolve_layer(std::string_view token, LayerLookup lookup, LayerState& state, const LayerContext& context,
                   LayerSource source = LayerSource::geometry);

/// DBTechNameType of `name`, then of its lowercase form (the first 255
/// characters, as LefHelper_DBTechNameType_LefLower).
[[nodiscard]] std::optional<tech::TileType> magic_type(const tech::Technology& tech, std::string_view name);

}  // namespace hefesto::lef
