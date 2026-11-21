// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Device recognition and substrate nodes (capabilities extract-devices,
// extract-substrate; openspec/changes/m7-extraction/design.md D10).
// find_devices() discovers the device regions and their terminals,
// select_pattern() picks the `device` line by identifier layers,
// substrate variant and parameter ranges (Q-EXTRACT-21), and the
// dimension functions compute L, W and the class-specific values with the
// pinned `sqrt` uses of design D22.
//
// Scaffold state (tasks.md 3.3): every function reports CORE-NOTIMPL-008
// or answers an empty value until EXTDEV lands it (tasks.md 7.x).
//
// Owner: INTEGRATION (this header); EXTDEV (the implementation).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/app/project_file.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::extract {

/// One terminal of a recognised device.
struct DeviceTerminal {
  std::size_t node = 0;  // index into the cell's NodeRegionList
  std::int64_t length = 0;
  std::vector<std::string> attributes;
};

/// One recognised device, ready for ExtWriter::device.
struct DeviceRecord {
  std::size_t pattern = 0;  // index into ExtStyle::devices
  db::Rect area;
  std::optional<std::size_t> substrate;  // node index; nullopt = the default substrate node
  std::vector<DeviceTerminal> terminals;
  std::int64_t length = 0;
  std::int64_t width = 0;
  std::vector<std::string> params;  // `name=value`, in written order
};

/// Gate (L, W) and resistor or capacitor dimensions.
struct Dimensions {
  std::int64_t length = 0;
  std::int64_t width = 0;
};

[[nodiscard]] diag::Outcome<std::vector<DeviceRecord>> find_devices(const db::CellDef& cell, const ExtStyle& style,
                                                                    const NodeRegionList& nodes,
                                                                    const compat::CompatSettings& compat);
/// The index into ExtStyle::devices of the line that matches `region`, or
/// nullopt when none does.
[[nodiscard]] std::optional<std::size_t> select_pattern(const ExtStyle& style, const DeviceRegion& region,
                                                        const Dimensions& dims,
                                                        const compat::CompatSettings& compat);
[[nodiscard]] Dimensions gate_dimensions(const DeviceRecord& device, const ExtStyle& style);
[[nodiscard]] Dimensions resistor_dimensions(const DeviceRecord& device, const ExtStyle& style);
[[nodiscard]] Dimensions capacitor_dimensions(const DeviceRecord& device, const ExtStyle& style);

/// The substrate regions of a cell and the global substrate node's name
/// (`$SUB` from the project setting, Q-EXTRACT-9).
struct SubstrateNodes {
  std::string global_name;
  std::vector<std::size_t> isolated;  // node indices of isolated substrate regions
};

[[nodiscard]] diag::Outcome<SubstrateNodes> prepare_substrate(const db::CellDef& cell, const ExtStyle& style,
                                                             const app::GlobalNets& nets);

}  // namespace hefesto::extract
