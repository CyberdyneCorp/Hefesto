// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell timestamps (cell-timestamps). Ported from Magic 8.3.694
// database/DBtimestmp.c (DBStampMismatch, DBFlagMismatches,
// DBFixMismatch, DBUpdateStamps, dbStampFunc); Magic is (c) the Regents of
// the University of California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).

#include "hefesto/cell/timestamps.hpp"

#include <algorithm>
#include <set>

#include "hefesto/db/hierarchy.hpp"

namespace hefesto::cell {

std::int64_t stamp_from_time(std::chrono::system_clock::time_point now) noexcept {
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  const std::int64_t whole = seconds;  // long or long long, by platform
  return stamp_as_written(whole);
}

std::int32_t stamp_as_written(std::int64_t stamp) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(stamp) & 0xffffffffU));
}

void TimestampLog::record(db::CellDef& def, const db::Rect& old_area) {
  entries_.push_back(StampMismatch{&def, old_area});
}

void TimestampLog::forget(const db::CellDef& def) {
  std::erase_if(entries_, [&](const StampMismatch& m) { return m.def == &def; });
}

StampMismatch TimestampLog::pop() {
  StampMismatch m = entries_.back();
  entries_.pop_back();
  return m;
}

void flag_mismatched_parents(db::CellDef& def) {
  for (db::CellUse* u : def.instances()) {
    if (u->parent != nullptr) {
      u->parent->flags().child_timestamps_changed = true;
    }
  }
}

std::vector<std::string> MismatchReport::messages() const {
  if (!processed) {
    return {};
  }
  std::string list = "Timestamp mismatches found in these cells: ";
  for (std::size_t i = 0; i < cells.size(); ++i) {
    list += (i == 0 ? "" : ", ") + cells[i];
  }
  return {"Processing timestamp mismatches.", list + "."};
}

namespace {

/// The area `old_area` (child coordinates) occupies in the parent for the
/// whole array of `use`: the first and last elements through the transform.
db::Rect array_area(const db::CellUse& use, const db::Rect& old_area) {
  const db::Rect first = db::apply(db::element_transform(use, use.array.xlo, use.array.ylo), old_area);
  const db::Rect last = db::apply(db::element_transform(use, use.array.xhi, use.array.yhi), old_area);
  return db::Rect{std::min(first.x0, last.x0), std::min(first.y0, last.y0), std::max(first.x1, last.x1),
                  std::max(first.y1, last.y1)};
}

void fix_mismatch(const StampMismatch& m, const std::function<void(db::CellDef&)>& read_cell,
                  MismatchReport& report) {
  db::CellDef& def = *m.def;
  if (!def.flags().available) {
    read_cell(def);
  }
  db::recompute_bbox(def);
  for (db::CellUse* u : def.instances()) {
    if (u->parent == nullptr) {
      continue;
    }
    report.rechecks.push_back(RecheckArea{u->parent, array_area(*u, m.old_area)});
    report.rechecks.push_back(RecheckArea{u->parent, u->bbox});
  }
}

void stamp_cell(db::CellDef& def, std::int64_t now);

void stamp_parents(db::CellDef& def, std::int64_t now) {
  const std::vector<db::CellUse*> uses = def.instances();
  for (db::CellUse* u : uses) {
    if (u->parent != nullptr) {
      u->parent->flags().child_timestamps_changed = true;
      stamp_cell(*u->parent, now);
    }
  }
}

/// dbStampFunc.
void stamp_cell(db::CellDef& def, std::int64_t now) {
  db::CellFlags& f = def.flags();
  if (def.timestamp() == now) {
    return;
  }
  f.needs_new_timestamp = false;
  if (f.no_edit) {
    return;
  }
  // Vendor-GDS content stamps come from gds-read (not in M1): such cells
  // keep their stamp and do not propagate.
  if (f.vendor_gds && !f.fixed_timestamp) {
    return;
  }
  if (!f.fixed_timestamp) {
    def.set_timestamp(now);
  }
  stamp_parents(def, now);
}

}  // namespace

MismatchReport process_mismatches(TimestampLog& log, const std::function<void(db::CellDef&)>& read_cell) {
  MismatchReport report;
  std::set<const db::CellDef*> done;
  std::vector<std::string> order;
  while (!log.empty()) {
    report.processed = true;
    const StampMismatch m = log.pop();
    if (!done.insert(m.def).second) {
      continue;
    }
    fix_mismatch(m, read_cell, report);
    order.push_back(m.def->name());
  }
  report.cells.assign(order.rbegin(), order.rend());
  return report;
}

void update_timestamps(db::CellLibrary& library, db::CellDef* only, std::int64_t now) {
  if (only != nullptr) {
    if (!only->flags().needs_new_timestamp) {
      return;  // DBUpdateStamps(def): only a pending cell is stamped
    }
    if (only->flags().no_edit || only->flags().fixed_timestamp) {
      only->flags().needs_new_timestamp = false;
      return;
    }
    stamp_cell(*only, now);
    return;
  }
  std::vector<db::CellDef*> pending;
  library.for_each([&](db::CellDef& def) {
    if (def.flags().needs_new_timestamp) {
      pending.push_back(&def);
    }
  });
  for (db::CellDef* def : pending) {
    stamp_cell(*def, now);
  }
}

void set_explicit_timestamp(db::CellDef& def, std::int64_t value, bool magic_compat) {
  if (def.timestamp() != value) {
    def.flags().needs_new_timestamp = false;
  }
  def.set_timestamp(value);
  if (!magic_compat) {
    def.flags().fixed_timestamp = true;
  }
}

}  // namespace hefesto::cell
