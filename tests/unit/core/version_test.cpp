// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/version.hpp"

TEST_CASE("core links headless with its pinned dependencies",
          "[cap:hefesto-architecture][req:Platform-independent core]") {
  const std::string expected = std::to_string(hefesto::kVersionMajor) + "." +
                               std::to_string(hefesto::kVersionMinor) + "." +
                               std::to_string(hefesto::kVersionPatch);
  CHECK(hefesto::version() == expected);
  CHECK(hefesto::linked_zlib_version() == "1.3.1");
}
