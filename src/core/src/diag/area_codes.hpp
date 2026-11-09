// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the per-area diagnostic code tables merged by diag/codes.cpp.
// Each function is defined in src/core/src/<area>/codes.cpp and owned by the
// area's workstream (openspec/changes/m1-core-viewer/design.md, D2/D3). The
// returned span must refer to storage with static duration (a namespace-scope
// constexpr std::array), sorted by code, and every code must start with the
// area prefix declared in diag/codes.cpp (TECH-, DB-, CELL-, DISPLAY-, VIEW-,
// PDK-, APP-).

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

}  // namespace hefesto::diag::area_codes
