// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the CIF reader (capability cif-format-io;
// hefesto-testing-strategy "Parser fuzzing"; m5-mask-gds-cif design D21,
// R9). The first byte is a mode:
//   bit 0  magic-compat OFF
//   bit 1  `cif rescale no`
//   bit 2  `cif limit 2` (the subdivision limit; default 100)
//   bit 3  write the top cells as CIF flat instead of hierarchical
//   bit 4  `cif *hier no` and `cif *array no` for the write
//   bit 5  write the top cells as GDS instead of CIF
// The rest is CIF text, read with cif::read_text (lexer, parser, symbol
// table and read engine) into a scratch session with the fixed technology
// tests/fuzz/data/mask_fuzz.tech and an edit cell `top`; then the top cells
// are written into memory (read-then-write never crashes).
//
// Resource caps: the reader's own (the symbol cap, the 2^61 integer
// saturation and the unit-subdivision cap); the write's output style has no
// cut arrays, and at most two top cells are written.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZ workstream.

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/cif/read/read.hpp"
#include "mask_fuzz_session.hpp"

namespace {

constexpr std::uint8_t kCompatOff = 0x01;
constexpr std::uint8_t kNoRescale = 0x02;
constexpr std::uint8_t kLimit = 0x04;
constexpr std::uint8_t kFlat = 0x08;
constexpr std::uint8_t kNoHier = 0x10;
constexpr std::uint8_t kGdsWrite = 0x20;
constexpr std::size_t kMaxTopWrites = 2;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace cif = hefesto::cif;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::fuzz::MaskFuzzSession session{(mode & kCompatOff) == 0};
  auto& editor = session.editor;
  auto top = editor.cells.create("top", editor.technology->plane_count());
  if (!top.ok()) {
    return 0;
  }
  top.value()->flags().available = true;
  editor.edit_cell = top.value();
  editor.mask.cif.rescale = (mode & kNoRescale) == 0;
  editor.mask.cif.limit = (mode & kLimit) != 0 ? 2 : 0;
  editor.mask.cif.hier = (mode & kNoHier) == 0;
  editor.mask.cif.array = (mode & kNoHier) == 0;

  cif::CifReadOptions options;
  options.settings = editor.mask.cif;
  options.warnings = editor.mask.warnings;
  options.files = &session.files;
  options.typed_name = "in";
  (void)cif::read_text(editor, text, options);

  if ((mode & kGdsWrite) != 0) {
    session.write_tops(kMaxTopWrites, false, 0);
  } else {
    session.write_cif_tops(kMaxTopWrites, (mode & kFlat) != 0);
  }
  return 0;
}
