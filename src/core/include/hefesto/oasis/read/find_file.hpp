// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS file lookup (capability oasis-read "Read command and file lookup";
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D12): the file `oasis read
// <name>` opens, along the cell search path, as gds::find_gds_file does for
// `gds read` (with the OASIS extensions and a gzip-compressed counterpart).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASR fills
// in the implementation in src/core/src/oasis/read/ (design D2).

#pragma once

#include <optional>
#include <string_view>

#include "hefesto/cell/search_path.hpp"

namespace hefesto::oasis {

/// The OASIS file `oasis read <name>` opens; nullopt when no candidate
/// opens. The candidates are `name` + `.oas`, `.oas.gz`, `.oasis`, `""`.
[[nodiscard]] std::optional<cell::FoundFile> find_oasis_file(cell::LookupContext& ctx, std::string_view name);

}  // namespace hefesto::oasis
