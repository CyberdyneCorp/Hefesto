// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared LEF/DEF via table (capabilities lef-read "LEF VIA and VIARULE
// definitions", "Via redefinition", def-read "Rectangle-based DEF vias",
// "Via redefinition"; openspec/changes/m6-lef-def-oasis/design.md D4, D6,
// D8): starting a VIA, VIARULE or DEF VIAS definition in
// lef::LefSession::vias, redefinition (LefRedefined, Q-LEFDEF-8 fixed) and
// adding a via rectangle (LefAddViaGeometry).
//
// Magic keeps vias and layers in one table (LefInfo). Hefesto keeps the via
// records in ViaTable and the tech `lef` layers in LayerTable; a via name
// that is a LayerTable name is a redefinition of that layer's entry, and the
// new via record shadows the layer name for via lookups.
//
// Owner: LEFVIA (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "hefesto/db/geometry.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/via/context.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::lef {

/// Which statement starts the definition.
enum class ViaSection : std::uint8_t {
  lef_via,      // LEF `VIA <name>`
  lef_viarule,  // LEF `VIARULE <name>`
  def_via,      // DEF VIAS `- <name>`
};

/// Starts the definition of via `name` and returns the record to read its
/// body into (valid until the next insertion into `vias`), or nullptr when
/// the section is skipped: a VIARULE whose name is already defined
/// (lefRead.c:3058-3080, defRead.c:1972-1995).
/// - A new name gets a VIA-class record with no types, contact area
///   (0,0,0,0) and canonical name `name`.
/// - A defined name (a via, or a tech `lef` layer name) is reported, LEF
///   `Cut type "<name>" redefined.` (LEF-VIA-001) or DEF `Composite via
///   "<name>" redefined.` (LEF-VIA-002), and redefined: the record is
///   emptied (extra rectangles dropped, types none, area (0,0,0,0)) and
///   keeps its class. When the name is one of several aliases of a layer
///   record, only `name` gets a new record, of class VIA (Q-LEFDEF-8 fix;
///   Magic leaves its class uninitialised); the layer's other names keep
///   the layer record, and `name` leaves `layers`
///   (LayerTable::redefine_alias: the record's canonical name moves to one
///   of its other names when it was `name`).
[[nodiscard]] ViaDefinition* begin_via(ViaTable& vias, LayerTable& layers, std::string_view name,
                                       ViaSection section, const ViaContext& context);

/// The number of LEF names of `layers` that share `name`'s record (0 when
/// `name` is not a layer name): Magic's record count of LefRedefined.
[[nodiscard]] std::size_t shared_names(const LayerTable& layers, std::string_view name);

/// Adds one via rectangle, already read in half internal units and made
/// canonical, on `layer` (nullopt: the layer did not resolve, and nothing
/// is added) to `via` (LefAddViaGeometry, lefRead.c:2646-2690): cut growing
/// first ("Cut growing"); then the first rectangle becomes the contact area
/// and normal type, and every later one is prepended to the extra list,
/// swapped with the contact area when it is a contact and the normal type
/// is not.
void add_via_rect(ViaDefinition& via, std::optional<tech::TileType> layer, const db::Rect& rect,
                  const ViaContext& context);

/// add_via_rect for a whole via body at constant cost per rectangle (a
/// hostile file can give 10^5 or more RECTs; prepending each one to the
/// vector would cost the square of that). `add` appends the extras in read
/// order; `commit` reverses them once and moves them to the head of the
/// extra list, which gives exactly add_via_rect's list. Readers call
/// `commit` before anything else reads or prepends to `via.shapes`; it may
/// be called again, ordering only what was added since.
class ViaRectBuilder {
 public:
  explicit ViaRectBuilder(ViaDefinition& via) noexcept : via_(via), first_(via.shapes.size()) {}

  void add(std::optional<tech::TileType> layer, const db::Rect& rect, const ViaContext& context);
  void commit();

 private:
  ViaDefinition& via_;
  std::size_t first_;  // the first extra appended since the last commit
};

/// LefTechScale on the via records (lef/lefTech.c:415-461): every
/// definition with a reference count of 1 or more (not a DEF write's
/// temporary via) has its contact area scaled by n/d, flooring like
/// DBScalePoint. With magic-compat ON the extra rectangles are left
/// unscaled, as in Magic; with it OFF they are scaled too. A factor that
/// is not strictly positive changes nothing.
void scale_via_areas(ViaTable& vias, std::int32_t n, std::int32_t d, bool magic_compat);

}  // namespace hefesto::lef
