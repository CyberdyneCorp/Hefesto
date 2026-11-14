// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell timestamps (capability cell-timestamps): the value domain, stamp
// generation with propagation to ancestors, explicit stamps, the mismatch
// log filled while cells are read and its deferred processing.
// (Magic ref: database/DBtimestmp.c, commands/CmdCD.c:1546-1566)
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"

namespace hefesto::cell {

/// "Timestamp value domain": whole seconds since the epoch truncated to a
/// signed 32-bit value (held in 64 bits, Q-CELL-35).
[[nodiscard]] std::int64_t stamp_from_time(std::chrono::system_clock::time_point now) noexcept;
/// A stored stamp as written to and read from files (signed 32-bit).
[[nodiscard]] std::int32_t stamp_as_written(std::int64_t stamp) noexcept;

struct StampMismatch {
  db::CellDef* def = nullptr;
  db::Rect old_area;
};

/// The pending mismatches (Magic's `mismatch` list: most recent first).
class TimestampLog {
 public:
  /// DBStampMismatch.
  void record(db::CellDef& def, const db::Rect& old_area);
  /// Drops the entries of a definition about to be deleted.
  void forget(const db::CellDef& def);
  [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
  [[nodiscard]] const std::vector<StampMismatch>& entries() const noexcept { return entries_; }
  /// Removes and returns the most recent entry.
  StampMismatch pop();

 private:
  std::vector<StampMismatch> entries_;  // back = most recent
};

/// DBFlagMismatches: every parent of `def` gets child_timestamps_changed.
void flag_mismatched_parents(db::CellDef& def);

/// A subcell re-check queued in a parent (the DRC capabilities own the
/// re-check itself; M1 reports the areas).
struct RecheckArea {
  db::CellDef* parent = nullptr;
  db::Rect area;
};

struct MismatchReport {
  bool processed = false;            // at least one mismatch was pending
  std::vector<std::string> cells;    // in report order (reverse of processing)
  std::vector<RecheckArea> rechecks;
  /// "Processing timestamp mismatches." and the "found in these cells" line.
  [[nodiscard]] std::vector<std::string> messages() const;
};

/// "Deferred mismatch processing". `read_cell` reads an unavailable cell
/// (ignoring the technology, following its dereference flag).
[[nodiscard]] MismatchReport process_mismatches(TimestampLog& log,
                                                const std::function<void(db::CellDef&)>& read_cell);

/// The content stamp of a read-only (vendor GDS) cell from the file its
/// GDS_FILE names, or 0 when it cannot be computed (gds::VendorStamper).
using VendorStampFn = std::function<std::uint32_t(const db::CellDef&)>;

/// "Generating new stamps" at time `now` for `only` (nullptr: every
/// pending cell of `library`, in its hash order). Mismatches must be
/// processed first by the caller. A read-only cell that is not fixed-stamp
/// takes `vendor_stamp`'s nonzero, changed stamp and propagates it to its
/// parents (gds-read "When content-hash timestamps are assigned"); without
/// `vendor_stamp` it keeps its stamp.
void update_timestamps(db::CellLibrary& library, db::CellDef* only, std::int64_t now,
                       const VendorStampFn& vendor_stamp = {});

/// `cellname timestamp`: assigns `value`; clears the pending mark when the
/// value differs; with `magic_compat` false also sets the fixed-stamp
/// attribute (Q-CELL-36).
void set_explicit_timestamp(db::CellDef& def, std::int64_t value, bool magic_compat);

}  // namespace hefesto::cell
