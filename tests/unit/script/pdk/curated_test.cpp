// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Curated-pack mode (tasks.md 6.7; Q-SCRIPT-46, D-30) and the `load
// <cell> -silent` form the sg13g2 generators use (design D14). Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/script/pdk/curated.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
using script::test::PdkSession;

TEST_CASE("Uncurated PDK refused", "[cap:scripting-pdk-integration][req:PDKs without bundled scripts in curated-pack mode]") {
  const script::CuratedPackPolicy policy{true};
  const auto refused = policy.check({"ihp-sg13", "ihp-sg13g2", "5e6d592e"}, false);
  REQUIRE_FALSE(refused.ok());
  CHECK(refused.diagnostics.front().code == "SCRIPT-PDK-005");
  CHECK(refused.diagnostics.front().text.find("Only curated PDKs run in this mode") != std::string::npos);
  CHECK(refused.diagnostics.front().text.find("ihp-sg13g2") != std::string::npos);
}

TEST_CASE("Curated pack mode", "[cap:hefesto-scripting][req:App Store fallback for iPadOS]") {
  const script::CuratedPackPolicy policy{true};
  CHECK(policy.curated_only());
  const auto bundled = policy.check({"sky130", "sky130A", "5740c4a8"}, true);
  REQUIRE(bundled.ok());
  REQUIRE(bundled.diagnostics.size() == 1);
  CHECK(bundled.diagnostics.front().code == "SCRIPT-PDK-009");
  CHECK(bundled.diagnostics.front().text.find("not executed") != std::string::npos);
}

TEST_CASE("PDK without a curated pack", "[cap:hefesto-scripting][req:App Store fallback for iPadOS]") {
  CHECK_FALSE(script::CuratedPackPolicy{true}.check({"gf180mcu", "gf180mcuA", ""}, false).ok());
  // The desktop builds are unaffected.
  const auto desktop = script::CuratedPackPolicy{false}.check({"gf180mcu", "gf180mcuA", ""}, false);
  CHECK(desktop.ok());
  CHECK(desktop.diagnostics.empty());
}

TEST_CASE("load <cell> -silent takes the option after the name", "[cap:scripting-pdk-integration][req:Command surface used by device generators]") {
  PdkSession s;
  s.eval("load npn13g2_base -silent");
  CHECK(s.eval("cellname list window") == "npn13g2_base");
  // Magic's CmdLoad reads options from the end only: the option first is a usage error.
  const auto first = s.dispatcher->execute({"load", {"-silent", "other"}, hefesto::cmd::Origin::script, std::nullopt});
  CHECK_FALSE(first.ok());
}
