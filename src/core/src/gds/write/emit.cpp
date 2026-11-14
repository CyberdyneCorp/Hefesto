// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The element stream of one GDS structure (see hefesto/gds/write/emit.hpp).
// Ports Magic 8.3.694 (fc5daa9e) calma/CalmaWrite.c calmaOutI2,
// calmaOutI4, calmaOutR8 and calmaOutStringRecord (their record forms and
// warnings), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
// Owner: GDSW.

#include "hefesto/gds/write/emit.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

#include "codes.hpp"

namespace hefesto::gds {

StringOptions string_options(const mask::OutputStyle& style, const GdsSettings& settings) noexcept {
  return StringOptions{style.has_option(mask::style_option::string_limit), settings.lower};
}

CharTable char_table(const mask::OutputStyle& style, const compat::CompatSettings& compat) {
  const bool permissive = style.has_option(mask::style_option::permissive_labels);
  return CharTable{permissive ? CharTable::Kind::permissive : CharTable::Kind::strict, compat};
}

ElementWriter::ElementWriter(RecordWriter& records, const StructureContext& context, diag::DiagnosticSink* sink)
    : records_(records), context_(context), sink_(sink), table_(char_table(*context.style, context.compat)) {}

diag::Status ElementWriter::empty(RecordType type) {
  return records_.write_empty(type);
}

diag::Status ElementWriter::int2(RecordType type, std::int32_t value) {
  // calmaOutI2 writes the low 16 bits.
  const auto low = static_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(value) & 0xffffU));
  const std::array<std::int16_t, 1> values{low};
  return records_.write_int2(type, values);
}

diag::Status ElementWriter::real8(RecordType type, double value) {
  const std::array<double, 1> values{value};
  return records_.write_real8(type, values);
}

diag::Status ElementWriter::string_record(RecordType type, std::string_view text) {
  const EncodedString encoded = encode_string(text, table_, string_options(style(), settings()), compat());
  const std::string_view source = text.substr(0, text.find('\0'));
  if (encoded.truncated) {
    // Magic prints the tail from the padded (even) length.
    const std::size_t padded = source.size() + (source.size() & 1U);
    const std::size_t from = std::min(padded - std::min<std::size_t>(padded, 32), source.size());
    warn("Warning:  Cellname " + std::string{source} + " truncated to " + std::string{source.substr(from)} +
         " (GDS format limit)");
  }
  if (encoded.unprintable) {
    warn("Warning: Unprintable character changed to 'X' in label.");
  }
  if (encoded.changed) {
    warn("Warning: characters changed in string '" + std::string{source} + "'; modified string is '" +
         encoded.mapped + "'");
  }
  return records_.write(type, DataType::string, encoded.data);
}

diag::Outcome<std::int32_t> ElementWriter::checked_i4(std::int64_t value) const {
  if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max()) {
    return diag::make(codes::kValueRange, "Calma output error:  value " + std::to_string(value) + " in cell " +
                                              cell().name() +
                                              " does not fit in a GDS 4-byte integer; the cell cannot be written.");
  }
  return static_cast<std::int32_t>(value);
}

diag::Status ElementWriter::xy(std::span<const OutputPoint> points) {
  std::vector<std::int32_t> values;
  values.reserve(points.size() * 2);
  for (const OutputPoint& p : points) {
    for (const std::int64_t v : {p.x, p.y}) {
      diag::Outcome<std::int32_t> checked = checked_i4(v);
      if (!checked.ok()) {
        return checked.error();
      }
      values.push_back(checked.value());
    }
  }
  return records_.write_int4(RecordType::xy, values);
}

diag::Status ElementWriter::layer_records(RecordType second, const mask::GdsLayer& layer) {
  if (diag::Status s = int2(RecordType::layer, layer.layer); !s.ok()) {
    return s;
  }
  return int2(second, layer.datatype);
}

diag::Status ElementWriter::boundary(const mask::GdsLayer& layer, std::span<const OutputPoint> points) {
  // The XY values are checked first, so a failing element writes nothing.
  for (const OutputPoint& p : points) {
    if (diag::Outcome<std::int32_t> x = checked_i4(p.x); !x.ok()) {
      return x.error();
    }
    if (diag::Outcome<std::int32_t> y = checked_i4(p.y); !y.ok()) {
      return y.error();
    }
  }
  if (diag::Status s = empty(RecordType::boundary); !s.ok()) {
    return s;
  }
  if (diag::Status s = layer_records(RecordType::datatype, layer); !s.ok()) {
    return s;
  }
  if (diag::Status s = xy(points); !s.ok()) {
    return s;
  }
  return empty(RecordType::endel);
}

void ElementWriter::warn(std::string text, std::string_view code) {
  if (sink_ != nullptr) {
    sink_->emit(diag::make(code.empty() ? codes::kNameChanged : code, std::move(text)));
  }
}

}  // namespace hefesto::gds
