// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS decoder: the record dispatch and the file-level records
// (capability oasis-format "File structure and magic bytes", "Record
// identifiers", "START record and table offsets", "END record and
// padding", "Validation signature", "Name records and reference numbers",
// "CELL record and cell content", "Modal variables and reset rules",
// "PROPERTY records", "Standard properties", "Extension records", "CBLOCK
// compression", "PAD record"; design D11, Q-OASIS-2, 5, 6, 8, 9).
//
// Owner: OASFMT (design D2).

#include "hefesto/oasis/codec/decoder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "decoder_impl.hpp"
#include "hefesto/oasis/codec/cblock.hpp"

namespace hefesto::oasis {

namespace {

using Handler = void (Decoder::Impl::*)();
using Impl = Decoder::Impl;

constexpr std::array<Handler, 35> kHandlers = {
    &Impl::pad,           &Impl::start_record,        &Impl::end_record,          &Impl::cellname,
    &Impl::cellname_explicit, &Impl::textstring,      &Impl::textstring_explicit, &Impl::propname,
    &Impl::propname_explicit, &Impl::propstring,      &Impl::propstring_explicit, &Impl::layername,
    &Impl::layername,     &Impl::cell_by_reference,   &Impl::cell_by_name,        &Impl::xyabsolute,
    &Impl::xyrelative,    &Impl::placement,           &Impl::placement,           &Impl::text,
    &Impl::rectangle,     &Impl::polygon,             &Impl::path,                &Impl::trapezoid,
    &Impl::trapezoid,     &Impl::trapezoid,           &Impl::ctrapezoid,          &Impl::circle,
    &Impl::property,      &Impl::property_repeat,     &Impl::xname,               &Impl::xname_explicit,
    &Impl::xelement,      &Impl::xgeometry,           &Impl::cblock,
};

constexpr std::array<std::string_view, 6> kTableNames = {"CELLNAME",   "TEXTSTRING", "PROPNAME",
                                                         "PROPSTRING", "LAYERNAME",  "XNAME"};

constexpr std::array<unsigned char, 13> kMagic = {'%', 'S', 'E', 'M', 'I', '-', 'O', 'A', 'S', 'I', 'S', '\r', '\n'};

std::string hex32(std::uint32_t value) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  std::string text = "0x";
  for (int shift = 28; shift >= 0; shift -= 4) {
    text.push_back(digits[(value >> static_cast<unsigned>(shift)) & 0xfU]);
  }
  return text;
}

/// Whether `id` is transparent for strict-mode contiguity and PROPERTY
/// attachment, or a name record (which the tracker sees itself).
bool keeps_runs(RecordId id) {
  switch (id) {
    case RecordId::pad:
    case RecordId::cblock:
    case RecordId::property:
    case RecordId::property_repeat:
      return true;
    default:
      return (id >= RecordId::cellname_implicit && id <= RecordId::layername_text) || id == RecordId::xname_implicit ||
             id == RecordId::xname;
  }
}

std::size_t used_index(NameTable table) {
  return static_cast<std::size_t>(table);
}

}  // namespace

std::string_view name_table_name(NameTable table) noexcept {
  return kTableNames[static_cast<std::size_t>(table)];
}

Decoder::Impl::Impl(gds::ByteSource& byte_source, DecoderOptions decoder_options)
    : source(&byte_source), options(decoder_options), in(byte_source) {}

std::optional<Record> Decoder::Impl::next() {
  if (finished) {
    return std::nullopt;
  }
  if (!magic_read) {
    magic();
  }
  if (in.at_end()) {
    detail::fail("OASIS-CODEC-002", "unexpected end of file: no END record");
  }
  record = Record{};
  record.in_cblock = in.in_cblock();
  record.offset = record.in_cblock ? in.cblock_offset() : in.file_position();
  record.cblock_position = record.in_cblock ? in.cblock_position() : 0;
  const std::uint64_t id = detail::read_unsigned(in);
  if (id >= kHandlers.size()) {
    detail::fail("OASIS-CODEC-004", "invalid record type " + std::to_string(id));
  }
  record.id = static_cast<RecordId>(id);
  if ((record.id == RecordId::start) == start.has_value()) {
    detail::fail("OASIS-CODEC-004", "invalid record type " + std::to_string(id) +
                                        (start ? ": a second START record" : ": START record expected"));
  }
  if (record.in_cblock && (record.id == RecordId::start || record.id == RecordId::end || record.id == RecordId::cblock)) {
    detail::fail("OASIS-CODEC-023", "record type " + std::to_string(id) + " inside a CBLOCK");
  }
  if (!keeps_runs(record.id)) {
    strict.other_record();
  }
  drop = {};
  (this->*kHandlers[id])();
  if (drop.dropped()) {
    diagnostics.push_back(*drop.diagnostic);
    record.element.reset();
  }
  return std::move(record);
}

void Decoder::Impl::magic() {
  for (const unsigned char expected : kMagic) {
    if (in.at_end() || in.get() != expected) {
      detail::fail("OASIS-CODEC-001", "not an OASIS file");
    }
  }
  magic_read = true;
}

void Decoder::Impl::warn(std::string_view code, std::string text) {
  diagnostics.push_back(diag::make(code, std::move(text)));
}

void Decoder::Impl::check(StringKind kind, std::string_view text) {
  if (auto problem = detail::check_string(kind, text)) {
    diagnostics.push_back(std::move(*problem));
  }
}

TableOffsets Decoder::Impl::read_tables() {
  TableOffsets tables{};
  for (TableEntry& entry : tables) {
    entry.strict = detail::read_unsigned(in) != 0;
    entry.offset = detail::read_unsigned(in);
  }
  return tables;
}

void Decoder::Impl::start_record() {
  StartInfo info;
  info.version = detail::read_string(in);
  if (info.version != "1.0") {
    detail::fail("OASIS-CODEC-013", "unsupported OASIS version " + info.version);
  }
  info.unit = detail::read_real(in);
  if (!(info.unit > 0.0) || !std::isfinite(info.unit)) {
    detail::fail("OASIS-CODEC-013", "invalid unit");
  }
  info.offsets_at_end = detail::read_unsigned(in) != 0;
  if (!info.offsets_at_end) {
    info.tables = read_tables();
  }
  start = info;
  record.start = std::move(info);
  modal.reset();
  target = PropertyTarget::file;
}

ValidationResult Decoder::Impl::validation(std::uint64_t scheme) {
  if (scheme > 2) {
    warn("OASIS-CODEC-014", "unknown validation scheme " + std::to_string(scheme));
    return {};
  }
  ValidationResult result{static_cast<ValidationScheme>(scheme)};
  if (result.scheme == ValidationScheme::none) {
    return result;
  }
  result.computed = result.scheme == ValidationScheme::crc32 ? in.crc32() : in.checksum32();
  for (unsigned i = 0; i < 4; ++i) {
    result.stored |= std::uint32_t{in.get()} << (8U * i);
  }
  if (!result.ok()) {
    warn("OASIS-CODEC-015",
         "OASIS validation failed (expected " + hex32(result.stored) + ", computed " + hex32(result.computed) + ")");
  }
  return result;
}

void Decoder::Impl::end_record() {
  EndInfo end;
  if (start->offsets_at_end) {
    end.tables = read_tables();
  }
  detail::skip_string(in);
  end.validation_scheme = detail::read_unsigned(in);
  end.validation = validation(end.validation_scheme);
  end.length = in.file_position() - record.offset;
  if (end.length != 256) {
    warn("OASIS-CODEC-014", "END record length " + std::to_string(end.length));
  }
  finished = true;
  if (!in.at_end()) {
    warn("OASIS-CODEC-003", "data after END record");
  }
  after_end(end);
  record.end = std::move(end);
}

void Decoder::Impl::after_end(const EndInfo& end) {
  if (options.check_strict) {
    const auto found = strict.finish(end.tables ? *end.tables : start->tables);
    diagnostics.insert(diagnostics.end(), found.begin(), found.end());
  }
  for (const auto& found : {reference_checks(), cell_checks(), standard_checks()}) {
    diagnostics.insert(diagnostics.end(), found.begin(), found.end());
  }
}

std::vector<diag::Diagnostic> Decoder::Impl::reference_checks() const {
  const std::array<const std::map<std::uint64_t, std::string>*, 4> tables = {&names.cellnames, &names.textstrings,
                                                                             &names.propnames, &names.propstrings};
  std::vector<diag::Diagnostic> out;
  for (std::size_t t = 0; t < tables.size(); ++t) {
    for (const std::uint64_t ref : used[t]) {
      if (!tables[t]->contains(ref)) {
        out.push_back(diag::make("OASIS-CODEC-018", "undefined " + std::string{kTableNames[t]} + " refnum " +
                                                        std::to_string(ref)));
      }
    }
  }
  return out;
}

std::vector<diag::Diagnostic> Decoder::Impl::cell_checks() const {
  std::map<std::string, int> seen;
  std::vector<diag::Diagnostic> out;
  for (const auto& cell : cells) {
    const auto* ref = std::get_if<std::uint64_t>(&cell);
    const auto it = ref != nullptr ? names.cellnames.find(*ref) : names.cellnames.end();
    if (ref != nullptr && it == names.cellnames.end()) {
      continue;  // reported as an undefined refnum
    }
    const std::string& name = ref != nullptr ? it->second : std::get<std::string>(cell);
    if (++seen[name] == 2) {
      out.push_back(diag::make("OASIS-CODEC-019", "cell " + name + " defined twice"));
    }
  }
  return out;
}

std::vector<diag::Diagnostic> Decoder::Impl::standard_checks() const {
  std::vector<diag::Diagnostic> out;
  for (const auto& entry : standard_names) {
    const auto* ref = std::get_if<std::uint64_t>(&entry);
    const auto it = ref != nullptr ? names.propnames.find(*ref) : names.propnames.end();
    if (ref != nullptr && it == names.propnames.end()) {
      continue;
    }
    const std::string& name = ref != nullptr ? it->second : std::get<std::string>(entry);
    if (!standard_property(name)) {
      out.push_back(diag::make("OASIS-CODEC-025", "unknown standard property " + name));
    }
  }
  return out;
}

std::uint64_t Decoder::Impl::define_name(NameTable table, bool implicit, const std::string& name) {
  const auto t = static_cast<std::size_t>(table);
  const Numbering wanted = implicit ? Numbering::implicit : Numbering::explicit_;
  if (numbering[t] != Numbering::none && numbering[t] != wanted) {
    detail::fail("OASIS-CODEC-016", "implicit and explicit " + std::string{kTableNames[t]} + " records mixed");
  }
  numbering[t] = wanted;
  const std::uint64_t ref = implicit ? implicit_next[t]++ : detail::read_unsigned(in);
  std::map<std::uint64_t, std::string>* tables[] = {&names.cellnames, &names.textstrings, &names.propnames,
                                                    &names.propstrings};
  const bool fresh = table == NameTable::xname ? !names.xnames.contains(ref) : tables[t]->try_emplace(ref, name).second;
  if (!fresh) {
    detail::fail("OASIS-CODEC-016", "duplicate " + std::string{kTableNames[t]} + " refnum " + std::to_string(ref));
  }
  return ref;
}

void Decoder::Impl::name_record(NameTable table, bool implicit, StringKind kind) {
  std::string name = detail::read_string(in);
  check(kind, name);
  const std::uint64_t ref = define_name(table, implicit, name);
  if (table == NameTable::cellname && !cellname_index.try_emplace(name, ref).second) {
    warn("OASIS-CODEC-017", "duplicate cell name " + name);
  }
  strict.name_record(table, record.offset);
  modal.reset();
  target = PropertyTarget::name_record;
  record.name = std::move(name);
  record.reference = ref;
}

void Decoder::Impl::cellname() {
  name_record(NameTable::cellname, true, StringKind::n_string);
}
void Decoder::Impl::cellname_explicit() {
  name_record(NameTable::cellname, false, StringKind::n_string);
}
void Decoder::Impl::textstring() {
  name_record(NameTable::textstring, true, StringKind::a_string);
}
void Decoder::Impl::textstring_explicit() {
  name_record(NameTable::textstring, false, StringKind::a_string);
}
void Decoder::Impl::propname() {
  name_record(NameTable::propname, true, StringKind::n_string);
}
void Decoder::Impl::propname_explicit() {
  name_record(NameTable::propname, false, StringKind::n_string);
}
void Decoder::Impl::propstring() {
  name_record(NameTable::propstring, true, StringKind::b_string);
}
void Decoder::Impl::propstring_explicit() {
  name_record(NameTable::propstring, false, StringKind::b_string);
}

void Decoder::Impl::layername() {
  LayerName layer;
  layer.name = detail::read_string(in);
  layer.text = record.id == RecordId::layername_text;
  check(StringKind::n_string, layer.name);
  layer.layers = detail::read_interval(in);
  layer.datatypes = detail::read_interval(in);
  if (layer.layers.empty() || layer.datatypes.empty()) {
    warn("OASIS-CODEC-012", "empty interval in LAYERNAME " + layer.name);
  }
  strict.name_record(NameTable::layername, record.offset);
  modal.reset();
  target = PropertyTarget::name_record;
  names.layernames.push_back(layer);
  record.name = layer.name;
  record.layer_name = std::move(layer);
}

void Decoder::Impl::xname() {
  XName x{detail::read_unsigned(in), detail::read_string(in)};
  const std::uint64_t ref = define_name(NameTable::xname, record.id == RecordId::xname_implicit, x.name);
  names.xnames.emplace(ref, x);
  strict.name_record(NameTable::xname, record.offset);
  modal.reset();
  target = PropertyTarget::name_record;
  record.name = x.name;
  record.reference = ref;
  record.xname = std::move(x);
}

void Decoder::Impl::xname_explicit() {
  xname();
}

void Decoder::Impl::cell_by_reference() {
  const std::uint64_t ref = detail::read_unsigned(in);
  used[used_index(NameTable::cellname)].insert(ref);
  cells.emplace_back(ref);
  record.reference = ref;
  modal.reset();
  target = PropertyTarget::cell;
}

void Decoder::Impl::cell_by_name() {
  std::string name = detail::read_string(in);
  check(StringKind::n_string, name);
  strict.inline_use(detail::InlineUse::cell_by_name);
  cells.emplace_back(name);
  record.name = std::move(name);
  modal.reset();
  target = PropertyTarget::cell;
}

void Decoder::Impl::xyabsolute() {
  modal.xy_absolute = true;
  target = PropertyTarget::cell;
}

void Decoder::Impl::xyrelative() {
  modal.xy_absolute = false;
  target = PropertyTarget::cell;
}

PropertyValue Decoder::Impl::property_value() {
  using Kind = PropertyValue::Kind;
  const std::uint64_t type = detail::read_unsigned(in);
  PropertyValue value;
  if (type <= 7) {
    value.kind = Kind::real;
    value.real = detail::read_real_data(in, type);
  } else if (type == 8) {
    value = PropertyValue::of_unsigned(detail::read_unsigned(in));
  } else if (type == 9) {
    value.kind = Kind::signed_integer;
    value.signed_value = detail::read_signed(in);
  } else if (type <= 12) {
    constexpr std::array<StringKind, 3> kinds = {StringKind::a_string, StringKind::b_string, StringKind::n_string};
    value = PropertyValue::of_string(static_cast<Kind>(static_cast<int>(Kind::a_string) + (type - 10)),
                                     detail::read_string(in));
    check(kinds[type - 10], value.string);
    strict.inline_use(detail::InlineUse::property_string);
  } else if (type <= 15) {
    value.kind = static_cast<Kind>(static_cast<int>(Kind::a_reference) + (type - 13));
    value.reference = detail::read_unsigned(in);
    used[used_index(NameTable::propstring)].insert(value.reference);
  } else {
    detail::fail("OASIS-CODEC-028", "invalid property value type " + std::to_string(type));
  }
  return value;
}

void Decoder::Impl::property_name(std::uint8_t info) {
  if ((info & 0x04U) == 0) {
    if (!modal.last_property_name && !modal.last_property_name_reference) {
      detail::fail("OASIS-CODEC-020", "modal variable last-property-name undefined");
    }
  } else if ((info & 0x02U) != 0) {
    modal.last_property_name_reference = detail::read_unsigned(in);
    modal.last_property_name.reset();
    used[used_index(NameTable::propname)].insert(*modal.last_property_name_reference);
  } else {
    modal.last_property_name = detail::read_string(in);
    modal.last_property_name_reference.reset();
    check(StringKind::n_string, *modal.last_property_name);
    strict.inline_use(detail::InlineUse::property_name);
  }
}

void Decoder::Impl::property() {
  const std::uint8_t info = in.get();
  Property p;
  p.standard = (info & 0x01U) != 0;
  property_name(info);
  p.name = modal.last_property_name.value_or("");
  p.name_reference = modal.last_property_name_reference;
  const unsigned count_field = info >> 4U;
  if ((info & 0x08U) != 0) {
    if (!modal.last_property_values) {
      detail::fail("OASIS-CODEC-020", "modal variable last-value-list undefined");
    }
    p.values = *modal.last_property_values;
  } else {
    const std::uint64_t count = count_field == 15 ? detail::read_unsigned(in) : count_field;
    for (std::uint64_t i = 0; i < count; ++i) {
      p.values.push_back(property_value());
    }
  }
  modal.last_property_values = p.values;
  modal.last_property_standard = p.standard;
  if (p.standard) {
    standard_names.push_back(p.name_reference ? std::variant<std::uint64_t, std::string>{*p.name_reference}
                                              : std::variant<std::uint64_t, std::string>{p.name});
  }
  record.property = std::move(p);
  record.target = target;
}

void Decoder::Impl::property_repeat() {
  if ((!modal.last_property_name && !modal.last_property_name_reference) || !modal.last_property_values ||
      !modal.last_property_standard) {
    detail::fail("OASIS-CODEC-020", "modal variable last-property-name undefined");
  }
  record.property = Property{modal.last_property_name.value_or(""), modal.last_property_name_reference,
                             *modal.last_property_standard, *modal.last_property_values};
  record.target = target;
}

namespace {

// The inflate cap of one CBLOCK (design D11): 256 times its compressed
// size, saturating, and at least `floor` (and at least 1 byte, as 0 would
// mean inflate_cblock's default cap).
std::uint64_t cblock_cap(std::uint64_t floor, std::uint64_t compressed) noexcept {
  constexpr std::uint64_t kRatio = 256;
  constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
  const std::uint64_t scaled = compressed > kMax / kRatio ? kMax : compressed * kRatio;
  return std::max({floor, scaled, std::uint64_t{1}});
}

}  // namespace

void Decoder::Impl::cblock() {
  const std::uint32_t type = detail::read_u32(in, "CBLOCK compression type");
  if (type != 0) {
    detail::fail("OASIS-CODEC-023", "unsupported CBLOCK compression type " + std::to_string(type));
  }
  const std::uint64_t uncompressed = detail::read_unsigned(in);
  const std::uint64_t compressed = detail::read_unsigned(in);
  const std::uint64_t cap = cblock_cap(options.inflate_limit, compressed);
  if (uncompressed > cap) {
    detail::fail("OASIS-CODEC-023", "CBLOCK of " + std::to_string(uncompressed) + " bytes is over the cap of " +
                                        std::to_string(cap) + " bytes");
  }
  const std::vector<std::byte> data = in.read_file_bytes(compressed);
  auto inflated = inflate_cblock(data, uncompressed, cap);
  if (!inflated.ok()) {
    throw detail::CodecError{inflated.error()};
  }
  in.push_cblock(std::move(inflated).value(), record.offset);
}

// --- Decoder -----------------------------------------------------------------

Decoder::Decoder(gds::ByteSource& source, DecoderOptions options)
    : impl_(std::make_unique<Impl>(source, options)) {}
Decoder::~Decoder() = default;
Decoder::Decoder(Decoder&&) noexcept = default;
Decoder& Decoder::operator=(Decoder&&) noexcept = default;

diag::Outcome<std::optional<Record>> Decoder::next() {
  if (impl_->failure) {
    return *impl_->failure;
  }
  try {
    return impl_->next();
  } catch (const detail::CodecError& error) {
    impl_->failure = error.diagnostic;
    impl_->finished = true;
    return error.diagnostic;
  }
}

gds::ByteSource& Decoder::source() const noexcept {
  return *impl_->source;
}
const ModalState& Decoder::modal() const noexcept {
  return impl_->modal;
}
const NameTables& Decoder::names() const noexcept {
  return impl_->names;
}
const DecoderOptions& Decoder::options() const noexcept {
  return impl_->options;
}
const std::optional<StartInfo>& Decoder::start() const noexcept {
  return impl_->start;
}
const std::vector<diag::Diagnostic>& Decoder::diagnostics() const noexcept {
  return impl_->diagnostics;
}
std::vector<diag::Diagnostic> Decoder::take_diagnostics() {
  return std::exchange(impl_->diagnostics, {});
}

}  // namespace hefesto::oasis
