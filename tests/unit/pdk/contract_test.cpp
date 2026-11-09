// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M1 scaffolding: the PDK contract compiles and its stubs behave as
// documented. PDK replaces these with requirement-tagged tests.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/pdk/catalog.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/pdk/manager.hpp"

namespace pdk = hefesto::pdk;

TEST_CASE("the headless fetcher reports an unsupported platform service", "[m1-scaffold]") {
  auto fetcher = pdk::make_unsupported_fetcher();
  const auto text = fetcher->get_text("https://example.invalid/manifest.json");
  REQUIRE_FALSE(text.ok());
  CHECK(text.error().code == hefesto::diag::codes::kPlatformUnsupported);
}

TEST_CASE("PDK commands are registered", "[m1-scaffold]") {
  hefesto::cmd::CommandRegistry registry;
  CHECK(pdk::register_pdk_commands(registry));
  CHECK(registry.find("pdk") != nullptr);
}
