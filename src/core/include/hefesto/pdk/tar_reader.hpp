// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Safe tar unpacking of PDK assets (hefesto-pdk-manager "Safe
// archive unpacking"). Reads ustar, GNU long names and pax extended headers
// (ciel's archives are pax), treats the archive as untrusted input.
// Public so that the fuzz target of the M1 exit criteria can reach it.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <functional>
#include <istream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::pdk {

struct UnpackReport {
  std::uint64_t bytes = 0;            // regular-file bytes written
  std::size_t files = 0;              // regular files written
  std::vector<std::string> skipped;   // links, devices and FIFOs not created
};

/// The destination of an entry name below `root`, or PDK-UNPACK-001 when the
/// name is absolute, contains a `..` component or leaves `root`. An empty
/// result path means "nothing to write" (e.g. the name `./`).
[[nodiscard]] diag::Outcome<platform::Path> safe_entry_path(const platform::Path& root, std::string_view name);

/// Selects the regular files to write by entry name (leading `./` removed);
/// an empty filter writes every file.
using EntryFilter = std::function<bool(std::string_view name)>;

/// Unpacks every regular file of the tar stream `in` below `root` (which
/// must exist) that `filter` accepts. Directory entries are skipped (directories are created as
/// needed); links, devices and FIFOs are skipped and reported. Fails with
/// PDK-UNPACK-001 (unsafe name), PDK-UNPACK-002 (more than `max_bytes`) or
/// PDK-UNPACK-003 (malformed archive); the caller removes `root` on failure.
[[nodiscard]] diag::Outcome<UnpackReport> unpack_tar(std::istream& in, const platform::Path& root,
                                                     std::uint64_t max_bytes, const EntryFilter& filter = {});

}  // namespace hefesto::pdk
