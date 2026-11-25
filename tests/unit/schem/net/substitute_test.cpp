// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Format string substitution and hierarchical parameters (capability
// schematic-netlisting "Format string substitution", "Hierarchical
// parameters", "Unnamed nets", Q-SCHEM-26; golden cases SCHEM-NL-11, NL-13,
// NL-19; tasks.md 8.2). Attributes are given as decoded property maps, so
// these tests do not depend on the property-string reader.
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/connectivity.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/net/substitute.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"

namespace schem = hefesto::schem;
namespace net_codes = hefesto::schem::net_codes;

namespace {

/// `res.sym` as the golden cases use it: pins P and M.
const schem::PropertyMap kResSymbol{{"type", "resistor"}, {"format", "@name @pinlist @value m=@m"}};
const schem::PropertyMap kResTemplate{{"name", "R1"}, {"value", "1k"}, {"m", "1"}};

schem::ElementContext resistor(const schem::PropertySource& instance, std::string p, std::string m) {
  schem::ElementContext c;
  c.instance = &instance;
  c.symbol = &kResSymbol;
  c.templ = &kResTemplate;
  c.symref = "res.sym";
  c.pins = {{"P", 1, std::move(p), false, nullptr}, {"M", 1, std::move(m), false, nullptr}};
  return c;
}

schem::ElementText build(const schem::ElementContext& c, const schem::ElementOptions& o = {}) {
  schem::ElementRunState state;
  return schem::build_element(c, o, state);
}

std::size_t count_code(const schem::ElementText& t, std::string_view code) {
  std::size_t n = 0;
  for (const auto& d : t.diagnostics) {
    if (d.code == code) ++n;
  }
  return n;
}

std::string with_format(std::string format) {
  const schem::PropertyMap instance{{"name", "X1"}, {"W", "2"}, {"L", "1"}, {"format", std::move(format)}};
  return build(resistor(instance, "A", "B")).text;
}

}  // namespace

TEST_CASE("Resistor element text", "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap instance{{"name", "R1"}, {"value", "1k"}, {"m", "1"}};
  const auto t = build(resistor(instance, "A", "B"));
  CHECK(t.text == "R1 ?1 A ?1 B  1k m=1\n");
  CHECK(t.diagnostics.empty());
}

TEST_CASE("Token ends at whitespace only", "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap instance{{"name", "M1"}, {"W", "2"}, {"L", "1"}, {"format", "@W/@L"}};
  const auto t = build(resistor(instance, "A", "B"));
  CHECK(t.text == "1\n");
  REQUIRE(count_code(t, net_codes::kTokenPrefix) == 1);
  const auto& warning = t.diagnostics.front();
  CHECK(warning.severity == hefesto::diag::Severity::warning);
  CHECK(warning.text == "token '@W/' is undefined while 'W' is defined");
}

TEST_CASE("format substitution: tokens, escapes and separators",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  CHECK(with_format("@W\\/@L") == "2/1\n");         // an escaped character ends a token and is copied
  CHECK(with_format("@W @L") == "2 1\n");
  CHECK(with_format("@W;@L") == "2;1\n");
  CHECK(with_format("a\\@b @name") == "a@b X1\n");  // escaped at sign
  CHECK(with_format("line\\\nnext") == "linenext\n");  // escaped LF dropped
  CHECK(with_format("%undef @undef") == "undef \n");
  CHECK(with_format("@symname @symname_ext @symref") == "res res.sym res.sym\n");
  CHECK(with_format("@@P @@M") == "?1 A  ?1 B \n");
  CHECK(with_format("@@P/@@M") == "?1 B \n");  // `/` does not end a token: pin `P/` is unknown
  CHECK(with_format("@#1 @#1:net_name @#5:name @#1:foo") == "?1 B  B  --UNDEF--\n");
  CHECK(with_format("@#M") == "?1 B \n");
  CHECK(with_format("@index") == "0\n");
  CHECK(with_format("") == "\n");
}

TEST_CASE("format substitution: no format produces nothing", "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap instance{{"name", "L1"}};
  auto c = resistor(instance, "A", "B");
  const schem::PropertyMap no_format{{"type", "label"}};
  c.symbol = &no_format;
  CHECK(build(c).text.empty());
}

TEST_CASE("format substitution: format preference and LVS formats",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap symbol{{"format", "sym"}, {"lvs_format", "symlvs"}};
  const schem::PropertyMap plain{{"name", "R1"}};
  const schem::PropertyMap own{{"name", "R1"}, {"format", "inst"}, {"lvs_format", "instlvs"}};
  auto c = resistor(plain, "A", "B");
  c.symbol = &symbol;
  CHECK(schem::select_format(c, false) == "sym");
  CHECK(schem::select_format(c, true) == "symlvs");
  c.instance = &own;
  CHECK(schem::select_format(c, false) == "inst");
  CHECK(schem::select_format(c, true) == "instlvs");
}

TEST_CASE("format substitution: pinlist skips spice_ignore pins and repeated names",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap instance{{"name", "X1"}, {"format", "@name @pinlist"}};
  auto c = resistor(instance, "A", "B");
  c.pins.push_back({"P", 1, "A", false, nullptr});  // pass-through copy of P
  c.pins.push_back({"S", 1, "SUB", true, nullptr});
  c.pins.push_back({"D", 4, "D[3],D[2],D[1],D[0]", false, nullptr});
  CHECK(build(c).text == "X1 ?1 A ?1 B ?4 D[3],D[2],D[1],D[0] \n");
}

TEST_CASE("format substitution: unconnected pins", "[cap:schematic-netlisting][req:Unnamed nets]") {
  const schem::PropertyMap instance{{"name", "X1"}, {"format", "@pinlist"}};
  auto c = resistor(instance, "", "B");
  c.index = 3;
  c.pins.push_back({"Q", 2, "", false, nullptr});
  schem::ElementRunState state;
  state.unconnected = 4;
  const auto t = schem::build_element(c, {}, state);
  CHECK(t.text == "?1 __UNCONNECTED_PIN__4 ?1 B ?2 __UNCONNECTED_PIN__5_1,__UNCONNECTED_PIN__5_0 \n");
  CHECK(state.unconnected == 6);
  REQUIRE(count_code(t, net_codes::kUnconnectedPin) == 2);
  CHECK(t.diagnostics[0].text == "unconnected pin,  Inst idx: 3, Pin idx: 0  Inst:X1");
}

TEST_CASE("format substitution: vector names, bus replacement and lab",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap instance{{"name", "R4[1:0]"}, {"value", "4"}, {"lab", "B[1:0]"}};
  schem::ElementOptions o;
  CHECK(build(resistor(instance, "net4_1,net4_0", "ADDR[5],ADDR[4]"), o).text ==
        "R4[1],R4[0] ?1 net4_1,net4_0 ?1 ADDR[5],ADDR[4]  4 m=1\n");
  o.bus_replacement = "<>";
  const schem::PropertyMap with_lab{{"name", "R1[1:0]"}, {"lab", "B[1:0]"}, {"format", "@name @lab"}};
  CHECK(build(resistor(with_lab, "A", "B"), o).text == "R1<1>,R1<0> B<1>,B<0>\n");
  const schem::PropertyMap bad{{"name", "R1[1:"}, {"format", "@name"}};
  const auto t = build(resistor(bad, "A", "B"), o);
  CHECK(t.text == "R1<1:\n");
  CHECK(count_code(t, net_codes::kBusSyntax) == 1);
}

TEST_CASE("format substitution: the instance name falls back to the template",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap instance{{"value", "3"}};
  CHECK(build(resistor(instance, "A", "B")).text == "R1 ?1 A ?1 B  3 m=1\n");
}

TEST_CASE("format substitution: spiceprefix and savecurrent", "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap templ{{"spiceprefix", "X"}, {"model", "nfet_01v8"}};
  const schem::PropertyMap instance{{"name", "M1"}, {"savecurrent", "true"}, {"format", "@spiceprefix@name @model@savecurrent"}};
  auto c = resistor(instance, "A", "B");
  c.templ = &templ;
  schem::ElementOptions o;
  auto t = build(c, o);
  CHECK(t.text == "M1 nfet_01v8\n.save I( ?1 M1 )\n");
  CHECK(t.spice_prefix == "X");
  o.spiceprefix = false;
  t = build(c, o);
  CHECK(t.spice_prefix.empty());
  const schem::PropertyMap off{{"name", "M1"}, {"savecurrent", "0"}, {"format", "@name@savecurrent"}};
  CHECK(build(resistor(off, "A", "B")).text == "M1\n");
}

TEST_CASE("format substitution: tcleval fails without a permitted interpreter",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap instance{{"name", "C1"}, {"format", "tcleval(.lib @name )"}};
  CHECK(build(resistor(instance, "A", "B")).text == "?\n\n");  // the failure text, then the line's LF
  const schem::PropertyMap token{{"name", "C1"}, {"format", "x @tcleval(1)"}};
  CHECK(build(resistor(token, "A", "B")).text == "x ?\n\n");
}

// Regression (SCHEM-NL-07/NL-08 replay, M8 round 5): a format line in
// tcleval(...) form is evaluated before its LF is appended. The LF was
// appended first and then removed with the blanks after the last `)`, so
// the IHP model block `tcleval( @value )` ran into the next netlist line.
TEST_CASE("format substitution: a tcleval line gets its LF after the result",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  hefesto::editor::EditorState editor;
  hefesto::platform::LocalFileSystem files;
  schem::TclEnvironment tcl(editor, {}, files, {});
  const schem::TclBridge bridge = tcl.bridge();
  schem::ElementOptions options;
  options.tcl = &bridge;
  const schem::PropertyMap instance{{"name", "MODEL"}, {"value", "\n.lib x.lib cap_typ\n"},
                                    {"format", "tcleval( @value )"}};
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    options.xschem_compat = compat;
    CHECK(build(resistor(instance, "A", "B"), options).text == " \n.lib x.lib cap_typ\n \n");
  }
}

TEST_CASE("format substitution: expr in values and on the whole line",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap templ{{"ad", "expr('int((@nf + 1)/2) * @W / @nf * 0.29')"}};
  const schem::PropertyMap instance{{"name", "M1"}, {"nf", "1"}, {"W", "1"}, {"format", "@name ad=@ad expr(1+1)"}};
  auto c = resistor(instance, "A", "B");
  c.templ = &templ;
  CHECK(build(c).text == "M1 ad=0.29 2\n");
}

TEST_CASE("SCHEM-NL-13 element texts", "[cap:schematic-netlisting][req:Format string substitution]") {
  // Before post-processing, which removes `?1` and the doubled space (8.3):
  // expected/nl13.spice has `R1 net1 net2 2` ... `R8 net15 net16 inf`.
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"expr(2-1)", "2"},          {"expr(2 - 1)", "1"},     {"expr(1e-3-1)", "0.001"},
      {"expr_eng(1.5e-7)", "150n"}, {"expr(W_N*2)", "W_N*2"}, {"expr('int((1 + 1)/2) * 1 / 1 * 0.29')", "0.29"},
      {"expr_eng4(123456)", "123.5k"}, {"expr(1/0)", "inf"}};
  for (std::size_t i = 0; i < cases.size(); ++i) {
    const std::string name = "R" + std::to_string(i + 1);
    const schem::PropertyMap instance{{"name", name}, {"value", "1"}, {"format", "@name @pinlist " + cases[i].first}};
    const std::string p = "net" + std::to_string(2 * i + 1);
    const std::string m = "net" + std::to_string(2 * i + 2);
    INFO(name);
    CHECK(build(resistor(instance, p, m)).text == name + " ?1 " + p + " ?1 " + m + "  " + cases[i].second + "\n");
  }
}

TEST_CASE("SCHEM-NL-11 element texts from computed nets", "[cap:schematic-netlisting][req:Format string substitution]") {
  // R1 on the A-B wire: expected/nl11.spice `R1 A net6 1` after post-processing.
  schem::SchematicNets s;
  s.wires = {{{0, 0}, {100, 0}, false}};
  schem::NetInstance l1{"l1", "label", false, false, "A", false, 1, {{"p", {0, 0}, 1}}};
  schem::NetInstance l2{"l2", "label", false, false, "B", false, 1, {{"p", {100, 0}, 1}}};
  schem::NetInstance r1{"R1", "resistor", false, false, std::nullopt, false, 1, {{"P", {50, 0}, 1}, {"M", {50, 60}, 1}}};
  s.instances = {l1, l2, r1};
  const auto n = schem::name_nets(s);
  const schem::PropertyMap instance{{"name", "R1"}, {"value", "1"}};
  const auto c = resistor(instance, schem::net_text(n.pin_nets[2][0], n, ""), schem::net_text(n.pin_nets[2][1], n, ""));
  CHECK(build(c).text == "R1 ?1 A ?1 net1  1 m=1\n");
}

TEST_CASE("Model passed down from the parent template", "[cap:schematic-netlisting][req:Hierarchical parameters]") {
  const schem::PropertyMap parent_template{{"name", "x1"}, {"modeln", "nfet_01v8"}};
  const schem::PropertyMap parent_instance{{"name", "x1"}};
  const schem::PropertyMap nmos_template{{"model", "nfet"}};
  const schem::PropertyMap instance{{"name", "M1"}, {"model", "@modeln"}, {"format", "@name @model"}};
  auto c = resistor(instance, "A", "B");
  c.templ = &nmos_template;
  c.parent_instance = &parent_instance;
  c.parent_template = &parent_template;
  CHECK(build(c).text == "M1 nfet_01v8\n");
  CHECK(schem::resolve_parameter(c, "model") == "nfet_01v8");
}

TEST_CASE("Instance value wins over template", "[cap:schematic-netlisting][req:Hierarchical parameters]") {
  const schem::PropertyMap templ{{"name", "R1"}, {"value", "1k"}};
  const schem::PropertyMap instance{{"name", "R5"}, {"value", "2k"}};
  auto c = resistor(instance, "A", "B");
  c.templ = &templ;
  CHECK(schem::resolve_parameter(c, "value") == "2k");
}

TEST_CASE("hierarchical parameters: parent instance values and unresolved tokens",
          "[cap:schematic-netlisting][req:Hierarchical parameters]") {
  const schem::PropertyMap parent_instance{{"name", "x1"}, {"WP", "4"}};
  const schem::PropertyMap instance{{"name", "M1"}, {"W", "@WP"}, {"L", "@nowhere"}, {"S", "self=@S"}};
  auto c = resistor(instance, "A", "B");
  c.parent_instance = &parent_instance;
  CHECK(schem::resolve_parameter(c, "W") == "4");
  CHECK(schem::resolve_parameter(c, "L") == "");  // still a token: empty
  CHECK(schem::resolve_parameter(c, "S") == "self=self=self=self=@S");  // one substitution per step, no recursion
  CHECK(schem::resolve_parameter(c, "WP") == "4");            // defined only in the parent instance
  CHECK_FALSE(schem::resolve_parameter(c, "absent").has_value());
}

TEST_CASE("hierarchical parameters: names substituted from the parents, nearest first",
          "[cap:schematic-netlisting][req:Hierarchical parameters]") {
  const schem::PropertyMap near{{"bus", "D"}};
  const schem::PropertyMap far{{"bus", "X"}, {"n", "3"}};
  const std::vector<const schem::PropertySource*> parents{&near, &far};
  CHECK(schem::substitute_from_parents("@bus @n", parents) == "D 3");
  CHECK(schem::substitute_from_parents("@bus[1]", parents) == "@bus[1]");  // the token is `@bus[1]`
  CHECK(schem::substitute_from_parents("plain", parents) == "plain");
  CHECK(schem::substitute_from_parents("@zz", parents) == "@zz");
}

TEST_CASE("format substitution: boolean values", "[cap:schematic-netlisting][req:Format string substitution]") {
  CHECK(schem::boolean_value("1") == true);
  CHECK(schem::boolean_value("000") == false);
  CHECK(schem::boolean_value("TRUE") == true);
  CHECK(schem::boolean_value("Off") == false);
  CHECK(schem::boolean_value("yes") == true);
  CHECK_FALSE(schem::boolean_value("TRUEISH").has_value());
  CHECK_FALSE(schem::boolean_value("").has_value());
}

TEST_CASE("format substitution: symbol names", "[cap:schematic-netlisting][req:Format string substitution]") {
  CHECK(schem::symbol_name("ramdec(4, 8)") == "ramdec_4_8");
  CHECK(schem::symbol_name("sky130_fd_pr/nfet_01v8.sym") == "nfet_01v8");
  CHECK(schem::symbol_name("res.sym") == "res");
  CHECK(schem::symbol_name("a b.sym") == "a_b");
}

TEST_CASE("format substitution: pin attributes and slots", "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap pin_attrs{{"name", "P"}, {"pinnumber", "1:5:9"}};
  const schem::PropertyMap instance{{"name", "U3:2"}, {"sig_type(M)", "bit"}, {"format", "@#0:pinnumber @#1:sig_type @#0:name"}};
  auto c = resistor(instance, "A", "B");
  c.pins[0].attributes = &pin_attrs;
  CHECK(build(c).text == "5 bit P\n");
}

TEST_CASE("Token start and lone markers", "[cap:schematic-netlisting][req:Format string substitution]") {
  CHECK(with_format("a @ b c@") == "a  c\n");
}

TEST_CASE("Out-of-range pin number", "[cap:schematic-netlisting][req:Format string substitution]") {
  const schem::PropertyMap instance{{"name", "R1"}, {"format", "@name X@#5 Y@#5:name Z@#1:foo"}};
  const auto t = build(resistor(instance, "A", "B"));
  CHECK(t.text == "R1 X Y Z--UNDEF--\n");
  CHECK(t.diagnostics.empty());
}

TEST_CASE("Template default found in step 2", "[cap:schematic-netlisting][req:Hierarchical parameters]") {
  const schem::PropertyMap symbol{{"type", "resistor"}, {"format", "@name @pinlist @value"}};
  const schem::PropertyMap templ{{"name", "R1"}, {"value", "1k"}};
  const schem::PropertyMap instance{{"name", "R1"}};
  schem::ElementContext c = resistor(instance, "A", "B");
  c.symbol = &symbol;
  c.templ = &templ;
  CHECK(schem::resolve_parameter(c, "value") == std::optional<std::string>{"1k"});
  CHECK(build(c).text == "R1 ?1 A ?1 B  1k\n");
}

TEST_CASE("hierarchical parameters: the parent instance comes before the template in step 2",
          "[cap:schematic-netlisting][req:Hierarchical parameters]") {
  // Step 1 reads the instance only, so a key the parent instance defines
  // wins over the symbol template's default.
  const schem::PropertyMap instance{{"name", "R1"}, {"value", "@v"}};
  const schem::PropertyMap parent{{"v", "5k"}};
  const schem::PropertyMap templ{{"name", "R1"}, {"value", "1k"}, {"v", "2k"}};
  schem::ElementContext c = resistor(instance, "A", "B");
  c.templ = &templ;
  c.parent_instance = &parent;
  CHECK(schem::resolve_parameter(c, "value") == std::optional<std::string>{"5k"});
}

// Regression (review of PR #80, point 5, the netlister's copy): every
// identifier prefix of an undefined token was looked up in turn, and each
// lookup re-read the property string, so a 400 kB token against 400 kB of
// properties took about 10^11 steps. The defined keys are now read once.
TEST_CASE("token prefix warning: linear in the token and the properties",
          "[cap:schematic-netlisting][req:Format string substitution]") {
  const std::string token = "@" + std::string(400000, 'a');
  std::string props = "name=X1 aaa=1 format=\"" + token + "\"";
  for (int i = 0; i < 40000; ++i) props += " k" + std::to_string(i) + "=1";
  const schem::PropertyString instance{props};
  const auto start = std::chrono::steady_clock::now();
  const auto t = build(resistor(instance, "A", "B"));
  const auto elapsed = std::chrono::steady_clock::now() - start;
  REQUIRE(count_code(t, net_codes::kTokenPrefix) == 1);
  const auto warning = std::find_if(t.diagnostics.begin(), t.diagnostics.end(),
                                    [](const auto& d) { return d.code == net_codes::kTokenPrefix; });
  CHECK(warning->text.ends_with("' is undefined while 'aaa' is defined"));
  CHECK(elapsed < std::chrono::seconds{10});
}
