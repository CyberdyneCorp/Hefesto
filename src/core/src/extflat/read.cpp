// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// read_flat: EFReadFile, then EFFlatBuild(EF_FLATNODES | EF_NOFLATSUBCKT)
// and EFVisitNodes, as `def write` calls them (defWrite.c:251-293;
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4, D10).
//
// Owner: EXTSUB (design D2).

#include "hefesto/extflat/read.hpp"

#include <utility>

#include "flatten.hpp"
#include "reader.hpp"
#include "state.hpp"

namespace hefesto::extflat {

diag::Outcome<Netlist> read_flat(cell::LookupContext& ctx, std::string_view root, const ReadOptions& options) {
  detail::ReadState st{ctx.sink, options};
  const auto root_def = detail::read_tree(st, ctx, root);
  if (!root_def.ok()) {
    return root_def.error();
  }
  std::vector<FlatNode> nodes = detail::flatten_nodes(st, root_def.value());
  if (st.fatal) {
    return *st.fatal;
  }
  Netlist netlist{std::move(nodes), std::move(st.stamps)};
  netlist.selected_style = std::move(st.selected_style);
  return netlist;
}

}  // namespace hefesto::extflat
