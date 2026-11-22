// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of extresist/network (network.hpp): CORE-NOTIMPL-008 until
// RESNET lands them (openspec/changes/m7-extraction tasks.md, RESNET).
// RESNET replaces this file with its own sources.
//
// Owner: RESNET (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extresist/network/network.hpp"

namespace hefesto::extresist {

diag::Outcome<NetGeometry> yank_net(const db::CellDef& /*cell*/, db::Point /*at*/, tech::TileType /*type*/,
                                    const tech::TypeMask& /*skip*/) {
  return cmd::m7_not_implemented_diagnostic("extresist yank");
}

diag::Status fracture(db::StitchEditor& /*editor*/, const db::Rect& /*area*/) {
  return cmd::m7_not_implemented_status("extresist fracture");
}

diag::Outcome<Network> build_network(const NetGeometry& /*geometry*/, const extract::ExtStyle& /*style*/,
                                     const DeviceAnnotations& /*devices*/) {
  return cmd::m7_not_implemented_diagnostic("extresist network");
}

diag::Status reduce(Network& /*network*/, const Tolerances& /*tolerances*/, HeapState& /*heap*/,
                    const compat::CompatSettings& /*compat*/) {
  return cmd::m7_not_implemented_status("extresist reduce");
}

}  // namespace hefesto::extresist
