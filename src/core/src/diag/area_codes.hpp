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
// PDK-, APP-, UNDO-, SELECT-, CONNECT-, UI-, WIRE-, DRC-, HINTS-). The drc
// area's table is assembled by src/core/src/drc/codes.cpp from one fragment
// per workstream (drc/<sub>/codes.inc; openspec/changes/m3-drc design.md D1).

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

}  // namespace hefesto::diag::area_codes
