// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of extract/node (regions.hpp, names.hpp, unique.hpp):
// neutral values or CORE-NOTIMPL-008 until EXTNODE lands them
// (openspec/changes/m7-extraction tasks.md 5.1-5.4). EXTNODE replaces this
// file with its own sources.
//
// Owner: EXTNODE (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/node/unique.hpp"

namespace hefesto::extract {

diag::Outcome<NodeRegionList> RegionFinder::find(const db::CellDef& /*cell*/, const db::Rect& /*area*/,
                                                 const tech::TypeMask& /*mask*/, const ConnectTable& /*connect*/,
                                                 RegionCallbacks& /*callbacks*/) {
  return cmd::m7_not_implemented_diagnostic("extract regions");
}

void reset(const db::CellDef& /*cell*/) {}

std::string node_name(const NodeRegion& /*node*/, const tech::Technology& /*technology*/,
                      const compat::CompatSettings& /*compat*/) {
  return {};
}

std::string generated_name(const NodeRegion& /*node*/, const tech::Technology& /*technology*/) {
  return {};
}

diag::Status UniqueRenamer::apply(db::CellDef& /*cell*/, UniqueMode /*mode*/) {
  return cmd::m7_not_implemented_status("extract unique");
}

void UniqueRenamer::revert() {
  renames_.clear();
}

}  // namespace hefesto::extract
