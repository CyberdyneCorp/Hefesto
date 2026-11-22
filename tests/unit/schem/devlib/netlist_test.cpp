// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Netlisting with the system library (capability
// schematic-symbols-and-libraries "System library": the symbols "produce
// identical netlists"; interface notes of simulator_commands.sym and
// simulator_commands_shown.sym). The Hefesto-authored benches of
// tests/data/schem/devlib/tb are netlisted through the public schem APIs
// with the bench folder's xschemrc and a sandboxed Tcl bridge.
//
// Owner: DEVLIB.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "netlist_support.hpp"

namespace schem = hefesto::schem;
namespace dl = hefesto::test::devlib;
using hefesto::platform::Path;

namespace {

constexpr const char* kSystemLibrary = "[cap:schematic-symbols-and-libraries][req:System library]";
const Path kBench = dl::kData / "tb";

std::string masked(const schem::NetlistResult& result) {
  return dl::replace_all(result.text, kBench.generic_string(), "/work/input");
}

}  // namespace

TEST_CASE("an IHP-style bench netlists with the system library", kSystemLibrary) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    dl::NetlistBench bench(dl::folder_input(kBench), {});
    const schem::NetlistResult result = bench.netlist(kBench / "ihp_style_tb.sch", {}, on);
    CHECK_FALSE(result.failed);
    std::string expected = dl::read_text(kBench / "ihp_style_tb.spice");
    if (!on) expected = dl::replace_all(expected, "ihp_style_tb.sch\n", "ihp_style_tb.sch\n** xschem-compat=off\n");
    CHECK(masked(result) == expected);
  }
}

TEST_CASE("simulator_commands emits its block only for the embedded simulator", kSystemLibrary) {
  // Interface note: LF, LF, the value, LF when sim_is_<simulator> returns
  // true; a single LF when it returns false or fails (sim_is_spectre does
  // not exist).
  dl::NetlistBench bench(dl::folder_input(kBench), {});
  const schem::NetlistResult result = bench.netlist(kBench / "sim_commands.sch");
  CHECK_FALSE(result.failed);
  const std::string text = masked(result);
  INFO(text);
  CHECK(text.find("\n\n* ngspice block\n") != std::string::npos);
  CHECK(text.find("xyce block") == std::string::npos);
  CHECK(text.find("other block") == std::string::npos);
  CHECK(text.find("?\n") == std::string::npos);  // every format evaluated
  CHECK(text == dl::read_text(kBench / "sim_commands.spice"));
}
