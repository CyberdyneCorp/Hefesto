// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Format recognition on open (capability oasis-command "Format recognition
// on open"; openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D15): the generic
// open path (the desktop picker, drag and drop, the iPadOS Files request and
// the scripting open) reads the first bytes of a file, after gzip
// decompression, and dispatches on them whatever the extension: OASIS by
// its 13-byte magic `%SEMI-OASIS\r\n`, GDS by its HEADER record, a Magic
// cell by its `magic` line.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASCMD fills
// in the implementation in src/core/src/oasis/command/ and wires the `app/`
// open paths (design D2, D15).

#pragma once

#include <cstdint>
#include <string_view>

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::oasis {

/// The kinds of layout file the generic open path tells apart.
enum class FileFormat : std::uint8_t { unknown, oasis, gds, magic };

/// The OASIS magic bytes (oasis-format "File structure").
inline constexpr std::string_view kOasisMagic = "%SEMI-OASIS\r\n";

/// The format of the stream `source` (already gzip-inflated by
/// gds::open_source), read from its current position; the position is
/// left unspecified.
[[nodiscard]] FileFormat sniff_format(gds::ByteSource& source);

}  // namespace hefesto::oasis
