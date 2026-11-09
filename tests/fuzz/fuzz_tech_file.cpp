// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology-file reader and loader (capability
// tech-file-format; hefesto-testing-strategy "Parser fuzzing"). The input is
// read as a whole technology file: tokenizing, include/repeat handling,
// section dispatch, every section parser and the post-load finalization.

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/tech_file.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace tech = hefesto::tech;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const bool compat = (mode & 1U) != 0;
  hefesto::platform::MemoryFileSystem fs;
  hefesto::diag::NullSink sink;
  tech::ReadOptions read_options;
  read_options.magic_compat = compat;
  (void)tech::read_tech_text(text, "/fuzz/f.tech", fs, "/fuzz", sink, read_options);
  tech::LoadOptions load_options;
  load_options.magic_compat = compat;
  (void)tech::load_technology_text(text, "/fuzz/f.tech", fs, load_options, sink);
  return 0;
}
