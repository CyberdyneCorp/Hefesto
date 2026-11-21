// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Helpers shared by the schem/file tests: reading text into a document with
// the warnings collected, round trips, and the SCHEM golden case files.
//
// Owner: FORMAT.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace hefesto::schem::test {

struct ReadResult {
  Document doc;
  std::vector<diag::Diagnostic> warnings;
};

inline ReadResult read_text(std::string_view text, FileKind kind = FileKind::schematic, bool xschem_compat = true) {
  diag::CollectingSink sink;
  ReadOptions options;
  options.kind = kind;
  options.xschem_compat = xschem_compat;
  options.warnings = &sink;
  auto read = read_document(text, options);
  REQUIRE(read.ok());
  return {std::move(read).value(), sink.take()};
}

/// read, then write.
inline std::string round_trip(std::string_view text, FileKind kind = FileKind::schematic, bool xschem_compat = true) {
  return write_document(read_text(text, kind, xschem_compat).doc);
}

inline std::vector<std::string> codes(const std::vector<diag::Diagnostic>& diagnostics) {
  std::vector<std::string> out;
  for (const auto& d : diagnostics) {
    out.push_back(d.code);
  }
  return out;
}

inline std::string slurp(const std::filesystem::path& path) {
  std::ifstream in{path, std::ios::binary};
  REQUIRE(in.good());
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

/// A file of a SCHEM golden case: tests/golden/SCHEM/cases/<id>/<relative>.
inline std::filesystem::path golden(std::string_view id, std::string_view relative) {
  return std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests" / "golden" / "SCHEM" / "cases" / std::string{id} /
         std::string{relative};
}

/// The header every file with no header text and empty globals starts with.
inline constexpr std::string_view kEmptyHeader =
    "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nF {}\nE {}\n";

}  // namespace hefesto::schem::test
