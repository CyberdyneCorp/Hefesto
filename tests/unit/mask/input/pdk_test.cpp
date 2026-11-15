// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cifinput sections of sky130A, gf180mcuD and ihp-sg13g2 at the M4 pins
// (tasks.md 7.4): the tech load reports only Magic's `Input style <name>:
// scaleFactor=<S>, multiplier=<M>` line, every style selects without a
// message in both magic-compat settings, and the style list and scales are
// the values Magic 8.3.694 holds (`cif listall istyle` and the tech-load
// line). The tech-load line of each PDK is Magic's own, from the golden
// captures on the M4 pins: sky130A DRC-PDK-1, gf180mcuD DRC-PDK-2,
// ihp-sg13g2 DRC-PDK-3. The style lists and the scales of the styles not
// current after the load (sky130A `sky130(vendor)` and `rdlimport`) follow
// CIFrdtech.c on the pinned *-cifin.tech sections; no Magic capture of
// `cif listall istyle` or of those styles' lines exists yet (tasks.md 7.4
// open item). The cases skip when the PDK data set is not installed
// (scripts/fetch_pdk.py).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "input_fixture.hpp"
#include "reference_data.hpp"

namespace mask = hefesto::mask;
namespace tech = hefesto::tech;
namespace fs = std::filesystem;

namespace {

struct Expected {
  std::vector<std::string> names;
  /// The native Q-MASK-311 warnings of the first style with magic-compat ON:
  /// `labels` under a templayer whose CIF index is no layout type.
  std::size_t templayer_label_warnings = 0;
  std::string load_line;                      // the tech-load line of the first style
  std::vector<std::pair<std::int64_t, std::int64_t>> scales;  // (S, M) of each style as read
  std::size_t min_rules = 0;                  // a lower bound on the first style's rules
};

/// The messages other than the Q-MASK-311 templayer-label warnings, and
/// how many of those there were.
std::pair<std::vector<std::string>, std::size_t> split_messages(const std::vector<hefesto::diag::Diagnostic>& messages) {
  std::pair<std::vector<std::string>, std::size_t> out;
  for (const auto& d : messages) {
    if (d.code == "MASK-INPUT-008") {
      ++out.second;
    } else {
      out.first.push_back(d.text);
    }
  }
  return out;
}

void compare_pdk(const fs::path& file, const Expected& expected) {
  hefesto::platform::LocalFileSystem files;
  hefesto::diag::CollectingSink load_sink;
  auto loaded = tech::load_technology(files, file.string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const tech::Technology& technology = *loaded.value();
  for (const bool magic : {true, false}) {
    INFO("magic-compat " << (magic ? "ON" : "OFF"));
    hefesto::diag::CollectingSink sink;
    mask::InputStyleSet set = mask::read_input_styles(technology, mask::test::mode(magic), sink);
    const auto [texts, warnings] = split_messages(sink.take());
    CHECK(texts == std::vector<std::string>{expected.load_line});
    if (magic) {
      CHECK(warnings == expected.templayer_label_warnings);
    }
    CHECK_FALSE(set.section_failed);
    CHECK(set.names() == expected.names);
    REQUIRE(set.styles.size() == expected.scales.size());
    for (std::size_t i = 0; i < set.styles.size(); ++i) {
      INFO("style " << set.styles[i]->name << " S " << set.styles[i]->scale_factor << " M "
                    << set.styles[i]->multiplier);
      CHECK(std::pair{set.styles[i]->scale_factor, set.styles[i]->multiplier} == expected.scales[i]);
      CHECK(split_messages(set.styles[i]->load_messages).first.size() == 1);  // only the Input style line
    }
    REQUIRE(set.current_style() != nullptr);
    CHECK(set.current_style()->rules.size() >= expected.min_rules);
    CHECK_FALSE(set.current_style()->calma.empty());
    for (const std::string& name : expected.names) {
      REQUIRE(mask::select_input_style(set, name, 1, 1, mask::test::mode(magic), sink).ok());
      CHECK(set.current_style()->name == name);
      CHECK(split_messages(sink.take()).first.size() <= 1);
    }
  }
}

}  // namespace

TEST_CASE("the sky130A cifinput section loads as in Magic",
          "[cap:mask-input-styles][req:Cifinput section grammar][req:Input-scale reporting]") {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("sky130A.tech not in .cache/pdks");
  }
  compare_pdk(*file, Expected{{"sky130()", "sky130(vendor)", "rdlimport"},
                              1,
                              "Input style sky130(): scaleFactor=2, multiplier=2",
                              {{2, 2}, {2, 2}, {1, 1}},
                              100});
}

TEST_CASE("the gf180mcuD cifinput section loads as in Magic",
          "[cap:mask-input-styles][req:Cifinput section grammar][req:Input-scale reporting]") {
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("gf180mcuD.tech not in .cache/pdks");
  }
  compare_pdk(*file, Expected{{"import"}, 0, "Input style import: scaleFactor=10, multiplier=2", {{10, 2}}, 100});
}

TEST_CASE("the ihp-sg13g2 cifinput section loads as in Magic",
          "[cap:mask-input-styles][req:Cifinput section grammar][req:Input-scale reporting]") {
  const auto file = hefesto::test::pinned_ihp_sg13g2_tech();
  if (!file) {
    SKIP("ihp-sg13g2.tech not in .cache/pdks");
  }
  compare_pdk(*file, Expected{{"sg13g2()"}, 0, "Input style sg13g2(): scaleFactor=2, multiplier=2", {{2, 2}}, 100});
}
