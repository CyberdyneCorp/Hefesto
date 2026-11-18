// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF file lookup (capability lef-read; openspec/changes/m6-lef-def-oasis
// design.md D6 "Files and dates"): `lef read <name>` tries the name as
// given, then `<name>.lef`, along the cell search path through
// cell::search_file (Q-LEFDEF-24, platform: the search path on the desktop,
// the document picker on iPadOS).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFR fills
// in the implementation in src/core/src/lef/read/ (design D2).

#pragma once

#include <optional>
#include <string_view>

#include "hefesto/cell/search_path.hpp"

namespace hefesto::lef {

/// The LEF file `lef read <name>` opens; nullopt when no candidate opens.
/// SCAFFOLD: returns nullopt until LEFR lands it.
[[nodiscard]] std::optional<cell::FoundFile> find_lef_file(cell::LookupContext& ctx, std::string_view name);

}  // namespace hefesto::lef
