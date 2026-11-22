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

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

/// The pinned xschem compatibility version written in every `v` record
/// ("Writer version string", Q-SCHEM-1). Independent of `xschem-compat`.
inline constexpr std::string_view kWriterVersion = "3.4.8RC";

/// At most this many `[` blocks are open at once while reading, in both
/// modes ("Embedded symbols"; input limit Q-SCHEM-32, approved 2026-11-21).
inline constexpr std::size_t kMaxEmbeddedBlockDepth = 64;

/// The symbol levels inside one embedded block: a nested block that would
/// open this level is not loaded and the rest of the block is not read
/// ("Embedded symbols", following "Schematic used as symbol" of
/// schematic-symbols-and-libraries).
inline constexpr int kMaxBlockSymbolLevel = 40;

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
  /// Whether an instance's symbol reference is found as a file ("Embedded
  /// symbols": inside a block, a `C` record followed by `[` has that nested
  /// block define its symbol in place only when the symbol is not found as
  /// a file; otherwise the nested block is skipped and the instance kept).
  /// Empty: no reference is found as a file.
  std::function<bool(std::string_view reference)> symbol_file_exists = {};
};

/// Parses a whole file. Malformed records are skipped with warnings to
/// `options.warnings` as "Malformed input" says; reading never fails, so the
/// Outcome always holds a document. `Document::path` is left empty (the
/// caller knows where the bytes came from). Work and memory are linear in
/// the input; nested embedded blocks are read without recursion, at most
/// kMaxEmbeddedBlockDepth open at once. A nested block that defines an
/// instance's symbol in place is merged into the symbol of the block around
/// it ("Embedded symbols"), so `Document::embedded` lists only the blocks of
/// the file's own level. Warnings are at most a few per input line.
[[nodiscard]] diag::Outcome<Document> read_document(std::string_view bytes, const ReadOptions& options);

/// The file text of `document` in canonical write order, LF line ends
/// ("Canonical write order", "Embedded symbols"). Records keep the order of
/// their lists; lines, boxes, arcs and polygons are grouped by layer, and
/// layer-5 boxes are written in pin-box order (sort_pin_boxes). A document
/// of file version 1.0 writes every instance reference with `.sym`
/// (instance_symbol_reference).
[[nodiscard]] std::string write_document(const Document& document);

/// "Pin boxes on save": when the document has a layer-5 box and every one
/// has a non-empty `sim_pinnumber`, puts the layer-5 boxes in ascending
/// order of its integer value (C `atoi`, clamped to `int`), stable on ties,
/// keeping their places among the other boxes. Does not touch `modified`.
/// save_document calls it before writing.
void sort_pin_boxes(Document& document);

/// The symbol reference of `instance` as it is resolved and written
/// ("Version record and header text"): with `.sym` in place of any
/// extension when the document's file version is `1.0`, else as stored.
[[nodiscard]] std::string instance_symbol_reference(const Document& document, const InstanceRecord& instance);

/// `reference` with `.sym` in place of the extension of its last path
/// element (or appended when it has none).
[[nodiscard]] std::string with_sym_extension(std::string reference);

/// `value` as a brace string: `{`, the value with `\`, `{` and `}` escaped
/// by a backslash, `}` ("Brace strings").
[[nodiscard]] std::string brace_string(std::string_view value);

}  // namespace hefesto::schem
