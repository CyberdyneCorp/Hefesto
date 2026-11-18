// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CBLOCK compression (capability oasis-format "CBLOCK";
// openspec/changes/m6-lef-def-oasis/design.md D3, D11, D13): a CBLOCK of
// comp-type 0 holds raw DEFLATE data (zlib inflateInit2/deflateInit2 with
// window bits -15, from the bundled zlib 1.3.1). Inflation stops at the
// declared uncompressed size and at the inflate cap (256 times the
// compressed size, at least 1 GiB, as gds::inflate_cap; security review
// M6-S4). Deflation uses the fixed level, window, memory level and strategy
// of Q-OASIS-64, so the bytes are the same on every platform.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASFMT fills
// in the implementation in src/core/src/oasis/codec/ (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::oasis {

/// The decompressed bytes of one CBLOCK: `compressed` inflated to exactly
/// `uncompressed_size` bytes, which must not exceed `cap` (0: the default
/// cap of `compressed.size()`). A damaged stream, a size mismatch or a
/// size over the cap is an error. SCAFFOLD: fails with CORE-NOTIMPL-006
/// until OASFMT lands it.
[[nodiscard]] diag::Outcome<std::vector<std::byte>> inflate_cblock(std::span<const std::byte> compressed,
                                                                   std::uint64_t uncompressed_size,
                                                                   std::uint64_t cap = 0);

/// The raw DEFLATE data of `raw` at `level` (1-9; `oasis compress`).
/// SCAFFOLD: fails with CORE-NOTIMPL-006 until OASFMT lands it.
[[nodiscard]] diag::Outcome<std::vector<std::byte>> deflate_cblock(std::span<const std::byte> raw, int level);

}  // namespace hefesto::oasis
