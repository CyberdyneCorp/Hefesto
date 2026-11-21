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
  /// `symbol` for a file whose name ends in `.sym` ("Global property
  /// records": a pre-1.2 `G` symbol property moves to `K`).
  FileKind kind = FileKind::schematic;
  /// `xschem-compat` (compat::CompatSettings::xschem_compat): the Q-SCHEM-3
  /// branch point of "Version record and header text".
  bool xschem_compat = true;
  /// Receives the warnings of "Malformed input" (skipped records, a missing
  /// close brace, a `[` with no instance); nullptr drops them. Not owned.
  diag::DiagnosticSink* warnings = nullptr;
  /// The file name put in the warnings' source locations (may be empty).
  std::string source_name;
};

/// Parses a whole file. Malformed records are skipped with warnings to
/// `options.warnings` as "Malformed input" says; reading never fails, so the
/// Outcome always holds a document. `Document::path` is left empty (the
/// caller knows where the bytes came from). Work and memory are linear in
/// the input; nested embedded blocks are read without recursion, and each
/// open `[` block costs a few machine words of reader state (about 12 bytes
/// of memory per input byte for a file made only of `[` lines), not a copy
/// of a document. Warnings are at most a few per input line.
[[nodiscard]] diag::Outcome<Document> read_document(std::string_view bytes, const ReadOptions& options);

/// The file text of `document` in canonical write order, LF line ends
/// ("Canonical write order", "Embedded symbols"). Records keep the order of
/// their lists; lines, boxes, arcs and polygons are grouped by layer.
[[nodiscard]] std::string write_document(const Document& document);

/// `value` as a brace string: `{`, the value with `\`, `{` and `}` escaped
/// by a backslash, `}` ("Brace strings").
[[nodiscard]] std::string brace_string(std::string_view value);

}  // namespace hefesto::schem
