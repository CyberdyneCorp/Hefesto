// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LVS netlist readers (openspec/changes/m9-lvs-simulation design.md D6,
// D18, "Area contracts" LVSREAD; capability lvs-netlist-input). SPICE and
// CDL, structural Verilog, `.sim` and legacy `.ext` are read into a
// NetlistDb; includes resolve through the platform file service.
//
// Owner: LVSREAD.

#pragma once

#include <cstddef>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/read/source.hpp"

namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::lvs {

/// The reader caps of design D18 (Q-LVS-36): each a setting that may be
/// lowered, never raised past twice the default.
struct ReadLimits {
  std::size_t include_depth = 64;
  std::size_t logical_line = std::size_t{16} << 20;  // 16 MiB
  std::size_t name_length = 4096;                    // 4 KiB
};

struct ReadOptions {
  bool netgen_compat = true;  // project setting netgen-compat (design D5)
  ReadLimits limits;
  platform::FileSystem* files = nullptr;  // paths and includes; required for a path Source
};

/// Reads `source` as `format` into `db` and returns the cell it defines last
/// (the top cell of a file).
[[nodiscard]] diag::Outcome<CellId> read_netlist(NetlistDb& db, const Source& source, Format format,
                                                 const ReadOptions& options);

/// The format of `source` ("Format selection"): an error when it is unknown.
[[nodiscard]] diag::Outcome<Format> detect_format(const Source& source);

}  // namespace hefesto::lvs
