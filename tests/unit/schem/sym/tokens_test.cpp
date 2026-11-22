// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Displayed-text tokens (capability schematic-symbols-and-libraries "Text
// substitution tokens"; Q-SCHEM-26, round-1 spec issue N6).
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/sym/symbol.hpp"
#include "hefesto/schem/sym/tokens.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;

namespace {

constexpr const char* kTokens = "[cap:schematic-symbols-and-libraries][req:Text substitution tokens]";

schem::Symbol res_symbol() {
  schem::Document doc;
  doc.kind = schem::FileKind::symbol;
  doc.globals.k = "type=resistor template=\"name=R1 value=1k\"";
  doc.boxes.push_back({5, {-2.5, -32.5}, {2.5, -27.5}, "name=P dir=inout pinnumber=1"});
  doc.boxes.push_back({5, {-2.5, 27.5}, {2.5, 32.5}, "name=M dir=inout pinnumber=2"});
  return schem::make_symbol("res.sym", "/lib/res.sym", doc);
}

struct Fixture {
  schem::Symbol symbol = res_symbol();
  schem::InstanceRecord r7{"devices/res.sym", {0, 0}, 0, 0, "name=R7 value=4.7k"};
  schem::InstanceRecord m1{"devices/res.sym", {0, 0}, 0, 0, "name=M1 W=2 L=1 value=2"};
  diag::CollectingSink sink;

  std::string on(const schem::InstanceRecord& instance, std::string_view text) {
    schem::TextTokenContext context;
    context.instance = &instance;
    context.symbol = &symbol;
    context.warnings = &sink;
    context.net = [](schem::PinIndex pin) { return pin == 0 ? std::string{"VDD"} : std::string{"OUT"}; };
    return schem::substitute_text(text, context);
  }
};

}  // namespace

TEST_CASE("Resistor label", kTokens) {
  Fixture f;
  CHECK(f.on(f.r7, "@name=@value") == "4.7k");
  const auto warnings = f.sink.take();
  REQUIRE(warnings.size() == 1);
  CHECK(warnings[0].code == "SCHEM-SYM-002");
  CHECK(warnings[0].text.find("@name=") != std::string::npos);
  CHECK(warnings[0].text.find("but name is") != std::string::npos);
}

TEST_CASE("Resistor label written with a separator", kTokens) {
  Fixture f;
  CHECK(f.on(f.r7, "@name @value") == "R7 4.7k");
  CHECK(f.sink.take().empty());
}

TEST_CASE("Token values from xschem translate", kTokens) {
  Fixture f;
  CHECK(f.on(f.m1, "@W/@L") == "1");
  CHECK(f.on(f.m1, "@W @L") == "2 1");
  CHECK(f.on(f.r7, "%undef") == "undef");
  CHECK(f.on(f.r7, "@undef").empty());
  CHECK(f.on(f.r7, "@symname") == "res");
  CHECK(f.on(f.r7, "@symname_ext") == "res.sym");
  CHECK(f.on(f.r7, "@symref") == "devices/res.sym");
  CHECK(f.on(f.r7, "@#0:pinnumber") == "1");
  CHECK(f.on(f.r7, "@#1:name") == "M");
}

TEST_CASE("Escaped at sign", kTokens) {
  Fixture f;
  CHECK(f.on(f.r7, "a\\@b @name") == "a@b R7");
}

TEST_CASE("pin tokens give nets and pin attributes", kTokens) {
  Fixture f;
  CHECK(f.on(f.r7, "@@P @@M") == "VDD OUT");
  CHECK(f.on(f.r7, "@#0 @#M @#1:net_name") == "VDD OUT OUT");
  // An instance attribute `attr(pin)=` wins over the pin box.
  const schem::InstanceRecord over{"res.sym", {0, 0}, 0, 0, "name=R2 pinnumber(P)=9"};
  CHECK(f.on(over, "@#0:pinnumber") == "9");
}

TEST_CASE("out-of-range and unknown pin tokens give nothing; a missing pin attribute gives --UNDEF--", kTokens) {
  Fixture f;
  CHECK(f.on(f.r7, "< @#7 >") == "<  >");
  CHECK(f.on(f.r7, "< @#99999999999999999999 >") == "<  >");
  CHECK(f.on(f.r7, "< @#Q:name >") == "<  >");
  CHECK(f.on(f.r7, "@#0:sig_type") == "--UNDEF--");
}

TEST_CASE("a lone @ or % gives nothing and the template supplies defaults", kTokens) {
  Fixture f;
  CHECK(f.on(f.r7, "a @ b c") == "a  c");  // the second character belongs to the token
  CHECK(f.on(f.r7, "x%") == "x");
  const schem::InstanceRecord bare{"res.sym", {0, 0}, 0, 0, "name=R3"};
  CHECK(f.on(bare, "@value") == "1k");
}

TEST_CASE("parentheses keep a token together across blanks", kTokens) {
  Fixture f;
  const schem::InstanceRecord inst{"res.sym", {0, 0}, 0, 0, "name=R4"};
  CHECK(f.on(inst, "@(a b) end") == " end");
}

TEST_CASE("schematic names, index and path", kTokens) {
  schem::TextTokenContext context;
  context.schematic = "/w/sub.sch";
  context.top_schematic = "/w/top.sch";
  context.index = 3;
  context.path = "x1.x2";
  context.lvs_ignore = true;
  CHECK(schem::substitute_text("@schname @schname_ext @topschname @index @path @lvs_ignore", context) ==
        "sub sub.sch top 3 x1.x2 1");
  // No instance: `@key` is empty and `%key` is the key.
  CHECK(schem::substitute_text("@value %value", context) == " value");
}

// Regression (review of PR #80, point 5): every identifier prefix of an
// undefined token was looked up in turn, and each lookup re-read (and
// copied) the instance's property string, so a 400 kB token against 400 kB
// of properties took about 10^11 steps. The defined keys are now read once.
TEST_CASE("an undefined token's prefix is found in linear time", kTokens) {
  Fixture f;
  std::string props = "name=X1 aaa=1";
  for (int i = 0; i < 40000; ++i) props += " k" + std::to_string(i) + "=1";
  const schem::InstanceRecord big{"devices/res.sym", {0, 0}, 0, 0, props};
  const std::string token = "@" + std::string(400000, 'a');
  const auto start = std::chrono::steady_clock::now();
  CHECK(f.on(big, token).empty());
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto warnings = f.sink.take();
  REQUIRE(warnings.size() == 1);
  CHECK(warnings[0].code == "SCHEM-SYM-002");
  CHECK(warnings[0].text.find(" is not defined, but aaa is:") != std::string::npos);
  CHECK(elapsed < std::chrono::seconds{10});
  // A predefined name and a `spice_get_` value count as defined prefixes.
  CHECK(f.on(f.r7, "@symnamex").empty());
  CHECK(f.on(f.r7, "@spice_get_v(x)").empty());
  const auto more = f.sink.take();
  REQUIRE(more.size() == 1);
  CHECK(more[0].text.find(" is not defined, but symname is:") != std::string::npos);
}
