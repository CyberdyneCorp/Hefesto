// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cifoutput catalog (tasks.md 4.1) on rules.tech and the reference
// technologies, and every DRC style of scmos, sky130A, gf180mcuD and
// ihp-sg13g2 compiled (tasks.md 4.9, 4.11). The PDK cases skip when their
// data set is not installed (scripts/fetch_pdk.py); the Magic printrules
// comparisons are golden cases (DRC-PR-1, DRC-PDK-1..3).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>

#include "hefesto/drc/rules/query.hpp"
#include "hefesto/platform/file_system.hpp"
#include "reference_data.hpp"
#include "rules_fixture.hpp"

namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
namespace fs = std::filesystem;

namespace {

std::shared_ptr<const tech::Technology> load_file(const fs::path& path) {
  hefesto::platform::LocalFileSystem files;
  hefesto::diag::CollectingSink sink;
  auto loaded = tech::load_technology(files, path.string(), tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

/// Compiles every style; checks the table is sane and the load reports no
/// unknown keyword, usage error or type name error.
void compile_every_style(const std::shared_ptr<const tech::Technology>& technology, std::size_t min_styles) {
  const drc::DrcStyleList styles = drc::style_list(*technology);
  CHECK(styles.names.size() >= min_styles);
  for (const std::string& style : styles.names) {
    INFO("style " << style);
    for (const bool compat : {true, false}) {
      const auto c = drc::test::compile_tech(technology, compat, style);
      CHECK(c.rules->style_name() == style);
      if (c.rules->all_rules().empty()) {
        continue;  // e.g. an empty style used to switch DRC off
      }
      CHECK(c.rules->halo().value > 0);
      CHECK(c.rules->step_size() == 16 * c.rules->halo().value);
      for (const auto& d : c.messages) {
        UNSCOPED_INFO(d.code << ": " << d.text);
        CHECK(d.text.find("Bad DRC rule type") == std::string::npos);
        CHECK(d.text.find("usage:") == std::string::npos);
        CHECK(d.text.find("layer (type) name") == std::string::npos);
        CHECK(d.text.find("Bad plane") == std::string::npos);
      }
      if (compat) {
        std::ostringstream out;
        drc::print_rules(*c.rules, *c.tech, out);
        CHECK(out.str().size() > 1000);
      }
      bool sane = true;
      for (const auto& rule : c.rules->all_rules()) {
        sane = sane && rule.check_plane < c.tech->plane_count() && rule.edge_plane < c.tech->plane_count() &&
               rule.why <= c.rules->why().size();
      }
      CHECK(sane);
    }
  }
}

}  // namespace

TEST_CASE("the cifoutput catalog of rules.tech", "[cap:drc-rule-language][req:Halo and step size]") {
  const auto c = drc::test::compile("");
  const drc::CifCatalog& cif = c.cif;
  REQUIRE(cif.styles().size() == 2);
  CHECK(cif.styles()[0].name == "lambda=1(test)");
  CHECK(cif.styles()[1].name == "second");
  REQUIRE(cif.current() != nullptr);
  CHECK(cif.current()->name == "lambda=1(test)");
  CHECK(cif.current()->scale_factor == 100);
  CHECK(cif.current()->layers == std::vector<std::string>{"CWN", "CM1", "CVA", "CCA", "CM2"});
  CHECK(cif.contact_border(c.type("m1c")) == drc::ContactBorder{20, 100});
  CHECK(cif.contact_border(c.type("ndc")) == drc::ContactBorder{5, 100});
  CHECK_FALSE(cif.contact_border(c.type("m1")).has_value());
  CHECK(cif.default_step_size() == 64);
  // `scalefactor 10 nanometers` with squares-grid 0 170 170: the default
  // grid of 1 keeps the expander (CIFTechOutputScale's gcd is 1).
  const drc::CifStyleInfo& second = cif.styles()[1];
  CHECK(second.scale_factor == 10);
  CHECK(second.expander == 10);
  drc::CifCatalog other = cif;
  REQUIRE(other.select("second").ok());
  CHECK(other.contact_border(c.type("m1c")) == std::nullopt);  // border 0
  CHECK(other.default_step_size() == 640);
  CHECK_FALSE(other.select("nosuch").ok());
}

TEST_CASE("the cifoutput catalog rescales with the grid", "[cap:drc-rule-language][req:Scale factor and distance units]") {
  auto c = drc::test::compile("");
  drc::CifCatalog cif = c.cif;
  const auto factor = cif.rescale(1, 2);
  CHECK(factor.first == 2);
  CHECK(cif.current()->scale_factor == 50);
  CHECK(cif.contact_border(c.type("m1c")) == drc::ContactBorder{20, 50});
  // Even sizes and an even scale factor: the scale factor times n.
  drc::CifCatalog odd = c.cif;
  (void)odd.rescale(3, 1);
  CHECK(odd.current()->scale_factor == 300);
  // The expander reduction: scalefactor 10 nanometers with a squares size
  // of 40 reduces by 10 (gcd(10, 40 / 2, ...)).
  const auto t = drc::test::load_tech_text([] {
    std::string text = drc::test::read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "rules.tech");
    const auto start = text.find("cifoutput\n");
    const auto end = text.find("cifinput\n");
    return text.substr(0, start) +
           "cifoutput\nstyle nm\n scalefactor 10 nanometers\n layer V m1c\n  squares 20 40 40\nend\n\n" +
           text.substr(end) + "\ndrc\nend\n\nextract\nend\n";
  }());
  hefesto::diag::CollectingSink sink;
  const auto nm = drc::CifCatalog::from_technology(*t, sink);
  REQUIRE(nm.current() != nullptr);
  CHECK(nm.current()->scale_factor == 1);
  CHECK(nm.current()->expander == 1);
  CHECK(nm.contact_border(*t->find_type("m1c").id) == drc::ContactBorder{2, 1});
}

TEST_CASE("cifstyle binds to the current output style only", "[cap:drc-rule-language][req:Halo and step size]") {
  const auto bound = drc::test::compile(R"(cifstyle lambda=1(test)
cifwidth CM1 300 "w"
cifspacing CM1 CM2 200 touching_illegal "s"
cifarea CM1 1000 500 "a"
cifmaxwidth CM1 900 bend_ok "m")");
  CHECK(bound.messages.empty());
  CHECK(bound.rules->details()->cif_bound);
  CHECK(bound.rules->details()->mask_layer_rules.size() == 4);
  CHECK(bound.rules->halo().value == 9);  // ceil(900 / 100)
  CHECK(bound.rules->why().find("m") == 9);
  // Bound to a style that is not current: rules silently skipped.
  const auto other = drc::test::compile(R"(cifstyle second
cifwidth M1 300 "w")");
  CHECK(other.messages.empty());
  CHECK(other.rules->details()->mask_layer_rules.empty());
  CHECK(other.rules->why().find("w") == 6);  // registered anyway
  // No binding: one warning.
  const auto none = drc::test::compile(R"(cifwidth CM1 300 "w"
cifwidth CM1 400 "w")");
  CHECK(none.count_code("DRC-TECH-001") == 1);
  CHECK(none.said("Missing cif style for drc\n\tThis message will not be repeated."));
  // Messages of the mask-layer statements.
  CHECK(drc::test::compile("cifstyle nosuch").said("Unknown DRC cifstyle nosuch"));
  const auto bad = drc::test::compile(R"(cifstyle lambda=1(test)
cifwidth NOPE 300 "w"
cifspacing CM1 CM2 200 touching_ok "s"
cifspacing CM1 CM1 200 sideways "s"
cifmaxwidth CM1 900 wiggly "m")");
  CHECK(bad.said("Unknown cif layer: NOPE"));
  CHECK(bad.said("Spacing check with touching ok must all be in one plane."));
  CHECK(bad.said("Badly formed drc spacing line"));
  CHECK(bad.said("unknown bend option wiggly"));
  CHECK(drc::test::compile("cifwidth CM1 300").said("Rule type \"cifwidth\" usage: cifwidth layers width why"));
}

TEST_CASE("switching the cifoutput style reloads a DRC style that binds one",
          "[cap:drc-rule-language][req:Style selection at run time]") {
  drc::test::FirstLoad first(drc::test::rules_tech(R"(cifstyle second
cifwidth M1 300 "w")"));
  REQUIRE(first.editor.drc.rules->details() != nullptr);
  CHECK_FALSE(first.editor.drc.rules->details()->cif_bound);
  hefesto::diag::CollectingSink sink;
  REQUIRE(drc::select_cif_style(first.editor, "second", sink).ok());
  CHECK(first.editor.drc.cif.current()->name == "second");
  CHECK(first.editor.drc.rules->details()->cif_bound);
  CHECK(first.editor.drc.rules->details()->mask_layer_rules.size() == 1);
  CHECK(first.editor.drc.rules->halo().value == 30);  // ceil(300 / 10)
  CHECK_FALSE(drc::select_cif_style(first.editor, "nosuch", sink).ok());
}

TEST_CASE("every DRC style of scmos compiles", "[cap:drc-rule-language][req:DRC section grammar]") {
  const fs::path scmos{HEFESTO_SCMOS_TECH};
  std::error_code ec;
  if (!fs::exists(scmos, ec)) {
    SKIP("scmos.tech is generated from build/ref/magic");
  }
  const auto technology = load_file(scmos);
  compile_every_style(technology, 1);
  const auto c = drc::test::compile_tech(technology);
  CHECK_FALSE(c.cif.styles().empty());
}

TEST_CASE("every DRC style of sky130A compiles", "[cap:drc-rule-language][req:DRC styles and variants]") {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("sky130A.tech not in .cache/pdks");
  }
  const auto technology = load_file(*file);
  CHECK(drc::style_list(*technology).names == std::vector<std::string>{"drc(fast)", "drc(full)", "drc(routing)"});
  compile_every_style(technology, 3);
  // The cifoutput styles Magic lists (`cif listall ostyle`); gdsii is current.
  const auto c = drc::test::compile_tech(technology, true, "drc(full)");
  REQUIRE_FALSE(c.cif.styles().empty());
  CHECK(c.cif.current()->name == "gdsii");
  CHECK(c.cif.styles()[1].name == "drc");
  // drc(full) binds `cifstyle drc`, which is not current: mask-layer rules
  // are skipped silently (DRC-CIF-1).
  CHECK(c.rules->details()->cif_style == "drc");
  CHECK_FALSE(c.rules->details()->cif_bound);
  CHECK(c.rules->details()->mask_layer_rules.empty());
}

TEST_CASE("every DRC style of gf180mcuD compiles", "[cap:drc-rule-language][req:DRC styles and variants]") {
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("gf180mcuD.tech not in .cache/pdks");
  }
  const auto technology = load_file(*file);
  compile_every_style(technology, 2);
  const auto c = drc::test::compile_tech(technology);
  REQUIRE(c.cif.current() != nullptr);
  CHECK(c.cif.current()->name == c.cif.styles()[0].name);
}

TEST_CASE("every DRC style of ihp-sg13g2 compiles", "[cap:drc-rule-language][req:DRC styles and variants]") {
  const auto file = hefesto::test::pinned_ihp_sg13g2_tech();
  if (!file) {
    SKIP("ihp-sg13g2.tech not in .cache/pdks");
  }
  const auto technology = load_file(*file);
  compile_every_style(technology, 1);
}
