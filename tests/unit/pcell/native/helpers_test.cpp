// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The bit-identical PDK helpers (design D10, Q-SCRIPT-40; capability
// pcell-toolkit "Native generator equivalence": "the numeric results of
// the PDK helper procedures, including their defects"): each C++ helper is
// compared with the PDK's own Tcl procedure (verbatim copies in
// tests/data/pcell/native/helpers_*.tcl) running in the embedded Tcl 8.6.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/pcell/native/helpers.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"

namespace pcell = hefesto::pcell;

namespace {

struct Reference {
  hefesto::editor::EditorState state;
  hefesto::script::Interpreter* interp = nullptr;

  Reference(const std::string& file, const std::string& cif_scale) {
    hefesto::script::SandboxPolicy policy;
    policy.granted_roots.push_back(HEFESTO_TEST_DATA_DIR);
    interp = &hefesto::script::open_project_interpreter(state, policy);
    REQUIRE(interp->evaluate("namespace eval magic {}; proc puts {args} {}; "
                             "proc magic::cif {args} {return " + cif_scale + "}").ok());
    const auto sourced = interp->evaluate("source " + std::string{HEFESTO_TEST_DATA_DIR} + "/" + file);
    INFO((sourced.diagnostics.empty() ? std::string{} : sourced.diagnostics.front().text));
    REQUIRE(sourced.ok());
  }

  // The Tcl result, or nullopt when the procedure raises an error.
  std::optional<std::string> call(std::string_view proc, std::string_view arg, std::string_view extra = {}) {
    std::vector<std::string> words{std::string{proc}, std::string{arg}};
    if (!extra.empty()) {
      words.emplace_back(extra);
    }
    const auto r = interp->evaluate("format %s [" + pcell::merge_tcl_list(words) + "]");
    if (!r.ok()) {
      return std::nullopt;
    }
    return pcell::tcl_string(r.value);
  }
};

const std::vector<std::string> kSpiceValues = {
    "1", "2.0", "0.15", "1.5u", "1.5U", "150n", "0.5um", "10f", "10fF", "3p", "3pF", "2k", "1meg", "1MEG", "2m",
    "4a", "1e-6", "1E3", "0x10", "010", "-2u", "+3", " 7 ", "1.0e", "abc", "", "1.2.3", "2*3", "(1+2)*3", "7/2",
    "7%3", "1/0", "1.0/0", "-1.0/0", "0.0/0", "0/0.0", "1e308*10", "1u/2", "2 * 1.0e-6", "08", "08.5", "1.", ".5", "5nm", "12345678901234", "1e20",
    "1e-20", "0.1", "1/3.0", "1k5", "1mil"};

const std::vector<std::string> kNumbers = {"1",    "0",     "-1",  "0.15", "1.5",  "2.0",  "0.005", "123",
                                           "0.42", "1e-3",  "7",   "-0.5", "99.99", "1000", "0.0001", "3.14159",
                                           "1.2345", "0.0005", "2.5", "10.4999"};

void check_helpers(Reference& tcl, const pcell::PdkHelpers& helpers) {
  for (const auto& v : kSpiceValues) {
    INFO("spice2float '" << v << "'");
    const auto native = helpers.spice2float(v, "FAULT");
    const auto expected = tcl.call("magic::spice2float", v, "FAULT");
    REQUIRE(expected.has_value());
    if (native) {  // nullopt: outside the reproduced subset (falls back to Tcl)
      CHECK(native->value == *expected);
      CHECK(native->faulted == (*expected == "FAULT"));
    }
  }
  for (const auto& v : kNumbers) {
    INFO("number '" << v << "'");
    CHECK(helpers.u2i(v) == tcl.call("magic::u2i", v));
    CHECK(helpers.i2u(v) == tcl.call("magic::i2u", v));
    CHECK(helpers.three_digit_past_decimal(v) == tcl.call("magic::3digitpastdecimal", v));
  }
}

}  // namespace

TEST_CASE("sky130 and gf180mcu helpers match the PDK procedures",
          "[cap:pcell-toolkit][req:Native generator equivalence]") {
  for (const char* scale : {"0.005", "0.01", "0.0050000000000000001"}) {
    INFO("cif scale " << scale);
    Reference tcl{"helpers_sky130.tcl", scale};
    for (const auto family : {pcell::HelperFamily::sky130, pcell::HelperFamily::gf180mcu}) {
      check_helpers(tcl, pcell::PdkHelpers{family, scale});
    }
  }
}

TEST_CASE("sg13g2 helpers match the PDK procedures (round, %.3f)",
          "[cap:pcell-toolkit][req:Native generator equivalence]") {
  for (const char* scale : {"0.005", "0.001"}) {
    INFO("cif scale " << scale);
    Reference tcl{"helpers_sg13g2.tcl", scale};
    check_helpers(tcl, pcell::PdkHelpers{pcell::HelperFamily::sg13g2, scale});
  }
}

TEST_CASE("spice2float keeps the PDK defects (Q-SCRIPT-40)", "[cap:pcell-toolkit][req:Native generator equivalence]") {
  const pcell::PdkHelpers h{pcell::HelperFamily::sky130, "0.005"};
  // `n` maps to "* 1.0 e-9", which is not an expression: the fault value.
  CHECK(h.spice2float("150n")->faulted);
  CHECK(h.spice2float("150n")->value == "0.0");
  // uF/nF/pF/aF are never matched after `string tolower`; `a` is 1e-15.
  CHECK(h.spice2float("4a")->value == h.spice2float("4f")->value);
  // The common forms.
  CHECK(h.spice2float("1.5u")->value == "1.5e-6");
  CHECK(h.spice2float("2")->value == "2");
  // Outside the reproduced subset: the Tcl generator is used.
  CHECK_FALSE(h.spice2float("sqrt(4)").has_value());
}
