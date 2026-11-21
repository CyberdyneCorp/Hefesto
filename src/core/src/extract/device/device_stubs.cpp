// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of extract/device (devices.hpp): CORE-NOTIMPL-008 or empty
// values until EXTDEV lands them (openspec/changes/m7-extraction tasks.md
// 7.x). EXTDEV replaces this file with its own sources.
//
// Owner: EXTDEV (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extract/device/devices.hpp"

namespace hefesto::extract {

diag::Outcome<std::vector<DeviceRecord>> find_devices(const db::CellDef& /*cell*/, const ExtStyle& /*style*/,
                                                      const NodeRegionList& /*nodes*/,
                                                      const compat::CompatSettings& /*compat*/) {
  return cmd::m7_not_implemented_diagnostic("extract devices");
}

std::optional<std::size_t> select_pattern(const ExtStyle& /*style*/, const DeviceRegion& /*region*/,
                                          const Dimensions& /*dims*/, const compat::CompatSettings& /*compat*/) {
  return std::nullopt;
}

Dimensions gate_dimensions(const DeviceRecord& /*device*/, const ExtStyle& /*style*/) {
  return {};
}

Dimensions resistor_dimensions(const DeviceRecord& /*device*/, const ExtStyle& /*style*/) {
  return {};
}

Dimensions capacitor_dimensions(const DeviceRecord& /*device*/, const ExtStyle& /*style*/) {
  return {};
}

diag::Outcome<SubstrateNodes> prepare_substrate(const db::CellDef& /*cell*/, const ExtStyle& /*style*/,
                                               const app::GlobalNets& /*nets*/) {
  return cmd::m7_not_implemented_diagnostic("extract substrate");
}

}  // namespace hefesto::extract
