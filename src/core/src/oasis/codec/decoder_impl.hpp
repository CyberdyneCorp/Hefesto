// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The decoder's state (capability oasis-format; design D11): one handler
// per record ID, the name-table bookkeeping for the checks made once the
// whole file is read (undefined refnums, cells defined twice, unknown
// standard properties), the strict-mode tracker and the PROPERTY target.
// decoder.cpp holds the file-level records, decoder_elements.cpp the
// elements.
//
// Owner: OASFMT (design D2).

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "hefesto/oasis/codec/decoder.hpp"
#include "input.hpp"
#include "strict.hpp"

namespace hefesto::oasis {

struct Decoder::Impl {
  Impl(gds::ByteSource& source, DecoderOptions decoder_options);

  /// The next record (nullopt after END); throws detail::CodecError.
  std::optional<Record> next();

  // --- file-level records (decoder.cpp) ---
  void magic();
  void pad() {}
  void start_record();
  void end_record();
  void cellname();
  void cellname_explicit();
  void textstring();
  void textstring_explicit();
  void propname();
  void propname_explicit();
  void propstring();
  void propstring_explicit();
  void layername();
  void cell_by_reference();
  void cell_by_name();
  void xyabsolute();
  void xyrelative();
  void property();
  void property_repeat();
  void xname();
  void xname_explicit();
  void cblock();

  // --- elements (decoder_elements.cpp) ---
  void placement();
  void text();
  void rectangle();
  void polygon();
  void path();
  void trapezoid();
  void ctrapezoid();
  void circle();
  void xelement();
  void xgeometry();

  // --- helpers ---
  void warn(std::string_view code, std::string text);
  void check(StringKind kind, std::string_view text);
  /// The name record of `table`: its refnum, with the mixing and duplicate checks.
  std::uint64_t define_name(NameTable table, bool implicit, const std::string& name);
  void name_record(NameTable table, bool implicit, StringKind kind);
  TableOffsets read_tables();
  PropertyValue property_value();
  void after_end(const EndInfo& end);
  [[nodiscard]] std::vector<diag::Diagnostic> reference_checks() const;
  [[nodiscard]] std::vector<diag::Diagnostic> cell_checks() const;
  [[nodiscard]] std::vector<diag::Diagnostic> standard_checks() const;
  ValidationResult validation(std::uint64_t scheme);

  /// An unsigned 32-bit field (layer, datatype...) or its modal value.
  std::uint64_t u32_field(bool present, std::optional<std::uint64_t>& slot, std::string_view name);
  /// An unsigned length field (width, radius...) or its modal value.
  std::int64_t length_field(bool present, std::optional<std::int64_t>& slot, std::string_view name);
  /// A coordinate, absolute or relative to its modal value (xy-mode).
  std::int64_t coordinate(bool present, std::optional<std::int64_t>& slot);
  std::optional<Repetition> repetition(bool present);
  /// The geometry fields shared by the elements: layer, datatype.
  void layer_fields(Element& e, std::uint8_t info);
  /// x, y and the repetition of a geometry element (info bits X, Y, R).
  void position_fields(Element& e, std::uint8_t info);
  void finish_element(Element e);
  void drop_unless_outline(const Element& e);

  gds::ByteSource* source;
  DecoderOptions options;
  detail::RecordInput in;
  ModalState modal;
  NameTables names;
  std::optional<StartInfo> start;
  std::vector<diag::Diagnostic> diagnostics;
  std::optional<diag::Diagnostic> failure;
  bool magic_read = false;
  bool finished = false;
  PropertyTarget target = PropertyTarget::file;
  detail::Drop drop;
  detail::StrictTracker strict;
  Record record;

  enum class Numbering : std::uint8_t { none, implicit, explicit_ };
  std::array<Numbering, 6> numbering{};
  std::array<std::uint64_t, 6> implicit_next{};
  std::map<std::string, std::uint64_t> cellname_index;
  /// The refnums referenced, per table (CELLNAME, TEXTSTRING, PROPNAME, PROPSTRING).
  std::array<std::set<std::uint64_t>, 4> used;
  std::vector<std::variant<std::uint64_t, std::string>> cells;
  std::vector<std::variant<std::uint64_t, std::string>> standard_names;
};

}  // namespace hefesto::oasis
