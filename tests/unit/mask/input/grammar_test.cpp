// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cifinput reader (capability mask-input-styles; tasks.md 7.1): the
// section grammar, style declaration and selection, variants, scalefactor
// and its normalisation, the grid limit, the CIF layer name table, rules,
// the GDS layer map, label statements and input-scale reporting.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "input_fixture.hpp"

namespace mask = hefesto::mask;
namespace diag = hefesto::diag;
using mask::test::read;

namespace {

/// The kinds of the operators of rule `r`.
std::vector<mask::OpKind> kinds(const mask::InputStyle& s, std::size_t r) {
  std::vector<mask::OpKind> out;
  for (const auto& op : s.rules.at(r).ops) {
    out.push_back(op.kind);
  }
  return out;
}

}  // namespace

TEST_CASE("an operator before any layer is rejected", "[cap:mask-input-styles][req:Cifinput section grammar]") {
  const auto r = read("style a\nscalefactor 100\nand CAA");
  CHECK(r.has("Must define layer before specifying operations."));
  CHECK(r.style().rules.empty());
  CHECK(r.style().cif_layers.empty());
}

TEST_CASE("a zero grow distance is rejected", "[cap:mask-input-styles][req:Cifinput section grammar]") {
  const auto r = read("style a\nscalefactor 100\nlayer m1 CMF\ngrow 0");
  CHECK(r.has("Grow/shrink distance must be greater than zero."));
  REQUIRE(r.style().rules.size() == 1);
  CHECK(kinds(r.style(), 0) == std::vector{mask::OpKind::op_or});
  CHECK(r.style().rules[0].simple);
}

TEST_CASE("statement errors of the cifinput grammar", "[cap:mask-input-styles][req:Cifinput section grammar]") {
  const auto r = read(
      "style a\nscalefactor 100\nlayer m1 CMF\nfoo CMF\nand\ngrow\nshrink 3 4\nmask-hints\ntagged\n"
      "boundary extra fields\nnot-square x\nlayer m1 A B C\nlayer nosuch A\ngrow 2\noptions bogus no-reconnect-labels\n"
      "templayer T\nfault F G\nscalefactor 1 2 3 4 5");
  const std::vector<std::string> expected = {
      "Unknown statement \"foo\".",
      "Wrong number of arguments in and statement.",
      "Wrong number of arguments in grow statement.",
      "Wrong number of arguments in shrink statement.",
      "Wrong number of arguments in mask-hints statement.",
      "Wrong number of arguments in tagged statement.",
      "boundary or mask-hints in a layer rule is not an input boundary or hint (Q-MASK-307).",
      "Wrong number of arguments in layer statement.",
      "Unrecognized layer (type) name \"nosuch\"",
      "Must define layer before specifying operations.",
      "Unknown cifinput option \"bogus\".",
      "Error:  Fault layers deprecated.  Treating as templayer",
      "Wrong number of arguments in scalefactor statement.",
      "Input style a: scaleFactor=100, multiplier=1",
  };
  CHECK(r.texts() == expected);
  const mask::InputStyle& s = r.style();
  REQUIRE(s.rules.size() == 3);  // m1, T, F
  CHECK(kinds(s, 0) == std::vector{mask::OpKind::op_or, mask::OpKind::boundary, mask::OpKind::not_square});
  CHECK_FALSE(s.rules[0].simple);
  CHECK(s.rules[1].temporary);
  CHECK(s.cif_layers[s.rules[1].cif_layer] == "T");
  CHECK(s.rules[2].temporary);
  CHECK(s.cif_layers[s.rules[2].cif_layer] == "F");
  // Every error message carries the cifinput line.
  CHECK(r.messages.front().code == "MASK-INPUT-001");
  CHECK(r.messages.back().code == "MASK-INPUT-003");
}

TEST_CASE("operators are appended in order with their operands",
          "[cap:mask-input-styles][req:Layer and templayer rules]") {
  const auto r = read(
      "style a\nscalefactor 100\nlayer ndc CCA\ngrow 100\nand CAA\nand-not CPG,,CMF\ngrow-grid 5\nshrink 3\n"
      "templayer XBND BND\nboundary\ntemplayer HM POLYHINT\nmask-hints poly\ntemplayer TG\ntagged VDD CMF\n"
      "templayer UP NWELL\ncopyup NWELL");
  const mask::InputStyle& s = r.style();
  REQUIRE(s.rules.size() == 5);
  CHECK(s.rules[0].type == r.type("ndc"));
  CHECK(kinds(s, 0) == std::vector{mask::OpKind::op_or, mask::OpKind::grow, mask::OpKind::op_and,
                                   mask::OpKind::and_not, mask::OpKind::grow_grid, mask::OpKind::shrink});
  CHECK(s.rules[0].ops[1].distance == 100);
  CHECK(s.rules[0].ops[4].distance == 5);
  CHECK(s.rules[0].ops[5].distance == 3);
  // CIF layers in order of first mention; a run of commas counts once.
  CHECK(s.cif_layers == std::vector<std::string>{"CCA", "CAA", "CPG", "CMF", "XBND", "BND", "HM", "POLYHINT", "TG",
                                                 "UP", "NWELL"});
  CHECK(s.rules[0].ops[3].cif_layers == std::vector<std::size_t>{2, 3});
  CHECK(s.rules[2].ops[1].text == "poly");
  CHECK(s.rules[3].ops[0].kind == mask::OpKind::tagged);
  CHECK(s.rules[3].ops[0].text == "VDD");
  CHECK(s.rules[3].ops[0].cif_layers == std::vector<std::size_t>{3});
  CHECK(s.rules[4].find(mask::OpKind::copyup) != nullptr);
}

TEST_CASE("boundary and mask-hints outside a plain templayer are warned about",
          "[cap:mask-input-styles][req:Input operator boundary]") {
  for (const bool magic : {true, false}) {
    const auto r = read("style a\nscalefactor 100\nlayer m1 CMF\nboundary\ntemplayer T X\ncopyup X\nmask-hints h\n"
                        "templayer P Y\nmask-hints h2",
                        magic);
    std::size_t warnings = 0;
    for (const auto& d : r.messages) {
      warnings += d.code == "MASK-INPUT-007" ? 1U : 0U;
    }
    CHECK(warnings == 2);
  }
}

// --- styles -------------------------------------------------------------------

TEST_CASE("variant styles expand to concatenated names",
          "[cap:mask-input-styles][req:Input style declaration and default style]") {
  const auto r = read("style scmos0.18 variants (p),(c),(pc),()\nscalefactor 100\nstyle other\nscalefactor 10");
  CHECK(r.set.names() == std::vector<std::string>{"scmos0.18(p)", "scmos0.18(c)", "scmos0.18(pc)", "scmos0.18()",
                                                  "other"});
  REQUIRE(r.set.current_style() != nullptr);
  CHECK(r.set.current_style()->name == "scmos0.18(p)");
  CHECK(r.style(4).scale_factor == 10);
}

TEST_CASE("statements before the first style are ignored",
          "[cap:mask-input-styles][req:Input style declaration and default style]") {
  const auto r = read("scalefactor 7\nstyle a\nscalefactor 100");
  CHECK(r.set.section_failed);  // Magic's CIFReadTechLine returns FALSE here
  CHECK(r.style().scale_factor == 100);
}

TEST_CASE("a prefix-matching style name is merged with magic-compat ON (Q-MASK-302)",
          "[cap:mask-input-styles][req:Input style declaration and default style]") {
  const auto r = read("style foo\nscalefactor 100\nlayer m1 A\nstyle fo\nscalefactor 50\nlayer m2 B", true);
  CHECK(r.set.names() == std::vector<std::string>{"foo"});
  CHECK(r.style().rules.size() == 1);
  CHECK(r.style().scale_factor == 100);
  CHECK(r.has("Input style \"fo\" is a prefix of input style \"foo\"."));
}

TEST_CASE("a prefix-matching style name is its own style with magic-compat OFF (Q-MASK-302)",
          "[cap:mask-input-styles][req:Input style declaration and default style]") {
  auto r = read("style foo\nscalefactor 100\nlayer m1 A\nstyle fo\nscalefactor 50\nlayer m2 B", false);
  CHECK(r.set.names() == std::vector<std::string>{"foo", "fo"});
  CHECK(r.has("Input style \"fo\" is a prefix of input style \"foo\"."));
  REQUIRE(mask::select_input_style(r.set, "fo", mask::test::mode(false)).ok());
  REQUIRE(r.set.current_style() != nullptr);
  CHECK(r.set.current_style()->name == "fo");
  CHECK(r.set.current_style()->scale_factor == 50);
  REQUIRE(r.set.current_style()->rules.size() == 1);
  CHECK(r.set.current_style()->rules[0].type == r.type("m2"));
}

TEST_CASE("statements for a non-matching variant are skipped",
          "[cap:mask-input-styles][req:Variant-conditional statements]") {
  auto r = read("style scmos0.18 variants (p),(c),(pc)\nscalefactor 100\nvariant (c),(pc)\nlayer m2 CMS\n"
                "scalefactor 50\nvariant *\nlayer m1 CMF");
  const mask::InputStyle& p = r.style(0);
  REQUIRE(p.rules.size() == 1);
  CHECK(p.rules[0].type == r.type("m1"));
  CHECK(p.scale_factor == 50);  // scalefactor applies in a skipped region
  const mask::InputStyle& c = r.style(1);
  REQUIRE(c.rules.size() == 2);
  CHECK(c.rules[0].type == r.type("m2"));
}

TEST_CASE("a wildcard variant re-enables processing", "[cap:mask-input-styles][req:Variant-conditional statements]") {
  const auto r = read("style x variants (p)\nscalefactor 100\nvariant (c)\nlayer m1 A\nvariant *\nlayer m2 B\n"
                      "variants (z),(p)\nlayer nwell C");
  REQUIRE(r.style().rules.size() == 2);
  CHECK(r.style().rules[0].type == r.type("m2"));
  CHECK(r.style().rules[1].type == r.type("nwell"));  // a later piece of the list matches
}

TEST_CASE("input style selection", "[cap:mask-input-styles][req:Input style selection]") {
  auto r = read("style lambda=1.0(gen)\nscalefactor 100\nstyle sky130(vendor)\nscalefactor 10\nstyle gds\n"
                "scalefactor 1\nstyle gds2\nscalefactor 2\nstyle gds1\nscalefactor 3");
  diag::CollectingSink sink;
  SECTION("unique prefix") {
    REQUIRE(mask::select_input_style(r.set, "sky", 1, 1, mask::test::mode(true), sink).ok());
    CHECK(r.set.current_style()->name == "sky130(vendor)");
    CHECK(mask::test::texts_of(sink.take()) ==
          std::vector<std::string>{"Input style sky130(vendor): scaleFactor=10, multiplier=1"});
  }
  SECTION("an exact name that is a prefix of another is selected in both modes (Q-MASK-303)") {
    for (const bool magic : {true, false}) {
      REQUIRE(mask::select_input_style(r.set, "gds", mask::test::mode(magic)).ok());
      CHECK(r.set.current_style()->name == "gds");
      REQUIRE(mask::select_input_style(r.set, "lambda", mask::test::mode(magic)).ok());
    }
  }
  SECTION("a prefix of two styles is ambiguous") {
    const diag::Status s = mask::select_input_style(r.set, "gds", mask::test::mode(true));
    REQUIRE(s.ok());  // exact
    const diag::Status ambiguous = mask::select_input_style(r.set, "g", mask::test::mode(true));
    REQUIRE_FALSE(ambiguous.ok());
    CHECK(ambiguous.error().code == "MASK-INPUT-004");
    CHECK(ambiguous.error().text == "CIF input style \"g\" is ambiguous.");
    CHECK(r.set.current_style()->name == "gds");
  }
  SECTION("an unknown name") {
    const diag::Status s = mask::select_input_style(r.set, "zz", mask::test::mode(true));
    REQUIRE_FALSE(s.ok());
    CHECK(s.error().text == "\"zz\" is not one of the CIF input styles Magic knows.");
    CHECK(r.set.current_style()->name == "lambda=1.0(gen)");
  }
  SECTION("selecting the current style is a no-op") {
    REQUIRE(mask::select_input_style(r.set, "lambda", 1, 2, mask::test::mode(true), sink).ok());
    CHECK(sink.take().empty());
    CHECK(r.set.current_style()->multiplier == 1);
  }
  SECTION("a selection applies the lambda ratio") {
    REQUIRE(mask::select_input_style(r.set, "sky", 1, 2, mask::test::mode(true), sink).ok());
    CHECK(r.set.current_style()->scale_factor == 5);  // 10 * 1 / (1 * 2), reduced
    CHECK(r.set.current_style()->multiplier == 1);
  }
}

// --- scale --------------------------------------------------------------------

TEST_CASE("scalefactor parsing", "[cap:mask-input-styles][req:Scalefactor statement]") {
  SECTION("integer centimicron scale") {
    const auto r = read("style a\nscalefactor 100");
    CHECK(r.style().scale_factor == 100);
    CHECK(r.style().multiplier == 1);
  }
  SECTION("decimal scale") {
    const auto r = read("style a\nscalefactor 1.5");
    CHECK(r.style().scale_source == mask::ScaleSource{3, 2, 0});
    CHECK(r.style().scale_factor == 3);
    CHECK(r.style().multiplier == 2);
  }
  SECTION("nanometer keyword") {
    const auto r = read("style a\nscalefactor 5 nanometers\nlayer m1 A\ngrow 1");
    CHECK(r.style().scale_factor == 5);
    CHECK(r.style().multiplier == 10);
  }
  SECTION("angstrom keyword and calmaonly") {
    const auto a = read("style a\nscalefactor 3 angstroms\nlayer m1 A\ngrow 1");
    CHECK(a.style().multiplier == 100);
    const auto c = read("style a\nscalefactor 3 calmaonly\nlayer m1 A\ngrow 1");
    CHECK(c.style().multiplier == 1);
  }
  SECTION("a non-positive value") {
    const auto r = read("style a\nscalefactor 0");
    CHECK(r.has("Scalefactor must be a strictly positive value."));
    CHECK(r.has("CIF input style \"a\" bad scalefactor; using 1."));
    CHECK(r.style().scale_factor == 1);
  }
  SECTION("no scalefactor") {
    const auto r = read("style a\nlayer m1 A");
    CHECK(r.has("CIF input style \"a\" bad scalefactor; using 1."));
    CHECK(r.has("Input style a: scaleFactor=1, multiplier=1"));
  }
}

TEST_CASE("a decimal scale with nanometers overwrites the multiplier with magic-compat ON (Q-MASK-300)",
          "[cap:mask-input-styles][req:Scalefactor statement]") {
  const auto r = read("style a\nscalefactor 2.5 nanometers\nlayer m1 A\ngrow 1", true);
  CHECK(r.style().scale_factor == 5);
  CHECK(r.style().multiplier == 10);
  CHECK(r.has_code("MASK-INPUT-006"));
}

TEST_CASE("a decimal scale with nanometers multiplies the multiplier with magic-compat OFF (Q-MASK-300)",
          "[cap:mask-input-styles][req:Scalefactor statement]") {
  const auto r = read("style a\nscalefactor 2.5 nanometers\nlayer m1 A\ngrow 1", false);
  CHECK(r.style().scale_factor == 5);
  CHECK(r.style().multiplier == 20);
  CHECK(r.has_code("MASK-INPUT-006"));
}

TEST_CASE("scale normalization", "[cap:mask-input-styles][req:Scale normalization]") {
  SECTION("common factors are divided out") {
    const auto r = read("style a\nscalefactor 10 nanometers\nlayer m1 A\ngrow 5");
    CHECK(r.style().scale_factor == 2);
    CHECK(r.style().multiplier == 2);
    CHECK(r.style().rules[0].ops[1].distance == 1);
    CHECK(r.has("Input style a: scaleFactor=2, multiplier=2"));
  }
  SECTION("no reduction possible") {
    const auto r = read("style a\nscalefactor 100\nlayer m1 A\ngrow 25");
    CHECK(r.style().scale_factor == 100);
    CHECK(r.style().rules[0].ops[1].distance == 25);
  }
  SECTION("rescale(n, d) with and without full reduction") {
    auto r = read("style a\nscalefactor 100\nlayer m1 A\ngrow 25");
    mask::InputStyle s = r.style();
    CHECK(s.technology_scale(1, 2, false).reduced == 2);  // the grid subdivision of a read
    CHECK(s.scale_factor == 50);
    CHECK(s.multiplier == 1);
    CHECK(s.rules[0].ops[1].distance == 25);
    CHECK(s.technology_scale(3, 1, true).reduced == 1);
    CHECK(s.scale_factor == 150);
  }
  SECTION("the session rescale follows the grid") {
    auto r = read("style a\nscalefactor 100\nlayer m1 A\ngrow 25");
    CHECK(mask::rescale_input_styles(r.set, 1, 2, mask::test::mode(true)).empty());
    CHECK(r.set.current_style()->scale_factor == 50);
    CHECK(r.set.base[0]->scale_factor == 100);
  }
  SECTION("Magic's smaller factor can truncate; magic-compat OFF warns") {
    // M = 12 and a distance of 4 give 4; gcd(S, M) = 6: Magic divides by 4 and truncates S = 6.
    auto r = read("style a\nscalefactor 6\nlayer m1 A\ngrow 1", false);
    mask::InputStyle s = r.style();
    s.multiplier = 12;
    s.rules[0].ops[1].distance = 4;
    const mask::InputScaleStep step = s.technology_scale(1, 1, true);
    CHECK(step.reduced == 4);
    CHECK(step.truncated);
    CHECK(s.scale_factor == 1);
    auto off = read("style a\nscalefactor 6\nlayer m1 A\ngrow 1", false);
    mask::InputStyleSet set = off.set;
    auto modified = std::make_shared<mask::InputStyle>(*set.current_style());
    modified->rules[0].ops[1].distance = 4;
    modified->multiplier = 3;
    set.replace_current(modified);
    CHECK(mask::rescale_input_styles(set, 1, 4, mask::test::mode(false)).size() == 1);
    CHECK(mask::rescale_input_styles(set, 1, 4, mask::test::mode(true)).empty());
  }
}

TEST_CASE("the grid limit", "[cap:mask-input-styles][req:Grid limit]") {
  SECTION("subdivision allowed by the grid limit") {
    const auto r = read("style a\nscalefactor 100\ngridlimit 5");
    CHECK(r.style().grid_limit == 5);
    CHECK_FALSE(r.style().limit_scale(1, 2));
  }
  SECTION("subdivision refused by the grid limit") {
    const auto r = read("style a\nscalefactor 100\ngridlimit 1000");
    CHECK(r.style().limit_scale(1, 2));
    CHECK(r.style().limit_scale(1, 3));
  }
  SECTION("not a multiple of the limit") {
    const auto r = read("style a\nscalefactor 100\ngrid 3");
    CHECK(r.style().limit_scale(1, 1));  // 1000 mod 3 != 0
  }
  SECTION("no limit") {
    const auto r = read("style a\nscalefactor 100");
    CHECK_FALSE(r.style().limit_scale(1, 1000000));
  }
  SECTION("bad values") {
    const auto r = read("style a\nscalefactor 100\ngridlimit -5\ngridlimit 2.5\ngridlimit");
    CHECK(r.style().grid_limit == 0);
    CHECK(r.has("Grid limit must be a positive integer."));
    CHECK(r.has("Unable to parse grid limit value."));
  }
}

// --- layers, calma, labels ----------------------------------------------------

TEST_CASE("a calma statement before the layer is declared", "[cap:mask-input-styles][req:CIF layer name table]") {
  const auto r = read("style a\nscalefactor 100\ncalma CPO 7 *\nlayer m1 CMF\ncalma CMF 4 *");
  CHECK(r.has("Unrecognized CIF layer: \"CPO\""));
  CHECK_FALSE(r.style().calma_layer(7, 0).has_value());
  CHECK(r.style().calma_layer(4, 3) == std::optional<std::size_t>{0});
  CHECK_FALSE(r.style().find_cif_layer("CPO").has_value());
}

TEST_CASE("the CIF layer and rule tables hold 255 entries",
          "[cap:mask-input-styles][req:CIF layer name table]") {
  std::string body = "style a\nscalefactor 100\n";
  for (int i = 0; i < 256; ++i) {
    body += "templayer L" + std::to_string(i) + "\n";
  }
  const auto r = read(body);
  CHECK(r.style().cif_layers.size() == 255);
  CHECK(r.style().rules.size() == 255);
  CHECK(r.has("Can't handle more than 255 layers per style."));
}

TEST_CASE("GDS layer and datatype mapping", "[cap:mask-input-styles][req:GDS layer and datatype mapping]") {
  SECTION("wildcard precedence") {
    const auto r = read("style a\nscalefactor 100\nlayer m1 CMF\nlayer poly CPG\ntemplayer CX\n"
                        "calma CMF 4 *\ncalma CPG 7 0\ncalma CX * 5");
    const mask::InputStyle& s = r.style();
    const auto cmf = s.find_cif_layer("CMF");
    const auto cpg = s.find_cif_layer("CPG");
    const auto cx = s.find_cif_layer("CX");
    CHECK(s.calma_layer(4, 9) == cmf);
    CHECK(s.calma_layer(7, 0) == cpg);
    CHECK_FALSE(s.calma_layer(7, 1).has_value());
    CHECK(s.calma_layer(3, 5) == cx);
    CHECK(s.calma_layer(7, 5) == cx);
  }
  SECTION("product of lists, repeated commas, a later statement replaces") {
    const auto r = read("style a\nscalefactor 100\nlayer m1 CMF\ntemplayer B\ncalma CMF 4,,5 0,20\ncalma B 5 20");
    const mask::InputStyle& s = r.style();
    CHECK(s.calma_layer(4, 0) == s.find_cif_layer("CMF"));
    CHECK(s.calma_layer(4, 20) == s.find_cif_layer("CMF"));
    CHECK(s.calma_layer(5, 0) == s.find_cif_layer("CMF"));
    CHECK(s.calma_layer(5, 20) == s.find_cif_layer("B"));
  }
  SECTION("bad numbers reject the whole statement") {
    const auto r = read("style a\nscalefactor 100\nlayer m1 CMF\ncalma CMF 4,256 0\ncalma CMF 4x 0\ncalma CMF 1 -2\n"
                        "calma CMF 1 2 3");
    CHECK(r.has("Calma layer and type numbers must be 0 to 255."));
    CHECK(r.has("Calma layer/type numbers must be numeric or '*'"));
    CHECK(r.has("Wrong number of arguments in calma statement."));
    CHECK(r.style().calma.empty());
  }
}

TEST_CASE("a list of 255 calma numbers depends on magic-compat (Q-MASK-326)",
          "[cap:mask-input-styles][req:GDS layer and datatype mapping]") {
  std::string list;
  for (int i = 0; i < 255; ++i) {
    list += (i == 0 ? "" : ",") + std::to_string(i);
  }
  const std::string body = "style a\nscalefactor 100\nlayer m1 CMF\ncalma CMF " + list + " 0";
  const auto on = read(body, true);
  CHECK(on.has("Too many layer/type numbers in line; maximum = 255"));
  CHECK(on.style().calma.empty());
  const auto off = read(body, false);
  CHECK(off.style().calma.size() == 255);
  CHECK(off.style().calma_layer(254, 0) == off.style().find_cif_layer("CMF"));
}

TEST_CASE("label layer statements", "[cap:mask-input-styles][req:Label layer statements]") {
  SECTION("port labels") {
    const auto r = read("style a\nscalefactor 100\nlayer m1 CMF\nlabels CMFPIN port");
    const auto pin = r.style().find_cif_layer("CMFPIN");
    REQUIRE(pin.has_value());
    CHECK(r.style().label_of(*pin) == mask::CifLayerLabels{static_cast<std::int32_t>(r.type("m1")), mask::LabelKind::port});
  }
  SECTION("ignore after labels keeps labels") {
    const auto r = read("style a\nscalefactor 100\nlayer m1 CMF\nlabels CMF\nignore CMF,CX");
    CHECK(r.style().label_of(0).type == r.type("m1"));
    CHECK(r.style().label_of(*r.style().find_cif_layer("CX")).type == mask::kDiscardLabels);
  }
  SECTION("text, sticky (Q-MASK-301), cell and bad kinds") {
    const auto r = read("style a\nscalefactor 100\nlayer m1 A\nlabels A text\nlabels B sticky\nlabels C cellid\n"
                        "labels D bogus\nlabels E text extra");
    const mask::InputStyle& s = r.style();
    CHECK(s.label_of(*s.find_cif_layer("A")).kind == mask::LabelKind::text);
    CHECK(s.label_of(*s.find_cif_layer("B")).kind == mask::LabelKind::text);
    CHECK(s.label_of(*s.find_cif_layer("C")).kind == mask::LabelKind::cell_id);
    CHECK_FALSE(s.find_cif_layer("D").has_value());
    CHECK(r.has("Wrong number of arguments in labels statement."));
  }
  SECTION("labels before a layer") {
    const auto r = read("style a\nscalefactor 100\nlabels A");
    CHECK(r.has("Must define layer before giving labels it holds."));
  }
  SECTION("a reassignment across planes is reported") {
    const auto r = read("style a\nscalefactor 100\nlayer m1 A\nlabels X\nlayer m2 B\nlabels X\nlayer poly C\nlabels X");
    CHECK(r.has("Labels on layer \"X\" attached to \"polysilicon\" supersedes prior attachment to \"metal2\"."));
    CHECK(r.has("Labels on layer \"X\" attached to \"metal2\" supersedes prior attachment to \"metal1\"."));
    CHECK(r.style().label_of(*r.style().find_cif_layer("X")).type == r.type("poly"));
  }
  SECTION("unknown options and ignore-unknown-layer-labels") {
    const auto r = read("style a\nscalefactor 100\noptions ignore-unknown-layer-labels no-reconnect-labels");
    CHECK(r.style().has_option(mask::input_option::ignore_unknown_layer_labels));
    CHECK(r.messages.size() == 1);  // only the Input style line
  }
}

TEST_CASE("labels under a templayer store the CIF index as a type with magic-compat ON (Q-MASK-311)",
          "[cap:mask-input-styles][req:Label layer statements]") {
  const auto r = read("style a\nscalefactor 100\nlayer m1 A\ntemplayer T B\nlabels LBL", true);
  const mask::InputStyle& s = r.style();
  const auto t = s.find_cif_layer("T");
  REQUIRE(t.has_value());
  CHECK(s.label_of(*s.find_cif_layer("LBL")).type == static_cast<std::int32_t>(*t));
  CHECK_FALSE(r.has_code("MASK-INPUT-008"));
}

TEST_CASE("labels under a templayer are ignored with magic-compat OFF (Q-MASK-311)",
          "[cap:mask-input-styles][req:Label layer statements]") {
  const auto r = read("style a\nscalefactor 100\nlayer m1 A\nlabels LBL\ntemplayer T B\nlabels LBL", false);
  const mask::InputStyle& s = r.style();
  CHECK(s.label_of(*s.find_cif_layer("LBL")).type == r.type("m1"));  // the earlier mapping stays
  CHECK(r.has_code("MASK-INPUT-008"));
}

TEST_CASE("labels under a templayer whose index is no type are ignored in both modes (Q-MASK-311)",
          "[cap:mask-input-styles][req:Label layer statements]") {
  std::string body = "style a\nscalefactor 100\nlayer m1 L0\n";
  for (int i = 1; i < 40; ++i) {
    body += "ignore L" + std::to_string(i) + "\n";
  }
  body += "templayer T X\nlabels LBL";
  const auto r = read(body, true);
  CHECK(r.has_code("MASK-INPUT-008"));
  CHECK(r.style().label_of(*r.style().find_cif_layer("LBL")).type == 0);
}

// --- reporting ------------------------------------------------------------------

TEST_CASE("microns per internal unit", "[cap:mask-input-styles][req:Input-scale reporting]") {
  const auto r = read("style a\nscalefactor 5 nanometers\nlayer m1 A\ngrow 1");
  CHECK(r.style().microns_per_unit() == 0.005f);
  CHECK(mask::input_scale(r.set) == std::optional<float>{0.005f});
  CHECK_FALSE(mask::input_scale(mask::InputStyleSet{}).has_value());
}

TEST_CASE("the tech-load line reports the normalised scale", "[cap:mask-input-styles][req:Input-scale reporting]") {
  const auto r = read("style gds\nscalefactor 10\nstyle gds2\nscalefactor 10");
  CHECK(r.texts() == std::vector<std::string>{"Input style gds: scaleFactor=10, multiplier=1"});
  CHECK(mask::test::texts_of(r.style(1).load_messages) ==
        std::vector<std::string>{"Input style gds2: scaleFactor=10, multiplier=1"});
}

TEST_CASE("magic-compat changes re-read the section", "[cap:mask-input-styles][req:Scalefactor statement]") {
  auto r = read("style a\nscalefactor 2.5 nanometers\nlayer m1 A\ngrow 1\nstyle b\nscalefactor 3", true);
  REQUIRE(mask::select_input_style(r.set, "b", mask::test::mode(true)).ok());
  mask::refresh_input_styles(r.set, *r.tech, 1, 1, mask::test::mode(false));
  CHECK(r.set.current_style()->name == "b");
  CHECK(r.set.base[0]->multiplier == 20);
  CHECK_FALSE(r.set.magic_compat);
}
