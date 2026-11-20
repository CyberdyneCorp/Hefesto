// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cross-workstream tests of the round-4 integration (openspec/changes/
// m6-lef-def-oasis tasks.md 12.1, 13.1, 14.2): `oasis write` (OASCMD's
// command over OASW's oasis::write) read back with `oasis read` (OASR), and
// a write cancelled through the session's write host (design D16).
// Owner: INTEGRATION.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <string_view>

#include "command_fixture.hpp"
#include "hefesto/mask/gen/progress.hpp"

using namespace oasis_command_test;  // NOLINT(google-build-using-namespace)

namespace {

/// An output style over the read style's layers (68/20 is metal1).
constexpr std::string_view kOutput =
    "style out\n"
    "scalefactor 1\n"
    "layer M1 m1\n"
    "labels m1\n"
    "calma 68 20\n"
    "layer M2 m2\n"
    "labels m2\n"
    "calma 69 20\n"
    "layer PO poly\n"
    "labels poly\n"
    "calma 66 20\n";

Bytes bytes_of(const std::string& text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  return Bytes(first, first + text.size());
}

}  // namespace

TEST_CASE("oasis write then oasis read gives back the same cells",
          "[cap:oasis-command][req:Command name and forms]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    CommandSession source(on, kOutput);
    source.put("cells.oas", two_cells(1000.0));
    REQUIRE(source.run("oasis read cells").ok());
    REQUIRE(source.has_cell("TOP"));
    REQUIRE(source.run("oasis write back").ok());
    const auto written = source.services.view().files.read("back.oas");
    REQUIRE(written.ok());
    CHECK(written.value().starts_with("%SEMI-OASIS\r\n"));

    CommandSession copy(on, kOutput);
    copy.put("back.oas", bytes_of(written.value()));
    REQUIRE(copy.run("oasis read back").ok());
    REQUIRE(copy.has_cell("TOP"));
    REQUIRE(copy.has_cell("A"));
    CHECK(copy.mag("A") == source.mag("A"));
    CHECK(copy.mag("TOP") == source.mag("TOP"));
  }
}

TEST_CASE("an oasis write cancelled through the write host leaves no file",
          "[cap:oasis-write][req:Write command and output file naming]") {
  CommandSession s(true, kOutput);
  s.put("cells.oas", two_cells(1000.0));
  REQUIRE(s.run("oasis read cells").ok());
  std::atomic<bool> cancel{true};
  hefesto::mask::Progress progress;
  progress.cancel = &cancel;
  int hosted = 0;
  s.editor.mask.write_host = [&](std::string_view, const hefesto::mask::ProgressWork& work) {
    ++hosted;
    return work(&progress);
  };
  const auto r = s.run("oasis write gone");
  CHECK(hosted == 1);
  CHECK_FALSE(r.ok());
  CHECK_FALSE(s.services.view().files.exists("gone.oas"));
  CHECK_FALSE(s.services.view().files.exists(".gone.oas.hefesto-tmp"));
}
