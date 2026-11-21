// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the per-area diagnostic code tables merged by diag/codes.cpp.
// Each function is defined in src/core/src/<area>/codes.cpp and owned by the
// area's workstream (openspec/changes/m1-core-viewer/design.md, D2/D3;
// m2-interactive-editing/design.md D1/D2 for the M2 areas). The
// returned span must refer to storage with static duration (a namespace-scope
// constexpr std::array), sorted by code, and every code must start with the
// area prefix declared in diag/codes.cpp (TECH-, DB-, CELL-, DISPLAY-, VIEW-,
// PDK-, APP-, UNDO-, SELECT-, CONNECT-, UI-, WIRE-, DRC-, HINTS-, SCRIPT-,
// PCELL-, TOOLS-, GUI-, MASK-, GDS-, CIF-, LEF-, DEF-, OASIS-, EXTFLAT-,
// EXTRACT-, SCHEM-, VIEW3D-, NETLIST-, EXTRESIST-, ANTENNA-). The drc area's table is assembled by
// src/core/src/drc/codes.cpp from one fragment per workstream
// (drc/<sub>/codes.inc; openspec/changes/m3-drc design.md D1); the script and
// pcell tables likewise from script/<sub>/codes.inc and pcell/<sub>/codes.inc
// (openspec/changes/m4-scripting/design.md D1), and the mask, gds and cif
// tables from <area>/<sub>/codes.inc (openspec/changes/m5-mask-gds-cif
// design.md D1), the lef, def, oasis and extract tables likewise and the
// extflat table from extflat/codes.inc (openspec/changes/m6-lef-def-oasis
// design.md D1), the schem table from schem/<sub>/codes.inc
// (openspec/changes/m8-schematic design.md D3), and the view3d table from
// view3d/<sub>/codes.inc (openspec/changes/m10-3d-view/design.md D2), the
// netlist and extresist tables from <area>/<sub>/codes.inc and the antenna
// table from antenna/codes.inc (openspec/changes/m7-extraction/design.md
// D2), sorted
// at compile time by diag/sorted_codes.hpp.

#pragma once

#include <span>

#include "hefesto/diag/codes.hpp"

namespace hefesto::diag::area_codes {

[[nodiscard]] std::span<const CodeInfo> tech() noexcept;
[[nodiscard]] std::span<const CodeInfo> db() noexcept;
[[nodiscard]] std::span<const CodeInfo> cell() noexcept;
[[nodiscard]] std::span<const CodeInfo> display() noexcept;
[[nodiscard]] std::span<const CodeInfo> view() noexcept;
[[nodiscard]] std::span<const CodeInfo> pdk() noexcept;
[[nodiscard]] std::span<const CodeInfo> app() noexcept;
// M2 areas (openspec/changes/m2-interactive-editing/design.md D1).
[[nodiscard]] std::span<const CodeInfo> undo() noexcept;
[[nodiscard]] std::span<const CodeInfo> select() noexcept;
[[nodiscard]] std::span<const CodeInfo> connect() noexcept;
[[nodiscard]] std::span<const CodeInfo> ui() noexcept;
[[nodiscard]] std::span<const CodeInfo> wiring() noexcept;
// M3 areas (openspec/changes/m3-drc/design.md D1).
[[nodiscard]] std::span<const CodeInfo> drc() noexcept;
[[nodiscard]] std::span<const CodeInfo> hints() noexcept;
// M4 areas (openspec/changes/m4-scripting/design.md D1).
[[nodiscard]] std::span<const CodeInfo> script() noexcept;
[[nodiscard]] std::span<const CodeInfo> pcell() noexcept;
[[nodiscard]] std::span<const CodeInfo> tools() noexcept;
[[nodiscard]] std::span<const CodeInfo> gui() noexcept;
// M5 areas (openspec/changes/m5-mask-gds-cif/design.md D1).
[[nodiscard]] std::span<const CodeInfo> mask() noexcept;
[[nodiscard]] std::span<const CodeInfo> gds() noexcept;
[[nodiscard]] std::span<const CodeInfo> cif() noexcept;
// M6 areas (openspec/changes/m6-lef-def-oasis/design.md D1).
[[nodiscard]] std::span<const CodeInfo> lef() noexcept;
[[nodiscard]] std::span<const CodeInfo> def() noexcept;
[[nodiscard]] std::span<const CodeInfo> oasis() noexcept;
[[nodiscard]] std::span<const CodeInfo> extflat() noexcept;
[[nodiscard]] std::span<const CodeInfo> extract() noexcept;
// M8 area (openspec/changes/m8-schematic/design.md D3).
[[nodiscard]] std::span<const CodeInfo> schem() noexcept;
// M10 area (openspec/changes/m10-3d-view/design.md D2).
[[nodiscard]] std::span<const CodeInfo> view3d() noexcept;
// M7 areas (openspec/changes/m7-extraction/design.md D2).
[[nodiscard]] std::span<const CodeInfo> netlist() noexcept;
[[nodiscard]] std::span<const CodeInfo> extresist() noexcept;
[[nodiscard]] std::span<const CodeInfo> antenna() noexcept;

}  // namespace hefesto::diag::area_codes
