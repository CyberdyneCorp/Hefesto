// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cut growing and the DEF-write contact regions, scaffold stubs
// (openspec/changes/m6-lef-def-oasis/design.md D6, D9).
//
// Owner: LEFVIA (design D2).

#include "hefesto/lef/via/cut.hpp"
#include "hefesto/lef/via/regions.hpp"

namespace hefesto::lef {

db::Rect grow_cut(const db::Rect& cut, const LayerRecord& /*record*/, const mask::OutputStyle& /*style*/) {
  return cut;
}

std::vector<ContactRegion> contact_regions(const db::CellDef& /*cell*/, const LayerTable& /*table*/,
                                           const compat::CompatSettings& /*compat*/) {
  return {};
}

std::string via_name(const ContactRegion& /*region*/, const LayerTable& /*table*/,
                     const compat::CompatSettings& /*compat*/) {
  return {};
}

}  // namespace hefesto::lef
