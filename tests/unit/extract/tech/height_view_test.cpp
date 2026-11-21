// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-tech-section, the subset behind extract::HeightView (M10 design
// D5, tasks 3.1 and 3.2): `lambda`, `units`, `planeorder`,
// `noplaneordering`, `height`, the end-of-style plane-order check, the
// height and thickness defaults, the micron conversion of h and t and
// ExtTechScale's rescale, on hand-written sections and the three pinned
// PDK technologies. Owner: TECH (M10).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace extract = hefesto::extract;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;
namespace mask = hefesto::mask;
namespace platform = hefesto::platform;

namespace {

// Five technology planes: well, active, metal1, metal2, metal3.
std::string tech_text(const std::string& extract_body, const std::string& cifoutput_body = "") {
  return "tech\n format 35\n heights\nend\n"
         "planes\n well,w\n active,a\n metal1,m1\n metal2,m2\n metal3,m3\nend\n"
         "types\n well nwell\n active ndiffusion,ndiff\n active polysilicon,poly\n metal1 metal1,m1\n"
         " metal2 metal2,m2\n metal3 metal3,m3\n metal1 via1\nend\n"
         "contact\n via1 metal1 metal2\nend\n"
         "styles\n styletype mos\nend\n"
         "compose\nend\n"
         "connect\n metal1 via1\n metal2 via1\nend\n"
         "cifoutput\n" +
         cifoutput_body + "end\ncifinput\nend\ndrc\nend\nextract\n" + extract_body + "end\n";
}

std::shared_ptr<const tech::Technology> load(const std::string& extract_body, const std::string& cifoutput_body = "") {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(tech_text(extract_body, cifoutput_body), "/t/heights.tech", fs,
                                           tech::LoadOptions{}, sink);
  for (const auto& d : sink.snapshot()) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

tech::TileType type_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

tech::PlaneId plane_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_plane(name);
  REQUIRE(found.ok());
  return *found.id;
}

struct Viewed {
  extract::HeightView view;
  std::vector<diag::Diagnostic> messages;
};

Viewed view(const tech::Technology& t, std::optional<std::string_view> style = std::nullopt,
            std::optional<float> dscale = std::nullopt) {
  diag::CollectingSink sink;
  Viewed v;
  const tech::RawSection& section = t.raw_sections().back();
  if (dscale) {
    v.view = extract::HeightView::from_section(section, t, sink, style, *dscale);
  } else if (style) {
    v.view = extract::HeightView::from_section(section, t, sink, *style);
  } else {
    v.view = extract::HeightView::from_technology(t, sink);
  }
  v.messages = sink.take();
  return v;
}

extract::TypeHeight height(const Viewed& v, const tech::Technology& t, std::string_view type) {
  const auto h = v.view.height(type_of(t, type));
  REQUIRE(h.has_value());
  return *h;
}

// Every tech plane ordered in declaration order.
const std::string kOrdered =
    " planeorder well 0\n planeorder active 1\n planeorder metal1 2\n planeorder metal2 3\n planeorder metal3 4\n";

}  // namespace

TEST_CASE("without an extract section there is no style, so h = t = 0 in the 3D view",
          "[cap:extract-tech-section][req:Layer height and thickness defaults]") {
  const extract::HeightView none;
  CHECK_FALSE(none.has_style());
  CHECK_FALSE(none.height(10).has_value());
  CHECK(none.style() == nullptr);
  CHECK_FALSE(none.rescaled(1, 2).has_style());
}

TEST_CASE("Default height: metal1 on a plane of order 3 gets h = 0.3, t = 0.05",
          "[cap:extract-tech-section][req:Layer height and thickness defaults]") {
  const auto t = load(" style s\n planeorder well 0\n planeorder active 1\n planeorder metal3 2\n"
                      " planeorder metal1 3\n planeorder metal2 4\n");
  const Viewed v = view(*t);
  CHECK(v.messages.empty());
  REQUIRE(v.view.has_style());
  CHECK(v.view.style_name() == "s");
  const extract::TypeHeight m1 = height(v, *t, "metal1");
  CHECK(m1.height == static_cast<float>(0.1 * 3));
  CHECK(m1.thickness == 0.05F);
  CHECK(m1.source == extract::HeightSource::defaulted);
  CHECK(height(v, *t, "nwell").height == 0.0F);  // order 0
  CHECK(height(v, *t, "nwell").thickness == 0.05F);
  CHECK(height(v, *t, "via1").height == static_cast<float>(0.1 * 3));  // home plane metal1
  // Built-in types keep 0.
  CHECK(v.view.height(tech::kSpace)->height == 0.0F);
  CHECK(v.view.height(tech::kSpace)->thickness == 0.0F);
}

TEST_CASE("height lines set h and t as floats; a zero value takes the default",
          "[cap:extract-tech-section][req:Layer height and thickness defaults]") {
  const auto t = load(" style s\n" + kOrdered +
                      " height metal1 1.3761 0.36\n height metal2 0 0.5\n height metal3 2.5 0\n");
  const Viewed v = view(*t);
  CHECK(v.messages.empty());
  const extract::TypeHeight m1 = height(v, *t, "metal1");
  CHECK(m1.height == 1.3761F);
  CHECK(m1.thickness == 0.36F);
  CHECK(m1.source == extract::HeightSource::height_line);
  const extract::TypeHeight m2 = height(v, *t, "metal2");
  CHECK(m2.height == static_cast<float>(0.1 * 3));
  CHECK(m2.thickness == 0.5F);
  CHECK(m2.source == extract::HeightSource::defaulted);
  const extract::TypeHeight m3 = height(v, *t, "metal3");
  CHECK(m3.height == 2.5F);
  CHECK(m3.thickness == 0.05F);
}

TEST_CASE("a non-numeric height or thickness is reported and the line has no effect",
          "[cap:extract-tech-section][req:Layer height and thickness defaults]") {
  const auto t = load(" style s\n" + kOrdered + " height metal1 high 0.36\n height metal2 1.0 thick\n");
  const Viewed v = view(*t);
  REQUIRE(v.messages.size() == 2);
  CHECK(v.messages[0].code == "EXTRACT-STYLE-003");
  CHECK(v.messages[0].text == "Layer height high must be numeric");
  CHECK(v.messages[1].text == "Layer thickness thick must be numeric");
  CHECK(height(v, *t, "metal1").height == static_cast<float>(0.1 * 2));
  CHECK(height(v, *t, "metal2").height == static_cast<float>(0.1 * 3));
}

TEST_CASE("Missing plane order falls back to default", "[cap:extract-tech-section][req:Plane ordering]") {
  const auto t = load(" style s\n planeorder active 1\n");
  const Viewed v = view(*t);
  REQUIRE(v.messages.size() == 1);
  CHECK(v.messages[0].code == "EXTRACT-STYLE-006");
  CHECK(v.messages[0].text ==
        "Warning: Extraction Style s\n"
        "  Missing planeorder for plane(s):\n    well, metal1, metal2, metal3,\n"
        "  Magic will use the default planeorder for this style:\n"
        "    well=0, active=1, metal1=2, metal2=3, metal3=4,");
  const extract::StyleHeights* s = v.view.style();
  REQUIRE(s != nullptr);
  CHECK(s->plane_order[plane_of(*t, "well")] == 0);
  CHECK(s->plane_order[plane_of(*t, "metal3")] == 4);
  CHECK(height(v, *t, "metal2").height == static_cast<float>(0.1 * 3));
}

TEST_CASE("conflicting plane orders are reported with the missing ones", "[cap:extract-tech-section][req:Plane ordering]") {
  const auto t = load(" style s\n planeorder well 0\n planeorder active 0\n planeorder metal1 2\n"
                      " planeorder metal2 3\n planeorder metal3 4\n");
  const Viewed v = view(*t);
  REQUIRE(v.messages.size() == 1);
  CHECK(v.messages[0].text ==
        "Warning: Extraction Style s\n"
        "  Conflicting planeorder for plane(s):\n    well,\n"
        "  Missing planeorder for plane(s):\n    active,\n"
        "  Magic will use the default planeorder for this style:\n"
        "    well=0, active=1, metal1=2, metal2=3, metal3=4,");
  CHECK(v.view.style()->plane_order[plane_of(*t, "active")] == 1);
}

TEST_CASE("planeorder errors: unknown plane, index out of range, the order with noplaneordering",
          "[cap:extract-tech-section][req:Plane ordering]") {
  const auto t = load(" style s\n" + kOrdered + " planeorder nosuch 1\n planeorder metal1 5\n noplaneordering\n");
  const Viewed v = view(*t);
  REQUIRE(v.messages.size() == 3);
  CHECK(v.messages[0].code == "EXTRACT-STYLE-005");
  CHECK(v.messages[0].text == "Unknown planeorder plane nosuch");
  CHECK(v.messages[1].text == "Planeorder index must be [0..4]");
  CHECK(v.messages[2].text == "\"noplaneordering\" specified after \"planeorder\".");
  CHECK(v.view.style()->plane_ordering);
  CHECK(v.view.style()->plane_order[plane_of(*t, "metal1")] == 2);

  // A planeorder after noplaneordering is reported and still applies.
  const auto t2 = load(" style s\n noplaneordering\n" + kOrdered);
  const Viewed v2 = view(*t2);
  REQUIRE(v2.messages.size() == 1);
  CHECK(v2.messages[0].text == "\"planeorder\" specified after \"noplaneordering\".");
  CHECK(v2.view.style()->plane_ordering);
  CHECK(height(v2, *t2, "metal3").height == static_cast<float>(0.1 * 4));
}

TEST_CASE("noplaneordering skips the check, the defaults and the conversion: a type without height keeps 0",
          "[cap:extract-tech-section][req:Plane ordering]") {
  // Regression (PR #77 review): extTechFinalStyle returns before zinit and
  // doConvert with noPlaneOrder (ExtTech.c:3801-3802), so metal2 keeps
  // h = t = 0, not h = -0.1 and t = 0.05.
  const auto t = load(" style s\n noplaneordering\n planeorder metal1 2\n");
  // The planeorder after noplaneordering re-enables the check, so test the
  // plain form too.
  const auto plain = load(" style s\n noplaneordering\n height metal1 1 0.5\n");
  const Viewed v = view(*plain);
  CHECK(v.messages.empty());
  CHECK_FALSE(v.view.style()->plane_ordering);
  CHECK(v.view.style()->plane_order[plane_of(*plain, "metal2")] == extract::kNoPlaneOrder);
  CHECK(height(v, *plain, "metal2").height == 0.0F);
  CHECK(height(v, *plain, "metal2").thickness == 0.0F);
  CHECK(height(v, *plain, "metal2").source == extract::HeightSource::defaulted);
  CHECK(height(v, *plain, "metal1").height == 1.0F);
  CHECK(height(v, *plain, "metal1").thickness == 0.5F);
  CHECK(height(v, *plain, "metal1").source == extract::HeightSource::height_line);
  CHECK(view(*t).messages.size() == 2);  // the order error and the fallback warning
}

TEST_CASE("noplaneordering with units microns leaves the height values in microns",
          "[cap:extract-tech-section][req:Plane ordering]") {
  // Regression (PR #77 review): the micron conversion is skipped too.
  const auto t = load(" style s\n units microns\n noplaneordering\n height metal1 1.0 0.5\n");
  const Viewed v = view(*t, std::nullopt, 0.005F);
  CHECK(v.messages.empty());
  CHECK(v.view.style()->units_microns);
  CHECK(v.view.style()->distance_scale == 1.0F);
  CHECK(height(v, *t, "metal1").height == 1.0F);
  CHECK(height(v, *t, "metal1").thickness == 0.5F);
  CHECK(height(v, *t, "metal2").height == 0.0F);
  CHECK(height(v, *t, "metal2").thickness == 0.0F);
}

TEST_CASE("units microns divides h and t by dscale; lambda and other units",
          "[cap:extract-tech-section][req:Micron conversion]") {
  const auto t = load(" style s\n units microns\n" + kOrdered + " height metal1 1.0 0.5\n");
  const Viewed v = view(*t, std::nullopt, 0.005F);
  CHECK(v.messages.empty());
  REQUIRE(v.view.has_style());
  CHECK(v.view.style()->units_microns);
  CHECK(v.view.style()->distance_scale == 0.005F);
  CHECK(height(v, *t, "metal1").height == 1.0F / 0.005F);
  CHECK(height(v, *t, "metal1").thickness == 0.5F / 0.005F);
  // The defaults are converted too (they are applied before the conversion).
  CHECK(height(v, *t, "metal2").height == static_cast<float>(0.1 * 3) / 0.005F);
  CHECK(height(v, *t, "metal2").thickness == 0.05F / 0.005F);

  const auto um = load(" style s\n units um\n" + kOrdered + " height metal1 1.0 0.5\n");
  CHECK(height(view(*um, std::nullopt, 0.5F), *um, "metal1").height == 2.0F);

  const auto bad = load(" style s\n units furlongs\n" + kOrdered + " height metal1 1.0 0.5\n");
  const Viewed b = view(*bad, std::nullopt, 0.5F);
  REQUIRE(b.messages.size() == 1);
  CHECK(b.messages[0].code == "EXTRACT-STYLE-004");
  CHECK(b.messages[0].text == "Units must be microns or lambda.  Using the default value (lambda).");
  CHECK_FALSE(b.view.style()->units_microns);
  CHECK(height(b, *bad, "metal1").height == 1.0F);
}

TEST_CASE("dscale is CIFGetOutputScale(1000) of the current output style at load",
          "[cap:extract-tech-section][req:Micron conversion]") {
  CHECK(extract::HeightView::microns_per_lambda(nullptr) == 1.0F);
  mask::OutputStyle style;
  style.scale.scale_factor = 10;
  style.scale.expander = 10;
  CHECK(extract::HeightView::microns_per_lambda(&style) == static_cast<float>(100) / static_cast<float>(10000));

  const auto t = load(" style s\n units microns\n" + kOrdered + " height metal1 1.0 0.5\n",
                      "style out\n scalefactor 50\n layer M1 m1\n calma 1 0\n");
  const std::optional<mask::OutputStyle> first = mask::read_first_output_style(*t);
  REQUIRE(first.has_value());
  const float dscale = extract::HeightView::microns_per_lambda(&*first);
  const Viewed v = view(*t);
  CHECK(v.view.style()->distance_scale == dscale);
  CHECK(height(v, *t, "metal1").height == 1.0F / dscale);
  // Without a cifoutput style dscale is 1.
  const auto none = load(" style s\n units microns\n" + kOrdered + " height metal1 1.0 0.5\n");
  CHECK(height(view(*none), *none, "metal1").height == 1.0F);
}

TEST_CASE("lambda sets units-per-lambda as a float", "[cap:extract-tech-section][req:Units, lambda, cscale and rscale]") {
  const auto t = load(" style s\n" + kOrdered + " lambda 2.5\n");
  CHECK(view(*t).view.style()->units_per_lambda == 2.5F);
  const auto d = load(" style s\n" + kOrdered);
  CHECK(view(*d).view.style()->units_per_lambda == 100.0F);
}

TEST_CASE("the height subset follows style and variant selection",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const auto t = load(" style s variants (a),(b)\n" + kOrdered +
                      " variants (a)\n height metal1 1.0 0.1\n"
                      " variants (b)\n height metal1 2.0 0.2\n"
                      " style other\n" + kOrdered + " height metal1 3.0 0.3\n");
  CHECK(height(view(*t), *t, "metal1").height == 1.0F);
  CHECK(height(view(*t, "s(b)"), *t, "metal1").height == 2.0F);
  const Viewed other = view(*t, "other");
  CHECK(other.view.style_name() == "other");
  CHECK(height(other, *t, "metal1").height == 3.0F);
  CHECK_FALSE(view(*t, "nosuch").view.has_style());
  // A bad line of an unselected style reports nothing.
  const auto quiet = load(" style s\n" + kOrdered + " style other\n height metal1 x 1\n");
  CHECK(view(*quiet).messages.empty());
}

TEST_CASE("StyleView and HeightView read one pass and report disjoint messages",
          "[cap:extract-tech-section][req:Extract section line grammar and keyword matching]") {
  const auto t = load(" style s\n" + kOrdered + " device nosuchclass x poly\n height metal1 x 1\n units feet\n");
  diag::CollectingSink types_sink;
  const auto types = extract::StyleView::from_technology(*t, types_sink);
  REQUIRE(types.has_style());
  const auto type_messages = types_sink.take();
  REQUIRE(type_messages.size() == 1);
  CHECK(type_messages[0].code == "EXTRACT-STYLE-002");

  const Viewed v = view(*t);
  REQUIRE(v.messages.size() == 2);
  CHECK(v.messages[0].code == "EXTRACT-STYLE-003");
  CHECK(v.messages[1].code == "EXTRACT-STYLE-004");
}

TEST_CASE("Half-lambda grid: h and t are lengths, rescaled as ExtTechScale does",
          "[cap:extract-tech-section][req:Scaling to internal units]") {
  const auto t = load(" style s\n" + kOrdered + " lambda 100\n height metal1 1.3761 0.36\n");
  const Viewed v = view(*t);
  // Two internal units per lambda: ExtTechScale(1, 2).
  const extract::HeightView fine = v.view.rescaled(1, 2);
  const auto m1 = *fine.height(type_of(*t, "metal1"));
  CHECK(m1.height == 1.3761F * 2.0F);
  CHECK(m1.thickness == 0.36F * 2.0F);
  CHECK(m1.source == extract::HeightSource::height_line);
  CHECK(fine.style()->units_per_lambda == 50.0F);
  // Magic's float steps: h *= d, then h /= n.
  const extract::HeightView odd = v.view.rescaled(3, 7);
  float expected = 1.3761F;
  expected *= 7.0F;
  expected /= 3.0F;
  CHECK(odd.height(type_of(*t, "metal1"))->height == expected);
  // Back to the coarser grid and the no-op cases.
  CHECK(fine.rescaled(2, 1).height(type_of(*t, "metal1"))->height == 1.3761F);
  CHECK(v.view.rescaled(1, 1).style()->heights == v.view.style()->heights);
  CHECK(v.view.rescaled(0, 2).style()->heights == v.view.style()->heights);
}

namespace {

struct PdkHeights {
  std::string_view type;
  float height_um;
  float thickness_um;
};

void check_pdk(const std::optional<std::filesystem::path>& file, std::string_view style,
               const std::vector<PdkHeights>& expected) {
  if (!file) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  platform::LocalFileSystem files;
  diag::CollectingSink load_sink;
  auto loaded = tech::load_technology(files, file->string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const tech::Technology& t = *loaded.value();
  const Viewed v = view(t);
  for (const auto& d : v.messages) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  CHECK(v.messages.empty());  // no new diagnostics
  REQUIRE(v.view.has_style());
  CHECK(v.view.style_name() == style);
  CHECK(v.view.style()->units_microns);
  CHECK(v.view.style()->plane_ordering);
  const std::optional<mask::OutputStyle> first = mask::read_first_output_style(t);
  const float dscale = extract::HeightView::microns_per_lambda(first ? &*first : nullptr);
  CHECK(v.view.style()->distance_scale == dscale);
  for (const PdkHeights& e : expected) {
    INFO(e.type);
    const extract::TypeHeight h = height(v, t, e.type);
    CHECK(h.height == e.height_um / dscale);
    CHECK(h.thickness == e.thickness_um / dscale);
  }
}

}  // namespace

TEST_CASE("the sky130A extract style gives its heights in internal units with no new diagnostics",
          "[cap:extract-tech-section][req:Layer height and thickness defaults]") {
  check_pdk(hefesto::test::pinned_sky130a_tech(), "ngspice()",
            {{"metal1", 1.3761F, 0.36F}, {"ndiff", 0.2062F, 0.12F}, {"dnwell", -0.1F, 0.1F}});
}

TEST_CASE("the gf180mcuD extract style gives its heights in internal units with no new diagnostics",
          "[cap:extract-tech-section][req:Layer height and thickness defaults]") {
  check_pdk(hefesto::test::pinned_gf180mcud_tech(), "ngspice()", {{"metal1", 1.23F, 0.55F}});
}

TEST_CASE("the ihp-sg13g2 extract style gives default heights with no new diagnostics",
          "[cap:extract-tech-section][req:Layer height and thickness defaults]") {
  // No `height` lines: metal1's plane has order 4.
  check_pdk(hefesto::test::pinned_ihp_sg13g2_tech(), "ngspice()",
            {{"metal1", static_cast<float>(0.1 * 4), 0.05F}});
}
