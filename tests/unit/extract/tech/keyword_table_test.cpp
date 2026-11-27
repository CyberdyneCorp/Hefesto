// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-tech-section through the dispatch table and the lazy style table
// (openspec/changes/m7-extraction tasks.md 4.1, 4.7): keyword matching
// with ambiguous prefixes, variant expansion, per-style parsing on
// selection (ExtLoadStyle), style defaults, and StyleView / HeightView as
// views of ExtStyle. Owner: EXTTECH.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "ext_style_fixture.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/extract/tech/style_view.hpp"

namespace hefesto::test::ext {
namespace {

// Every type of tech_text() but the connected ones, so no style of a test
// leaves a non-electrical type behind.
const std::string kAllTypes = " areacap metal1,metal2,metal3,nwell,pdiff,comment 1\n";

const std::string kLegalKeywords =
    "Illegal keyword.  Legal keywords are:\n\t areacap connect contact cscale defaultareacap defaultoverlap "
    "defaultperimeter defaultsideoverlap defaultsidewall device devresist disconnect fet fetresist "
    "fringeshieldhalo height antenna model tiedown lambda overlap perimc planeorder noplaneordering resist "
    "rscale sidehalo sideoverlap sidewall step style substrate units variants";

const tech::RawSection& extract_section(const tech::Technology& t) {
  const tech::RawSection* found = nullptr;
  for (const tech::RawSection& raw : t.raw_sections()) {
    if (raw.name == "extract") {
      found = &raw;
    }
  }
  REQUIRE(found != nullptr);
  return *found;
}

std::vector<std::string> texts(const std::vector<diag::Diagnostic>& messages) {
  std::vector<std::string> out;
  for (const auto& d : messages) {
    out.push_back(d.text);
  }
  return out;
}

std::vector<std::string> selected_texts(const Loaded& l, std::string_view name) {
  diag::CollectingSink sink;
  REQUIRE(l.table.select(name, sink) != nullptr);
  return texts(sink.take());
}

TEST_CASE("an ambiguous keyword prefix is an illegal keyword; an exact name wins over a longer one",
          "[cap:extract-tech-section][req:Extract section line grammar and keyword matching]") {
  // `side` names sidehalo, sideoverlap and sidewall; `de` the five default*
  // keywords, device and devresist; `fet` is exact although `fetresist`
  // starts with it; the match ignores case.
  const Loaded l = load(" style s\n" + kOrdered + kAllTypes + " side 10\n de metal1 1\n AREAcap metal1 30\n" +
                        " fet nfet ndiff 2 nfet GND! pwell 100 5\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 2);
  for (const auto& d : messages) {
    CHECK(d.code == "EXTRACT-STYLE-020");
    CHECK(d.text == kLegalKeywords);
  }
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  CHECK(s.side_halo == 0);
  CHECK(s.caps.tables.area[type_of(t, "metal1")] == 30.0);
  const auto records = s.records_for(type_of(t, "nfet"));
  REQUIRE(records.size() == 1);
  CHECK(records[0]->device_class == extract::DeviceClass::fet);
  CHECK(records[0]->area_cap == 5.0);
}

TEST_CASE("the illegal-keyword list is keyTable in Magic's order",
          "[cap:extract-tech-section][req:Extract section line grammar and keyword matching]") {
  const Loaded l = load(" style s\n" + kOrdered + kAllTypes + " stepsize 3\n");
  const auto messages = line_messages(l.messages);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].text == kLegalKeywords);
  CHECK(parsed(l).step_size == 100);
}

TEST_CASE("Variant style names: each variant is a style; variants lines gate the lines after them",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const Loaded l = load(" style s variants a,b,c\n" + kOrdered + kAllTypes +
                        " areacap metal1 1\n variants b,c\n areacap metal2 2\n variants *\n areacap metal3 3\n"
                        " variants a\n cscale 7\n style t\n" + kOrdered + kAllTypes + " areacap metal1 9\n");
  REQUIRE(l.table.names().size() == 4);
  CHECK(l.table.names()[0] == "sa");
  CHECK(l.table.names()[1] == "sb");
  CHECK(l.table.names()[2] == "sc");
  CHECK(l.table.names()[3] == "t");
  CHECK(line_messages(l.messages).empty());
  const auto& t = *l.tech;
  const auto m1 = type_of(t, "metal1");
  const auto m2 = type_of(t, "metal2");
  const auto m3 = type_of(t, "metal3");
  const auto sa = l.table.select("sa");
  CHECK(sa->caps.tables.area[m1] == 1.0);
  CHECK(sa->caps.tables.area[m2] == 1.0);  // only kAllTypes' value
  CHECK(sa->caps.tables.area[m3] == 3.0);
  CHECK(sa->cscale == 7.0);
  for (const std::string_view name : {"sb", "sc"}) {
    INFO(name);
    const auto s = l.table.select(name);
    CHECK(s->name == name);
    CHECK(s->caps.tables.area[m2] == 2.0);
    CHECK(s->caps.tables.area[m3] == 3.0);
    CHECK(s->cscale == 1000.0);
  }
  const auto other = l.table.select("t");
  CHECK(other->caps.tables.area[m1] == 9.0);
  CHECK(other->caps.tables.area[m3] == 1.0);
}

TEST_CASE("a style is parsed when it is selected: its line errors are reported then, on every selection",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  // Magic's tech load reads the section for the first style only; the
  // argument count is checked on every line whatever its style, the
  // keyword's own checks only in the style being read (ExtLoadStyle).
  const Loaded l = load(" style a\n" + kOrdered + kAllTypes + " style b\n" + kOrdered + kAllTypes +
                        " cscale x\n perimc metal1 space 20 extra\n");
  const std::string malformed =
      "Malformed line for keyword perimc.  Correct usage:\n\tperimc intypes outtypes capacitance";
  CHECK(texts(line_messages(l.messages)) == std::vector<std::string>{malformed});
  const std::vector<std::string> b{"Cannot parse cap scale value \"x\"", malformed};
  CHECK(selected_texts(l, "b") == b);
  CHECK(selected_texts(l, "b") == b);  // re-read on every selection
  CHECK(selected_texts(l, "a") == std::vector<std::string>{malformed});
  CHECK(l.table.select("b")->cscale == 1.0);
}

TEST_CASE("every style starts from the defaults, whatever an earlier style set",
          "[cap:extract-tech-section][req:Style defaults]") {
  const Loaded l = load(" style a\n" + kOrdered + kAllTypes +
                        " step 50\n sidehalo 3\n cscale 2\n rscale 4\n units microns\n lambda 10\n"
                        " resist metal1 100\n substrate pwell well\n style b\n" + kOrdered + kAllTypes);
  const auto a = l.table.select("a");
  CHECK(a->cscale == 2.0);
  CHECK(a->units_microns);
  CHECK(a->substrate.plane.has_value());
  for (int pass = 0; pass < 2; ++pass) {
    INFO("selection " << pass);
    const auto b = l.table.select("b");
    CHECK(b->step_size == 100);
    CHECK(b->side_halo == 0);
    CHECK(b->cscale == 1000.0);
    CHECK(b->rscale == 1000.0);
    CHECK_FALSE(b->units_microns);
    CHECK(b->units_per_lambda == 100.0F);
    CHECK(b->resist_classes.empty());
    CHECK_FALSE(b->substrate.plane.has_value());
    CHECK(b->corner_chop[type_of(*l.tech, "metal1")] == 1.0F);
    (void)l.table.select("a");
  }
}

TEST_CASE("StyleView of a selected style: the style's gate, tie-down, substrate and active types",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const std::string body = " style a\n" + kOrdered + " device mosfet n nfet ndiff pwell GND!\n tiedown ndiff\n" +
                           " substrate pwell well\n style b\n" + kOrdered +
                           " device mosfet p pfet pdiff nwell Vdd\n tiedown pdiff\n substrate nwell well\n";
  const Loaded l = load(body, false);
  const auto& t = *l.tech;
  const extract::StyleView view = extract::StyleView::from_style(l.table.select("b"));
  REQUIRE(view.has_style());
  CHECK(view.style_name() == "b");
  CHECK(view.gate_types() == types_of(t, "pfet"));
  CHECK(view.diff_types() == types_of(t, "pdiff"));
  CHECK(view.substrate_types() == types_of(t, "nwell"));
  // The same sets as the reader's own view of that style (magic-compat
  // OFF: no mentions carried over from the tech-load style).
  diag::CollectingSink sink;
  const extract::StyleView read = extract::StyleView::from_section(extract_section(t), t, sink, "b");
  CHECK(read.gate_types() == view.gate_types());
  CHECK(read.diff_types() == view.diff_types());
  CHECK(read.substrate_types() == view.substrate_types());
  CHECK(read.active_types() == view.active_types());
  CHECK_FALSE(view.active_types()->test(type_of(t, "comment")));
  CHECK_FALSE(extract::StyleView::from_style(std::shared_ptr<const extract::ExtStyle>{}).has_style());
}

TEST_CASE("HeightView of the session style: micron conversion and rescale take ExtStyle's one path",
          "[cap:extract-tech-section][req:Scaling to internal units]") {
  const Loaded l = load(" style s\n" + kOrdered + kAllTypes + " units microns\n height metal1 1.3761 0.36\n", true,
                        kHalfMicronCif);
  const auto& t = *l.tech;
  compat::CompatSettings compat;
  // The hook's order: micron_convert with the cifoutput scale, then
  // ExtTechScale(1, 2) for two internal units per lambda.
  const extract::HeightView session = extract::HeightView::from_style(finished(l).rescale(1, 2, compat));
  diag::CollectingSink sink;
  const extract::HeightView view = extract::HeightView::from_technology(t, sink).rescaled(1, 2);
  CHECK(sink.take().empty());
  REQUIRE(session.has_style());
  REQUIRE(view.has_style());
  CHECK(session.style()->heights == view.style()->heights);
  CHECK(session.style()->units_per_lambda == view.style()->units_per_lambda);
  CHECK(session.style()->distance_scale == 0.5F);
  CHECK(session.style()->plane_order == view.style()->plane_order);
  const auto m1 = *session.height(type_of(t, "metal1"));
  CHECK(m1.source == extract::HeightSource::height_line);
  float expected = 1.3761F / 0.5F;
  expected *= 2.0F;
  CHECK(m1.height == expected);
  CHECK(session.height(type_of(t, "metal2"))->source == extract::HeightSource::defaulted);
}

}  // namespace
}  // namespace hefesto::test::ext
