// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared parts of fuzz_cifoutput_section and fuzz_cifinput_section
// (m5-mask-gds-cif design D21): the base technologies whose `cifoutput` or
// `cifinput` section the fuzz input replaces, and the technology load.
//
// Bases (bits 0-2 of the mode byte): 0 tests/fuzz/data/mask_fuzz.tech,
// 1 tests/data/cif/masktest.tech, 2 the section's STYLES or INPUT test
// technology (tests/data/mask/style/styles.tech or
// tests/data/mask/input/input.tech), 3 sky130A, 4 gf180mcuD, 5 ihp-sg13g2
// (the pinned PDK sets in .cache/pdks); a base that is not available falls
// back to 0. An empty body keeps the base's own section, so a one-byte seed
// reads a real section.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZ workstream.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace hefesto::fuzz {

inline constexpr std::size_t kSectionBases = 6;

inline std::optional<std::filesystem::path> section_base_path(std::size_t base, std::string_view own_test_tech) {
  const std::filesystem::path root{HEFESTO_SOURCE_DIR};
  switch (base) {
    case 1:
      return root / "tests/data/cif/masktest.tech";
    case 2:
      return root / own_test_tech;
    case 3:
      return test::pinned_sky130a_tech();
    case 4:
      return test::pinned_gf180mcud_tech();
    case 5:
      return test::pinned_ihp_sg13g2_tech();
    default:
      return root / "tests/fuzz/data/mask_fuzz.tech";
  }
}

/// The base technologies split around `section`; `own_test_tech` is base 2.
class SectionBases {
 public:
  SectionBases(std::string_view section, std::string_view own_test_tech) : section_(section) {
    for (std::size_t i = 0; i < kSectionBases; ++i) {
      const auto path = section_base_path(i, own_test_tech);
      if (path && std::filesystem::exists(*path)) {
        bases_[i] = split_section(tech_with_includes(*path), section);
      }
    }
  }

  /// The technology text of base `base` (mod kSectionBases) with `body` as
  /// the section (the base's own body when `body` is empty).
  [[nodiscard]] std::string text(std::size_t base, std::string_view body) const {
    const SectionSplit& wanted = bases_[base % kSectionBases];
    const SectionSplit& chosen = wanted.without.empty() ? bases_[0] : wanted;
    return chosen.with_body(section_, body.empty() ? std::string_view{chosen.body} : body);
  }

 private:
  std::string section_;
  std::array<SectionSplit, kSectionBases> bases_;
};

/// Loads `text` as a technology; null when the load fails.
inline std::shared_ptr<const tech::Technology> load_fuzz_technology(const std::string& text, bool magic_compat) {
  platform::MemoryFileSystem fs;
  diag::NullSink sink;
  tech::LoadOptions options;
  options.magic_compat = magic_compat;
  auto loaded = tech::load_technology_text(text, "/fuzz/section.tech", fs, options, sink);
  return loaded.ok() ? loaded.value() : nullptr;
}

}  // namespace hefesto::fuzz
