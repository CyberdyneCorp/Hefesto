// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology `drc` section compiler (capability
// drc-rule-language; hefesto-testing-strategy "Parser fuzzing"; m3-drc
// tasks.md 11.2). The first byte picks the base technology (bits 0-2) and
// the magic-compat mode (bit 3); the rest is the body of a `drc` section
// that replaces the base technology's own one. An empty body keeps the
// base's own section, so a one-byte seed compiles a real rule deck. Every
// declared style is compiled, rescaled by 1/2 and printed (printrules,
// rulestats); the cifoutput catalog is read and rescaled too.
//
// Bases: 0 tests/data/drc/rules/rules.tech, 1 the DRC golden technology
// tools/golden/authoring/drctest.tech, 2 Magic's scmos.tech (generated from
// build/ref/magic), 3 sky130A, 4 gf180mcuD, 5 ihp-sg13g2 (the pinned PDK
// sets in .cache/pdks); a base that is not available falls back to 0.
//
// Owner: INTEGRATION (tests/fuzz/, openspec/changes/m3-drc/design.md D2).

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace {

constexpr std::size_t kBases = 6;

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in{path, std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::optional<std::filesystem::path> base_path(std::size_t base) {
  namespace test = hefesto::test;
  switch (base) {
    case 1:
      return std::filesystem::path{HEFESTO_SOURCE_DIR} / "tools/golden/authoring/drctest.tech";
    case 2: {
      const std::filesystem::path scmos{HEFESTO_SCMOS_TECH};
      return std::filesystem::exists(scmos) ? std::optional{scmos} : std::nullopt;
    }
    case 3:
      return test::pinned_sky130a_tech();
    case 4:
      return test::pinned_gf180mcud_tech();
    case 5:
      return test::pinned_ihp_sg13g2_tech();
    default:
      return std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/drc/rules/rules.tech";
  }
}

std::string_view trimmed(std::string_view line) {
  const auto first = line.find_first_not_of(" \t\r");
  if (first == std::string_view::npos) {
    return {};
  }
  const auto last = line.find_last_not_of(" \t\r");
  return line.substr(first, last - first + 1);
}

/// A technology text split around its `drc` section: the text without the
/// section and the section's body.
struct BaseText {
  std::string without_drc;
  std::string drc_body;
};

BaseText split_drc(const std::string& text) {
  BaseText out;
  std::istringstream in{text};
  std::string line;
  bool in_section = false;
  bool in_drc = false;
  while (std::getline(in, line)) {
    const std::string_view word = trimmed(line);
    const bool comment = word.empty() || word.front() == '#';
    if (!in_section && !comment && word.find_first_of(" \t") == std::string_view::npos) {
      in_section = true;
      in_drc = word == "drc";
      if (in_drc) {
        continue;
      }
    } else if (in_section && word == "end") {
      in_section = false;
      if (in_drc) {
        in_drc = false;
        continue;
      }
    }
    (in_drc ? out.drc_body : out.without_drc) += line + "\n";
  }
  return out;
}

const BaseText& base_text(std::size_t base) {
  static const std::array<BaseText, kBases> bases = [] {
    std::array<BaseText, kBases> texts;
    for (std::size_t i = 0; i < kBases; ++i) {
      const auto path = base_path(i);
      texts[i] = path ? split_drc(read_file(*path)) : BaseText{};
    }
    return texts;
  }();
  const BaseText& chosen = bases[base % kBases];
  return chosen.without_drc.empty() ? bases[0] : chosen;
}

void compile_every_style(const hefesto::tech::Technology& technology, const hefesto::compat::CompatSettings& compat) {
  namespace drc = hefesto::drc;
  hefesto::diag::NullSink sink;
  drc::CifCatalog cif = drc::CifCatalog::from_technology(technology, sink);
  for (const std::string& name : drc::style_list(technology).names) {
    const auto rules = drc::load_style(technology, name, hefesto::tech::GridScale{}, cif, compat, sink);
    const auto finer = drc::rescale(*rules, 1, 2, compat);
    std::ostringstream text;
    drc::print_rules(*finer, technology, text);
    drc::rule_stats(*finer, text);
  }
  (void)cif.rescale(1, 2);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, body] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const BaseText& base = base_text(mode & 7U);
  std::string text = base.without_drc;
  text += "drc\n";
  text += body.empty() ? std::string_view{base.drc_body} : body;
  text += "\nend\n";
  hefesto::compat::CompatSettings compat;
  compat.magic_compat = (mode & 8U) == 0;
  hefesto::platform::MemoryFileSystem fs;
  hefesto::diag::NullSink sink;
  hefesto::tech::LoadOptions options;
  options.magic_compat = compat.magic_compat;
  const auto technology = hefesto::tech::load_technology_text(text, "/fuzz/drc.tech", fs, options, sink);
  if (technology.ok()) {
    compile_every_style(*technology.value(), compat);
  }
  return 0;
}
