// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-tech-section, the subset behind extract::StyleView (Open
// Question 2, decided (a); design D10): style and variant selection, the
// gate types of `device`/`fet` lines, `tiedown`, `substrate`, and the
// active types left by "Non-electrical types", on hand-written sections,
// the smoke technology of tests/data/extflat and the three pinned PDK
// technologies. Owner: EXTSUB.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/extract/tech/style_view.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace extract = hefesto::extract;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;

namespace {

// The smoke technology's sections with `extract_body` as its extract
// section (none when nullopt).
std::string tech_text(const std::optional<std::string>& extract_body) {
  std::string text =
      "tech\n format 35\n mini\nend\n"
      "planes\n active,a\n metal1,m1\n metal2,m2\nend\n"
      "types\n active ndiffusion,ndiff\n active pdiffusion,pdiff\n active polysilicon,poly\n"
      " active ntransistor,nfet\n active ptransistor,pfet\n active nwell\n metal1 metal1,m1\n"
      " metal2 metal2,m2\n active pdcontact,pdc\n metal1 via1\n active marker\nend\n"
      "contact\n pdc pdiff metal1\n via1 metal1 metal2\nend\n"
      "styles\n styletype mos\nend\n"
      "compose\n compose nfet poly ndiff\nend\n"
      "connect\n metal1 via1\n metal2 via1\nend\n"
      "cifoutput\nend\ncifinput\nend\ndrc\nend\n";
  if (extract_body) {
    text += "extract\n" + *extract_body + "end\n";
  }
  return text;
}

std::shared_ptr<const tech::Technology> load(const std::optional<std::string>& extract_body) {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(tech_text(extract_body), "/t/mini.tech", fs, tech::LoadOptions{}, sink);
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

tech::TypeMask mask_of(const tech::Technology& t, std::initializer_list<std::string_view> names) {
  tech::TypeMask mask;
  for (const auto name : names) {
    mask.set(type_of(t, name));
  }
  return mask;
}

struct Viewed {
  extract::StyleView view;
  std::vector<diag::Diagnostic> messages;
};

Viewed view(const tech::Technology& t, std::optional<std::string_view> style = std::nullopt) {
  diag::CollectingSink sink;
  Viewed v;
  v.view = style ? extract::StyleView::from_section(t.raw_sections().back(), t, sink, *style)
                 : extract::StyleView::from_technology(t, sink);
  v.messages = sink.take();
  return v;
}

}  // namespace

TEST_CASE("without a style every query is nullopt (Q-LEFDEF-40: empty sets)", "[cap:extract-tech-section][req:Style defaults]") {
  const extract::StyleView none;
  CHECK_FALSE(none.has_style());
  CHECK_FALSE(none.style_name().has_value());
  CHECK_FALSE(none.gate_types().has_value());
  CHECK_FALSE(none.diff_types().has_value());
  CHECK_FALSE(none.active_types().has_value());
  CHECK_FALSE(none.substrate_types().has_value());
  // The built-in technology has an empty extract section, which gives
  // Magic's "default" style with empty device, tie-down and substrate sets.
  const tech::Technology builtin;
  const Viewed v = view(builtin);
  REQUIRE(v.view.has_style());
  CHECK(v.view.style_name() == "default");
  CHECK(v.view.gate_types()->empty());
  CHECK(v.view.diff_types()->empty());
}

TEST_CASE("an extract section without a style line gives the default style", "[cap:extract-tech-section][req:Style defaults]") {
  const auto t = load(std::string{" tiedown ndiff\n areacap m1 10\n"});  // before any style: ignored
  const Viewed v = view(*t);
  REQUIRE(v.view.has_style());
  CHECK(v.view.style_name() == "default");
  CHECK(v.view.diff_types()->empty());
  CHECK(v.view.gate_types()->empty());
  CHECK(v.view.substrate_types()->empty());
  // Only the types that connect to another type stay active.
  const auto active = *v.view.active_types();
  CHECK(active.test(type_of(*t, "m1")));
  CHECK(active.test(type_of(*t, "via1")));
  CHECK_FALSE(active.test(type_of(*t, "ndiff")));
  CHECK_FALSE(active.test(tech::kSpace));
  CHECK_FALSE(active.test(tech::kErrorP));
  CHECK(active.test(tech::kMagnet));
}

TEST_CASE("the first declared style loads, variants gate their lines", "[cap:extract-tech-section][req:Extraction styles and variants]") {
  const auto t = load(std::string{
      " style s variants (a),(b)\n"
      " variants (a)\n tiedown ndiff\n"
      " variants (b)\n tiedown pdiff\n"
      " variants *\n tiedown poly\n"
      " style other\n tiedown nwell\n"});
  const Viewed first = view(*t);
  REQUIRE(first.view.has_style());
  CHECK(first.view.style_name() == "s(a)");
  CHECK(*first.view.diff_types() == mask_of(*t, {"ndiff", "poly"}));

  const Viewed b = view(*t, "s(b)");
  CHECK(*b.view.diff_types() == mask_of(*t, {"pdiff", "poly"}));
  const Viewed other = view(*t, "other");
  CHECK(*other.view.diff_types() == mask_of(*t, {"nwell"}));
  CHECK_FALSE(view(*t, "s(c)").view.has_style());
  CHECK(first.messages.empty());
}

TEST_CASE("mosfet, msubcircuit and fet lines give the gate types", "[cap:extract-tech-section][req:Device declaration parsing]") {
  const auto t = load(std::string{
      " style s\n"
      " device mosfet nmos nfet ndiff pwell VSUB l=l w=w\n"
      " device msubcircuit pmos pfet pdiff,pdc None VDD\n"
      " device resistor rpoly poly pdc\n"
      " fet nwell ndiff 2 old GND! 0 0\n"
      " device transmogrifier x pdiff ndiff\n"
      " device mosfet short nfet\n"});
  const Viewed v = view(*t);
  CHECK(*v.view.gate_types() == mask_of(*t, {"nfet", "pfet", "nwell"}));
  REQUIRE(v.messages.size() == 2);
  CHECK(v.messages[0].code == "EXTRACT-STYLE-002");
  CHECK(v.messages[0].text.starts_with("Illegal device.  Legal devices are:"));
  CHECK(v.messages[1].code == "EXTRACT-STYLE-001");
  CHECK(v.messages[1].text.starts_with("Malformed line for keyword device.  Correct usage:"));
}

TEST_CASE("the last substrate line sets the substrate types", "[cap:extract-tech-section][req:Substrate declaration]") {
  const auto t = load(std::string{" style s\n substrate nwell active\n substrate ndiff,pdiff active $SUB -nwell\n"});
  const Viewed v = view(*t);
  CHECK(*v.view.substrate_types() == mask_of(*t, {"ndiff", "pdiff"}));
  CHECK(v.view.active_types()->test(type_of(*t, "nwell")));  // the shield type is mentioned
}

TEST_CASE("tiedown lines give the diffusion types, antenna lines none",
          "[cap:extract-tech-section][req:Antenna keywords parse-level reference]") {
  const auto t = load(std::string{" style s\n antenna poly surface 400 none\n tied ndiff\n tiedown pdiff\n"});
  const Viewed v = view(*t);
  CHECK(*v.view.diff_types() == mask_of(*t, {"ndiff", "pdiff"}));
  CHECK(v.messages.empty());
}

TEST_CASE("types no extract line mentions are not active", "[cap:extract-tech-section][req:Non-electrical types]") {
  const auto t = load(std::string{
      " style s\n"
      " defaultareacap nwell active 10\n"  // no planeorder yet: nwell is not read
      " planeorder active 0\n"
      " defaultperimeter pfet active 5\n"
      " resist ndiff,pdiff 100\n"
      " resist poly None\n"
      " sidewall m2 ~(m2)/m2 ~(m2)/m2 m2 3\n"});
  const Viewed v = view(*t);
  const auto active = *v.view.active_types();
  CHECK(active.test(type_of(*t, "ndiff")));
  CHECK(active.test(type_of(*t, "pdiff")));
  CHECK(active.test(type_of(*t, "pfet")));
  CHECK(active.test(type_of(*t, "m2")));
  CHECK_FALSE(active.test(type_of(*t, "poly")));    // resist None
  CHECK_FALSE(active.test(type_of(*t, "nwell")));   // read before the plane order only
  CHECK_FALSE(active.test(type_of(*t, "nfet")));
  CHECK_FALSE(active.test(type_of(*t, "marker")));
}

TEST_CASE("keywords match by unique prefix; malformed lines are skipped",
          "[cap:extract-tech-section][req:Extract section line grammar and keyword matching]") {
  const auto t = load(std::string{" style s\n TIEDOWN ndiff\n tiedown pdiff extra\n de mosfet n nfet ndiff\n"});
  const Viewed v = view(*t);
  CHECK(*v.view.diff_types() == mask_of(*t, {"ndiff"}));
  CHECK(v.view.gate_types()->empty());  // `de` is ambiguous (default..., device, devresist)
}

TEST_CASE("the smoke technology's style leaves ntransistor non-electrical", "[cap:extract-tech-section][req:Non-electrical types]") {
  platform::LocalFileSystem files;
  diag::CollectingSink load_sink;
  const std::string file = std::string{HEFESTO_SOURCE_DIR} + "/tests/data/extflat/smoke/input/extsmoke.tech";
  auto loaded = tech::load_technology(files, file, tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const tech::Technology& t = *loaded.value();
  const Viewed v = view(t);
  REQUIRE(v.view.has_style());
  CHECK(v.view.style_name() == "extsmoke");
  CHECK(v.messages.empty());
  const auto active = *v.view.active_types();
  // Magic's probe.log: "...non-electrical types:\n    ntransistor".
  for (tech::TileType ty = tech::kFirstUserType; ty < t.user_type_end(); ++ty) {
    INFO(t.type_name(ty));
    CHECK(active.test(ty) == (t.type_name(ty) != "ntransistor"));
  }
}

namespace {

void check_pdk(const std::optional<std::filesystem::path>& file, std::string_view style, std::string_view gate,
               std::string_view diff, std::string_view substrate) {
  if (!file) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  platform::LocalFileSystem files;
  diag::CollectingSink load_sink;
  auto loaded = tech::load_technology(files, file->string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const tech::Technology& t = *loaded.value();
  const Viewed v = view(t);
  CHECK(v.messages.empty());  // no new diagnostics
  REQUIRE(v.view.has_style());
  CHECK(v.view.style_name() == style);
  CHECK(v.view.gate_types()->test(type_of(t, gate)));
  CHECK(v.view.diff_types()->test(type_of(t, diff)));
  CHECK(v.view.substrate_types()->test(type_of(t, substrate)));
  CHECK_FALSE(v.view.gate_types()->test(type_of(t, diff)));
  CHECK(v.view.active_types()->test(type_of(t, gate)));
}

}  // namespace

TEST_CASE("the sky130A extract section gives its style's type sets", "[cap:extract-tech-section][req:Extraction styles and variants]") {
  check_pdk(hefesto::test::pinned_sky130a_tech(), "ngspice()", "nfet", "ndiff", "pwell");
}

TEST_CASE("the gf180mcuD extract section gives its style's type sets", "[cap:extract-tech-section][req:Extraction styles and variants]") {
  check_pdk(hefesto::test::pinned_gf180mcud_tech(), "ngspice()", "nfet", "ndiff", "pwell");
}

TEST_CASE("the ihp-sg13g2 extract section gives its style's type sets",
          "[cap:extract-tech-section][req:Extraction styles and variants]") {
  check_pdk(hefesto::test::pinned_ihp_sg13g2_tech(), "ngspice()", "nfet", "ndiff", "pwell");
}
