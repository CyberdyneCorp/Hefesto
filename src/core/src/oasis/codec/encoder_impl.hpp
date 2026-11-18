// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The encoder's state (capability oasis-format; oasis-write "File layout",
// "Reference numbers and name tables", "Modal field omission", "CBLOCK
// compression"; design D11). encoder.cpp holds the file structure, the
// tables and CBLOCKs, encoder_elements.cpp the element records. Inside, an
// error is thrown as detail::CodecError and returned by the public method.
//
// Owner: OASFMT (design D2).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "hefesto/oasis/codec/decoder.hpp"
#include "hefesto/oasis/codec/encoder.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

struct Encoder::Impl {
  Impl(gds::ByteSink& byte_sink, EncoderOptions encoder_options);

  /// One name table of strict mode: names by implicit refnum.
  struct Table {
    std::vector<std::string> names;
    std::map<std::string, std::uint64_t, std::less<>> index;

    std::uint64_t ref(std::string_view name);
  };

  enum class Phase : std::uint8_t { created, header, cells, finished };

  // --- structure (encoder.cpp) ---
  void begin();
  void header_property(const Property& p);
  void layer_name(std::string_view name, std::uint32_t layer, std::uint32_t datatype);
  void begin_cell(std::string_view name);
  void body_property(const Property& p);
  void finish();
  void write(std::span<const std::byte> bytes);
  void emit(const std::vector<std::byte>& record);
  void flush_chunk();
  void write_table(NameTable table);
  void write_end();
  [[nodiscard]] std::vector<std::byte> property_record(const Property& p);
  void property_value(std::vector<std::byte>& out, const PropertyValue& v);
  void name_field(std::vector<std::byte>& out, std::string_view name, Table& table, StringKind kind);

  // --- elements (encoder_elements.cpp) ---
  void element(const Element& e);
  void placement(std::vector<std::byte>& out, const Element& e);
  void text(std::vector<std::byte>& out, const Element& e);
  void rectangle(std::vector<std::byte>& out, const Element& e);
  void polygon(std::vector<std::byte>& out, const Element& e);
  void path(std::vector<std::byte>& out, const Element& e);
  void trapezoid(std::vector<std::byte>& out, const Element& e);
  void ctrapezoid(std::vector<std::byte>& out, const Element& e);
  void circle(std::vector<std::byte>& out, const Element& e);
  void xelement(std::vector<std::byte>& out, const Element& e);
  void xgeometry(std::vector<std::byte>& out, const Element& e);
  /// Sets the layer and datatype bits, with the values in `fields`.
  std::uint8_t layer_bits(std::vector<std::byte>& fields, const Element& e);
  /// Sets the X, Y and R bits (geometry modal variables), with the values in `fields`.
  std::uint8_t position_bits(std::vector<std::byte>& fields, const Element& e, std::optional<std::int64_t>& mx,
                             std::optional<std::int64_t>& my, std::uint8_t bit_x, std::uint8_t bit_y,
                             std::uint8_t bit_r);
  /// Writes a length field when it differs from `slot`, and sets `slot`.
  bool length(std::vector<std::byte>& fields, std::int64_t value, std::optional<std::int64_t>& slot);

  gds::ByteSink* sink;
  EncoderOptions options;
  ModalState modal;
  Phase phase = Phase::created;
  std::uint32_t crc = 0;
  std::uint32_t sum = 0;
  std::vector<std::byte> chunk;
  bool in_sequence = false;
  Table cellnames;
  Table textstrings;
  Table propnames;
  Table propstrings;
  std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t>> layernames;
  std::set<std::tuple<std::string, std::uint32_t, std::uint32_t>> layernames_seen;
  std::map<std::string, std::uint64_t, std::less<>> cell_offsets;
  TableOffsets tables{};
};

}  // namespace hefesto::oasis
