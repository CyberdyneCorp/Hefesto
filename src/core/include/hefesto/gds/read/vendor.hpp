// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only vendor views and their content-hash timestamps (capability
// gds-read "Read-only vendor views", "Content-hash timestamps for read-only
// cells", "When content-hash timestamps are assigned";
// openspec/changes/m5-mask-gds-cif/design.md D11; Q-MASK-128, 129):
// - the units check of a read-only read;
// - the GDS_FILE value of a path (PDK-variable and `~` substitution);
// - the stamp of a read-only cell from a stamp table (calmaStampLookup),
//   its assignment at the end of a read (calmaHashStamps), and the
//   VendorStamper that recomputes stamps from the file named by GDS_FILE
//   when timestamps are updated (CalmaVendorStamp), which
//   cell::update_timestamps calls through its vendor-stamp hook.
//
// Owner: GDSR (design D2).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/gds/read/stamps.hpp"

namespace hefesto::gds {

/// The units check of `gds readonly yes` (calmaParseUnits): nullopt when
/// meters per DB unit `mpd` is 1 nm, or 1 Angstrom when the current output
/// style uses angstrom units; otherwise the error GDS-READ-003
/// "Incompatible scale factor of <mpd>, must be 1e-9." (or "1e-10.").
/// Magic then prints "Cannot read this file in read-only mode." and reads
/// nothing.
[[nodiscard]] std::optional<diag::Diagnostic> readonly_units_error(double mpd, bool angstroms);

/// The GDS_FILE value of `path` (the search-path name with magic-compat
/// ON, the canonical path with it OFF, Q-MASK-129, chosen by the caller):
/// a leading PDK_PATH, PDKPATH, PDK_ROOT or PDKROOT value becomes
/// `$<NAME>`, else a leading home directory of an absolute path becomes
/// `~` (applied to the GDS path itself, Q-MASK-128). It is
/// cell::substitute_path, so the PDK prefix must end at a path separator
/// as for cell paths (Q-CELL-20).
[[nodiscard]] std::string gds_file_value(std::string_view path, const cell::PathVariables& vars);

/// DBPropGetDouble: the first value of a double property, or the leading
/// integer of a string property (0 when it has none); nullopt when the
/// cell has no such property of either type.
[[nodiscard]] std::optional<std::int64_t> property_integer(const db::CellDef& def, std::string_view name);

/// calmaStampLookup: the stamp in `table` of the structure at the cell's
/// GDS_START, or 0 when the cell lacks GDS_END or GDS_START or no structure
/// starts there.
[[nodiscard]] std::uint32_t cell_content_stamp(const StampTable& table, const db::CellDef& def);

/// calmaHashStamps: every read-only cell of `cells` (created by the read)
/// that is not fixed-stamp and has a nonzero stamp in `table` gets that
/// stamp and no longer needs a new timestamp. The read calls it when
/// `readonly` is on, the read was not interrupted and the date stamp is
/// the default or 0.
void assign_content_stamps(const StampTable& table, std::span<db::CellDef* const> cells);

/// CalmaVendorStamp with its per-pass cache of scanned files.
class VendorStamper {
 public:
  /// Looks GDS files up through `ctx` (which must outlive the stamper).
  explicit VendorStamper(cell::LookupContext& ctx) : ctx_(ctx) {}

  /// The content stamp of read-only cell `def` from the file named by its
  /// GDS_FILE: `f` (or `f.gz`) on the cell search path and the library
  /// path, then, for a name not starting with `/`, `~` or `$`, relative to
  /// the directory of the cell's .mag file. 0 when the cell has no
  /// GDS_FILE, the file is not found or cannot be read, or no structure
  /// starts at its GDS_START. Each file is read and scanned once until
  /// flush().
  [[nodiscard]] std::uint32_t stamp(const db::CellDef& def);

  /// Drops the scanned files (CalmaVendorStampFlush, at the end of an
  /// update pass).
  void flush() noexcept { cache_.clear(); }

 private:
  [[nodiscard]] std::optional<platform::Path> find(const db::CellDef& def, const std::string& name);
  [[nodiscard]] const StampTable* table_for(const platform::Path& path);

  cell::LookupContext& ctx_;
  std::map<std::string, std::optional<StampTable>> cache_;  // by the path opened; nullopt: unreadable
};

}  // namespace hefesto::gds
