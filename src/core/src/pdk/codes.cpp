// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "pdk" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix PDK-), then regenerate
// docs/diagnostics/codes/pdk.md with `hefesto-cli --diagnostic-codes=pdk`.

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kPdkCodes = {
    CodeInfo{"PDK-ENABLE-001", error,
             "A PDK variant path under the PDK root exists and is not a symbolic link; enabling changed nothing."},
    CodeInfo{"PDK-FETCH-001", error, "A catalog document or PDK asset could not be downloaded."},
    CodeInfo{"PDK-FETCH-002", error, "The catalog or release source has no such document (HTTP 404)."},
    CodeInfo{"PDK-INFO-001", info, "Progress of a PDK manager operation (download, verification, unpacking, install)."},
    CodeInfo{"PDK-INSTALL-001", error, "The PDK install could not create, move or remove files; nothing partial was kept."},
    CodeInfo{"PDK-INSTALL-002", error, "The requested PDK version is not in the catalog."},
    CodeInfo{"PDK-INSTALL-003", error, "The version manifest has no asset for a requested part (common or a library)."},
    CodeInfo{"PDK-INSTALL-004", error, "The PDK version (or the pinned version) is not installed."},
    CodeInfo{"PDK-LIBRARY-001", error, "The library is not available for the PDK variant; nothing was downloaded."},
    CodeInfo{"PDK-LICENSE-001", error,
             "The upstream license of the PDK family has not been accepted; a headless install needs -accept-license."},
    CodeInfo{"PDK-MANIFEST-001", error, "A catalog manifest or release document is malformed."},
    CodeInfo{"PDK-PACK-001", error,
             "An offline PDK pack or its pack.json is malformed, or names an asset the pack does not contain; nothing was "
             "installed."},
    CodeInfo{"PDK-PIN-001", warning,
             "A project is re-pinned to another PDK version; recorded DRC, extraction and sign-off results become stale."},
    CodeInfo{"PDK-PROGRESS-001", info,
             "Machine-readable install progress (`pdk install -progress`, `pdk import -progress`): phase, part, "
             "bytes, total, asset."},
    CodeInfo{"PDK-SELECT-001", error, "The selector is not a valid PDK family or variant."},
    CodeInfo{"PDK-UNINSTALL-001", error,
             "The PDK version is pinned by a project in the recent-projects list; uninstalling it needs -force."},
    CodeInfo{"PDK-UNPACK-001", error, "An archive entry is absolute, contains '..' or leaves the version directory; the install was aborted."},
    CodeInfo{"PDK-UNPACK-002", error, "The unpacked size of an asset exceeds the configured limit; the install was aborted."},
    CodeInfo{"PDK-UNPACK-003", error, "An asset is not a valid (Zstandard-compressed) tar archive; the install was aborted."},
    CodeInfo{"PDK-UNPACK-004", warning, "An archive entry is a link, device or FIFO and was not created."},
    CodeInfo{"PDK-VERIFY-001", error, "A downloaded asset does not match the catalog size or SHA-256; it was deleted."},
    CodeInfo{"PDK-VERIFY-002", error,
             "The PDK source is unverified (no Hefesto catalog signature) or an asset has no known SHA-256; installing needs "
             "explicit acceptance (-accept-unverified)."},
};

}  // namespace

std::span<const CodeInfo> pdk() noexcept {
  return kPdkCodes;
}

}  // namespace hefesto::diag::area_codes
