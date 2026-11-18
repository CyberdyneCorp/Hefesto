// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The via geometry generator and polygon painting, scaffold stubs
// (openspec/changes/m6-lef-def-oasis/design.md D4).
//
// Owner: LEFVIA (design D2).

#include "hefesto/lef/via/generate.hpp"
#include "hefesto/lef/via/polygon.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::lef {

ViaGeometry ViaGenerator::generated(const GeneratedViaParams& /*params*/, const LayerTable& /*table*/,
                                    const mask::OutputStyle& /*style*/, const compat::CompatSettings& /*compat*/) {
  return {};
}

diag::Status paint_polygon(db::CellDef& /*cell*/, tech::TileType /*type*/, std::span<const db::Point> /*vertices*/,
                           std::string_view /*port_name*/) {
  return cmd::not_implemented_status("lef paint_polygon");
}

}  // namespace hefesto::lef
