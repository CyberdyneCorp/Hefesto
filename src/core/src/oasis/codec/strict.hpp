// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The strict-mode checks (capability oasis-format "Strict mode"; design D11,
// Q-OASIS-7). The decoder reports every record to the tracker while it
// reads; the table offsets, and so which tables are strict, are known only
// at END, where finish() turns the observations into warnings. A violation
// never changes the decoded content.
//
// Owner: OASFMT (design D2).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/oasis/codec/decoder.hpp"

namespace hefesto::oasis::detail {

/// The kinds of inline reference a strict table forbids (rule 3).
enum class InlineUse : std::uint8_t { cell_by_name, placement_by_name, text_string, property_name, property_string };

class StrictTracker {
 public:
  /// A name record of `table` at file offset `offset` (the CBLOCK's offset
  /// for a record inside a CBLOCK).
  void name_record(NameTable table, std::uint64_t offset);
  /// A record that is neither a name record nor PAD, CBLOCK or PROPERTY:
  /// it ends every open run of name records.
  void other_record();
  /// An inline name or string where a strict table requires a refnum.
  void inline_use(InlineUse use);

  /// The violations of the strict tables in `tables`.
  [[nodiscard]] std::vector<diag::Diagnostic> finish(const TableOffsets& tables) const;

 private:
  struct Run {
    std::optional<std::uint64_t> first_offset;
    std::uint64_t count = 0;
    bool open = false;
    bool split = false;
  };

  std::array<Run, 6> runs_{};
  std::array<std::uint64_t, 5> inline_uses_{};
};

}  // namespace hefesto::oasis::detail
