// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF parser's state and command handlers, split over parser.cpp (the
// main loop, scaling, points and transforms), parser_cells.cpp (DS, DF, DD,
// C, 9, 91 and the end of the read), parser_shapes.cpp (L, B, R, P, W) and
// parser_labels.cpp (94, 95). Internal to src/core/src/cif/read/.
// Owner: CIF.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cif/read/lexer.hpp"
#include "hefesto/cif/read/parser.hpp"
#include "hefesto/cif/read/wire.hpp"

namespace hefesto::cif::detail {

/// A transform with 64-bit translation, composed while a call is parsed.
/// The rotation entries are -1, 0 or 1.
struct Transform64 {
  std::int64_t a = 1, b = 0, c = 0, d = 0, e = 1, f = 0;
};

/// A rectangle in read units before its range check.
struct Rect64 {
  std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

class Parser {
 public:
  Parser(std::string_view text, const ParseStyle& style, const ParseOptions& options, CifReadTarget& target,
         ReadMessages& messages, ReadSession& session);

  void run();
  /// The current line (cifLineNumber).
  [[nodiscard]] std::int64_t line() const noexcept { return lex_.line(); }

 private:
  // parser.cpp: the main loop and the command table.
  [[nodiscard]] bool command();
  [[nodiscard]] bool definition_command();
  void user_extension();
  void unknown_command();
  void end_command();
  [[nodiscard]] bool cancelled() const noexcept;
  void error(std::string_view text) { messages_.error(lex_.line(), text); }
  void warning(std::string_view text) { messages_.warning(lex_.line(), text); }
  void check(const diag::Status& status);
  void outside_plane(std::string_view element);

  // parser.cpp: integers, points and paths (CIFParseInteger, CIFParsePoint,
  // CIFParsePath) and the mid-read subdivision.
  [[nodiscard]] std::optional<std::int64_t> unsigned_integer();
  [[nodiscard]] std::int64_t mul(std::int64_t a, std::int64_t b);
  /// Clamps a sum of two in-range values to +-kIntegerLimit, marking the
  /// command's geometry out of range when it had to.
  [[nodiscard]] std::int64_t saturate(std::int64_t v);
  [[nodiscard]] std::int64_t scale_value(std::int64_t v, std::int64_t iscale, ReadPoint* earlier);
  [[nodiscard]] std::optional<ReadPoint> point(std::int64_t iscale);
  [[nodiscard]] std::optional<std::vector<ReadPoint>> path(std::int64_t iscale);
  [[nodiscard]] bool rescale(std::int64_t factor);
  [[nodiscard]] std::int64_t rescale_limit() const noexcept;

  // parser.cpp: transforms (CIFParseTransform, CIFDirectionToTrans).
  [[nodiscard]] Transform64 direction(std::int64_t x, std::int64_t y);
  [[nodiscard]] bool transform(Transform64& t);
  [[nodiscard]] bool transform_step(int ch, Transform64& t);
  [[nodiscard]] bool translate(Transform64& t);

  // parser_cells.cpp.
  void report_symbol_cap();
  void grow_graph();
  void start_definition();
  void read_symbol_scale();
  void open_definition(std::int64_t number);
  void finish_definition();
  void close_definition();
  void delete_definition();
  void call();
  void place(SymbolTable::CellId child, const Transform64& t, bool well_formed);
  [[nodiscard]] bool is_ancestor(SymbolTable::CellId child, SymbolTable::CellId cell);
  void user9();
  void rename_current(const std::string& name);
  void user91();
  void discard_pending_id();
  void cleanup();

  // parser_shapes.cpp.
  [[nodiscard]] bool geometry_allowed() const noexcept;
  void layer();
  void box();
  [[nodiscard]] std::optional<Rect64> box_extent();
  [[nodiscard]] static Rect64 rotate(const Transform64& t, const Rect64& r);
  void flash();
  void polygon();
  void wire();
  void paint_rect(const Rect64& r, std::string_view element);
  void paint_polygon(const std::vector<ReadPoint>& points, std::string_view element);
  void paint_wire(const std::vector<ReadPoint>& points, std::int64_t width);

  // parser_labels.cpp.
  void user94();
  void user95();
  [[nodiscard]] std::optional<std::pair<ReadPoint, ReadPoint>> area_label_corners();
  void label_layer(CifLabel& label, bool point_label);
  [[nodiscard]] bool default_label_sticky(std::int32_t type) const;
  void put_label(CifLabel label, const ReadPoint& ll, const ReadPoint& ur);

  Lexer lex_;
  const ParseStyle& style_;
  const ParseOptions& options_;
  CifReadTarget& target_;
  ReadMessages& messages_;
  ReadSession& session_;

  std::int64_t scale_n_ = 1;                    // cifReadScale1 (N)
  std::int64_t scale_d_ = 1;                    // cifReadScale2 (Dn)
  std::int64_t multiplier_ = 1;                 // M, multiplied by every rescale
  bool range_error_ = false;                    // a scaled value overflowed in the current command
  std::optional<std::size_t> layer_;            // cifReadPlane: the current layer
  std::optional<std::size_t> saved_layer_;      // cifOldReadPlane
  std::int32_t label_type_ = 0;                 // cifCurLabelType
  bool in_definition_ = false;                  // cifSubcellBeingRead
  std::optional<SymbolTable::CellId> current_;  // the cell being defined (nullopt: dropped by the cap)
  std::optional<std::string> pending_id_;       // cifSubcellId
  bool copyup_warned_ = false;
  bool symbol_cap_reported_ = false;
  std::vector<std::vector<SymbolTable::CellId>> children_;  // calls among this read's cells
  std::vector<bool> has_parent_;
  std::vector<std::uint32_t> visited_;          // is_ancestor marks
  std::uint32_t visit_epoch_ = 0;
};

}  // namespace hefesto::cif::detail
