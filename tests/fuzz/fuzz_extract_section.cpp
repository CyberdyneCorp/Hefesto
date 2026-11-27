// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology `extract` section (task 19.2,
// m7-extraction design D22, D24 M7-S2; capabilities extract-tech-section,
// antenna-check "Antenna tech-file keywords grammar" and "Antenna
// coefficient unit conversion"; hefesto-testing-strategy "Parser fuzzing").
// fuzz_tech_file reads extract sections inside whole technology files;
// this target keeps a valid technology around the section, so every input
// reaches the section's keyword parsers.
//
// The first byte is a mode: bits 0-2 pick the base technology
// (mask_section_fuzz.hpp; base 2 is tests/fuzz/data/exttest_fuzz.tech,
// the M7 golden cases' technology with a fuller section, and bases 3-5
// the pinned PDKs, whose extract sections are the largest real ones), bit 3 magic-compat OFF,
// bits 4-5 the grid rescale n (1-4) and bits 6-7 its d (1-4). The rest is
// the body of an `extract` section that replaces the base's own one (an
// empty body keeps it).
//
// Every style of the section is read (load_styles, ExtTechLine), selected
// (ExtLoadStyle), micron-converted with the base's first CIF output style
// (extTechFinalStyle) and rescaled by n/d (ExtTechScale), and its antenna
// rules are read through antenna::RuleView.
//
// Resource caps (harness, design D22; recorded as approved under the
// standing rule of 2026-11-25): inputs above 64 KiB are ignored, as the
// other section targets' bodies are bounded by their seeds. The section's
// own numbers saturate (Q-EXTRACT-30), so no product cap is needed.
//
// Owner: INTEGRATION (tests/fuzz/, design D2).

#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/antenna/antenna.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/tech_file.hpp"
#include "mask_section_fuzz.hpp"

namespace {

constexpr std::size_t kMaxInput = std::size_t{64} << 10;

const hefesto::fuzz::SectionBases& bases() {
  static const hefesto::fuzz::SectionBases instance{"extract", "tests/fuzz/data/exttest_fuzz.tech"};
  return instance;
}

void read_styles(const hefesto::tech::Technology& technology, const hefesto::compat::CompatSettings& compat,
                 std::int32_t n, std::int32_t d) {
  hefesto::diag::NullSink sink;
  const auto cif = hefesto::mask::read_first_output_style(technology);
  for (const hefesto::tech::RawSection& raw : technology.raw_sections()) {
    if (raw.name != "extract") {
      continue;
    }
    const auto table = hefesto::extract::load_styles(raw, technology, sink, compat);
    for (const auto& name : table.names()) {
      const auto style = table.select(name, sink);
      if (style == nullptr) {
        continue;
      }
      const hefesto::extract::ExtStyle converted = style->micron_convert(cif ? &*cif : nullptr, sink);
      const hefesto::extract::ExtStyle scaled = converted.rescale(n, d, compat);
      const auto rules = hefesto::antenna::RuleView::from_style(scaled);
      for (std::size_t t = 0; t < technology.type_count(); ++t) {
        const auto type = static_cast<hefesto::tech::TileType>(t);
        (void)rules.ratio_gate(type);
        (void)rules.ratio_diff_prop(type);
        (void)rules.thickness(type);
      }
    }
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > kMaxInput) {
    return 0;
  }
  const auto [mode, body] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::compat::CompatSettings compat;
  compat.magic_compat = (mode & 8U) == 0;
  const auto technology = hefesto::fuzz::load_fuzz_technology(bases().text(mode & 7U, body), compat.magic_compat);
  if (technology == nullptr) {
    return 0;
  }
  const auto n = static_cast<std::int32_t>(1 + ((mode >> 4U) & 3U));
  const auto d = static_cast<std::int32_t>(1 + ((mode >> 6U) & 3U));
  read_styles(*technology, compat, n, d);
  return 0;
}
