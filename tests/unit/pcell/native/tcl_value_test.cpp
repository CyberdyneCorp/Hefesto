// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl value semantics of the native generator ports (design D10;
// capability pcell-toolkit "Native generator equivalence"): ::tcl::mathop
// arithmetic, `expr` comparisons, `string is int`, booleans and the string
// representations of computed values, each compared with the embedded Tcl
// 8.6 over a table of operands.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/script/project.hpp"
#include "pcell/native/tcl_value.hpp"

namespace native = hefesto::pcell::native;

namespace {

struct Tcl {
  hefesto::editor::EditorState state;
  hefesto::script::Interpreter* interp = nullptr;

  Tcl() {
    interp = &hefesto::script::open_project_interpreter(state, hefesto::script::SandboxPolicy{});
    REQUIRE(interp->evaluate("namespace path {::tcl::mathop ::tcl::mathfunc}").ok());
  }

  // The result of `script`, or "ERROR" when it raises.
  std::string eval(const std::string& script) {
    const auto r = interp->evaluate("format %s [" + script + "]");
    return r.ok() ? hefesto::pcell::tcl_string(r.value) : std::string{"ERROR"};
  }
};

// The native result as Tcl would print it, "ERROR" for a TclError.
template <typename F>
std::string native_text(F&& f) {
  try {
    return f();
  } catch (const native::TclError&) {
    return "ERROR";
  }
}

const std::vector<std::string> kOperands = {"0",    "1",     "2",     "-3",   "7",     "0.5",  "2.0",  "0.15",
                                            "0.42", "0.420", "1e-3",  "1.05", "-0.06", "100",  "+50",  "010",
                                            "0x10", "29",    "0.0005", "3.0", "1e20",  "abc",  ""};

}  // namespace

TEST_CASE("mathop arithmetic matches Tcl", "[cap:pcell-toolkit][req:Native generator equivalence]") {
  Tcl tcl;
  for (const auto& a : kOperands) {
    for (const auto& b : kOperands) {
      INFO("a='" << a << "' b='" << b << "'");
      const std::string args = hefesto::pcell::merge_tcl_list(std::vector<std::string>{a, b});
      CHECK(native_text([&] { return native::add(a, b).str(); }) == tcl.eval("+ " + args));
      CHECK(native_text([&] { return native::sub(a, b).str(); }) == tcl.eval("- " + args));
      CHECK(native_text([&] { return native::mul(a, b).str(); }) == tcl.eval("* " + args));
      CHECK(native_text([&] { return native::div(a, b).str(); }) == tcl.eval("/ " + args));
    }
  }
}

TEST_CASE("expr comparisons match Tcl: numeric for numbers, else string",
          "[cap:pcell-toolkit][req:Native generator equivalence]") {
  Tcl tcl;
  REQUIRE(tcl.interp->evaluate("proc cmp {a b} {list [expr {$a < $b}] [expr {$a > $b}] [expr {$a == $b}]}").ok());
  for (const auto& a : kOperands) {
    for (const auto& b : kOperands) {
      INFO("a='" << a << "' b='" << b << "'");
      const std::string native_result = native_text([&] {
        return std::string{native::lt(a, b) ? "1" : "0"} + " " + (native::gt(a, b) ? "1" : "0") + " " +
               (native::eq(a, b) ? "1" : "0");
      });
      CHECK(native_result == tcl.eval("cmp " + hefesto::pcell::merge_tcl_list(std::vector<std::string>{a, b})));
    }
  }
}

TEST_CASE("string is int, abs and booleans match Tcl", "[cap:pcell-toolkit][req:Native generator equivalence]") {
  Tcl tcl;
  std::vector<std::string> values = kOperands;
  values.insert(values.end(), {"4294967295", "4294967296", "-4294967295", " 3 ", "08", "1.5", "true", "no", "on",
                               "off", "yes", "f"});
  for (const auto& v : values) {
    INFO("v='" << v << "'");
    const std::string quoted = hefesto::pcell::merge_tcl_list(std::vector<std::string>{v});
    CHECK((native::string_is_int(v) ? "1" : "0") == tcl.eval("string is int " + quoted));
    CHECK(native_text([&] { return native::Val{v}.truth() ? std::string{"1"} : std::string{"0"}; }) ==
          tcl.eval("expr {" + quoted + " ? 1 : 0}"));
    if (native::Val{v}.is_number()) {
      CHECK(native::abs(v).str() == tcl.eval("expr {abs(" + quoted + ")}"));
    }
  }
}

TEST_CASE("a value keeps its text until arithmetic replaces it", "[cap:pcell-toolkit][req:Native generator equivalence]") {
  // `${w}um` of a parameter is its dictionary text; a computed value is
  // formatted by Tcl_PrintDouble.
  CHECK(native::um("0.420") == "0.420um");
  CHECK(native::um(native::div("0.420", "2.0")) == "0.21um");
  CHECK(native::div("1", "2").str() == "0");  // integer floor division
  CHECK(native::div("-1", "2").str() == "-1");
  CHECK(native::mul("2", "0.5").str() == "1.0");
  CHECK(native::Val::real(0.38499999139457941).str() == "0.3849999913945794");
  CHECK(native::first_char("+50") == "+");
}
