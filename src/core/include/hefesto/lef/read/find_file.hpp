// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF file lookup (capability lef-read; openspec/changes/archive/2026-11-22-m6-lef-def-oasis
// design.md D6 "Files and dates"): `lef read <name>` tries the name as
// given, then `<name>.lef`, along the cell search path through
// cell::search_file (Q-LEFDEF-24, platform: the search path on the desktop,
// the document picker on iPadOS).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFR fills
// in the implementation in src/core/src/lef/read/ (design D2).
//
// The tries, in order (lefFileOpen): a name whose last component has an
// extension other than `.lef` as given, then with that extension replaced
// by `.lef`; a name ending in `.lef` as given; a name with no extension with
// `.lef` appended. Each searches the current directory, the cell search
// path and the library path; a `.gz` file of the same name is found too.

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cell/search_path.hpp"

namespace hefesto::lef {

/// The LEF file `lef read <name>` opens; nullopt when no candidate opens.
[[nodiscard]] std::optional<cell::FoundFile> find_lef_file(cell::LookupContext& ctx, std::string_view name);

/// The last name find_lef_file tries: `name` ending in `.lef` (the name
/// "Cannot open input file" reports).
[[nodiscard]] std::string lef_candidate(std::string_view name);

}  // namespace hefesto::lef
