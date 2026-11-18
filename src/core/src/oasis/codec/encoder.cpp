// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS encoder: file structure, name tables, PROPERTY records, CBLOCKs
// and the END record (capability oasis-format "START record and table
// offsets", "END record and padding", "Validation signature", "Strict
// mode", "CBLOCK compression"; oasis-write "File layout", "Reference
// numbers and name tables", "CBLOCK compression", "Strict and non-strict
// modes", "Standard properties", "Layer names", "END record and
// validation"; design D11, Q-OASIS-7, 8, 64).
//
// Owner: OASFMT (design D2).

#include "hefesto/oasis/codec/encoder.hpp"

#include <array>
#include <utility>

#include "encoder_impl.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/cblock.hpp"
#include "hefesto/oasis/codec/primitives.hpp"

namespace hefesto::oasis {

namespace {

using Impl = Encoder::Impl;

constexpr std::size_t kChunkFlush = 1048576;
constexpr std::size_t kEndLength = 256;
constexpr std::array<unsigned char, 13> kMagic = {'%', 'S', 'E', 'M', 'I', '-', 'O', 'A', 'S', 'I', 'S', '\r', '\n'};
/// The PROPNAME refnums of strict mode (oasis-write "Reference numbers and name tables").
constexpr std::array<StandardProperty, 4> kSeededNames = {
    StandardProperty::max_signed_integer_width, StandardProperty::max_unsigned_integer_width,
    StandardProperty::top_cell, StandardProperty::cell_offset};
/// The tables in the order strict mode writes them.
constexpr std::array<NameTable, 5> kTableOrder = {NameTable::propname, NameTable::propstring, NameTable::layername,
                                                  NameTable::textstring, NameTable::cellname};

[[noreturn]] void reject(std::string text) {
  detail::fail("OASIS-CODEC-026", std::move(text));
}

void put(std::vector<std::byte>& out, std::uint8_t byte) {
  out.push_back(std::byte{byte});
}

/// Runs `body` and turns a CodecError into the returned status.
template <typename F>
diag::Status guarded(F body) {
  try {
    body();
    return diag::Done{};
  } catch (const detail::CodecError& error) {
    return error.diagnostic;
  }
}

}  // namespace

std::uint64_t Encoder::Impl::Table::ref(std::string_view name) {
  const auto [it, fresh] = index.try_emplace(std::string{name}, names.size());
  if (fresh) {
    names.emplace_back(name);
  }
  return it->second;
}

Encoder::Impl::Impl(gds::ByteSink& byte_sink, EncoderOptions encoder_options)
    : sink(&byte_sink), options(encoder_options) {
  if (options.strict) {
    for (const StandardProperty p : kSeededNames) {
      (void)propnames.ref(standard_property_name(p));
    }
  }
}

void Encoder::Impl::write(std::span<const std::byte> bytes) {
  crc = update_signature(ValidationScheme::crc32, crc, bytes);
  sum = update_signature(ValidationScheme::checksum32, sum, bytes);
  if (auto status = sink->write(bytes); !status.ok()) {
    throw detail::CodecError{status.error()};
  }
}

void Encoder::Impl::emit(const std::vector<std::byte>& record) {
  if (options.compress == 0 || !in_sequence) {
    write(record);
    return;
  }
  chunk.insert(chunk.end(), record.begin(), record.end());
  if (chunk.size() >= kChunkFlush) {
    flush_chunk();
  }
}

void Encoder::Impl::flush_chunk() {
  if (chunk.empty()) {
    return;
  }
  auto compressed = deflate_cblock(chunk, options.compress);
  if (!compressed.ok()) {
    throw detail::CodecError{compressed.error()};
  }
  const std::size_t header = 2 + detail::unsigned_length(chunk.size()) + detail::unsigned_length(compressed.value().size());
  if (compressed.value().size() + header < chunk.size()) {
    std::vector<std::byte> block;
    put(block, 34);
    put(block, 0);
    write_unsigned(block, chunk.size());
    write_unsigned(block, compressed.value().size());
    block.insert(block.end(), compressed.value().begin(), compressed.value().end());
    write(block);
  } else {
    write(chunk);
  }
  chunk.clear();
}

void Encoder::Impl::begin() {
  if (phase != Phase::created) {
    reject("OASIS encoder: begin() called twice");
  }
  std::vector<std::byte> out;
  for (const unsigned char c : kMagic) {
    put(out, c);
  }
  put(out, 1);
  write_string(out, "1.0");
  write_real(out, options.unit);
  write_unsigned(out, options.strict ? 1 : 0);
  if (!options.strict) {
    out.insert(out.end(), 12, std::byte{0});
  }
  write(out);
  phase = Phase::header;
}

void Encoder::Impl::name_field(std::vector<std::byte>& out, std::string_view name, Table& table, StringKind kind) {
  if (!legal_string(kind, name)) {
    reject("OASIS encoder: illegal name \"" + std::string{name} + "\"");
  }
  if (options.strict) {
    write_unsigned(out, table.ref(name));
  } else {
    write_string(out, name);
  }
}

void Encoder::Impl::property_value(std::vector<std::byte>& out, const PropertyValue& v) {
  using Kind = PropertyValue::Kind;
  switch (v.kind) {
    case Kind::real:
      write_real(out, v.real);
      return;
    case Kind::unsigned_integer:
      put(out, 8);
      write_unsigned(out, v.unsigned_value);
      return;
    case Kind::signed_integer:
      if (v.signed_value == std::numeric_limits<std::int64_t>::min()) {
        reject("OASIS encoder: signed property value out of range");
      }
      put(out, 9);
      write_signed(out, v.signed_value);
      return;
    case Kind::a_string:
    case Kind::b_string:
    case Kind::n_string: {
      const auto offset = static_cast<std::uint8_t>(static_cast<int>(v.kind) - static_cast<int>(Kind::a_string));
      const std::array<StringKind, 3> kinds = {StringKind::a_string, StringKind::b_string, StringKind::n_string};
      put(out, static_cast<std::uint8_t>((options.strict ? 13 : 10) + offset));
      name_field(out, v.string, propstrings, kinds[offset]);
      return;
    }
    default:
      reject("OASIS encoder: unresolved PROPSTRING reference in a property value");
  }
}

std::vector<std::byte> Encoder::Impl::property_record(const Property& p) {
  std::vector<std::byte> out;
  const std::size_t count = p.values.size();
  const unsigned uuuu = count < 15 ? static_cast<unsigned>(count) : 15U;
  put(out, 28);
  put(out, static_cast<std::uint8_t>((uuuu << 4U) | 0x04U | (options.strict ? 0x02U : 0U) | (p.standard ? 1U : 0U)));
  if (p.name.empty()) {
    reject("OASIS encoder: property without a name");
  }
  name_field(out, p.name, propnames, StringKind::n_string);
  if (uuuu == 15) {
    write_unsigned(out, count);
  }
  for (const PropertyValue& v : p.values) {
    property_value(out, v);
  }
  return out;
}

void Encoder::Impl::header_property(const Property& p) {
  if (phase != Phase::header) {
    reject("OASIS encoder: file property after the first cell");
  }
  write(property_record(p));
}

void Encoder::Impl::layer_name(std::string_view name, std::uint32_t layer, std::uint32_t datatype) {
  if (phase != Phase::header) {
    reject("OASIS encoder: LAYERNAME after the first cell");
  }
  if (!legal_string(StringKind::n_string, name)) {
    reject("OASIS encoder: illegal layer name \"" + std::string{name} + "\"");
  }
  const auto key = std::make_tuple(std::string{name}, layer, datatype);
  if (!layernames_seen.insert(key).second) {
    return;
  }
  if (options.strict) {
    layernames.push_back(key);
    return;
  }
  std::vector<std::byte> out;
  for (const std::uint8_t id : {std::uint8_t{11}, std::uint8_t{12}}) {
    put(out, id);
    write_string(out, name);
    put(out, 3);
    write_unsigned(out, layer);
    put(out, 3);
    write_unsigned(out, datatype);
  }
  write(out);
}

void Encoder::Impl::begin_cell(std::string_view name) {
  if (phase != Phase::header && phase != Phase::cells) {
    reject("OASIS encoder: begin_cell() outside begin() and finish()");
  }
  flush_chunk();
  in_sequence = false;
  if (!cell_offsets.try_emplace(std::string{name}, sink->position()).second) {
    reject("OASIS encoder: cell " + std::string{name} + " written twice");
  }
  std::vector<std::byte> out;
  put(out, options.strict ? 13 : 14);
  name_field(out, name, cellnames, StringKind::n_string);
  write(out);
  modal.reset();
  phase = Phase::cells;
  in_sequence = true;
}

void Encoder::Impl::body_property(const Property& p) {
  if (phase == Phase::header) {
    header_property(p);
    return;
  }
  if (phase != Phase::cells) {
    reject("OASIS encoder: property() outside a cell");
  }
  emit(property_record(p));
}

void Encoder::Impl::write_table(NameTable table) {
  std::vector<std::byte> out;
  const auto names_of = [&](Table& t, std::uint8_t id) {
    for (const std::string& name : t.names) {
      out.clear();
      put(out, id);
      write_string(out, name);
      if (table == NameTable::cellname) {
        const auto it = cell_offsets.find(name);
        const std::uint64_t offset = it != cell_offsets.end() ? it->second : 0;
        out.insert(out.end(), {std::byte{28}, std::byte{0x17}, std::byte{3}, std::byte{8}});
        write_unsigned(out, offset);
      }
      emit(out);
    }
  };
  switch (table) {
    case NameTable::propname:
      names_of(propnames, 7);
      return;
    case NameTable::propstring:
      names_of(propstrings, 9);
      return;
    case NameTable::textstring:
      names_of(textstrings, 5);
      return;
    case NameTable::cellname:
      names_of(cellnames, 3);
      return;
    default:
      break;
  }
  for (const auto& [name, layer, datatype] : layernames) {
    for (const std::uint8_t id : {std::uint8_t{11}, std::uint8_t{12}}) {
      out.clear();
      put(out, id);
      write_string(out, name);
      put(out, 3);
      write_unsigned(out, layer);
      put(out, 3);
      write_unsigned(out, datatype);
      emit(out);
    }
  }
}

void Encoder::Impl::write_end() {
  std::vector<std::byte> out;
  put(out, 2);
  if (options.strict) {
    for (const TableEntry& entry : tables) {
      put(out, 1);
      write_unsigned(out, entry.offset);
    }
  }
  const std::size_t signature_bytes = options.validation == ValidationScheme::none ? 0 : 4;
  const std::size_t padding = kEndLength - (out.size() + 1 + signature_bytes);  // the b-string, length field included
  write_unsigned(out, padding - 2);
  out.insert(out.end(), padding - 2, std::byte{0});
  put(out, static_cast<std::uint8_t>(options.validation));
  write(out);
  if (signature_bytes == 0) {
    return;
  }
  const std::uint32_t value = options.validation == ValidationScheme::crc32 ? crc : sum;
  std::vector<std::byte> stored;
  for (unsigned i = 0; i < 4; ++i) {
    put(stored, static_cast<std::uint8_t>((value >> (8U * i)) & 0xffU));
  }
  write(stored);
}

void Encoder::Impl::finish() {
  if (phase != Phase::header && phase != Phase::cells) {
    reject("OASIS encoder: finish() outside begin()");
  }
  flush_chunk();
  in_sequence = false;
  if (options.strict) {
    for (const NameTable table : kTableOrder) {
      const std::uint64_t at = sink->position();
      in_sequence = true;
      write_table(table);
      flush_chunk();
      in_sequence = false;
      const bool written = sink->position() != at;
      tables[static_cast<std::size_t>(table)] = {true, written ? at : 0};
    }
    tables[static_cast<std::size_t>(NameTable::xname)] = {true, 0};
  }
  write_end();
  phase = Phase::finished;
}

// --- Encoder -----------------------------------------------------------------

Encoder::Encoder(gds::ByteSink& sink, EncoderOptions options) : impl_(std::make_unique<Impl>(sink, options)) {}
Encoder::~Encoder() = default;
Encoder::Encoder(Encoder&&) noexcept = default;
Encoder& Encoder::operator=(Encoder&&) noexcept = default;

diag::Status Encoder::begin() {
  return guarded([this] { impl_->begin(); });
}

diag::Status Encoder::standard_properties(std::optional<std::string_view> top_cell) {
  return guarded([&] {
    for (const StandardProperty p : {StandardProperty::max_signed_integer_width,
                                     StandardProperty::max_unsigned_integer_width}) {
      impl_->header_property(Property{std::string{standard_property_name(p)}, std::nullopt, true,
                                      {PropertyValue::of_unsigned(4)}});
    }
    if (top_cell) {
      impl_->header_property(Property{std::string{standard_property_name(StandardProperty::top_cell)}, std::nullopt,
                                      true,
                                      {PropertyValue::of_string(PropertyValue::Kind::n_string, std::string{*top_cell})}});
    }
  });
}

diag::Status Encoder::file_property(const Property& property) {
  return guarded([&] { impl_->header_property(property); });
}

diag::Status Encoder::layer_name(std::string_view name, std::uint32_t layer, std::uint32_t datatype) {
  return guarded([&] { impl_->layer_name(name, layer, datatype); });
}

diag::Status Encoder::begin_cell(std::string_view name) {
  return guarded([&] { impl_->begin_cell(name); });
}

diag::Status Encoder::element(const Element& value) {
  return guarded([&] { impl_->element(value); });
}

diag::Status Encoder::property(const Property& value) {
  return guarded([&] { impl_->body_property(value); });
}

diag::Status Encoder::finish() {
  return guarded([this] { impl_->finish(); });
}

gds::ByteSink& Encoder::sink() const noexcept {
  return *impl_->sink;
}
const ModalState& Encoder::modal() const noexcept {
  return impl_->modal;
}
const EncoderOptions& Encoder::options() const noexcept {
  return impl_->options;
}

}  // namespace hefesto::oasis
