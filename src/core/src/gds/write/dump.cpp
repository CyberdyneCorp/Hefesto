// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Full dump of a vendor library (gds-write "Full dump of a vendor
// library"; see vendor.hpp). Ports Magic 8.3.694 (fc5daa9e)
// calma/CalmaWrite.c calmaFullDump and calmaDumpStructure, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Decisions:
// - Q-MASK-211 (compat): the prefix comes from the session's glibc TYPE_3
//   random() (EditorState::mask.random, shared with `random`) with
//   magic-compat ON, and from an FNV-1a hash of the resolved file name,
//   with the next free prefix on a collision, with it OFF. Magic loops
//   forever once its 936 prefixes are taken; Hefesto reports an error and
//   dumps without a prefix.
// - Q-MASK-212 (fix): the units are always checked; a refused library is
//   remembered for the write, so it is reported once.
// - Q-MASK-213 (fix): a library without a structure writes nothing.
// - Q-MASK-214 (compat): see LibraryWriter (the structures appended at the
//   end of the write).
// - Q-MASK-215 (compat): ON every structure's creation date is the
//   `datestamp` or the triggering cell's timestamp; OFF the source
//   structure's creation date unless `datestamp` is set.
// - Q-MASK-128 (fix): the library comparison substitutes the file name as
//   GDS_FILE values are (gds::gds_file_value).
// Magic's tables are hash tables; the undefined references are kept in the
// order they were recorded and the libraries in dump order, which decides
// the order of the structures appended at the end of a write with several
// undefined references and the prefix lookup of a later definition.
// Owner: GDSW.

#include <array>
#include <cctype>

#include "codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/read/vendor.hpp"
#include "hefesto/gds/stream/encode.hpp"
#include "hefesto/gds/stream/values.hpp"
#include "vendor.hpp"

namespace hefesto::gds::detail {

namespace {

// Record types Magic skips in a library header (calmaInt.h).
constexpr std::array<std::uint8_t, 3> kSkipBeforeLib{0x39, 0x3a, 0x3b};  // LIBDIRSIZE, SRFNAME, LIBSECUR
constexpr std::array<std::uint8_t, 8> kHeaderSkip{0x36, 0x37, 0x38, 0x1f, 0x20, 0x23, 0x24, 0x22};
constexpr std::size_t kPrefixCount = 26 * 36;
constexpr std::size_t kRandomTries = 64 * kPrefixCount;

std::string prefix_at(std::size_t index) {
  const std::size_t second = index % 36;
  return std::string{static_cast<char>('A' + index / 36),
                     static_cast<char>(second < 26 ? 'A' + second : '0' + (second - 26)), '_'};
}

std::uint64_t fnv1a(std::string_view text) {
  std::uint64_t h = 14695981039346656037ULL;
  for (const char c : text) {
    h ^= static_cast<std::uint8_t>(c);
    h *= 1099511628211ULL;
  }
  return h;
}

bool no_prefix_view(const db::CellDef& def) {
  const db::PropertyValue* view = def.properties().find("LEFview");
  const auto* text = view != nullptr ? std::get_if<std::string>(view) : nullptr;
  if (text == nullptr || text->size() != 9) {
    return false;
  }
  std::string lower;
  for (const char c : *text) {
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return lower == "no_prefix";
}

const std::string* string_property(const db::CellDef& def, std::string_view name) {
  const db::PropertyValue* value = def.properties().find(name);
  return value != nullptr ? std::get_if<std::string>(value) : nullptr;
}

}  // namespace

// One calmaFullDump: the reader of the vendor file and calmaDefHash.
struct FullDump::Dump {
  Dump(VendorIo& io_, const db::CellDef& def_, ByteSource& source, const std::string& file_)
      : io(io_), def(def_), reader(source), file(file_), out(io_.records, io_.context(def_), &io_.sink) {}

  VendorIo& io;
  const db::CellDef& def;
  RecordReader reader;
  const std::string& file;
  ElementWriter out;
  std::string libname;
  std::string prefix;
  std::map<std::string, std::string> names;  // structure -> '0' (forward) or '1' (defined) + written name

  void error(const std::string& text) {
    io.report(codes::kDumpError, "Error while reading cell \"" + def.name() + "\" (byte position " +
                                     std::to_string(reader.position()) + "): " + text);
  }

  // calmaSkipExact, calmaReadStringRecord.
  std::optional<Record> expect(RecordType type) {
    diag::Outcome<std::optional<Record>> next = reader.next();
    if (!next.ok() || !next.value()) {
      error("Unexpected EOF.");
      return std::nullopt;
    }
    if (next.value()->type != static_cast<std::uint8_t>(type)) {
      error("Expected record type " + std::to_string(static_cast<int>(type)) + " but got " +
            std::to_string(static_cast<int>(next.value()->type)) + ".");
      return std::nullopt;
    }
    return std::move(*next.value());
  }

  // calmaSkipSet.
  template <std::size_t N>
  void skip_set(const std::array<std::uint8_t, N>& types) {
    for (;;) {
      diag::Outcome<std::optional<Record>> next = reader.peek();
      if (!next.ok() || !next.value() ||
          std::find(types.begin(), types.end(), next.value()->type) == types.end()) {
        return;
      }
      (void)reader.next();
    }
  }

  // calmaSkipTo(ENDSTR).
  void skip_to_endstr() {
    for (;;) {
      diag::Outcome<std::optional<Record>> next = reader.next();
      if (!next.ok() || !next.value() || next.value()->type == static_cast<std::uint8_t>(RecordType::endstr)) {
        return;
      }
    }
  }

  bool fail(const diag::Status& s) {
    if (!s.ok() && io.error.ok()) {
      io.error = s;
    }
    return s.ok();
  }
};

bool FullDump::seen(const std::string& file) const {
  return refused_.contains(file) ||
         std::any_of(libraries_.begin(), libraries_.end(), [&](const auto& lib) { return lib.first == file; });
}

bool FullDump::refused(const std::string& file) const {
  return refused_.contains(file);
}

std::vector<FullDump::Undefined> FullDump::undefined() const {
  std::vector<Undefined> out;
  for (const auto& [name, recorded] : undefined_) {
    if (!recorded.empty() && recorded.front() == '0') {
      out.push_back(Undefined{name, recorded.substr(1)});
    }
  }
  return out;
}

std::string FullDump::choose_prefix(VendorIo& io, const db::CellDef& def, const std::string& file) {
  if (no_prefix_view(def)) {
    return {};
  }
  if (io.prepared.compat.magic_compat) {
    util::LibcRandom& random = io.prepared.editor->mask.random;
    for (std::size_t tries = 0; tries < kRandomTries && prefixes_.size() < kPrefixCount; ++tries) {
      const auto first = static_cast<std::size_t>(random.next() % 26);
      const auto second = static_cast<std::size_t>(random.next() % 36);
      std::string prefix = prefix_at(first * 36 + second);
      if (prefixes_.insert(prefix).second) {
        return prefix;
      }
    }
  }
  const std::size_t base = fnv1a(file) % kPrefixCount;
  for (std::size_t k = 0; k < kPrefixCount; ++k) {
    std::string prefix = prefix_at((base + k) % kPrefixCount);
    if (prefixes_.insert(prefix).second) {
      return prefix;
    }
  }
  io.report(codes::kDumpPrefix, "Calma output error:  every library prefix is taken; the structures of " + file +
                                    " are written without one.");
  return {};
}

std::optional<std::string> FullDump::undefined_prefix(VendorIo& io, const std::string& name) {
  const auto entry = std::find_if(undefined_.begin(), undefined_.end(), [&](const auto& u) { return u.first == name; });
  const std::string recorded = entry->second;
  for (const auto& [file, prefix] : libraries_) {
    if (recorded.compare(1, prefix.size(), prefix) == 0) {
      undefined_.erase(entry);
      return prefix;
    }
  }
  io.report(codes::kDumpError, "Error:  Unreferenced cell " + recorded + " prefix is unrecorded!");
  return std::nullopt;
}

void FullDump::record_undefined(const std::map<std::string, std::string>& names) {
  for (const auto& entry_names : names) {
    const std::string& name = entry_names.first;  // a named variable: clang 14 cannot capture a binding
    const std::string& recorded = entry_names.second;
    if (recorded.empty() || recorded.front() != '0') {
      continue;
    }
    const auto entry =
        std::find_if(undefined_.begin(), undefined_.end(), [&](const auto& u) { return u.first == name; });
    if (entry != undefined_.end()) {
      entry->second = recorded;
    } else {
      undefined_.emplace_back(name, recorded);
    }
  }
}

namespace {

// The BGNSTR of a dumped structure (Q-MASK-215).
std::array<std::int16_t, 12> dump_dates(const FullDump::Dump& d, const Record& source_bgnstr) {
  const VendorIo& io = d.io;
  const GdsSettings& settings = io.prepared.options.settings;
  std::array<std::int16_t, 12> dates = structure_dates(settings.datestamp.value_or(d.def.timestamp()), io.now,
                                                       io.prepared.compat, io.prepared.options.time_zone);
  const std::vector<std::int16_t> source = i2_values(source_bgnstr.data);
  if (!io.prepared.compat.magic_compat && !settings.datestamp && source.size() >= 6) {
    std::copy(source.begin(), source.begin() + 6, dates.begin());
  }
  return dates;
}

// The written name of structure `name` (calmaDumpStructure's STRNAME).
std::string structure_name(FullDump::Dump& d, const std::string& name, const std::optional<std::string>& prefix) {
  const auto known = d.names.find(name);
  if (known != d.names.end()) {
    if (known->second.front() != '0') {
      d.io.report(codes::kDumpError, "Structure " + name + " defined redundantly in GDS");
    } else {
      known->second.front() = '1';
    }
    return known->second.substr(1);
  }
  std::string written = name;
  if (name != d.def.name()) {
    const db::CellDef* existing = d.io.prepared.editor->cells.find(name);
    const std::string* library = existing != nullptr ? string_property(*existing, "GDS_FILE") : nullptr;
    const bool same_library = existing != nullptr && library != nullptr &&
                              existing->properties().find("LEFview") != nullptr &&
                              gds_file_value(d.file, d.io.prepared.editor->path_variables) == *library;
    if (!same_library && prefix) {
      written = *prefix + name;
    }
  }
  d.names[name] = "1" + written;
  return written;
}

// An SNAME of a dumped structure.
diag::Status reference_name(FullDump::Dump& d, const Record& r, const std::optional<std::string>& prefix) {
  const std::string name = string_value(r.data);
  const auto known = d.names.find(name);
  if (known != d.names.end()) {
    return d.out.string_record(RecordType::sname, known->second.substr(1));
  }
  d.io.report(codes::kDumpForward, "Diagnostic:  " + name + " is a forward reference?");
  std::string written = prefix.value_or("") + name;
  if (d.io.prepared.editor->cells.find(written) != nullptr) {
    written += "[[0]]";
  }
  d.names[name] = "0" + written;
  return d.out.string_record(RecordType::sname, written);
}

}  // namespace

bool FullDump::dump(VendorIo& io, const db::CellDef& def, ByteSource& source, const std::string& file) {
  Dump d(io, def, source, file);
  if (!d.expect(RecordType::header) || !d.expect(RecordType::bgnlib)) {
    return true;
  }
  d.skip_set(kSkipBeforeLib);
  const std::optional<Record> libname = d.expect(RecordType::libname);
  if (!libname) {
    return true;
  }
  d.libname = string_value(libname->data);
  d.skip_set(kHeaderSkip);
  const std::optional<Record> units = d.expect(RecordType::units);
  const std::optional<double> mpd = units ? r8_at(units->data, 1) : std::nullopt;
  if (!mpd) {
    return true;
  }
  const bool angstroms = io.prepared.style->has_option(mask::style_option::angstroms);
  if (const std::optional<diag::Diagnostic> bad = readonly_units_error(*mpd, angstroms)) {
    io.report(codes::kDumpUnits, bad->text + "\nError:  Library " + d.libname + " has incompatible database units!");
    refused_.insert(file);
    return false;
  }
  libraries_.emplace_back(file, choose_prefix(io, def, file));
  while (dump_structure(d)) {
  }
  record_undefined(d.names);
  return true;
}

// calmaDumpStructure: false at the end of the library (or on an error).
bool FullDump::dump_structure(Dump& d) {
  diag::Outcome<std::optional<Record>> peeked = d.reader.peek();
  if (!peeked.ok() || !peeked.value() || peeked.value()->type != static_cast<std::uint8_t>(RecordType::bgnstr)) {
    return false;  // Q-MASK-213: nothing is written for a library without a structure
  }
  const std::optional<Record> bgnstr = d.expect(RecordType::bgnstr);
  const std::optional<Record> strname = d.expect(RecordType::strname);
  if (!strname) {
    d.skip_to_endstr();
    return false;
  }
  const std::string name = string_value(strname->data);
  d.io.report(codes::kDumpProgress, "Reading \"" + name + "\".");
  if (!d.fail(d.io.records.write_int2(RecordType::bgnstr, dump_dates(d, *bgnstr)))) {
    return false;
  }
  const bool undefined_before =
      std::any_of(undefined_.begin(), undefined_.end(), [&](const auto& u) { return u.first == name; });
  const std::optional<std::string> prefix =
      undefined_before ? undefined_prefix(d.io, name) : std::optional<std::string>{libraries_.back().second};
  if (!d.fail(d.out.string_record(RecordType::strname, structure_name(d, name, prefix)))) {
    return false;
  }
  return copy_elements(d, prefix);
}

// The records after STRNAME up to the next BGNSTR or ENDLIB.
bool FullDump::copy_elements(Dump& d, const std::optional<std::string>& prefix) {
  for (;;) {
    diag::Outcome<std::optional<Record>> next = d.reader.next();
    if (!next.ok()) {
      d.error("Unexpected EOF.");
      return false;
    }
    if (!next.value()) {
      return false;
    }
    Record r = std::move(*next.value());
    const auto type = static_cast<RecordType>(r.type);
    if (type == RecordType::bgnstr || type == RecordType::endlib) {
      d.reader.unread(std::move(r));
      return type == RecordType::bgnstr;
    }
    const diag::Status s = type == RecordType::sname ? reference_name(d, r, prefix)
                                                     : d.io.records.write(type, static_cast<DataType>(r.data_type), r.data);
    if (!d.fail(s)) {
      return false;
    }
  }
}

}  // namespace hefesto::gds::detail
