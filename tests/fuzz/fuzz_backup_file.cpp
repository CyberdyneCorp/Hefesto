// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for crash backup and archive files (`crash recover`,
// `archive read`; capability cell-io-safety; hefesto-testing-strategy
// "Parser fuzzing"). The first byte selects the kind; the records are then
// rendered back, which must not fail on anything the parser accepted.

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/cell/io_safety.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace cell = hefesto::cell;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const char* kind = (mode % 2U == 0U) ? "backup" : "archive";
  if (const auto parsed = cell::parse_backup_file(text, kind, "fuzz"); parsed.ok()) {
    (void)cell::render_backup_file(parsed.value().entries, parsed.value().root_cell);
  }
  cell::BackupFile partial;
  (void)cell::parse_backup_records(text, kind, "fuzz", partial);
  return 0;
}
