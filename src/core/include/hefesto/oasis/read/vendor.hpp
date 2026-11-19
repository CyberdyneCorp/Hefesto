// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only OASIS vendor views (capability oasis-read "Read-only OASIS
// vendor views", "Content-hash timestamps for read-only OASIS cells";
// openspec/changes/m6-lef-def-oasis/design.md D12, D17): the stamp of a
// read-only OASIS cell is the content stamp of the cell named by its
// OASIS_CELL in the file named by its OASIS_FILE. cell/timestamps.cpp
// restamps such cells through VendorStamper, as it restamps read-only GDS
// cells through gds::VendorStamper.
//
// Owner: OASR (design D2).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/gds/read/stamps.hpp"

namespace hefesto::oasis {

/// The properties of a read-only OASIS cell.
inline constexpr std::string_view kOasisFile = "OASIS_FILE";
inline constexpr std::string_view kOasisCell = "OASIS_CELL";

/// Whether `def` is a read-only OASIS cell: it has an OASIS_FILE string.
[[nodiscard]] bool is_oasis_vendor(const db::CellDef& def);

/// The stamp of the first cell named `name` in `table` (a table of
/// content_stamps); 0 when the file defines no such cell.
[[nodiscard]] std::uint32_t lookup_stamp(const gds::StampTable& table, std::string_view name);

class VendorStamper {
 public:
  /// Looks OASIS files up through `ctx` (which must outlive the stamper).
  explicit VendorStamper(cell::LookupContext& ctx) : ctx_(ctx) {}

  /// The content stamp of read-only cell `def`: its OASIS_CELL in the file
  /// its OASIS_FILE names, found as gds::VendorStamper finds a GDS_FILE
  /// (`f` or `f.gz` on the cell search path and the library path, then
  /// relative to the directory of the cell's .mag file). 0 when a property
  /// is missing, the file cannot be read or does not define the cell. Each
  /// file is read and scanned once until flush().
  [[nodiscard]] std::uint32_t stamp(const db::CellDef& def);

  /// Drops the scanned files (at the end of an update pass).
  void flush() noexcept { cache_.clear(); }

 private:
  [[nodiscard]] std::optional<platform::Path> find(const db::CellDef& def, const std::string& name);
  [[nodiscard]] const gds::StampTable* table_for(const platform::Path& path);

  cell::LookupContext& ctx_;
  std::map<std::string, std::optional<gds::StampTable>> cache_;  // by the path opened; nullopt: unreadable
};

}  // namespace hefesto::oasis
