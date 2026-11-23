// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology-file reader and loader (capability
// tech-file-format; hefesto-testing-strategy "Parser fuzzing"). The input is
// read as a whole technology file: tokenizing, include/repeat handling,
// section dispatch, every section parser and the post-load finalization;
// a technology that loads then has every `drc` style compiled (RULES) and
// every `extract` style read, micron-converted and rescaled (EXTTECH).

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/tech_file.hpp"

namespace {

// Every extraction style through load_styles, micron_convert and a
// half-grid rescale (Q-EXTRACT-30: hostile numbers saturate).
void extract_styles(const hefesto::tech::Technology& technology, const hefesto::compat::CompatSettings& settings,
                    hefesto::diag::DiagnosticSink& sink) {
  for (const hefesto::tech::RawSection& raw : technology.raw_sections()) {
    if (raw.name != "extract") {
      continue;
    }
    const auto table = hefesto::extract::load_styles(raw, technology, sink, settings);
    const auto cif = hefesto::mask::read_first_output_style(technology);
    for (const auto& name : table.names()) {
      if (const auto style = table.select(name)) {
        (void)style->micron_convert(cif ? &*cif : nullptr).rescale(1, 2, settings);
      }
    }
  }
}

}  // namespace

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
    extract_styles(*loaded.value(), settings, sink);
  }
  return 0;
}
