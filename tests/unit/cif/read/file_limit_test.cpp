// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The file size limit of cif::read (CIF-READ-010): the text is held in
// memory while it is parsed, so a larger file is refused before it is read.
// Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include "../cif_fixture.hpp"
#include "hefesto/cif/read/read.hpp"

namespace {

using namespace cif_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

}  // namespace

TEST_CASE("a CIF file larger than the read's limit is refused before it is read",
          "[cap:cif-format-io][req:cif read, write and flat options]") {
  Fixture f;
  REQUIRE(f.fs.write_atomic("/big.cif", "DS 1 1 1;\n9 big;\nL CMF;\nB 40 20 20 10;\nDF;\nC 1;\nE\n").ok());
  cif::CifReadOptions options;
  options.files = &f.fs;
  options.max_file_bytes = 16;
  options.typed_name = "big";
  const cmd::CommandResult refused = cif::read(f.ed, "/big.cif", options);
  CHECK_FALSE(refused.ok());
  REQUIRE_FALSE(refused.diagnostics.empty());
  CHECK(refused.diagnostics.back().code == "CIF-READ-010");
  CHECK(f.ed.cells.find("big") == nullptr);
  options.max_file_bytes = cif::kMaxCifFileBytes;
  CHECK(cif::read(f.ed, "/big.cif", options).ok());
}
