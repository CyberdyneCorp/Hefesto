// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `expr` evaluation (capability schematic-netlisting "expr evaluation",
// Q-SCHEM-12; golden case SCHEM-NL-13; tasks.md 8.2).
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>

#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/expr.hpp"

namespace schem = hefesto::schem;
namespace net_codes = hefesto::schem::net_codes;

namespace {

schem::ExprText run(std::string_view text, bool compat = true) {
  return schem::substitute_expr(text, schem::ExprOptions{compat, 4});
}

std::size_t count_code(const schem::ExprText& r, std::string_view code) {
  std::size_t n = 0;
  for (const auto& d : r.warnings) {
    n += d.code == code ? 1 : 0;
  }
  return n;
}

}  // namespace

TEST_CASE("sky130 drain area", "[cap:schematic-netlisting][req:expr evaluation]") {
  CHECK(run("expr('int((1 + 1)/2) * 1 / 1 * 0.29')").text == "0.29");
}

TEST_CASE("Parameter left symbolic", "[cap:schematic-netlisting][req:expr evaluation]") {
  CHECK(run("expr('int((1 + 1)/2) * W_N / 1 * 0.29')").text == "int((1 + 1)/2) * W_N / 1 * 0.29");
}

TEST_CASE("Minus inside a number, xschem-compat ON", "[cap:schematic-netlisting][req:expr evaluation]") {
  const auto a = run("expr(2-1)", true);
  CHECK(a.text == "2");
  CHECK(count_code(a, net_codes::kExprMinus) == 1);
  const auto b = run("expr(1e-3-1)", true);
  CHECK(b.text == "0.001");
  CHECK(count_code(b, net_codes::kExprMinus) == 1);
}

TEST_CASE("Minus inside a number, xschem-compat OFF", "[cap:schematic-netlisting][req:expr evaluation]") {
  const auto a = run("expr(2-1)", false);
  CHECK(a.text == "1");
  CHECK(count_code(a, net_codes::kExprMinus) == 1);
  const auto b = run("expr(1e-3-1)", false);
  CHECK(b.text == "-0.999");
  CHECK(count_code(b, net_codes::kExprMinus) == 1);
  const auto c = run("expr(2 - 1)", false);
  CHECK(c.text == "1");
  CHECK(c.warnings.empty());
  CHECK(run("expr(2 - 1)", true).warnings.empty());
}

TEST_CASE("Engineering notation and division by zero", "[cap:schematic-netlisting][req:expr evaluation]") {
  CHECK(run("expr_eng(1.5e-7)").text == "150n");
  CHECK(run("expr_eng4(123456)").text == "123.5k");
  CHECK(run("expr(W_N*2)").text == "W_N*2");
  CHECK(run("expr(1/0)").text == "inf");
}

TEST_CASE("SCHEM-NL-13 element values", "[cap:schematic-netlisting][req:expr evaluation]") {
  // The expected values of tests/golden/SCHEM/cases/SCHEM-NL-13/expected/nl13.spice.
  CHECK(run("R1 expr(2-1)").text == "R1 2");
  CHECK(run("R2 expr(2 - 1)").text == "R2 1");
  CHECK(run("R3 expr(1e-3-1)").text == "R3 0.001");
  CHECK(run("R4 expr_eng(1.5e-7)").text == "R4 150n");
  CHECK(run("R5 expr(W_N*2)").text == "R5 W_N*2");
  CHECK(run("R6 expr('int((1 + 1)/2) * 1 / 1 * 0.29')").text == "R6 0.29");
  CHECK(run("R7 expr_eng4(123456)").text == "R7 123.5k");
  CHECK(run("R8 expr(1/0)").text == "R8 inf");
}

TEST_CASE("expr: numbers, SI suffixes, constants and functions", "[cap:schematic-netlisting][req:expr evaluation]") {
  CHECK(schem::evaluate_expr("1k", true) == "1000");
  CHECK(schem::evaluate_expr("2meg", true) == "2000000");
  CHECK(schem::evaluate_expr("2MEG", true) == "2000000");
  CHECK(schem::evaluate_expr("2M", true) == "2000000");
  CHECK(schem::evaluate_expr("2m", true) == "0.002");
  CHECK(schem::evaluate_expr("3x", true) == "3000000");
  CHECK(schem::evaluate_expr("1u+1n", true) == "1.001e-06");
  CHECK(schem::evaluate_expr("5f", true) == "5e-15");
  CHECK(schem::evaluate_expr("1T*1G", true) == "1e+21");
  CHECK(schem::evaluate_expr("2a", true) == "2e-18");
  CHECK(schem::evaluate_expr(".5", true) == "0.5");
  CHECK(schem::evaluate_expr("1e", true) == "1");      // %lf leaves the `e`
  CHECK(schem::evaluate_expr("0x10", true) == "16");   // %lf reads hexadecimal
  CHECK(schem::evaluate_expr("pi", true) == "3.14159265358979");
  CHECK(schem::evaluate_expr("abszero+c*0", true) == "273.15");
  CHECK(schem::evaluate_expr("int(-2.7)", true) == "-2");
  CHECK(schem::evaluate_expr("round(2.5)", true) == "3");
  CHECK(schem::evaluate_expr("round6(1.23456789)", true) == "1.23457");
  CHECK(schem::evaluate_expr("round1(0.0372)", true) == "0.04");
  CHECK(schem::evaluate_expr("log(1000)", true) == "3");
  CHECK(schem::evaluate_expr("ln(exp(2))", true) == "2");
  CHECK(schem::evaluate_expr("sqrt(16)", true) == "4");
}

TEST_CASE("expr: operator precedence and associativity", "[cap:schematic-netlisting][req:expr evaluation]") {
  CHECK(schem::evaluate_expr("1 + 2 * 3", true) == "7");
  CHECK(schem::evaluate_expr("-2^2", true) == "-4");
  CHECK(schem::evaluate_expr("2^3^2", true) == "512");
  CHECK(schem::evaluate_expr("7 % 3", true) == "1");
  CHECK(schem::evaluate_expr("7.9 % 3.2", true) == "1");
  CHECK(schem::evaluate_expr("1 < 2 == 1", true) == "1");
  CHECK(schem::evaluate_expr("2 >= 3", true) == "0");
  CHECK(schem::evaluate_expr("1 ? 2 : 3", true) == "2");
  CHECK(schem::evaluate_expr("0 ? 2 : 0 ? 3 : 4", true) == "4");
  CHECK(schem::evaluate_expr("(1 + 2) * 3", true) == "9");
  CHECK(schem::evaluate_expr("10 / 4", true) == "2.5");
}

TEST_CASE("expr: errors copy the rest of the text without markers",
          "[cap:schematic-netlisting][req:expr evaluation]") {
  CHECK(run("a expr(1+1) b expr(foo) c expr(2) d").text == "a 2 b foo c 2 d");
  CHECK(run("expr(1 +)").text == "1 +");
  CHECK(run("expr(nosuch(1))").text == "nosuch(1)");
  CHECK(run("x expr(1").text == "x 1");
  CHECK(run("expr(1) (kept)").text == "1 (kept)");
  CHECK(run("no markers here").text == "no markers here");
  CHECK(run("expr()").text.empty());
}

TEST_CASE("expr: engineering notation thresholds", "[cap:schematic-netlisting][req:expr evaluation]") {
  CHECK(schem::format_engineering(0.0, 4) == "0");
  CHECK(schem::format_engineering(1e-24, 4) == "0");
  CHECK(schem::format_engineering(0.5, 4) == "0.5");
  CHECK(schem::format_engineering(0.05, 4) == "50m");
  CHECK(schem::format_engineering(2.5e9, 4) == "2.5G");
  CHECK(schem::format_engineering(3.3e6, 4) == "3.3MEG");
  CHECK(schem::format_engineering(-4.7e3, 4) == "-4.7k");
  CHECK(schem::format_engineering(1e13, 4) == "10T");
  CHECK(schem::format_engineering(1e-20, 4) == "0.01a");
  CHECK(schem::format_engineering(123456, 2) == "1.2e+02k");
  CHECK(run("expr_eng(1.23456e-4)").text == "123.5u");
}

TEST_CASE("expr: %.15g printing is locale independent", "[cap:schematic-netlisting][req:expr evaluation]") {
  CHECK(schem::format_g(0.1 + 0.2, 15) == "0.3");
  CHECK(schem::format_g(1e-5, 15) == "1e-05");
  CHECK(schem::format_g(123456789012345678.0, 15) == "1.23456789012346e+17");
  CHECK(schem::format_g(-std::numeric_limits<double>::infinity(), 15) == "-inf");
  CHECK(schem::format_g(std::nan(""), 15) == "nan");
  CHECK(schem::format_g(-0.0, 15) == "-0");
}

TEST_CASE("expr: number lexing branch point (Q-SCHEM-12)", "[cap:schematic-netlisting][req:expr evaluation]") {
  const auto on = schem::lex_number("2-1)", true);
  CHECK(on.length == 3);
  CHECK(on.value == 2.0);
  CHECK(on.mode_dependent);
  const auto off = schem::lex_number("2-1)", false);
  CHECK(off.length == 1);
  CHECK(off.mode_dependent);
  const auto exponent = schem::lex_number("1e-3", false);
  CHECK(exponent.length == 4);
  CHECK_FALSE(exponent.mode_dependent);
  CHECK(schem::lex_number("1e999", true).value == std::numeric_limits<double>::infinity());
  CHECK(schem::lex_number("1e-999", true).value == 0.0);
  CHECK(schem::lex_number("0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                          "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                          "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                          "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
                          true)
            .value == 0.0);
}

TEST_CASE("expr: hostile nesting fails without exhausting the stack",
          "[cap:schematic-netlisting][req:expr evaluation]") {
  std::string deep = "expr(" + std::string(100000, '(') + "1" + std::string(100000, ')') + ")";
  const auto r = run(deep);
  CHECK(count_code(r, net_codes::kSizeLimit) == 1);
  CHECK(r.text.size() == 200001);  // copied without the marker
  std::string minus = "expr(" + std::string(100000, '-') + "1)";
  CHECK(count_code(run(minus), net_codes::kSizeLimit) == 1);
  std::string within = "expr(" + std::string(50, '(') + "1" + std::string(50, ')') + ")";
  CHECK(run(within).text == "1");
}
