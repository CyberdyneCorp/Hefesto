// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability pcell-toolkit: "Parameter value normalization for naming",
// "Deterministic generated-cell naming", "Legacy random cell names"
// (Q-SCRIPT-21, Q-SCRIPT-22). The number grammar and the hash are also
// checked against the embedded Tcl 8.6 running toolkit.tcl's own procedures.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <regex>
#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/naming.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"

namespace pcell = hefesto::pcell;

namespace {

pcell::ParameterDict dict(std::vector<pcell::ParameterDict::Entry> entries) {
  return pcell::ParameterDict{std::move(entries)};
}

// toolkit.tcl's normalize_value and murmur3_32 (tcltk/toolkit.tcl:1778-1869),
// verbatim, for a differential check against Tcl 8.6.
constexpr const char* kToolkitNaming = R"TCL(
namespace eval ref {}
proc ref::murmur3_32 {key {seed 0}} {
    set length [string length $key]
    set c1 0xcc9e2d51
    set c2 0x1b873593
    set h1 $seed
    set nblocks [expr {$length / 4}]
    for {set i 0} {$i < $nblocks} {incr i} {
        binary scan [string range $key [expr {$i*4}] [expr {$i*4+3}]] i k1
        set k1 [expr {($k1 * $c1) & 0xffffffff}]
        set k1 [expr {(($k1 << 15) | (($k1 & 0xffffffff) >> 17)) & 0xffffffff}]
        set k1 [expr {($k1 * $c2) & 0xffffffff}]
        set h1 [expr {$h1 ^ $k1}]
        set h1 [expr {(($h1 << 13) | (($h1 & 0xffffffff) >> 19)) & 0xffffffff}]
        set h1 [expr {(($h1 * 5) + 0xe6546b64) & 0xffffffff}]
    }
    set k1 0
    set tail_index [expr {$nblocks * 4}]
    set tail [string range $key $tail_index end]
    set remaining [string length $tail]
    if {$remaining >= 3} {
        binary scan [string index $tail 2] c b
        set k1 [expr {$k1 ^ (($b & 0xff) << 16)}]
    }
    if {$remaining >= 2} {
        binary scan [string index $tail 1] c b
        set k1 [expr {$k1 ^ (($b & 0xff) << 8)}]
    }
    if {$remaining >= 1} {
        binary scan [string index $tail 0] c b
        set k1 [expr {$k1 ^ ($b & 0xff)}]
        set k1 [expr {($k1 * $c1) & 0xffffffff}]
        set k1 [expr {(($k1 << 15) | (($k1 & 0xffffffff) >> 17)) & 0xffffffff}]
        set k1 [expr {($k1 * $c2) & 0xffffffff}]
        set h1 [expr {$h1 ^ $k1}]
    }
    set h1 [expr {$h1 ^ $length}]
    set h1 [expr {$h1 ^ (($h1 & 0xffffffff) >> 16)}]
    set h1 [expr {($h1 * 0x85ebca6b) & 0xffffffff}]
    set h1 [expr {$h1 ^ (($h1 & 0xffffffff) >> 13)}]
    set h1 [expr {($h1 * 0xc2b2ae35) & 0xffffffff}]
    set h1 [expr {$h1 ^ (($h1 & 0xffffffff) >> 16)}]
    return $h1
}
proc ref::normalize_value {value} {
    if {[string is double -strict $value]} {
        set num [expr {double($value)}]
        if {abs($num - round($num)) < 1e-9} {
            return [format "%d" [expr {int(round($num))}]]
        }
        set str [format "%.3f" $num]
        regsub {\.?0+$} $str "" str
        return $str
    }
    return $value
}
)TCL";

struct TclReference {
  hefesto::editor::EditorState state;
  std::unique_ptr<hefesto::script::Interpreter> interp =
      hefesto::script::create_project_interpreter(state, hefesto::script::SandboxPolicy{});

  TclReference() { REQUIRE(interp->evaluate(kToolkitNaming).ok()); }

  std::string call(std::string_view proc, std::string_view arg) {
    const std::vector<std::string> words{std::string{proc}, std::string{arg}};
    const auto r = interp->evaluate(pcell::merge_tcl_list(words));
    REQUIRE(r.ok());
    return pcell::tcl_string(r.value);
  }
};

}  // namespace

TEST_CASE("normalization table", "[cap:pcell-toolkit][req:Parameter value normalization for naming]") {
  const std::vector<std::pair<std::string, std::string>> table = {
      {"2", "2"},   {"2.0", "2"},       {"2e0", "2"},    {"0.1500", "0.15"}, {"1.23456", "1.235"}, {"-0.5", "-0.5"},
      {"0.0004", "0"}, {"10.5", "10.5"}, {"0x10", "16"}, {" 3 ", "3"},       {"abc", "abc"}};
  for (const auto& [in, out] : table) {
    INFO(in);
    CHECK(pcell::normalize_value(in) == out);
  }
}

TEST_CASE("list-valued and non-numeric values are verbatim",
          "[cap:pcell-toolkit][req:Parameter value normalization for naming]") {
  CHECK(pcell::normalize_value("sky130_fd_pr__nfet_01v8 sky130_fd_pr__nfet_01v8_lvt") ==
        "sky130_fd_pr__nfet_01v8 sky130_fd_pr__nfet_01v8_lvt");
  CHECK(pcell::normalize_value("a b") == "a b");
  CHECK(pcell::normalize_value("") == "");
}

TEST_CASE("Infinite and NaN values are used verbatim (Q-SCRIPT-21)",
          "[cap:pcell-toolkit][req:Parameter value normalization for naming]") {
  for (const char* v : {"inf", "-inf", "Inf", " INFINITY ", "nan", "-NaN", "+inf"}) {
    INFO(v);
    CHECK(pcell::tcl_strict_double(v).has_value());
    CHECK(pcell::normalize_value(v) == v);
  }
  // A device created with an infinite value gets a generated name.
  CHECK(pcell::generated_cell_name("nfet", dict({{"w", "inf"}})).size() == std::string{"nfet_XXXXXX"}.size());
}

TEST_CASE("the number grammar matches Tcl 8.6 string is double -strict",
          "[cap:pcell-toolkit][req:Parameter value normalization for naming]") {
  TclReference tcl;
  const std::vector<std::string> values = {
      "0", "00", "010", "08", "08.5", "019e1", "0x1F", "0X", "0b101", "0b2", "0o17", "0o8", "1.", ".5", ".",
      "-.5e-3", "1e", "1e+", "1e+2", "+", "-", " 7", "7 ", "\t7\n", "1 2", "1_000", "abc", "0.0004", "-0.0004",
      "2.5", "-2.5", "3.4995", "1.0005", "123456789012", "1e300", "-1e-300", "100.0004", "9.9999999999",
      "4.0000000001", "0.1234", "1e19", "-1e19", "0x7fffffffffffffff", "07777777777777777777777", "1.5e2",
      "0e5", "00.5", "inf", "infinity", "infin", "nan", "nan(1f)", "nan(", "Inf", "-Infinity"};
  for (const auto& v : values) {
    INFO("value '" << v << "'");
    const bool tcl_double = tcl.interp->evaluate("string is double -strict " + pcell::tcl_list_element(v)).value.to_string() == "1";
    CHECK(pcell::tcl_strict_double(v).has_value() == tcl_double);
    const auto number = pcell::tcl_strict_double(v);
    if (tcl_double && number && std::isfinite(*number)) {
      CHECK(pcell::normalize_value(v) == tcl.call("ref::normalize_value", v));
    }
  }
}

TEST_CASE("golden hash values", "[cap:pcell-toolkit][req:Deterministic generated-cell naming]") {
  CHECK(pcell::gencell_hash("0.15;1;1;") == 0x5f8a415dU);
  CHECK(pcell::generated_cell_name("nfet", dict({{"w", "1.0"}, {"l", "0.15"}, {"nf", "1"}})) == "nfet_XCJNSH");
  // Equivalent numeric spellings share a cell.
  CHECK(pcell::gencell_suffix(dict({{"w", "1"}, {"l", "0.15"}, {"nf", "1"}})) == "XCJNSH");
  // Empty parameter set.
  CHECK(pcell::gencell_hash("") == 0U);
  CHECK(pcell::gencell_suffix(pcell::ParameterDict{}) == "222222");
  // Four-key example.
  CHECK(pcell::gencell_hash("0.15;1;2;0.42;") == 0x2cea7c7fU);
  CHECK(pcell::gencell_suffix(dict({{"nf", "2"}, {"w", "0.42"}, {"l", "0.15"}, {"m", "1"}})) == "Z5ZNGQ");
}

TEST_CASE("Murmur reference vectors", "[cap:pcell-toolkit][req:Deterministic generated-cell naming]") {
  CHECK(pcell::gencell_hash("hello") == 0x248bfa47U);
  CHECK(pcell::gencell_hash("abc") == 0xb3dd93faU);
  CHECK(pcell::gencell_hash("a") == 0x3c2569b2U);
  CHECK(pcell::gencell_hash("") == 0U);
}

TEST_CASE("non-ASCII names hash the characters' low bytes as Tcl 8.6 does (Q-SCRIPT-22)",
          "[cap:pcell-toolkit][req:Deterministic generated-cell naming]") {
  TclReference tcl;
  const std::vector<std::string> inputs = {
      "\xC3\xA9;",                       // U+00E9
      "\xCE\xBC" "m;1;",                 // U+03BC
      "\xE2\x82\xAC;\xE2\x82\xAC;x",     // U+20AC
      "\xF0\x9F\x98\x80;",               // U+1F600: a surrogate pair in Tcl 8.6
      "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E;0.15;",
      "abcd", "abcde", "abcdef", "abcdefg"};
  for (const auto& in : inputs) {
    INFO(in);
    CHECK(std::to_string(pcell::gencell_hash(in)) == tcl.call("ref::murmur3_32", in));
  }
}

TEST_CASE("Name shape of the legacy random names", "[cap:pcell-toolkit][req:Legacy random cell names]") {
  int next = 0;
  std::vector<std::string> taken = {"res_0123ab"};
  const std::vector<int> draws = {0, 1, 2, 3, 10, 11, 35, 35, 35, 35, 35, 35};
  const auto name = pcell::random_gencell_name(
      "res", [&](int range) {
        REQUIRE(range == 36);
        return draws[static_cast<std::size_t>(next++)];
      },
      [&](const std::string& n) { return std::find(taken.begin(), taken.end(), n) != taken.end(); });
  // The first draw "res_0123ab" is taken, so it is redrawn.
  CHECK(name == "res_zzzzzz");
  CHECK(std::regex_match(name, std::regex{"^res_[0-9a-z]{6}$"}));
}
