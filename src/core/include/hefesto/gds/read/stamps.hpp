// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Content-hash timestamps of read-only vendor cells (capability gds-read
// "When content-hash timestamps are assigned", cell-timestamps;
// openspec/changes/m5-mask-gds-cif/design.md D11): the structure table of a
// GDS byte span and the FNV-1a stamp of each structure, as a pure function
// so it can be unit-tested with the spec's three vectors (143581310,
// 1837575894, 1595775465) and fuzzed. cell/timestamps.cpp's M1 hook calls
// it when a read-only cell is created and when its GDS_FILE is re-resolved.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSR fills
// in the implementation in src/core/src/gds/read/stamps.cpp (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace hefesto::gds {

/// One structure of a GDS stream and its content stamp.
struct StructureStamp {
  std::string name;          // STRNAME, up to its first NUL
  std::uint64_t start = 0;   // offset after the STRNAME record (GDS_START)
  std::uint64_t end = 0;     // offset after ENDSTR (GDS_END); 0 if the stream ends first
  std::uint32_t stamp = 0;   // the FNV-1a content stamp (1 to 2^31 - 1)

  friend bool operator==(const StructureStamp&, const StructureStamp&) = default;
};

/// The structures of `bytes` in file order (so `start` increases) with
/// their stamps. As in Magic, a BGNSTR not followed by STRNAME is not
/// tabulated, and a structure cut off by the end of the stream (or a record
/// with L < 4) is tabulated with the records it has.
using StampTable = std::vector<StructureStamp>;

[[nodiscard]] StampTable content_stamps(std::span<const std::byte> bytes);

/// calmaStampLookup: the stamp of the structure whose GDS_START is
/// `start`, or 0 when none starts there.
[[nodiscard]] std::uint32_t lookup_stamp(const StampTable& table, std::uint64_t start);

}  // namespace hefesto::gds
