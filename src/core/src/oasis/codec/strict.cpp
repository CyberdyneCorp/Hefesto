// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The strict-mode checks (see strict.hpp). The set of violations follows
// the KLayout reader's warnings (Q-OASIS-7):
// 1. a strict table's records are contiguous from its offset (only PAD,
//    CBLOCK and the PROPERTY records of those names in between);
// 2. no record of the table appears elsewhere;
// 3. references to its names are by refnum only;
// 4. a strict table at offset 0 has no records.
//
// Owner: OASFMT (design D2).

#include "strict.hpp"

#include <string_view>

namespace hefesto::oasis::detail {

namespace {

/// The table whose strictness forbids `use`, and the use's description.
struct UseRule {
  NameTable table;
  std::string_view text;
};

constexpr std::array<UseRule, 5> kUses = {{
    {NameTable::cellname, "CELL record by name"},
    {NameTable::cellname, "PLACEMENT by cell name"},
    {NameTable::textstring, "inline TEXT string"},
    {NameTable::propname, "inline PROPERTY name"},
    {NameTable::propstring, "inline string PROPERTY value"},
}};

diag::Diagnostic violation(std::string detail) {
  return diag::make("OASIS-CODEC-024", "strict mode violation: " + std::move(detail));
}

}  // namespace

void StrictTracker::name_record(NameTable table, std::uint64_t offset) {
  Run& run = runs_[static_cast<std::size_t>(table)];
  const bool continues = run.open;
  other_record();  // a name record ends the runs of the other tables
  if (!continues && run.count > 0) {
    run.split = true;
  }
  if (!run.first_offset) {
    run.first_offset = offset;
  }
  run.open = true;
  ++run.count;
}

void StrictTracker::other_record() {
  for (Run& run : runs_) {
    run.open = false;
  }
}

void StrictTracker::inline_use(InlineUse use) {
  ++inline_uses_[static_cast<std::size_t>(use)];
}

std::vector<diag::Diagnostic> StrictTracker::finish(const TableOffsets& tables) const {
  std::vector<diag::Diagnostic> out;
  for (std::size_t i = 0; i < runs_.size(); ++i) {
    const TableEntry& entry = tables[i];
    const Run& run = runs_[i];
    const std::string name{name_table_name(static_cast<NameTable>(i))};
    if (!entry.strict || run.count == 0) {
      continue;
    }
    if (entry.offset == 0) {
      out.push_back(violation(name + " records in a strict table with offset 0"));
      continue;
    }
    if (run.first_offset != entry.offset) {
      out.push_back(violation(name + " table does not start at its offset " + std::to_string(entry.offset)));
    }
    if (run.split) {
      out.push_back(violation(name + " records are not contiguous"));
    }
  }
  for (std::size_t i = 0; i < kUses.size(); ++i) {
    if (inline_uses_[i] != 0 && tables[static_cast<std::size_t>(kUses[i].table)].strict) {
      out.push_back(violation(std::string{kUses[i].text} + " with a strict " +
                              std::string{name_table_name(kUses[i].table)} + " table (" +
                              std::to_string(inline_uses_[i]) + " times)"));
    }
  }
  return out;
}

}  // namespace hefesto::oasis::detail
