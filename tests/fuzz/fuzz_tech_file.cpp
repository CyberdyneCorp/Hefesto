// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology-file reader and loader (capability
// tech-file-format; hefesto-testing-strategy "Parser fuzzing"). The input is
// read as a whole technology file: tokenizing, include/repeat handling,
// section dispatch, every section parser and the post-load finalization;
// a technology that loads then has every `drc` style compiled (RULES).

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/style.hpp"
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
  const auto loaded = tech::load_technology_text(text, "/fuzz/f.tech", fs, load_options, sink);
  if (loaded.ok()) {
    namespace drc = hefesto::drc;
    hefesto::compat::CompatSettings settings;
    settings.magic_compat = compat;
    const drc::CifCatalog cif = drc::CifCatalog::from_technology(*loaded.value(), sink);
    for (const auto& style : drc::style_list(*loaded.value()).names) {
      (void)drc::load_style(*loaded.value(), style, tech::GridScale{}, cif, settings, sink);
    }
  }
  return 0;
}
