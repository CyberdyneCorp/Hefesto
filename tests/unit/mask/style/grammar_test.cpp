// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cifoutput statements (tasks.md 3.1): grammar, style declaration and
// loading, scalefactor, units/stepsize/gridlimit, layer declarations,
// layer lists, labels, the GDS mapping, min-width/render/options, and the
// shared style/variant/scale core.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/diag/format.hpp"
#include "hefesto/mask/style/core.hpp"
#include "style_fixture.hpp"

namespace mask = hefesto::mask;
namespace tech = hefesto::tech;
using mask::test::read;

TEST_CASE("a minimal cifoutput style", "[cap:mask-output-styles][req:cifoutput section grammar]") {
  const auto r = read("style gdsii\nscalefactor 10 nanometers\nlayer M1 metal1\ncalma 68 20");
  CHECK(r.messages.empty());
  REQUIRE(r.set.styles.size() == 1);
  const mask::OutputStyle& s = r.style();
  CHECK(s.name == "gdsii");
  REQUIRE(s.layers.size() == 1);
  CHECK(s.layers[0].name == "M1");
  REQUIRE(s.layers[0].ops.size() == 1);
  CHECK(s.layers[0].ops[0].kind == mask::OpKind::op_or);
  CHECK(s.layers[0].ops[0].types == r.types({"metal1"}));
  CHECK(s.layers[0].gds == mask::GdsLayer{68, 20});
  CHECK(s.layers[0].gds_written());
}

TEST_CASE("an unknown cifoutput statement is reported and skipped",
          "[cap:mask-output-styles][req:cifoutput section grammar]") {
  const auto r = read("style s\nscalefactor 1\nlayer A metal1\nfoo 3\ngrow 2");
  REQUIRE(r.messages.size() == 1);
  CHECK(r.messages[0].code == "MASK-STYLE-001");
  CHECK(r.messages[0].text == "Unknown statement \"foo\".");
  // Magic's line context in the magic rendering.
  CHECK(hefesto::diag::format(r.messages[0], hefesto::diag::Format::magic) ==
        "/t/styles.tech: line " + std::to_string(std::get<hefesto::diag::SourceLocation>(r.messages[0].location).line) +
            ": section cifoutput:\n\tUnknown statement \"foo\".");
  REQUIRE(r.style().layers[0].ops.size() == 2);
  CHECK(r.style().layers[0].ops[1].kind == mask::OpKind::grow);
}

TEST_CASE("style lines declare names in order, variants as base + suffix",
          "[cap:mask-output-styles][req:Style declaration and the style list]") {
  // Scenario "Variant names".
  const auto r = read("style sky130 variant (vendor),(),(fill)\nscalefactor 1\nlayer A metal1");
  CHECK(r.set.names() == std::vector<std::string>{"sky130(vendor)", "sky130()", "sky130(fill)"});
  REQUIRE(r.set.current_style() != nullptr);
  CHECK(r.set.current_style()->name == "sky130(vendor)");
  CHECK(r.set.current == std::size_t{0});
}

TEST_CASE("a style name Magic's prefix test drops is registered at the end",
          "[cap:mask-output-styles][req:Style declaration and the style list]") {
  // Scenario "Prefix shadowing" (Q-MASK-10, fix): the reader does not depend
  // on magic-compat, so both settings give this result.
  const auto r = read("style gdsfoo\nscalefactor 1\nlayer A metal1\nstyle cif\nscalefactor 1\nlayer C metal2\n"
                      "style gds\nscalefactor 2\nlayer B metal2");
  CHECK(r.set.names() == std::vector<std::string>{"gdsfoo", "cif", "gds"});
  REQUIRE(r.messages.size() == 1);
  CHECK(r.messages[0].code == "MASK-STYLE-003");
  CHECK(r.messages[0].native_only);
  CHECK(r.messages[0].text.find("\"gds\" is a prefix of the earlier style \"gdsfoo\"") != std::string::npos);
  // The appended style loads its own statements.
  CHECK(r.style(2).name == "gds");
  CHECK(r.style(2).scale.scale_factor == 2);
  REQUIRE(r.style(2).layers.size() == 1);
  CHECK(r.style(2).layers[0].name == "B");
  for (const bool compat : {true, false}) {
    auto set = r.set;
    REQUIRE(mask::select_output_style(set, "gds", mask::test::compat_mode(compat)).ok());
    CHECK(set.current_style()->name == "gds");
  }
}

TEST_CASE("a variant style read again is not registered twice",
          "[cap:mask-output-styles][req:Style declaration and the style list]") {
  const auto r = read("style sky130 variant (vendor),()\nscalefactor 1\nlayer A metal1\n"
                      "style sky130 variant (vendor)\nscalefactor 1\nlayer B metal1");
  CHECK(r.set.names() == std::vector<std::string>{"sky130(vendor)", "sky130()"});
  CHECK(r.messages.empty());
}

TEST_CASE("a style line with a wrong argument count and statements before any style",
          "[cap:mask-output-styles][req:Style declaration and the style list]") {
  const auto bad = read("style a b\nstyle s\nscalefactor 1\nlayer A metal1");
  CHECK(mask::test::texts_of(bad.messages) ==
        std::vector<std::string>{"Wrong number of arguments in style statement."});
  CHECK(bad.set.names() == std::vector<std::string>{"s"});
  CHECK_FALSE(bad.set.section_failed);
  // No implicit `default` style (Q-MASK-17): the section fails.
  const auto early = read("scalefactor 1\nstyle s\nscalefactor 1\nlayer A metal1");
  CHECK(early.set.section_failed);
  CHECK(early.set.names() == std::vector<std::string>{"s"});
}

TEST_CASE("variant lines select the statements of the style being loaded",
          "[cap:mask-output-styles][req:Style loading and variant-conditional statements]") {
  // Scenario "Variant-conditional layer".
  const auto r = read("style s variant A,B\nscalefactor 10\nvariant A\nlayer X metal1\nvariant *\nlayer Y metal2");
  REQUIRE(r.set.names() == std::vector<std::string>{"sA", "sB"});
  const mask::OutputStyle& sb = r.style(1);
  CHECK(sb.name == "sB");
  CHECK(sb.scale.scale_factor == 10);
  REQUIRE(sb.layers.size() == 1);
  CHECK(sb.layers[0].name == "Y");
  CHECK(r.style(0).layers.size() == 2);
  // scalefactor applies under a non-matching variant; the next style line ends the style.
  const auto s2 = read("style s variant A,B\nvariant A\nscalefactor 3\nlayer X metal1\nstyle t\nscalefactor 5\n"
                       "layer Z metal1");
  CHECK(s2.style(1).scale.scale_factor == 3);
  CHECK(s2.style(1).layers.empty());
  CHECK(s2.style(2).name == "t");
  CHECK(s2.style(2).layers.size() == 1);
}

TEST_CASE("a variant equal to the whole style name does not match",
          "[cap:mask-output-styles][req:Style loading and variant-conditional statements]") {
  // Scenario "Variant with empty remainder does not match".
  mask::StyleStateMachine m;
  (void)m.feed({"style", "s", "variant", "()"});
  CHECK(m.loading() == "s()");
  CHECK(m.applies());
  (void)m.feed({"variant", "s()"});
  CHECK(m.state() == mask::StyleStateMachine::State::suspended);
  (void)m.feed({"variant", "x,()"});
  CHECK(m.applies());
  // cifinput's rule accepts a suffix as long as the name.
  mask::StyleStateMachine in{{"", mask::StyleStateMachine::Registration::prefix,
                              mask::StyleStateMachine::VariantRule::not_longer}};
  (void)in.feed({"style", "s", "variant", "()"});
  (void)in.feed({"variant", "s()"});
  CHECK(in.applies());
  // Magic's prefix registration drops a name an earlier one starts with.
  (void)in.feed({"style", "s("});
  CHECK(in.names() == std::vector<std::string>{"s()"});
  CHECK(in.state() == mask::StyleStateMachine::State::loaded);
  const auto fed = in.feed({"variant", "a", "b"});
  CHECK(fed.kind == mask::StyleStateMachine::LineKind::statement);  // not active any more
}

TEST_CASE("scalefactor parsing", "[cap:mask-output-styles][req:scalefactor statement]") {
  CHECK(mask::parse_scale("4.5") == mask::ScaleValue{9, 2});
  CHECK(mask::parse_scale("10") == mask::ScaleValue{10, 1});
  CHECK(mask::parse_scale("2.5") == mask::ScaleValue{5, 2});
  CHECK(mask::parse_scale("0.25") == mask::ScaleValue{1, 4});
  CHECK_FALSE(mask::parse_scale("").has_value());
  // Raw values before normalisation (scenarios "Decimal scale",
  // "Nanometres", "Decimal angstroms"); bloat-all keeps them (Q-MASK-13).
  const char* keep = "\nlayer A metal1\nbloat-all metal1 metal2";
  // 9/2 is doubled for the odd scale factor, then bloat-all's doubled flag
  // lets the factor 2 reduce again.
  CHECK(read(std::string{"style s\nscalefactor 4.5"} + keep).style().scale == mask::StyleScale{9, 2, 1});
  CHECK(read(std::string{"style s\nscalefactor 10 nanometers"} + keep).style().scale == mask::StyleScale{10, 10, 1});
  CHECK(read(std::string{"style s\nscalefactor 2.5 angstroms"} + keep).style().scale == mask::StyleScale{5, 200, 1});
  CHECK(read(std::string{"style s\nscalefactor 10 calmaonly"} + keep).style().scale == mask::StyleScale{10, 1, 1});
}

TEST_CASE("scalefactor errors", "[cap:mask-output-styles][req:scalefactor statement]") {
  const auto zero = read("style s\nscalefactor 0\nlayer A metal1");
  CHECK(mask::test::texts_of(zero.messages) ==
        std::vector<std::string>{"Scalefactor must be a strictly positive value.",
                                 "No valid scale factor was given for s CIF."});
  CHECK(zero.style().scale.scale_factor == 1);
  const auto args = read("style s\nscalefactor 1 2 3 4\nlayer A metal1");
  CHECK(mask::test::texts_of(args.messages) ==
        std::vector<std::string>{"Wrong number of arguments in scalefactor statement.",
                                 "No valid scale factor was given for s CIF."});
}

TEST_CASE("units, stepsize and gridlimit", "[cap:mask-output-styles][req:units, stepsize and gridlimit statements]") {
  const auto r = read("style s\nscalefactor 10\nunits angstroms\nstepsize 100\ngridlimit 2\nlayer A metal1\ngrow 4");
  CHECK(r.style().has_option(mask::style_option::angstroms));
  CHECK(r.style().step_size == 100);
  CHECK(r.style().grid_limit == 2);
  const auto other = read("style s\nscalefactor 10\nunits nanometers\nlayer A metal1");
  CHECK_FALSE(other.style().has_option(mask::style_option::angstroms));
  CHECK(other.messages.empty());
  const auto bad = read("style s\nscalefactor 10\nstepsize 0\ngridlimit x\ngrid -3\nunits\nlayer A metal1");
  CHECK(mask::test::texts_of(bad.messages) ==
        std::vector<std::string>{"Step size must be positive integer.", "Unable to parse grid limit value.",
                                 "Grid limit must be a positive integer.",
                                 "Wrong number of arguments in units statement."});
  CHECK(bad.style().step_size == 0);
  CHECK(bad.style().grid_limit == 0);
}

TEST_CASE("gridlimit is stored as written and warned about only in a bloat-all style with expander other than 10",
          "[cap:mask-output-styles][req:units, stepsize and gridlimit statements]") {
  // Scenario "gridlimit stored verbatim" (Q-MASK-8, amended 2026-11-14): a
  // style without bloat-all reduces, like the PDK density and metfill
  // styles, and stays quiet.
  const auto r = read("style s\nscalefactor 10 nanometers\ngridlimit 5\nlayer A metal1");
  CHECK(r.style().scale == mask::StyleScale{1, 1, 1});
  CHECK(r.style().grid_limit == 5);
  CHECK(r.messages.empty());
  // The PDK case: bloat-all keeps the expander at 10, no warning.
  const auto pdk = read("style s\nscalefactor 10 nanometers\ngridlimit 5\nlayer A metal1\nbloat-all metal1 metal2");
  CHECK(pdk.messages.empty());
  // Scenario "gridlimit in a bloat-all style with expander 1": warned.
  const auto warned = read("style s\nscalefactor 10\ngridlimit 5\nlayer A metal1\nbloat-all metal1 metal2");
  CHECK(warned.style().scale.expander == 1);
  REQUIRE(warned.messages.size() == 1);
  CHECK(warned.messages[0].code == "MASK-STYLE-004");
  CHECK(warned.messages[0].native_only);
  CHECK(warned.messages[0].text.find("expander 1, not 10") != std::string::npos);
  // The warning follows a later normalisation too (both modes).
  for (const bool compat : {true, false}) {
    const mask::RescaleStep step = mask::rescale_step(pdk.style(), 1, 3, mask::test::compat_mode(compat));
    CHECK(step.style->scale.expander == 10);
    for (const auto& d : step.messages) {
      CHECK(d.code != "MASK-STYLE-004");
    }
    const mask::RescaleStep finer = mask::rescale_step(warned.style(), 2, 1, mask::test::compat_mode(compat));
    REQUIRE_FALSE(finer.messages.empty());
    CHECK(finer.messages.back().code == "MASK-STYLE-004");
  }
}

TEST_CASE("mask layer declarations", "[cap:mask-output-styles][req:Mask layer declarations]") {
  // Scenario "Inline OR".
  const auto r = read("style s\nscalefactor 1\ntemplayer XPOLY poly,pc\nlabellayer T metal1\nlayer L metal2");
  const mask::OutputLayer& x = r.style().layers[0];
  CHECK(x.temporary);
  CHECK_FALSE(x.label_layer);
  REQUIRE(x.ops.size() == 1);
  CHECK(x.ops[0].kind == mask::OpKind::op_or);
  CHECK(x.ops[0].types == r.types({"poly", "pc", "pm12c"}));  // with the stacked pc type
  CHECK(r.style().layers[1].label_layer);
  CHECK_FALSE(r.style().layers[2].temporary);
  CHECK_FALSE(r.style().layers[2].gds.has_value());
  CHECK(r.style().layers[2].min_width == 0);
  CHECK(r.messages.empty());
}

TEST_CASE("a layer without material is warned about", "[cap:mask-output-styles][req:Mask layer declarations]") {
  // Scenario "Empty layer warning", also at the end of the section.
  const auto r = read("style s\nscalefactor 1\nlayer A\nlayer B metal1\nlayer C");
  CHECK(mask::test::texts_of(r.messages) ==
        std::vector<std::string>{"Layer \"A\" contains no material.", "Layer \"C\" contains no material."});
  CHECK(r.messages[0].code == "MASK-STYLE-002");
  CHECK(r.style().layers.size() == 3);
  CHECK(r.style().layers[0].ops.empty());
  // A labels statement counts as material.
  CHECK(read("style s\nscalefactor 1\nlayer A\nlabels metal1\nlayer B metal1").messages.empty());
  const auto args = read("style s\nscalefactor 1\nlayer\ngrow 2");
  CHECK(mask::test::texts_of(args.messages) ==
        std::vector<std::string>{"Wrong number of arguments in layer statement.",
                                 "Must define layer before specifying operation."});
}

TEST_CASE("layer lists of types and earlier mask layers", "[cap:mask-output-styles][req:Layer list syntax]") {
  // Scenario "Mixed list".
  const auto r = read("style s\nscalefactor 1\ntemplayer XN ndiff\nlayer N metal1\nor XN,ndiff");
  const mask::LayerOp& op = r.style().layers[1].ops[1];
  CHECK(op.cif_layers == std::vector<std::size_t>{0});
  CHECK(op.types == r.types({"ndiff"}));
  // Every earlier layer of the name; never the layer being defined.
  const auto dup = read("style s\nscalefactor 1\ntemplayer X ndiff\ntemplayer X pdiff\nlayer X X");
  CHECK(dup.style().layers[2].ops[0].cif_layers == std::vector<std::size_t>{0, 1});
  // A multi-type alias, a parenthesised group and a plane-qualified name.
  const auto alias = read("style s\nscalefactor 1\nlayer A allm1\nlayer B (poly,ndiff)\nlayer C m2c/metal2");
  const tech::TypeMask allm1 = alias.tech->aliases().front().second;
  CHECK((alias.style().layers[0].ops[0].types & allm1) == allm1);
  CHECK(alias.style().layers[1].ops[0].types == alias.types({"poly", "ndiff"}));
  CHECK(alias.style().layers[2].ops[0].types.test(alias.type("m2c")));
  CHECK(alias.messages.empty());
}

TEST_CASE("layer list errors", "[cap:mask-output-styles][req:Layer list syntax]") {
  const auto r = read("style s\nscalefactor 1\ntemplayer metal1 poly\nlayer A metal1,frob,space,,ndiff");
  CHECK(mask::test::texts_of(r.messages) ==
        std::vector<std::string>{"Ambiguous layer (type) \"metal1\".", "Unrecognized layer (type) \"frob\".",
                                 "\"Space\" layer not permitted in CIF rules."});
  CHECK(r.style().layers[1].ops[0].types == r.types({"ndiff"}));
  CHECK(r.style().layers[1].ops[0].cif_layers.empty());
}

TEST_CASE("a * item adds the contacts with the type as a residue", "[cap:mask-output-styles][req:Layer list syntax]") {
  // Scenario "Residue expansion".
  const auto r = read("style s\nscalefactor 1\nlayer A *metal1");
  CHECK(r.style().layers[0].ops[0].types.test(r.type("metal1")));
  CHECK(r.style().layers[0].ops[0].types.test(r.type("pc")));
  CHECK(r.style().layers[0].ops[0].types.test(r.type("ndc")));
  CHECK(r.style().layers[0].ops[0].types.test(r.type("m2c")));
  CHECK_FALSE(r.style().layers[0].ops[0].types.test(r.type("poly")));
}

TEST_CASE("labels map types to label, port text and port layers",
          "[cap:mask-output-styles][req:labels statement and label layer mapping]") {
  // Scenarios "Separate port geometry layer" and "Port only".
  const auto r = read("style s\nscalefactor 1\nlayer L0 poly\nlayer L1 poly\nlayer L2 poly\nlayer M1TXT metal1\n"
                      "labels metal1\nlayer M1PIN metal1\nlabels metal1 port\nlayer L5 metal2\nlabels metal2 port\n"
                      "layer L6 ndiff\nlabels ndiff noport\nlayer L7 pdiff\nlabels pdiff text");
  const mask::OutputStyle& s = r.style();
  const auto m1 = r.type("metal1");
  const auto m2 = r.type("metal2");
  CHECK(s.label_layer[m1] == 3);
  CHECK(s.port_text[m1] == 3);
  CHECK(s.port_layer[m1] == 4);
  CHECK(s.port_layer[m2] == 5);
  CHECK(s.port_text[m2] == 5);
  CHECK(s.label_layer[m2] == mask::kNoLayer);
  CHECK(s.label_layer[r.type("ndiff")] == 6);
  CHECK(s.port_text[r.type("ndiff")] == mask::kNoLayer);
  // `text` is the same as no option (Q-MASK-11).
  CHECK(s.label_layer[r.type("pdiff")] == 7);
  CHECK(s.port_text[r.type("pdiff")] == 7);
  CHECK(s.layers[4].label_port_only);
  CHECK_FALSE(s.layers[3].label_port_only);
  CHECK(r.messages.empty());
}

TEST_CASE("labels statement errors", "[cap:mask-output-styles][req:labels statement and label layer mapping]") {
  const auto r = read("style s\nscalefactor 1\nlabels metal1\ntemplayer T metal1\nlabels metal1\n"
                      "labels metal1 sideways");
  CHECK(mask::test::texts_of(r.messages) ==
        std::vector<std::string>{"Must define layer before giving labels it holds.",
                                 "Why are you attaching labels to a temporary layer?",
                                 "Why are you attaching labels to a temporary layer?",
                                 "Unknown option sideways for labels statement.",
                                 "Wrong number of arguments in labels statement."});
  // The temporary-layer warning still maps the label.
  CHECK(r.style().label_layer[r.type("metal1")] == 0);
}

TEST_CASE("calma statements set the GDS layer and datatype", "[cap:mask-output-styles][req:GDS layer mapping statement]") {
  // Scenario "Out of range still stored".
  const auto r = read("style s\nscalefactor 1\nlayer A metal1\ncalma 300 0\nlayer B metal2\ngds 5 x7\n"
                      "templayer T poly\ncalma 1 2\nlayer C metal1\ncalma 1");
  CHECK(mask::test::texts_of(r.messages) ==
        std::vector<std::string>{"Calma layer and type numbers must be 0 to 255.",
                                 "Calma layer and type numbers must be 0 to 255.",
                                 "Why assign a Calma number to a temporary layer?",
                                 "Wrong number of arguments in calma statement."});
  CHECK(r.style().layers[0].gds == mask::GdsLayer{300, 0});
  CHECK_FALSE(r.style().layers[0].gds_written());
  CHECK(r.style().layers[1].gds == mask::GdsLayer{5, 0});
  CHECK(r.style().layers[2].gds == mask::GdsLayer{1, 2});
  CHECK_FALSE(r.style().layers[2].gds_written());  // temporary layers are never written
  CHECK(read("style s\nscalefactor 1\ncalma 1 2").texts() ==
        std::vector<std::string>{"Must define layers before giving their Calma types."});
}

TEST_CASE("min-width, render and options", "[cap:mask-output-styles][req:min-width, render and options statements]") {
  // Scenario "Unknown option ignored".
  const auto r = read("style s\nscalefactor 1\nlayer A metal1\nmin-width 30\noptions grow-euclidean frobnicate\n"
                      "options calma-permissive-labels set-minimum-grid see-no-vendor no-errors string-limit");
  CHECK(r.messages.empty());
  const mask::OutputStyle& s = r.style();
  CHECK(s.layers[0].min_width == 30);
  CHECK(s.has_option(mask::style_option::grow_slivers));
  CHECK(s.has_option(mask::style_option::grow_euclidean));
  CHECK(s.has_option(mask::style_option::permissive_labels));
  CHECK(s.has_option(mask::style_option::minimum_grid));
  CHECK(s.has_option(mask::style_option::no_errors));
  CHECK(s.has_option(mask::style_option::string_limit));
  CHECK(s.see_no_vendor);  // Q-MASK-401: stored, no effect
  // render clears the current layer; a later operation needs a new layer.
  const auto render = read("style s\nscalefactor 1\nlayer A metal1\nrender A metal1 0.1 0.2\ngrow 2\nrender A\n"
                           "options\nmin-width 3");
  CHECK(render.texts() == std::vector<std::string>{"Must define layer before specifying operation.",
                                                   "Wrong number of arguments in render statement.",
                                                   "Wrong number of arguments in options statement.",
                                                   "Must define layers before assigning a minimum width."});
  CHECK(render.style().layers[0].ops.size() == 1);
}
