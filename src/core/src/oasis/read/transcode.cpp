// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pass 2 of the OASIS reader: the cell and record loop of the
// GDS-equivalent stream (capability oasis-read "Two-pass decoding and name
// resolution" pass 2, "Units and input scale", "Cell processing", "Cell
// timestamps", "Unsupported and extension content", "Read diagnostics").
//
// The stream is a GDSII library whose UNITS hold mpd = 1e-6 / unit, so
// gds-read derives the read scale exactly as oasis-read specifies, and
// whose structures are the complete cells of pass 1 in file order, named
// as resolved (`#<n>` for an undefined CELLNAME, Q-OASIS-34). The BGNSTR
// date is a fixed, known one: the cell timestamp comes from the session
// stamp gds::read is given (Q-OASIS-30). The LIBNAME is the top cell, so
// gds-read's library-cell rule loads it ("Top cell selection").
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <array>
#include <limits>

#include "hefesto/gds/stream/encode.hpp"
#include "transcoder.hpp"

namespace hefesto::oasis::read_detail {

namespace {

using gds::DataType;
using gds::RecordType;

void put_u16(std::vector<std::byte>& out, std::uint32_t v) {
  out.push_back(static_cast<std::byte>((v >> 8U) & 0xffU));
  out.push_back(static_cast<std::byte>(v & 0xffU));
}

/// Records that end the content of a cell.
bool ends_cell(RecordId id) {
  return id == RecordId::end || id == RecordId::xname_implicit || id == RecordId::xname ||
         (id >= RecordId::cellname_implicit && id <= RecordId::layername_text);
}

/// A fixed BGNSTR date (1970-01-01): known, so no "Unknown timestamp
/// format" message; the stamp itself comes from the session stamp.
constexpr std::array<std::int16_t, 12> kDate{70, 1, 1, 0, 0, 0, 70, 1, 1, 0, 0, 0};

}  // namespace

// --- GDS records ------------------------------------------------------------------

void GdsBuilder::record(RecordType type, DataType data, std::span<const std::byte> payload) {
  put_u16(bytes_, static_cast<std::uint32_t>(payload.size() + 4));
  bytes_.push_back(static_cast<std::byte>(type));
  bytes_.push_back(static_cast<std::byte>(data));
  bytes_.insert(bytes_.end(), payload.begin(), payload.end());
}

void GdsBuilder::empty(RecordType type) {
  record(type, DataType::none, {});
}

void GdsBuilder::int2(RecordType type, std::span<const std::int16_t> values) {
  std::vector<std::byte> p;
  p.reserve(values.size() * 2);
  for (const std::int16_t v : values) {
    put_u16(p, static_cast<std::uint16_t>(v));
  }
  record(type, DataType::int2, p);
}

void GdsBuilder::int4(RecordType type, std::span<const std::int32_t> values) {
  std::vector<std::byte> p;
  p.reserve(values.size() * 4);
  for (const std::int32_t v : values) {
    const auto u = static_cast<std::uint32_t>(v);
    put_u16(p, u >> 16U);
    put_u16(p, u & 0xffffU);
  }
  record(type, DataType::int4, p);
}

void GdsBuilder::real8(RecordType type, std::span<const double> values) {
  std::vector<std::byte> p;
  for (const double v : values) {
    const auto r = gds::encode_real8(v);
    p.insert(p.end(), r.begin(), r.end());
  }
  record(type, DataType::real8, p);
}

void GdsBuilder::string(RecordType type, std::string_view text) {
  std::vector<std::byte> p;
  p.reserve(text.size() + 1);
  for (const char c : text) {
    p.push_back(static_cast<std::byte>(c));
  }
  if (p.size() % 2 != 0) {
    p.push_back(std::byte{0});
  }
  record(type, DataType::string, p);
}

std::optional<std::int32_t> coordinate(std::int64_t base, std::int64_t offset) noexcept {
  std::int64_t sum = 0;
  if (__builtin_add_overflow(base, offset, &sum) || sum < std::numeric_limits<std::int32_t>::min() ||
      sum > std::numeric_limits<std::int32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(sum);
}

std::int16_t int2_number(std::uint64_t value) noexcept {
  return static_cast<std::int16_t>(value > static_cast<std::uint64_t>(kMaxGdsInt2) ? kMaxGdsInt2 : value);
}

// --- the record loop -------------------------------------------------------------

Transcoder::Transcoder(const FileIndex& index, const TranscodeOptions& options, ReadMessages& messages)
    : index_(index), options_(options), messages_(messages) {}

Transcoded Transcoder::run(gds::ByteSource& source) {
  library_begin();
  Decoder decoder(source);
  for (;;) {
    auto record = decoder.next();
    if (!record.ok() || !record.value() || !take(*record.value())) {
      break;  // pass 1 reported the error; its cell is not in the index
    }
  }
  if (in_cell_) {
    end_cell();
  }
  gds_.empty(RecordType::endlib);
  out_.gds = gds_.take();
  return std::move(out_);
}

void Transcoder::library_begin() {
  gds_.int2(RecordType::header, std::array<std::int16_t, 1>{600});
  gds_.int2(RecordType::bgnlib, kDate);
  // Cut as the top cell's STRNAME is (begin_cell reports it), so the
  // library cell rule still finds it.
  gds_.string(RecordType::libname, std::string_view{options_.top_cell}.substr(0, kMaxString));
  const double unit = index_.unit > 0.0 ? index_.unit : 1.0;
  gds_.real8(RecordType::units, std::array<double, 2>{1.0 / unit, 1.0e-6 / unit});
}

bool Transcoder::take(const Record& r) {
  position_ = StreamPosition::of(r);
  switch (r.id) {
    case RecordId::cell_by_reference:
    case RecordId::cell_by_name:
      return take_cell(r);
    case RecordId::property:
    case RecordId::property_repeat:
      property(r);
      return true;
    case RecordId::pad:
    case RecordId::cblock:
      return true;
    default:
      break;
  }
  flush_placement();
  if (ends_cell(r.id)) {
    if (in_cell_) {
      end_cell();
    }
    if (r.id == RecordId::xname || r.id == RecordId::xname_implicit) {
      unsupported("XNAME");
    }
    return true;
  }
  if (in_cell_ && r.element) {
    element(*r.element);
  }
  return !capped_;
}

bool Transcoder::take_cell(const Record& r) {
  if (in_cell_) {
    end_cell();
  }
  if (capped_ || next_cell_ >= index_.cells.size()) {
    return false;  // the cells after a fatal error are not read
  }
  begin_cell(r);
  return true;
}

void Transcoder::begin_cell(const Record& /*r*/) {
  const IndexedCell& cell = index_.cells[next_cell_++];
  cell_ = cell.name;
  reported_.clear();
  in_cell_ = true;
  out_.positions.emplace_back(gds_.size(), position_);
  gds_.int2(RecordType::bgnstr, kDate);
  std::string name = cell.name;
  if (name.size() > kMaxString) {
    error("OASIS-READ-006", "cell name of " + std::to_string(name.size()) + " bytes cut to " +
                                std::to_string(kMaxString));
    name.resize(kMaxString);
  }
  gds_.string(RecordType::strname, name);
  out_.structures.emplace_back(gds_.size(), cell.name);
}

void Transcoder::end_cell() {
  flush_placement();
  gds_.empty(RecordType::endstr);
  in_cell_ = false;
}

void Transcoder::element(const Element& e) {
  switch (e.kind) {
    case ElementKind::placement:
      pending_ = Placement{e, position_, {}, {}};
      return;
    case ElementKind::xgeometry:
      unsupported("XGEOMETRY");
      return;
    case ElementKind::xelement:
      unsupported("XELEMENT");
      return;
    default:
      out_.positions.emplace_back(gds_.size(), position_);
      shape(e);
      return;
  }
}

void Transcoder::unsupported(std::string_view kind) {
  if (!in_cell_) {
    if (!xname_reported_) {
      xname_reported_ = true;
      messages_.print(diag::make("OASIS-READ-005", std::string{kind} + " not supported: skipping."));
    }
    return;
  }
  if (reported_.insert(std::string{kind}).second) {
    error("OASIS-READ-005", std::string{kind} + " not supported: skipping.");
  }
}

void Transcoder::property(const Record& r) {
  if (pending_ && r.property && r.target == PropertyTarget::element) {
    placement_properties(*r.property);
  }
  // Properties of geometry, TEXT, cells, names and the file are discarded
  // as gds-read discards user properties (Q-OASIS-37).
}

void Transcoder::error(std::string_view code, std::string text) {
  messages_.counted(cell_, position_, diag::make(code, std::move(text)));
}

bool Transcoder::room() {
  if (capped_) {
    return false;
  }
  const ReadLimits& limits = options_.limits;
  if (gds_.size() <= limits.stream_bytes && ++instances_ <= limits.instances) {
    return true;
  }
  capped_ = true;
  error("OASIS-READ-007", "the file has more than " + std::to_string(limits.instances) +
                              " element instances, or a GDS-equivalent stream over " +
                              std::to_string(limits.stream_bytes) + " bytes; the rest of the file is not read");
  return false;
}

Transcoded transcode(gds::ByteSource& source, const FileIndex& index, const TranscodeOptions& options,
                     ReadMessages& messages) {
  Transcoder transcoder(index, options, messages);
  return transcoder.run(source);
}

}  // namespace hefesto::oasis::read_detail
