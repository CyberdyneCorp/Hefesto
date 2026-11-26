// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the DEF reader (capability def-read: every section,
// generated vias, routes, pins, blockages; hefesto-testing-strategy
// "Parser fuzzing"; m6-lef-def-oasis design D21). The first byte is a mode:
//   bit 0  magic-compat OFF
//   bit 1  the input is stored as one gzip member (read decompressed,
//          Q-LEFDEF-132)
//   bit 2  -labels
//   bit 3  -noblockage
//   bit 4  the input is read a second time with -annotate (the cells the
//          first read created are annotated)
//   bit 5  the input is read a second time as given (via redefinition,
//          duplicate components)
//   bit 6  the fixed LEF library is not read first (no macros, vias or
//          rules: every COMPONENTS master and via is unknown)
//   bit 7  `def write -anal -allspecial` and `lef write -tech`
// After the reads, `def write` and `lef write` of the cells no other cell
// uses (at most two) run into memory, so the writers' caps (Q-LEFDEF-150)
// and crash fixes (Q-LEFDEF-140) are fuzzed with the reads' tables.
// The rest is the DEF text, read after the fixed library
// tests/fuzz/data/lefdef_fuzz.lef into a scratch session with the fixed
// technology tests/fuzz/data/lefdef_fuzz.tech.
//
// Resource caps: the reader's own (declared counts are never allocated
// from the file; a generated via above 65536 cuts or 4096 rows or
// columns is refused, Q-LEFDEF-117, Q-LEFDEF-153; undefined values are
// clipped, Q-LEFDEF-130; M6-S2).
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZSEC
// workstream.

#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/def/read/read.hpp"
#include "lefdef_fuzz_session.hpp"

namespace {

constexpr std::uint8_t kCompatOff = 0x01;
constexpr std::uint8_t kGzip = 0x02;
constexpr std::uint8_t kLabels = 0x04;
constexpr std::uint8_t kNoBlockage = 0x08;
constexpr std::uint8_t kAnnotateAgain = 0x10;
constexpr std::uint8_t kReadAgain = 0x20;
constexpr std::uint8_t kNoLibrary = 0x40;
constexpr std::uint8_t kWriteAll = 0x80;
constexpr const char* kInputPath = "/in.def";

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, rest] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::fuzz::LefDefFuzzSession session{(mode & kCompatOff) == 0};
  if (!session.loaded || !session.store(kInputPath, rest, (mode & kGzip) != 0)) {
    return 0;
  }
  if ((mode & kNoLibrary) == 0) {
    session.read_fixed_lef();
  }
  hefesto::def::DefReadOptions options;
  options.labels = (mode & kLabels) != 0;
  options.no_blockage = (mode & kNoBlockage) != 0;
  session.read_def(kInputPath, options);
  if ((mode & kAnnotateAgain) != 0) {
    hefesto::def::DefReadOptions annotate = options;
    annotate.annotate = true;
    session.read_def(kInputPath, annotate);
  }
  if ((mode & kReadAgain) != 0) {
    session.read_def(kInputPath, options);
  }
  session.write_def((mode & kWriteAll) != 0);
  session.write_lef((mode & kWriteAll) != 0);
  return 0;
}
