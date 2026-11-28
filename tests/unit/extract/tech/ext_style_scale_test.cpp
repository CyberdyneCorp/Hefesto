// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-tech-section, the conversions of ExtStyle (openspec/changes/
// m7-extraction tasks.md 4.6): micron conversion (Q-EXTRACT-3, -19, -21),
// ExtTechScale (Q-EXTRACT-25), the height defaults of the full read, the
// style table, EX-059's `scalegrid 1 2` values, and the three pinned PDKs
// loading with no new diagnostics. Owner: EXTTECH.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "ext_style_fixture.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "reference_data.hpp"

using namespace hefesto::test::ext;  // NOLINT(google-build-using-namespace): test fixture helpers

namespace {

// A copy: callers pass temporaries.
extract::DevicePattern only_record(const extract::ExtStyle& s, tech::TileType type) {
  const auto records = s.records_for(type);
  REQUIRE(records.size() == 1);
  return *records.front();
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path);
  REQUIRE(in.good());
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

compat::CompatSettings compat_mode(bool on) {
  compat::CompatSettings c;
  c.magic_compat = on;
  return c;
}

}  // namespace

// --- Micron conversion --------------------------------------------------------

TEST_CASE("Micron conversion of area cap: dscale squared, per-length values by dscale",
          "[cap:extract-tech-section][req:Micron conversion]") {
  const Loaded l = load(" style s\n units microns\n" + kOrdered +
                            " areacap metal1 40\n perimc metal1 space 8\n overlap metal2 metal1 20\n"
                            " sideoverlap metal2 space metal1 6\n sidewall metal2 space space metal2 30 0.25\n"
                            " height metal1 1.0 0.5\n antenna metal1 surface 1 2 8\n",
                        true, kHalfMicronCif);
  const extract::ExtStyle s = finished(l);
  const auto& t = *l.tech;
  const tech::TileType m1 = type_of(t, "metal1");
  const tech::TileType m2 = type_of(t, "metal2");
  const extract::CapTables& c = s.caps.tables;
  CHECK(c.area[m1] == 40.0 * 0.25);
  CHECK(c.perimeter.at({m1, tech::kSpace}) == 8.0 * 0.5);
  CHECK(c.overlap.at({m2, m1}) == 20.0 * 0.25);
  CHECK(c.side_overlap.at({m2, tech::kSpace})[0].cap == 6.0 * 0.5);
  // Sidewall coefficients are not scaled; offsets are divided by dscale.
  CHECK(c.side_couple.at({m2, tech::kSpace})[0].cap == 15.0);
  CHECK(c.side_couple.at({m2, tech::kSpace})[0].offset == 0.25F / 0.5F);
  CHECK(s.heights[m1].height == 1.0F / 0.5F);
  CHECK(s.heights[m1].thickness == 0.5F / 0.5F);
  CHECK(s.antenna[0].ratio_diff[1] == 8.0 * 0.25);  // the proportional ratio times dscale^2
  CHECK(s.caps.area[0].cap == 10.0);                 // the lists follow
  CHECK(s.converted);
  // A second conversion changes nothing.
  CHECK(s.micron_convert(nullptr) == s);
}

TEST_CASE("Micron halo: (int)(halo / dscale + 0.5) / 1000; the step is converted too",
          "[cap:extract-tech-section][req:Micron conversion]") {
  const Loaded l = load(" style s\n units microns\n" + kOrdered + " sidehalo 2\n step 7\n", true, kHalfMicronCif);
  const extract::ExtStyle s = finished(l);
  CHECK(s.side_halo == 4);
  CHECK(s.step_size == 14);
}

TEST_CASE("Micron conversion of device perimeter cap with magic-compat ON: dscale squared",
          "[cap:extract-tech-section][req:Micron conversion]") {
  const std::string body = " style s\n units microns\n" + kOrdered + " device mosfet nfet nfet ndiff pwell GND! 100 4\n";
  const Loaded l = load(body, true, kHalfMicronCif);
  const extract::DevicePattern& d = only_record(finished(l), type_of(*l.tech, "nfet"));
  CHECK(d.perimeter_cap == 100.0 * 0.25);
  CHECK(d.area_cap == 4.0 * 0.25);
}

TEST_CASE("Micron conversion of device perimeter cap with magic-compat OFF: dscale",
          "[cap:extract-tech-section][req:Micron conversion]") {
  const std::string body = " style s\n units microns\n" + kOrdered + " device mosfet nfet nfet ndiff pwell GND! 100 4\n";
  const Loaded l = load(body, false, kHalfMicronCif);
  const extract::DevicePattern& d = only_record(finished(l), type_of(*l.tech, "nfet"));
  CHECK(d.perimeter_cap == 100.0 * 0.5);
  CHECK(d.area_cap == 4.0 * 0.25);
}

TEST_CASE("micron parameter limits: ON truncates per record of the shared list, OFF keeps milli-units once",
          "[cap:extract-tech-section][req:Micron conversion]") {
  // Two types share the parameter list: Magic converts it once per record.
  const std::string body = " style s\n units microns\n" + kOrdered +
                           " device msubcircuit n nfet,pfet ndiff pwell VSUBS l>0.3 l<=1.25 a<=2 l=l a=a\n";
  const Loaded on = load(body, true, kHalfMicronCif);
  const extract::DevicePattern& d = only_record(finished(on), type_of(*on.tech, "nfet"));
  // Integer limits, both passes run once per record (two here): l min 301
  // -> 602 -> 1204 -> 1 -> 0, l max 1250 -> 2500 -> 5000 -> 5 -> 0, a max
  // 2000 -> 8000 -> 32000 -> 32 -> 0.
  CHECK(d.params[0].minimum == 0);
  CHECK(d.params[0].maximum == 0);
  CHECK(d.params[1].maximum == 0);
  const Loaded off = load(body, false, kHalfMicronCif);
  const extract::DevicePattern& e = only_record(finished(off), type_of(*off.tech, "nfet"));
  CHECK(e.params[0].minimum == 301 / 0.5);
  CHECK(e.params[0].maximum == 1250 / 0.5);
  CHECK(e.params[1].maximum == 2000 / 0.25);
}

TEST_CASE("micron conversion on a 1:2 grid uses the cifoutput scale of the lambda grid (the sky130 rc order)",
          "[cap:extract-tech-section][req:Micron conversion]") {
  // sky130A.magicrc runs `scalegrid 1 2` before `tech load`: the cifoutput
  // style is installed scaled to half-lambda units, but TechLoad (and
  // ExtLoadStyle) convert with the style as read, in microns per lambda.
  const std::string body = " style s\n units microns\n" + kOrdered +
                           " areacap metal1 40\n device msubcircuit n nfet ndiff pwell VSUBS w<0.42 w=w\n";
  for (const bool on : {true, false}) {
    CAPTURE(on);
    const Loaded l = load(body, on, kHalfMicronCif);
    const std::optional<mask::OutputStyle> cif = mask::read_first_output_style(*l.tech);
    REQUIRE(cif.has_value());
    const std::shared_ptr<const mask::OutputStyle> installed = cif->rescale(1, 2, compat_mode(on));
    REQUIRE(extract::HeightView::microns_per_lambda(installed.get()) == 0.25F);
    diag::CollectingSink sink;
    const extract::ExtStyle expected = parsed(l).micron_convert(&*cif);
    const extract::ExtStyle got = parsed(l).micron_convert(installed.get(), 1, 2, compat_mode(on), sink);
    CHECK(got == expected);
    const tech::TileType m1 = type_of(*l.tech, "metal1");
    CHECK(got.caps.tables.area[m1] == 40.0 * 0.25);
    const extract::DevicePattern& d = only_record(got, type_of(*l.tech, "nfet"));
    // w < 0.42: maximum 419 milli-microns, 838 milli-lambda; ON then / 1000.
    const auto limit = std::find_if(d.params.begin(), d.params.end(),
                                    [](const extract::DeviceParam& p) { return p.code == "w" && p.maximum >= 0; });
    REQUIRE(limit != d.params.end());
    CHECK(limit->maximum == (on ? 0.0 : 419 / 0.5));
    // A 1:1 grid passes the style through unchanged.
    CHECK(parsed(l).micron_convert(&*cif, 1, 1, compat_mode(on), sink) == expected);
  }
}

TEST_CASE("noplaneordering skips the whole conversion: halo and limits stay in milli-units",
          "[cap:extract-tech-section][req:Micron conversion]") {
  const Loaded l = load(" style s\n units microns\n noplaneordering\n sidehalo 2\n areacap metal1 40\n", true,
                        kHalfMicronCif);
  const extract::ExtStyle s = finished(l);
  CHECK_FALSE(s.plane_ordering);
  CHECK(s.side_halo == 2000);
  CHECK(s.caps.tables.area[type_of(*l.tech, "metal1")] == 40.0);
  CHECK(s.heights[type_of(*l.tech, "metal2")].height == 0.0F);
}

// --- Layer height and thickness defaults ---------------------------------------

TEST_CASE("Default height in the full read: 0.1 x plane order, thickness 0.05",
          "[cap:extract-tech-section][req:Layer height and thickness defaults]") {
  const Loaded l = load(" style s\n planeorder well 0\n planeorder active 1\n planeorder metal3 2\n"
                        " planeorder metal1 3\n planeorder metal2 4\n height metal2 2 0.5\n");
  const extract::ExtStyle s = finished(l);
  const auto& t = *l.tech;
  CHECK(s.heights[type_of(t, "metal1")].height == static_cast<float>(0.1 * 3));
  CHECK(s.heights[type_of(t, "metal1")].thickness == 0.05F);
  CHECK(s.heights[type_of(t, "metal2")].height == 2.0F);
  CHECK(s.plane_order[plane_of(t, "metal1")] == 3);
}

// --- Scaling to internal units -----------------------------------------------

TEST_CASE("Half-lambda grid: ExtTechScale(1, 2) halves units-per-lambda and the area caps' unit",
          "[cap:extract-tech-section][req:Scaling to internal units]") {
  const Loaded l = load(" style s\n" + kOrdered + " lambda 100\n areacap metal1 30\n perimc metal1 space 20\n sidehalo 4\n"
                        " overlap metal2 metal1 8\n sideoverlap metal2 space metal1 6\n"
                        " sidewall metal2 space space metal2 30 0.25\n height metal1 1 0.5\n");
  const extract::ExtStyle s = finished(l).rescale(1, 2, {});
  const auto& t = *l.tech;
  const tech::TileType m1 = type_of(t, "metal1");
  const tech::TileType m2 = type_of(t, "metal2");
  const extract::CapTables& c = s.caps.tables;
  CHECK(s.units_per_lambda == 50.0F);
  CHECK(c.area[m1] == 7.5);
  CHECK(c.perimeter.at({m1, tech::kSpace}) == 10.0);
  CHECK(s.side_halo == 8);
  CHECK(c.overlap.at({m2, m1}) == 2.0);
  CHECK(c.side_overlap.at({m2, tech::kSpace})[0].cap == 3.0);
  CHECK(c.side_couple.at({m2, tech::kSpace})[0].cap == 15.0);  // never scaled
  CHECK(c.side_couple.at({m2, tech::kSpace})[0].offset == 0.5F);
  CHECK(c.fringe_mult.at({m1, tech::kSpace}) == static_cast<float>(static_cast<double>(30.0F) * 0.02) / 2.0F);
  CHECK(s.heights[m1].height == 2.0F);
  CHECK(s.step_size == 200);
  // The no-op cases.
  CHECK(finished(l).rescale(1, 1, {}) == finished(l));
  CHECK(finished(l).rescale(0, 2, {}) == finished(l));
}

TEST_CASE("rescale: gscap and parameter limits per mode (Q-EXTRACT-3, Q-EXTRACT-21)",
          "[cap:extract-tech-section][req:Scaling to internal units]") {
  const std::string body = " style s\n" + kOrdered +
                           " device msubcircuit n nfet ndiff pwell VSUBS l<=3 a<=5 l=l a=a\n"
                           " device mosfet m pfet pdiff nwell VDD! 12 8\n";
  const Loaded on = load(body, true);
  const extract::ExtStyle a = finished(on).rescale(2, 3, compat_mode(true));
  CHECK(only_record(a, type_of(*on.tech, "nfet")).params[0].maximum == 4);  // 3 * 3 / 2, integer
  CHECK(only_record(a, type_of(*on.tech, "nfet")).params[1].maximum == 11);  // (int)(5 * 9) / 4
  CHECK(only_record(a, type_of(*on.tech, "pfet")).perimeter_cap == 12.0 * 4.0 / 9.0);
  const Loaded off = load(body, false);
  const extract::ExtStyle b = finished(off).rescale(2, 3, compat_mode(false));
  CHECK(only_record(b, type_of(*off.tech, "nfet")).params[0].maximum == 3000.0 * 3 / 2);
  CHECK(only_record(b, type_of(*off.tech, "pfet")).perimeter_cap == 12.0 * 2 / 3);
}

TEST_CASE("rescale by a factor past the int range clamps instead of overflowing",
          "[cap:extract-tech-section][req:Scaling to internal units]") {
  // Magic computes scalen * scalen and the integer limits in int, which
  // overflows for a large `scalegrid` factor (undefined behaviour); both
  // modes take the squares in float and clamp the limits to the int range.
  const std::string body = " style s\n" + kOrdered + " device msubcircuit n nfet ndiff pwell VSUBS l<=3 a<=5 l=l a=a\n";
  const Loaded on = load(body, true);
  const extract::ExtStyle a = finished(on).rescale(1, 1000000000, compat_mode(true));
  constexpr double kIntMax = std::numeric_limits<std::int32_t>::max();
  CHECK(only_record(a, type_of(*on.tech, "nfet")).params[0].maximum == kIntMax);
  CHECK(only_record(a, type_of(*on.tech, "nfet")).params[1].maximum == kIntMax);
}

TEST_CASE("rescale of step and halo: DBScaleValue ON, rounded exact values OFF (Q-EXTRACT-19)",
          "[cap:extract-tech-section][req:Scaling to internal units]") {
  const std::string body = " style s\n" + kOrdered + " sidehalo 2.5\n step 7.9\n";
  const extract::ExtStyle on = finished(load(body, true)).rescale(1, 3, compat_mode(true));
  CHECK(on.side_halo == 6);  // 2 * 3
  CHECK(on.step_size == 21);
  const extract::ExtStyle off = finished(load(body, false)).rescale(1, 3, compat_mode(false));
  CHECK(off.side_halo == 8);   // 2.5 * 3 = 7.5, rounded
  CHECK(off.step_size == 24);  // 7.9 * 3 = 23.7
  CHECK(extract::scale_distance(7, 2, 1) == 3);
  CHECK(extract::scale_distance(-7, 2, 1) == -4);
  CHECK(extract::scale_distance(4, 1, 2) == 8);
}

TEST_CASE("Q-EXTRACT-25: a style reloaded from the table has the tech values again",
          "[cap:extract-tech-section][req:Scaling to internal units]") {
  const Loaded l = load(" style s\n" + kOrdered + " sidehalo 4\n");
  const extract::ExtStyle scaled = finished(l).rescale(1, 2, {});
  CHECK(scaled.side_halo == 8);
  CHECK(finished(l).side_halo == 4);  // the table keeps the parsed style
}

TEST_CASE("scalegrid 1 2 gives EX-059's values on exttest.tech",
          "[cap:extract-tech-section][req:Scaling to internal units]") {
  const auto t = load_tech(read_file(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "ex059_exttest.tech"));
  const Loaded l = load_full(t);
  REQUIRE(l.messages.size() == 1);  // the non-electrical list, as Magic prints it
  CHECK(l.messages[0].text.ends_with("    nwell dnwell pdiff rpoly metal3 mimcap thkox "));
  const extract::ExtStyle s = finished(l).rescale(1, 2, {});
  const tech::TileType m1 = type_of(*t, "metal1");
  // `scale 1 1 50` (units per lambda 50), `extract halo` 8.
  CHECK(s.units_per_lambda == 50.0F);
  CHECK(s.side_halo == 8);
  CHECK(s.cscale == 1.0);
  CHECK(s.rscale == 1.0);
  // ex.mag's metal1 rect 0 0 10 4 is 20 x 8 internal units: 1760 aF.
  const double cap = s.caps.tables.area[m1] * (20 * 8) + s.caps.tables.perimeter.at({m1, tech::kSpace}) * (2 * (20 + 8));
  CHECK(cap == 1760.0);
}

// --- The style table ---------------------------------------------------------

TEST_CASE("load_styles: names in declaration order, variants expanded, each style parsed",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const Loaded l = load(" style ngspice variants (),(si)\n variants (si)\n areacap metal1 25\n variants ()\n"
                        " areacap metal1 30\n style other\n areacap metal1 1\n");
  REQUIRE(l.table.names().size() == 3);
  CHECK(l.table.names()[0] == "ngspice()");
  CHECK(l.table.names()[1] == "ngspice(si)");
  CHECK(l.table.names()[2] == "other");
  const auto& t = *l.tech;
  CHECK(l.table.select("ngspice()")->caps.tables.area[type_of(t, "metal1")] == 30.0);
  CHECK(l.table.select("ngspice(si)")->caps.tables.area[type_of(t, "metal1")] == 25.0);
  CHECK(l.table.select("other")->name == "other");
  CHECK(l.table.select("nosuch") == nullptr);
  CHECK(extract::StyleTable{}.select("other") == nullptr);
}

TEST_CASE("a section without a style line gives the style default",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const Loaded l = load("");
  REQUIRE(l.table.names().size() == 1);
  CHECK(l.table.names()[0] == "default");
  CHECK(l.table.select("default") != nullptr);
}

TEST_CASE("select with a sink replays the style's messages as ExtLoadStyle does",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const Loaded l = load(" style a\n" + kOrdered + " areacap metal1,metal2,metal3,nwell,pdiff,comment 1\n style b\n" +
                        kOrdered + " cscale x\n areacap metal1,metal2,metal3,nwell,pdiff,comment 1\n");
  CHECK(l.messages.empty());  // the tech load reads a only
  diag::CollectingSink sink;
  REQUIRE(l.table.select("b", sink) != nullptr);
  const auto replayed = sink.take();
  REQUIRE(replayed.size() == 1);
  CHECK(replayed[0].text == "Cannot parse cap scale value \"x\"");
}

// --- The three pinned PDKs -----------------------------------------------------

namespace {

// Loads a PDK technology and its extract styles; only the informative
// messages Magic prints at tech load (non-electrical list, contact sizes)
// may come out.
Loaded load_pdk(const std::optional<std::filesystem::path>& file) {
  platform::LocalFileSystem files;
  diag::CollectingSink load_sink;
  auto loaded = tech::load_technology(files, file->string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  Loaded l = load_full(loaded.value());
  for (const auto& d : l.messages) {
    UNSCOPED_INFO(d.code << ": " << d.text);
    CHECK((d.code == "EXTRACT-STYLE-018" || d.code == "EXTRACT-STYLE-017"));
  }
  return l;
}

void check_every_style(const Loaded& l) {
  for (const std::string& name : l.table.names()) {
    INFO(name);
    diag::CollectingSink sink;
    const auto style = l.table.select(name, sink);
    REQUIRE(style != nullptr);
    for (const auto& d : sink.take()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
      CHECK((d.code == "EXTRACT-STYLE-018" || d.code == "EXTRACT-STYLE-017"));
    }
    const std::optional<mask::OutputStyle> cif = mask::read_first_output_style(*l.tech);
    const extract::ExtStyle converted = style->micron_convert(cif ? &*cif : nullptr);
    CHECK(converted.step_size > 0);
  }
}

}  // namespace

TEST_CASE("sky130A loads its extract styles with no new diagnostics; the photodiode line (Q-EXTRACT-2)",
          "[cap:extract-tech-section][req:Device declaration parsing]") {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  const Loaded l = load_pdk(file);
  REQUIRE(l.messages.size() == 1);
  CHECK(l.messages[0].text.ends_with("\n    ubm "));
  CHECK(l.table.names().front() == "ngspice()");
  const extract::ExtStyle s = parsed(l);
  const auto& t = *l.tech;
  CHECK(s.units_microns);
  CHECK(s.step_size == 7);
  CHECK(s.side_halo == 8000);
  CHECK(s.cscale == 1.0);
  CHECK(s.antenna_model == extract::kAntennaPartial);
  REQUIRE(s.substrate.plane.has_value());
  CHECK(s.substrate.node_name == "$SUB");
  // device ndiode ..._diode_ps2dn photo pwell,space/w error a=area
  const auto photo = s.records_for(type_of(t, "photo"));
  REQUIRE_FALSE(photo.empty());
  const extract::DevicePattern* diode = nullptr;
  for (const auto* d : photo) {
    if (d->device_class == extract::DeviceClass::ndiode) {
      diode = d;
    }
  }
  REQUIRE(diode != nullptr);
  CHECK(diode->model == "sky130_fd_pr__model__parasitic__diode_ps2dn");
  REQUIRE(diode->terminals.size() == 1);
  CHECK(diode->terminals[0] == types_of(t, "pwell,space/w"));
  CHECK(diode->substrate_types == diode->terminals[0]);
  CHECK(diode->substrate_node == "error");
  REQUIRE(diode->params.size() == 1);
  CHECK(diode->params[0].code == "a");
  CHECK_FALSE(s.device_info().empty());
  check_every_style(l);
}

TEST_CASE("gf180mcuD loads its extract styles with no new diagnostics",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const auto file = hefesto::test::pinned_gf180mcud_tech();
  if (!file) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  const Loaded l = load_pdk(file);
  CHECK(l.table.names().front() == "ngspice()");
  CHECK_FALSE(parsed(l).device_info().empty());
  check_every_style(l);
}

TEST_CASE("ihp-sg13g2 loads its extract styles with no new diagnostics",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const auto file = hefesto::test::pinned_ihp_sg13g2_tech();
  if (!file) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  const Loaded l = load_pdk(file);
  CHECK(l.table.names().front() == "ngspice()");
  CHECK_FALSE(parsed(l).device_info().empty());
  check_every_style(l);
}
