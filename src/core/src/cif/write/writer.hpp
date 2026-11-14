// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the CIF writer (src/core/src/cif/write/): the buffered text
// output and the symbol writer that numbers, orders and writes the symbols
// of one write (capability cif-format-io "CIF output symbol numbering and
// order", "CIF output symbol definition format", "CIF output labels", "CIF
// output calls and arrays", "CIF output preconditions and flat output").
// Owner: CIF.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "format.hpp"
#include "hefesto/cif/write/write.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/mask/gen/context.hpp"
#include "hefesto/mask/gen/generate.hpp"

namespace hefesto::cif::detail {

/// Text written to a byte sink in chunks; the first failure is kept and
/// later output is dropped.
class TextOut {
 public:
  explicit TextOut(gds::ByteSink& sink) : sink_(sink) {}

  void put(std::string_view text);
  /// Writes what is buffered; the first failure seen so far.
  [[nodiscard]] diag::Status flush();
  [[nodiscard]] const diag::Status& status() const noexcept { return status_; }

 private:
  static constexpr std::size_t kChunk = std::size_t{1} << 16;
  gds::ByteSink& sink_;
  std::string buffer_;
  diag::Status status_ = diag::Done{};
};

/// The symbols of one write after the preamble: CIFWrite's numbering and
/// cifOut loop, or CIFWriteFlat's single symbol, then `C <root>;` and `End`.
class SymbolWriter {
 public:
  SymbolWriter(const PreparedWrite& prepared, TextOut& out, diag::DiagnosticSink& sink, mask::GenerationContext& ctx);

  /// CIFWrite after the prepare phase. Fails only when cancelled.
  [[nodiscard]] diag::Status write_hierarchical();
  /// CIFWriteFlat. Fails only when cancelled.
  [[nodiscard]] diag::Status write_flat();

 private:
  struct Pending {
    const db::CellDef* def = nullptr;
    mask::HierarchyOptions inherited;
  };

  [[nodiscard]] std::int64_t& number_of(const db::CellDef& def) { return numbers_[&def]; }
  void mark_children(const db::CellDef& def, const mask::HierarchyOptions& options);
  [[nodiscard]] diag::Status symbol(const db::CellDef& def, std::int64_t number, const mask::HierarchyOptions& options);
  [[nodiscard]] diag::Status layers(const db::CellDef& def, const mask::HierarchyOptions& options);
  void layer_tiles(const mask::OutputLayer& layer, const mask::MaskPlane& plane);
  void labels(const db::CellDef& def);
  [[nodiscard]] diag::Status calls(const db::CellDef& def);
  [[nodiscard]] diag::Status cancelled() const;
  void finish(std::int64_t root_number);

  const PreparedWrite& prepared_;
  TextOut& out_;
  diag::DiagnosticSink& sink_;
  mask::GenerationContext& ctx_;
  mask::GenerationSource source_;
  OutputNumbers numbers_scale_;
  mask::HierarchyOptions base_;
  std::unordered_map<const db::CellDef*, std::int64_t> numbers_;
  std::vector<Pending> stack_;
  std::int64_t next_ = -2;
};

}  // namespace hefesto::cif::detail
