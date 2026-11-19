// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The state of one MACRO or SITE section shared by its statement handlers
// (private to lef/read; capability lef-read "MACRO and SITE cell
// creation", "PORT and OBS geometry", "Port labels in creation mode",
// "Annotation of existing cells"; design D6): macro.cpp reads the section,
// pin.cpp its PIN sections and port_geometry.cpp the PORT and OBS bodies.
//
// Owner: LEFR (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/tech/types.hpp"
#include "lef/read/reader.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::lef::detail {

/// The cell a MACRO or SITE section reads into.
struct MacroState {
  db::CellDef& cell;
  bool annotating = false;  // the cell existed (Magic's is_imported)
  db::Point offset;         // gdsOffset: added to every PIN and OBS coordinate
};

/// One rectangle of a PORT's geometry list (LinkedRect), internal units.
struct PortShape {
  tech::TileType type = 0;
  db::Rect rect;
};

/// LefReadGeometry for a PORT (`port`) or an OBS body: paints the cell and
/// returns the PORT's geometry list in Magic's order (each RECT prepended,
/// each POLYGON's rectangles appended); an OBS returns nothing.
std::vector<PortShape> read_geometry(Reader& reader, MacroState& macro, bool port);

/// A pin's class, use and shape (lef-read "PIN attributes").
struct PinAttributes {
  db::PortClass port_class = db::PortClass::default_class;
  db::PortUse use = db::PortUse::default_use;
  db::PortShape shape = db::PortShape::default_shape;
};

/// LefReadPin: the PIN section `name` (`number`: the macro's pin ordinal).
void read_pin(Reader& reader, MacroState& macro, const std::string& name, std::int64_t number);

}  // namespace hefesto::lef::detail
