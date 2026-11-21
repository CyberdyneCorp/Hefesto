// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the LEF reader (capability lef-read: the tokenizer,
// every section, macro creation and annotation; hefesto-testing-strategy
// "Parser fuzzing"; m6-lef-def-oasis design D21). The first byte is a mode:
//   bit 0  magic-compat OFF
//   bit 1  the input is stored as one gzip member (read decompressed)
//   bit 2  the fixed library tests/fuzz/data/lefdef_fuzz.lef is read first,
//          so the input redefines its vias, rules and macros
//   bit 3  the input is read a second time with -annotate (its macros
//          annotate the cells the first read created)
//   bit 4  the input is read a second time without -annotate (duplicate
//          macros, via and rule redefinition)
// The rest is the LEF text, read with `lef read` semantics into a scratch
// session with the fixed technology tests/fuzz/data/lefdef_fuzz.tech.
//
// Resource caps: the reader's own (RECT ITERATE and via ROWCOL repeats,
// 65536, Q-LEFDEF-118; coordinates checked against the plane, M6-S2); the
// input is at most a few MB (libFuzzer's -max_len), so the string tokens
// are bounded by it (M6-S1).
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZSEC
// workstream.

#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "lefdef_fuzz_session.hpp"

namespace {

constexpr std::uint8_t kCompatOff = 0x01;
constexpr std::uint8_t kGzip = 0x02;
constexpr std::uint8_t kFixedFirst = 0x04;
constexpr std::uint8_t kAnnotateAgain = 0x08;
constexpr std::uint8_t kReadAgain = 0x10;
constexpr const char* kInputPath = "/in.lef";

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, rest] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::fuzz::LefDefFuzzSession session{(mode & kCompatOff) == 0};
  if (!session.loaded || !session.store(kInputPath, rest, (mode & kGzip) != 0)) {
    return 0;
  }
  if ((mode & kFixedFirst) != 0) {
    session.read_fixed_lef();
  }
  session.read_lef(kInputPath, false);
  if ((mode & kAnnotateAgain) != 0) {
    session.read_lef(kInputPath, true);
  }
  if ((mode & kReadAgain) != 0) {
    session.read_lef(kInputPath, false);
  }
  return 0;
}
