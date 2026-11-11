// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cifoutput catalog against the values Magic 8.3.694 holds for scmos,
// sky130A, gf180mcuD and ihp-sg13g2 (tasks.md 4.1): the style list, the
// style current after `tech load`, and for each style made current by
// `cif ostyle` its scalefactor, expander, reducer and every contact's
// squares border (drcGetContactBorder), at the load grid and after the
// PDK's `scalegrid`. The reference files (tests/data/drc/rules/magic_cif/)
// were read from Magic's CIFCurStyle and CIFGetContactSize with a probe
// command; the PDK cases skip when their data set is not installed.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/platform/file_system.hpp"
#include "reference_data.hpp"
#include "rules_fixture.hpp"

namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
namespace fs = std::filesystem;

namespace {

struct MagicContact {
  std::string type;
  std::int32_t border = 0;
};

struct MagicStyle {
  std::string name;
  std::int32_t scale = 0;
  std::int32_t expander = 0;
  std::int32_t reducer = 0;
  std::vector<MagicContact> contacts;
};

/// One reference file: `ostyles` names, the `loaded` style, then one
/// `style` block per `cif ostyle`.
struct MagicCif {
  std::vector<std::string> ostyles;
  MagicStyle loaded;
  std::vector<MagicStyle> styles;
};

MagicStyle read_style(std::istringstream& words) {
  MagicStyle s;
  words >> s.name >> s.scale >> s.expander >> s.reducer;
  return s;
}

MagicCif read_magic(const std::string& name) {
  std::ifstream in(fs::path{HEFESTO_TEST_DATA_DIR} / "magic_cif" / (name + ".txt"));
  REQUIRE(in.good());
  MagicCif out;
  MagicStyle* block = nullptr;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string key;
    words >> key;
    if (key == "ostyles") {
      for (std::string s; words >> s;) {
        out.ostyles.push_back(s);
      }
    } else if (key == "loaded") {
      out.loaded = read_style(words);
      block = &out.loaded;
    } else if (key == "style") {
      out.styles.push_back(read_style(words));
      block = &out.styles.back();
    } else if (key == "contact" && block != nullptr) {
      MagicContact c;
      words >> c.type >> c.border;
      block->contacts.push_back(c);
    }
  }
  return out;
}

std::vector<std::string> catalog_names(const drc::CifCatalog& cif) {
  std::vector<std::string> names;
  for (const auto& style : cif.styles()) {
    names.push_back(style.name);
  }
  return names;
}

/// The current style of `cif` against Magic's CIFCurStyle: numbers, and
/// drcGetContactBorder of every contact type (none where Magic finds no
/// squares operator or a border of 0).
void check_style(const drc::CifCatalog& cif, const tech::Technology& technology, const MagicStyle& magic) {
  INFO("cif ostyle " << magic.name);
  const drc::CifStyleInfo* style = cif.current();
  REQUIRE(style != nullptr);
  CHECK(style->name == magic.name);
  CHECK(style->scale_factor == magic.scale);
  CHECK(style->expander == magic.expander);
  CHECK(style->reducer == magic.reducer);
  std::vector<bool> listed(technology.type_count(), false);
  for (const MagicContact& c : magic.contacts) {
    INFO("contact " << c.type);
    const auto type = technology.find_type(c.type);
    REQUIRE(type.id.has_value());
    listed[*type.id] = true;
    const auto border = cif.contact_border(*type.id);
    if (c.border > 0) {
      CHECK(border == drc::ContactBorder{c.border, magic.scale});
    } else {
      CHECK_FALSE(border.has_value());
    }
  }
  for (tech::TileType t = 0; t < technology.type_count(); ++t) {
    if (technology.is_contact(t) && !listed[t]) {
      INFO("contact without squares " << technology.type_name(t));
      CHECK_FALSE(cif.contact_border(t).has_value());
    }
  }
}

/// `tech load`, then `scalegrid scale_n scale_d` when given, then every
/// `cif ostyle` in turn, as the reference file `name` lists them.
void compare_with_magic(const fs::path& file, const std::string& name, std::int32_t scale_n, std::int32_t scale_d) {
  hefesto::platform::LocalFileSystem files;
  hefesto::diag::CollectingSink sink;
  auto loaded = tech::load_technology(files, file.string(), tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  const tech::Technology& technology = *loaded.value();
  drc::CifCatalog cif = drc::CifCatalog::from_technology(technology, sink);
  if (scale_n != scale_d) {
    (void)cif.rescale(scale_n, scale_d);
  }
  const MagicCif magic = read_magic(name);
  CHECK(catalog_names(cif) == magic.ostyles);
  check_style(cif, technology, magic.loaded);
  for (const MagicStyle& style : magic.styles) {
    REQUIRE(cif.select(style.name).ok());
    check_style(cif, technology, style);
  }
}

}  // namespace

TEST_CASE("the cifoutput catalog of scmos matches Magic's", "[cap:drc-rule-language][req:Halo and step size]") {
  const fs::path scmos{HEFESTO_SCMOS_TECH};
  std::error_code ec;
  if (!fs::exists(scmos, ec)) {
    SKIP("scmos.tech is generated from build/ref/magic");
  }
  compare_with_magic(scmos, "scmos-plain", 1, 1);
}

TEST_CASE("the cifoutput catalog of sky130A matches Magic's", "[cap:drc-rule-language][req:Halo and step size]") {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("sky130A.tech not in .cache/pdks");
  }
  compare_with_magic(*file, "sky130A-plain", 1, 1);
  compare_with_magic(*file, "sky130A-grid", 1, 2);
}

TEST_CASE("the cifoutput catalog of gf180mcuD matches Magic's", "[cap:drc-rule-language][req:Halo and step size]") {
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("gf180mcuD.tech not in .cache/pdks");
  }
  compare_with_magic(*file, "gf180mcuD-plain", 1, 1);
  compare_with_magic(*file, "gf180mcuD-grid", 1, 10);
}

TEST_CASE("the cifoutput catalog of ihp-sg13g2 matches Magic's", "[cap:drc-rule-language][req:Halo and step size]") {
  const auto file = hefesto::test::pinned_ihp_sg13g2_tech();
  if (!file) {
    SKIP("ihp-sg13g2.tech not in .cache/pdks");
  }
  compare_with_magic(*file, "ihp-sg13g2-plain", 1, 1);
  compare_with_magic(*file, "ihp-sg13g2-grid", 1, 2);
}
