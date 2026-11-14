// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cifoutput sections of sky130A, gf180mcuD and ihp-sg13g2 at the M4
// pins (tasks.md 3.7): the tech load reports nothing, `cif ostyle` of each
// style reports nothing Magic would not (only Hefesto's native Q-MASK-8
// warning for the fill/density styles that reduce below expander 10), and the
// style list, the style current after the load and each style's scale
// (cs_scaleFactor, cs_expander, cs_reducer; `cif scale out` is
// scaleFactor / expander / 100 microns) match the values Magic 8.3.694
// holds, read with a probe into tests/data/drc/rules/magic_cif/ (m3-drc
// task 4.1, re-read on the M4 pins). The cases skip when the PDK data set
// is not installed (scripts/fetch_pdk.py).
//
// The layer count of each style (cs_nLayers: every `layer`, `templayer` and
// `labellayer` line of the style's variant) is written from the pinned
// *-cifout.tech files: no Magic capture of per-style layer counts exists
// yet (a probe in golden-capture.yml, tasks.md 3.7 open item).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "reference_data.hpp"
#include "style_fixture.hpp"

namespace mask = hefesto::mask;
namespace tech = hefesto::tech;
namespace fs = std::filesystem;

namespace {

struct MagicScale {
  std::string name;
  std::int32_t scale = 0;
  std::int32_t expander = 0;
  std::int32_t reducer = 0;
};

/// A Magic reference file: `ostyles`, `loaded` and one `style` line per `cif ostyle`.
struct MagicStyles {
  std::vector<std::string> names;
  MagicScale loaded;
  std::vector<MagicScale> styles;
};

MagicScale scale_of(std::istringstream& words) {
  MagicScale s;
  words >> s.name >> s.scale >> s.expander >> s.reducer;
  return s;
}

MagicStyles read_magic(const std::string& name) {
  std::ifstream in(fs::path{HEFESTO_SOURCE_DIR} / "tests/data/drc/rules/magic_cif" / (name + ".txt"));
  REQUIRE(in.good());
  MagicStyles out;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string key;
    words >> key;
    if (key == "ostyles") {
      for (std::string s; words >> s;) {
        out.names.push_back(s);
      }
    } else if (key == "loaded") {
      out.loaded = scale_of(words);
    } else if (key == "style") {
      out.styles.push_back(scale_of(words));
    }
  }
  return out;
}

void check_scale(const mask::OutputStyle& style, const MagicScale& magic) {
  INFO("style " << magic.name);
  CHECK(style.name == magic.name);
  CHECK(style.scale == mask::StyleScale{magic.scale, magic.expander, magic.reducer});
}

/// cs_nLayers of each style, counted from the pinned *-cifout.tech files.
using LayerCounts = std::map<std::string, std::size_t>;

void compare_pdk(const fs::path& file, const std::string& reference, const LayerCounts& layer_counts) {
  hefesto::platform::LocalFileSystem files;
  hefesto::diag::CollectingSink load_sink;
  auto loaded = tech::load_technology(files, file.string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const tech::Technology& technology = *loaded.value();
  hefesto::diag::CollectingSink sink;
  mask::OutputStyleSet set = mask::read_output_styles(technology, sink);
  for (const auto& d : sink.take()) {
    FAIL_CHECK("tech load: " << d.code << ": " << d.text);
  }
  CHECK_FALSE(set.section_failed);
  const MagicStyles magic = read_magic(reference);
  CHECK(set.names() == magic.names);
  REQUIRE(set.current_style() != nullptr);
  check_scale(*set.current_style(), magic.loaded);
  // Every style selects without a message in both modes: the styles that
  // reduce below expander 10 (sky130A and IHP density, gf180mcuD metfill)
  // have no bloat-all, so the Q-MASK-8 warning stays quiet (amended
  // 2026-11-14), and no PDK style truncates its scale (Q-MASK-29).
  for (const bool compat : {true, false}) {
    for (const MagicScale& m : magic.styles) {
      REQUIRE(mask::select_output_style(set, m.name, 1, 1, mask::test::compat_mode(compat), sink).ok());
      for (const auto& d : sink.take()) {
        FAIL_CHECK("cif ostyle " << m.name << " (magic-compat " << (compat ? "ON" : "OFF") << "): " << d.code
                                 << ": " << d.text);
      }
      check_scale(*set.current_style(), m);
      REQUIRE(layer_counts.contains(m.name));
      CHECK(set.current_style()->layers.size() == layer_counts.at(m.name));
      CHECK(set.current_style()->halo() > 0);
    }
  }
  // The technology's grid values are the first style's (tech/loader.cpp).
  REQUIRE(technology.cif_output_grid().has_value());
  CHECK(technology.cif_output_grid()->scale_factor == magic.loaded.scale);
  CHECK(technology.cif_output_grid()->expander == magic.loaded.expander);
}

}  // namespace

TEST_CASE("the sky130A cifoutput section loads as in Magic",
          "[cap:mask-output-styles][req:cifoutput section grammar][req:Unit normalisation]") {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("sky130A.tech not in .cache/pdks");
  }
  compare_pdk(*file, "sky130A-plain",
              LayerCounts{{"gdsii()", 117},
                          {"gdsii(origfill)", 117},
                          {"drc", 47},
                          {"density", 8},
                          {"wafflefill()", 72},
                          {"wafflefill(tiled)", 72}});
}

TEST_CASE("the gf180mcuD cifoutput section loads as in Magic",
          "[cap:mask-output-styles][req:cifoutput section grammar][req:Unit normalisation]") {
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("gf180mcuD.tech not in .cache/pdks");
  }
  compare_pdk(*file, "gf180mcuD-plain", LayerCounts{{"gdsii", 97}, {"metfill", 17}, {"drc", 25}});
}

TEST_CASE("the ihp-sg13g2 cifoutput section loads as in Magic",
          "[cap:mask-output-styles][req:cifoutput section grammar][req:Unit normalisation]") {
  const auto file = hefesto::test::pinned_ihp_sg13g2_tech();
  if (!file) {
    SKIP("ihp-sg13g2.tech not in .cache/pdks");
  }
  compare_pdk(*file, "ihp-sg13g2-plain",
              LayerCounts{{"gdsii", 148}, {"drc", 62}, {"density", 9}, {"patternfill()", 70}, {"patternfill(tiled)", 70}});
}
