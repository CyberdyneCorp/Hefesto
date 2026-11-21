// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Reading and writing `.sch`/`.sym` files (openspec/changes/m8-schematic
// design.md D6, "Area contracts" FORMAT; capability schematic-file-format).
// The reader treats its input as hostile (bounds, sizes, recursion of
// embedded blocks); the writer follows "Canonical write order".
//
// Owner: FORMAT.

#pragma once

#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

/// The pinned xschem compatibility version written in every `v` record
/// ("Writer version string", Q-SCHEM-1). Independent of `xschem-compat`.
inline constexpr std::string_view kWriterVersion = "3.4.8RC";

struct ReadOptions {
  FileKind kind = FileKind::schematic;
  /// `xschem-compat` (compat::CompatSettings::xschem_compat): the Q-SCHEM-3
  /// branch point of "Version record and header text".
  bool xschem_compat = true;
  /// Receives the warnings of "Malformed input" (skipped records, a missing
  /// close brace, a `[` with no instance); nullptr drops them. Not owned.
  diag::DiagnosticSink* warnings = nullptr;
};

/// Parses a whole file. Malformed records are skipped with warnings to
/// `options.warnings` as "Malformed input" says. `Document::path` is left
/// empty (the caller knows where the bytes came from).
[[nodiscard]] diag::Outcome<Document> read_document(std::string_view bytes, const ReadOptions& options);

/// The file text of `document` in canonical write order, LF line ends.
[[nodiscard]] std::string write_document(const Document& document);

}  // namespace hefesto::schem
