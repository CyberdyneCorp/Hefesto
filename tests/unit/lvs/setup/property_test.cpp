// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-setup-file: every `property` sub-command (Q-LVS-6). Netlists and
// setup texts are Hefesto-authored.
//
// Owner: LVSSETUP.

#include <catch2/catch_test_macros.hpp>

#include "setup_fixture.hpp"

namespace {

using setup_test::logged;
using setup_test::logged_error;
using setup_test::Pair;
using setup_test::property;
namespace lvs = hefesto::lvs;
namespace rule = hefesto::lvs::merge_rule;

constexpr std::string_view kDevices =
    ".subckt top a b g\n"
    "M1 a g b b nfet W=1 L=0.15 as=1 ad=1 ps=1 pd=1\n"
    "R1 a b 1k\n"
    "C1 a b 1p\n"
    "L1 a b 1n\n"
    "X1 a b cap_mim w=2 l=3\n"
    "X2 a b cap_mim w=4 l=1 area=9\n"
    ".ends\n"
    ".subckt cap_mim p n\n.ends\n";

}  // namespace

TEST_CASE("property default sets the merge rules and drops diffusion keys", "[cap:lvs-setup-file][req:property default]") {
  Pair pair(kDevices);
  const auto state = pair.ok("property {* nfet} add as ad ps pd\nproperty default\n");
  lvs::Cell& fet = pair.cell(1, "nfet");
  CHECK(property(fet, "as") == nullptr);
  CHECK(property(fet, "pd") == nullptr);
  CHECK(property(fet, "W")->merge == (rule::kParallelAdd | rule::kParallelCritical));
  lvs::Cell& res = pair.cell(1, "r");
  CHECK(property(res, "value")->merge == (rule::kSeriesAdd | rule::kParallelPar));
  CHECK(res.flags.series_combinable);
  CHECK(property(pair.cell(1, "c"), "value")->merge == (rule::kParallelAdd | rule::kSeriesPar));
  CHECK(pair.cell(1, "l").flags.series_combinable);
  CHECK(logged(state, "No property w found for device r"));
  CHECK(logged(state, "No property l found for device r"));
}

TEST_CASE("property add with types and tolerances", "[cap:lvs-setup-file][req:property add and type declarations]") {
  Pair pair;
  const auto state = pair.ok(
      "property \"-circuit2 nfet\" add {nf integer 0} {w2 double 0.02}\n"
      "property \"-circuit2 nfet\" {mult integer} {model string 3} {area 0.05} {f value} {e expression 2*w}\n"
      "property \"-circuit2 nfet\" add nf\n"
      "puts [property \"-circuit2 nfet\"]\n");
  INFO(setup_test::log_text(state));
  lvs::Cell& fet = pair.cell(2, "nfet");
  CHECK(property(fet, "nf")->type == lvs::PropertyType::integer);
  CHECK(property(fet, "nf")->tolerance == 0.0);
  CHECK(property(fet, "w2")->tolerance == 0.02);
  CHECK(property(fet, "mult")->default_value == "0");
  CHECK(property(fet, "model")->type == lvs::PropertyType::string);
  CHECK(property(fet, "model")->tolerance == 3.0);
  CHECK(property(fet, "area")->type == lvs::PropertyType::real);
  CHECK(property(fet, "area")->tolerance == 0.05);
  CHECK(property(fet, "f")->type == lvs::PropertyType::value);
  CHECK(property(fet, "e")->type == lvs::PropertyType::expression);
  CHECK(property(pair.cell(1, "nfet"), "nf") == nullptr);
  CHECK(logged(state, "Device nfet already has property named \"nf\""));
  CHECK(logged(state, "{L double 0.01} {W double 0.01} {M integer 0} {nf integer 0} "
                      "{w2 double 0.02} {mult integer 0} {model string 3.0} {area double 0.05} {f value 0.01} "
                      "{e expression 0.0}"));
}

namespace {

// Q-LVS-6: the single-word rule is copied (a numeric key is a double with
// 0.01, a word a string with 0) and a non-numeric {key tol} is an error,
// in both modes.
void single_word_and_bad_tolerance(bool compat) {
  Pair pair;
  const auto state = pair.ok(
      "property {nfet 0} add sw 42\n"
      "property {nfet 0} add {t abc}\n",
      compat);
  lvs::Cell& fet = pair.cell(1, "nfet");
  CHECK(property(fet, "sw")->type == lvs::PropertyType::string);
  CHECK(property(fet, "sw")->tolerance == 0.0);
  CHECK(property(fet, "42")->type == lvs::PropertyType::real);
  CHECK(property(fet, "42")->tolerance == 0.01);
  CHECK(property(fet, "t") == nullptr);
  CHECK(logged_error(state, "Error setup.tcl:2 (ignoring), Property t: tolerance \"abc\" is not a number"));
}

}  // namespace

TEST_CASE("property add single-word rule and bad tolerance (Q-LVS-6, netgen-compat ON)",
          "[cap:lvs-setup-file][req:property add and type declarations]") {
  single_word_and_bad_tolerance(true);
}

TEST_CASE("property add single-word rule and bad tolerance (Q-LVS-6, netgen-compat OFF)",
          "[cap:lvs-setup-file][req:property add and type declarations]") {
  single_word_and_bad_tolerance(false);
}

TEST_CASE("A third word that is no option adds and keeps the bad-option result",
          "[cap:lvs-setup-file][req:property add and type declarations]") {
  Pair pair;
  const auto state = pair.ok("puts [property {nfet 0} {zz integer}]\nputs [property {nfet 0} mer]\n");
  CHECK(logged(state, "bad option \"zz integer\": must be add, create, remove, delete, tolerance, merge, serial, "
                      "series, parallel, associate, or derive"));
  CHECK(property(pair.cell(1, "nfet"), "zz")->type == lvs::PropertyType::integer);
  CHECK(property(pair.cell(1, "nfet"), "mer") == nullptr);  // a unique prefix of `merge` is that option
  CHECK(state.errors == 0);
}

TEST_CASE("property remove, delete all, tolerance and associate", "[cap:lvs-setup-file][req:property remove, tolerance and associate]") {
  Pair pair;
  const auto state = pair.ok(
      "property \"-circuit1 nfet\" delete M\n"
      "property \"-circuit1 nfet\" delete nosuch\n"
      "property \"-circuit1 nfet\" tolerance {W inf} {L 0.05} {M 2}\n"
      "puts [property \"-circuit1 nfet\" tolerance bad]\n"
      "property \"-circuit1 nfet\" associate {W source} {L drain}\n"
      "puts [property \"-circuit1 nfet\" associate {W}]\n"
      "property \"-circuit1 pfet\" remove\n");
  lvs::Cell& fet = pair.cell(1, "nfet");
  CHECK(property(fet, "M") == nullptr);
  CHECK(logged(state, "No property nosuch found for device nfet"));
  CHECK(property(fet, "W")->tolerance == 1e300);
  CHECK(property(fet, "L")->tolerance == 0.05);
  CHECK(logged(state, "Not a {key tolerance} pair list."));
  CHECK(property(fet, "W")->pin == "source");
  CHECK(property(fet, "L")->pin == "drain");
  CHECK(logged(state, "Not a {key pin} pair list."));
  CHECK(pair.cell(1, "pfet").properties.empty());
  CHECK_FALSE(pair.cell(2, "pfet").properties.empty());
}

TEST_CASE("An infinite integer tolerance is 2^30", "[cap:lvs-setup-file][req:property remove, tolerance and associate]") {
  Pair pair;
  pair.ok("property {nfet 0} tolerance {M Infinity}\n");
  CHECK(property(pair.cell(1, "nfet"), "M")->tolerance == static_cast<double>(1U << 30U));
}

TEST_CASE("sky130-style series and parallel controls", "[cap:lvs-setup-file][req:property series and parallel controls]") {
  Pair pair(".subckt top a b\nX1 a b r1 w=1 l=2\n.ends\n.subckt r1 p n\n.ends\n");
  pair.ok(
      "property \"-circuit1 r1\" add {w double} {l double}\n"
      "property \"-circuit1 r1\" series enable\n"
      "property \"-circuit1 r1\" series {w critical}\n"
      "property \"-circuit1 r1\" series {l add}\n"
      "property \"-circuit1 r1\" parallel {w add} {l par}\n");
  lvs::Cell& r1 = pair.cell(1, "r1");
  CHECK(r1.flags.series_combinable);
  CHECK(property(r1, "w")->merge == (rule::kSeriesCritical | rule::kParallelAdd));
  CHECK(property(r1, "l")->merge == (rule::kSeriesAdd | rule::kParallelPar));
}

TEST_CASE("series par replaces the series rule; none disables", "[cap:lvs-setup-file][req:property series and parallel controls]") {
  Pair pair;
  const auto state = pair.ok(
      "property {nfet 0} series {W add} {W critical}\n"
      "property {nfet 0} series {W par}\n"
      "puts [property {nfet 0} series]\n"
      "property {nfet 0} parallel {L none}\n"
      "puts [property {nfet 0} parallel]\n"
      "puts [property {nfet 0} parallel off {bad}]\n"
      "property {nfet 0} parallel yes\n");
  lvs::Cell& fet = pair.cell(1, "nfet");
  CHECK(property(fet, "W")->merge == rule::kSeriesPar);
  CHECK(logged(state, "1"));
  CHECK(logged(state, "0"));
  CHECK(logged(state, "Not a {key merge_type} pair list."));
  CHECK_FALSE(fet.flags.no_parallel);
}

TEST_CASE("Global parallel and series forms", "[cap:lvs-setup-file][req:property series and parallel controls]") {
  Pair pair;
  auto state = pair.ok("property parallel none\nproperty series all\n");
  CHECK(pair.cell(1, "nfet").flags.no_parallel);
  CHECK(pair.cell(2, "pfet").flags.no_parallel);
  CHECK(pair.cell(2, "pfet").flags.series_combinable);
  CHECK(state.parallel_none);
  state = pair.ok("property parallel all\nproperty parallel connected\nproperty series none\nproperty parallel bogus\n"
                  "property series open\n");
  CHECK_FALSE(pair.cell(1, "nfet").flags.no_parallel);
  CHECK_FALSE(pair.cell(1, "nfet").flags.series_combinable);
  CHECK_FALSE(state.parallel_open);
  CHECK(logged_error(state, "Error setup.tcl:4 (ignoring), Bad option, should be property parallel none|all|connected"));
  CHECK(logged_error(state, "Error setup.tcl:5 (ignoring), Bad option, should be property series none|all"));
  state = pair.ok("property parallel open\n");
  CHECK(state.parallel_open);
}

TEST_CASE("Deprecated merge replaces the whole rule", "[cap:lvs-setup-file][req:property series and parallel controls]") {
  Pair pair;
  pair.ok(
      "property {nfet 0} series {W add}\n"
      "property {nfet 0} merge {W par_critical} {L ser_critical}\n"
      "property {nfet 1} merge {W none} {L series}\n");
  CHECK(property(pair.cell(1, "nfet"), "W")->merge == (rule::kParallelPar | rule::kLegacyParallelCritical));
  CHECK(property(pair.cell(1, "nfet"), "L")->merge == (rule::kSeriesAdd | rule::kLegacySeriesCritical));
  CHECK(property(pair.cell(2, "nfet"), "W")->merge == 0);
  CHECK(property(pair.cell(2, "nfet"), "L")->merge == rule::kSeriesAdd);
}

TEST_CASE("property topology", "[cap:lvs-setup-file][req:property topology and derive]") {
  Pair pair;
  auto state = pair.ok("puts [property topology]\nproperty topology strict\nputs [property topology]\n");
  CHECK(logged(state, "Relaxed topology property matching."));
  CHECK(logged(state, "Strict topology property matching."));
  CHECK(state.strict_topology);
}

TEST_CASE("property derive area and perimeter", "[cap:lvs-setup-file][req:property topology and derive]") {
  Pair pair(kDevices);
  const auto state = pair.ok("property {cap_mim 0} derive area w l\nproperty {cap_mim 1} derive perimeter w l\n"
                             "property cap_mim derive area\n");
  lvs::Cell& top = pair.cell(1, "top");
  const auto value = [](const lvs::Instance& instance, std::string_view key) {
    const auto* p = lvs::find_property(instance.properties.front(), key);
    return p != nullptr ? p->value : std::string{"-"};
  };
  CHECK(value(top.instances.at(4), "area") == "6");
  CHECK(value(top.instances.at(5), "area") == "9");  // already present: kept
  CHECK(property(pair.cell(1, "cap_mim"), "area")->tolerance == 0.01);
  CHECK(value(pair.cell(2, "top").instances.at(4), "perimeter") == "10");
  CHECK(logged_error(state,
                     "Error setup.tcl:3 (ignoring), wrong # args: should be \"property <valid_cellname> derive "
                     "area|perimeter <wkey> <lkey>\""));
}
