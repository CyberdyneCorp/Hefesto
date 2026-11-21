// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the `.sch`/`.sym` reader and writer (capability
// schematic-file-format; hefesto-testing-strategy "Parser fuzzing";
// openspec/changes/m8-schematic design D19). Each input is read, written,
// read again and written again, so the writer sees whatever the reader
// accepts and the second read sees the writer's own output.
//
// Mode byte: bit 0 xschem-compat OFF, bit 1 symbol file (`.sym`).
// Resource bounds: none beyond the input; the reader is linear in the input
// and keeps nested embedded blocks on the heap (no recursion).
//
// Owner: FORMAT.

#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/file/transform.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace schem = hefesto::schem;
  const auto split = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::diag::NullSink sink;
  schem::ReadOptions options;
  options.xschem_compat = (split.mode & 1U) == 0;
  options.kind = (split.mode & 2U) != 0 ? schem::FileKind::symbol : schem::FileKind::schematic;
  options.warnings = &sink;
  auto first = schem::read_document(split.rest, options);
  if (!first.ok()) {
    return 0;
  }
  const std::string written = schem::write_document(first.value());
  auto second = schem::read_document(written, options);
  if (second.ok()) {
    (void)schem::write_document(second.value());
  }
  for (const auto& instance : first.value().instances) {
    const schem::Placement placement{instance.origin, instance.rotation, instance.flip};
    (void)schem::transform_point(placement, {1.0, 2.0});
    (void)schem::transform_text(placement, instance.rotation, instance.flip);
    (void)schem::transform_arc_start(placement, instance.origin.x, instance.origin.y);
  }
  return 0;
}
