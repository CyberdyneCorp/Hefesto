// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Content-hash timestamps of read-only OASIS cells (capability oasis-read
// "Content-hash timestamps for read-only OASIS cells";
// openspec/changes/m6-lef-def-oasis/design.md D12): the FNV-1a 64 hash of
// each cell's decompressed bytes (CBLOCKs inflated), then gds-read's
// memoized stamp(s) procedure, so a read-only OASIS cell gets a timestamp
// that changes only when its content does. The table has the gds::StampTable
// layout (start: the CELL record's offset in the decompressed stream); a
// cell's stamp is looked up by name (oasis::lookup_stamp, vendor.hpp).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASR fills
// in the implementation in src/core/src/oasis/read/ (design D2).

#pragma once

#include "hefesto/gds/read/stamps.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::oasis {

/// The content stamps of every CELL record of the OASIS stream `source`,
/// read from its start (empty when it is not OASIS). A cell the stream
/// ends inside has no entry.
[[nodiscard]] gds::StampTable content_stamps(gds::ByteSource& source);

}  // namespace hefesto::oasis
