// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Verbatim copy of read-only cells (gds-write "Verbatim copy of read-only
// cells", Q-MASK-216; see vendor.hpp). Ports Magic 8.3.694 (fc5daa9e)
// calma/CalmaWrite.c CalmaOpenVendorGDS and the copy branch of
// calmaProcessDef, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Differences from Magic, for malformed or huge input only:
// - the copied bytes are streamed in 64 KiB chunks (Magic reads the whole
//   structure into one buffer); a first pass checks that [start, end) is
//   readable and ends with ENDSTR, so nothing is written before every
//   check has passed, as in Magic;
// - a negative GDS_START is a bad reference (Magic allocates a negative
//   size);
// - Magic's two "Size of data requested" / "Length of data read" lines have
//   no line feed; they are one message here.
// Owner: GDSW.

#include <algorithm>
#include <array>
#include <span>

#include "codes.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/read/vendor.hpp"
#include "hefesto/gds/stream/values.hpp"
#include "vendor.hpp"

namespace hefesto::gds::detail {

namespace {

constexpr std::size_t kChunk = 64 * 1024;
constexpr std::int64_t kRecordMax = 65535;  // CALMARECORDMAX
constexpr std::string_view kFallback = "Calma output error:  Can't write cell from vendor GDS.  Using magic's internal definition";

std::uint8_t byte_at(std::span<const std::byte> bytes, std::size_t i) {
  return std::to_integer<std::uint8_t>(bytes[i]);
}

// Reads up to `out.size()` bytes; the count read (short only at the end).
diag::Outcome<std::size_t> read_up_to(ByteSource& source, std::span<std::byte> out) {
  std::size_t total = 0;
  while (total < out.size()) {
    diag::Outcome<std::size_t> got = source.read(out.subspan(total));
    if (!got.ok()) {
      return got.error();
    }
    if (got.value() == 0) {
      break;
    }
    total += got.value();
  }
  return total;
}

// The path `name` opens as on `path` and `library`, only when the file
// found has that name (PaZOpen adds no compressed counterpart).
std::optional<platform::Path> exact(cell::LookupContext& ctx, const std::string& name,
                                    const std::vector<std::string>& path, const std::vector<std::string>* library) {
  std::optional<cell::FoundFile> found = cell::search_file(ctx, name, path, library);
  if (found && found->path.filename() == platform::Path{name}.filename()) {
    return found->path;
  }
  return std::nullopt;
}

std::optional<platform::Path> find_vendor_path(VendorIo& io, const db::CellDef& def, const std::string& name) {
  const editor::EditorState& editor = *io.prepared.editor;
  diag::NullSink quiet;
  cell::LookupContext ctx{io.files(), editor.search_paths, editor.path_variables, quiet};
  if (auto p = exact(ctx, name, editor.search_paths.path, &editor.search_paths.library)) {
    return p;
  }
  if (auto p = exact(ctx, name + ".gz", editor.search_paths.path, &editor.search_paths.library)) {
    return p;
  }
  const std::string mag = def.file_path().generic_string();
  const std::size_t slash = mag.rfind('/');
  if (name.empty() || name.front() == '/' || name.front() == '~' || name.front() == '$' ||
      slash == std::string::npos) {
    return std::nullopt;
  }
  const std::string relative = mag.substr(0, slash) + "/" + name;
  const std::vector<std::string> dot{"."};
  if (auto p = exact(ctx, relative, dot, nullptr)) {
    return p;
  }
  return exact(ctx, relative + ".gz", dot, nullptr);
}

// [start, end) of the copy and whether it is old style (GDS_BEGIN).
struct CopySpan {
  std::int64_t start = 0;
  std::int64_t end = 0;
  bool old_style = false;

  [[nodiscard]] std::uint64_t size() const noexcept { return static_cast<std::uint64_t>(end - start); }
};

CopySpan copy_span(const db::CellDef& def) {
  CopySpan span;
  span.end = property_integer(def, "GDS_END").value_or(0);
  const std::optional<std::int64_t> begin = property_integer(def, "GDS_BEGIN");
  span.old_style = begin.has_value();
  span.start = begin ? *begin : property_integer(def, "GDS_START").value_or(0);
  return span;
}

void fallback(const VendorIo& io, std::string first) {
  io.report(codes::kVendorCheck, std::move(first) + "\n" + std::string{kFallback});
}

// The STRNAME header search of a new-style copy: the shortest even record
// length r >= 6 whose header `r>>8, r&0xff, 06, 06` ends at `start`.
bool check_structure_name(const VendorIo& io, const db::CellDef& def, ByteSource& source, std::int64_t start) {
  const std::int64_t block_start = std::max<std::int64_t>(0, start - kRecordMax);
  const auto block_size = static_cast<std::size_t>(start - block_start);
  std::vector<std::byte> block(block_size);
  diag::Outcome<std::size_t> got = source.seek(static_cast<std::uint64_t>(block_start)).ok()
                                       ? read_up_to(source, block)
                                       : diag::Outcome<std::size_t>{std::size_t{0}};
  if (!got.ok() || got.value() != block_size) {
    io.report(codes::kVendorCheck, "Calma output error:  Can't read cell from vendor GDS.  Using magic's internal definition");
    return false;
  }
  for (std::size_t length = 6; length <= block_size; length += 2) {
    const std::size_t at = block_size - length;
    if (byte_at(block, at) == ((length >> 8U) & 0xffU) && byte_at(block, at + 1) == (length & 0xffU) &&
        byte_at(block, at + 2) == 0x06 && byte_at(block, at + 3) == 0x06) {
      const std::string name = string_value(std::span<const std::byte>{block}.subspan(at + 4));
      if (name != def.name()) {
        io.report(codes::kVendorName, "Calma output warning:  Structure definition has name " + name +
                                          " but cell definition has name " + def.name() +
                                          ".\nThe structure definition will be given the cell name.");
      }
      return true;
    }
  }
  fallback(io, "Calma output error:  Structure name not found ending at GDS file position " + std::to_string(start));
  return false;
}

// The BGNSTR header an old-style GDS_BEGIN points at.
bool check_old_style(const VendorIo& io, ByteSource& source, std::int64_t start) {
  std::array<std::byte, 4> header{};
  diag::Outcome<std::size_t> got = source.seek(static_cast<std::uint64_t>(start)).ok()
                                       ? read_up_to(source, header)
                                       : diag::Outcome<std::size_t>{std::size_t{0}};
  if (!got.ok() || got.value() != 4 || byte_at(header, 2) != 0x05 || byte_at(header, 3) != 0x02) {
    fallback(io, "Calma output error:  Structure definition not found at GDS file position " + std::to_string(start));
    return false;
  }
  return true;
}

// The readable length of [start, end) and its last four bytes.
struct Measured {
  std::uint64_t length = 0;
  std::array<std::uint8_t, 4> tail{};
};

diag::Outcome<Measured> measure(ByteSource& source, const CopySpan& span) {
  if (diag::Status s = source.seek(static_cast<std::uint64_t>(span.start)); !s.ok()) {
    return Measured{};
  }
  Measured m;
  std::vector<std::byte> chunk(kChunk);
  while (m.length < span.size()) {
    const auto want = static_cast<std::size_t>(std::min<std::uint64_t>(kChunk, span.size() - m.length));
    diag::Outcome<std::size_t> got = read_up_to(source, std::span<std::byte>{chunk}.first(want));
    if (!got.ok()) {
      return got.error();
    }
    for (std::size_t i = 0; i < got.value(); ++i) {
      m.tail = {m.tail[1], m.tail[2], m.tail[3], byte_at(chunk, i)};
    }
    m.length += got.value();
    if (got.value() < want) {
      break;
    }
  }
  return m;
}

bool check_content(const VendorIo& io, ByteSource& source, const CopySpan& span) {
  diag::Outcome<Measured> measured = measure(source, span);
  const Measured m = measured.ok() ? measured.value() : Measured{};
  if (m.length != span.size()) {
    io.report(codes::kVendorCheck, "Calma output error:  Can't read cell from vendor GDS.  Using magic's internal "
                                   "definition\nSize of data requested: " +
                                       std::to_string(span.size()) + "Length of data read: " + std::to_string(m.length));
    return false;
  }
  if (m.tail != std::array<std::uint8_t, 4>{0x00, 0x04, 0x07, 0x00}) {
    fallback(io, "Calma output error:  Structure end definition not found at GDS file position " +
                     std::to_string(span.end));
    return false;
  }
  return true;
}

bool sane(const VendorIo& io, const db::CellDef& def, ByteSource& source, const CopySpan& span) {
  if (span.start < 0 || span.end < span.start || span.end - span.start < 4) {
    io.report(codes::kVendorCheck, "Calma output error:  Bad vendor GDS file reference!");
    return false;
  }
  const bool header_ok =
      span.old_style ? check_old_style(io, source, span.start) : check_structure_name(io, def, source, span.start);
  return header_ok && check_content(io, source, span);
}

// The children whose structures the copied SNAMEs may name: each use's
// cell and, for a magnified copy, its GDS_MAG_SOURCE cell.
std::vector<const db::CellDef*> referenced_children(const VendorIo& io, const db::CellDef& def) {
  std::vector<const db::CellDef*> out;
  for (const db::CellUse* use : db::uses_in_search_order(def, io.prepared.compat)) {
    if (use->def == nullptr) {
      continue;
    }
    out.push_back(use->def);
    const db::PropertyValue* source = use->def->properties().find("GDS_MAG_SOURCE");
    const auto* name = source != nullptr ? std::get_if<std::string>(source) : nullptr;
    if (name != nullptr) {
      if (const db::CellDef* original = io.prepared.editor->cells.find(*name)) {
        out.push_back(original);
      }
    }
  }
  return out;
}

// The structure name `child` is written as (the payload text).
std::string written_name(VendorIo& io, ElementWriter& out, const db::CellDef& child) {
  const std::string chosen = io.names.name_of(out, child, io.id_of(child));
  return string_value(
      encode_string(chosen, out.table(), string_options(out.style(), out.settings()), out.compat()).data);
}

// The child written as `sname`, else the child named `sname`.
const db::CellDef* child_named(VendorIo& io, ElementWriter& out, const std::vector<const db::CellDef*>& children,
                               const std::string& sname, bool by_cell_name) {
  for (const db::CellDef* child : children) {
    if (written_name(io, out, *child) == sname) {
      return child;
    }
  }
  if (by_cell_name) {
    for (const db::CellDef* child : children) {
      if (child->name() == sname) {
        return child;
      }
    }
  }
  return nullptr;
}

// The records of [start, end), or nullopt when they do not frame it.
std::optional<std::vector<Record>> region_records(ByteSource& source, const CopySpan& span) {
  if (!source.seek(static_cast<std::uint64_t>(span.start)).ok()) {
    return std::nullopt;
  }
  RecordReader reader(source);
  std::vector<Record> records;
  std::uint64_t at = static_cast<std::uint64_t>(span.start);
  while (at < static_cast<std::uint64_t>(span.end)) {
    diag::Outcome<std::optional<Record>> next = reader.next();
    if (!next.ok() || !next.value()) {
      return std::nullopt;
    }
    at += 4 + next.value()->data.size();
    records.push_back(std::move(*next.value()));
  }
  if (at != static_cast<std::uint64_t>(span.end)) {
    return std::nullopt;
  }
  return records;
}

diag::Status copy_raw(VendorIo& io, ByteSource& source, const CopySpan& span) {
  if (diag::Status s = io.records.flush(); !s.ok()) {
    return s;
  }
  if (diag::Status s = source.seek(static_cast<std::uint64_t>(span.start)); !s.ok()) {
    return s;
  }
  std::vector<std::byte> chunk(kChunk);
  std::uint64_t left = span.size();
  while (left > 0) {
    const auto want = static_cast<std::size_t>(std::min<std::uint64_t>(kChunk, left));
    diag::Outcome<std::size_t> got = read_up_to(source, std::span<std::byte>{chunk}.first(want));
    if (!got.ok() || got.value() != want) {
      return diag::make(codes::kVendorWrite, "short read");
    }
    if (diag::Status s = io.raw.write(std::span<const std::byte>{chunk}.first(want)); !s.ok()) {
      return s;
    }
    left -= want;
  }
  return diag::Done{};
}

// Q-MASK-216: with magic-compat ON the copy is verbatim, with a warning
// for each SNAME that names none of the children's structures; with it OFF
// each SNAME naming a child is rewritten to the child's output name.
diag::Status copy_body(VendorIo& io, const db::CellDef& def, ByteSource& source, const CopySpan& span,
                       ElementWriter& out) {
  const std::optional<std::vector<Record>> records = region_records(source, span);
  const std::vector<const db::CellDef*> children = referenced_children(io, def);
  const bool rewrite = !io.prepared.compat.magic_compat && records.has_value();
  for (const Record& r : records ? *records : std::vector<Record>{}) {
    if (r.type != static_cast<std::uint8_t>(RecordType::sname)) {
      continue;
    }
    const std::string sname = string_value(r.data);
    if (child_named(io, out, children, sname, rewrite) == nullptr) {
      io.report_native(codes::kVendorSname, "Warning: the copied structure of cell " + def.name() + " references \"" +
                                         sname + "\", which is not the structure name of any of its children.");
    }
  }
  if (!rewrite) {
    return copy_raw(io, source, span);
  }
  for (const Record& r : *records) {
    const auto type = static_cast<RecordType>(r.type);
    const db::CellDef* child =
        type == RecordType::sname ? child_named(io, out, children, string_value(r.data), true) : nullptr;
    diag::Status s = child != nullptr
                         ? out.string_record(type, io.names.name_of(out, *child, io.id_of(*child)))
                         : io.records.write(type, static_cast<DataType>(r.data_type), r.data);
    if (!s.ok()) {
      return s;
    }
  }
  return diag::Done{};
}

}  // namespace

platform::FileSystem& VendorIo::files() const {
  static platform::LocalFileSystem local;
  return prepared.options.files != nullptr ? *prepared.options.files : local;
}

StructureContext VendorIo::context(const db::CellDef& def) const {
  const tech::Technology* technology = prepared.editor != nullptr ? prepared.editor->technology.get() : nullptr;
  return StructureContext{&def, prepared.style.get(), &prepared.options.settings, prepared.compat, {}, technology};
}

void VendorIo::report(std::string_view code, std::string text) const {
  sink.emit(diag::make(code, std::move(text)));
}

void VendorIo::report_native(std::string_view code, std::string text) const {
  sink.emit(diag::make_native(code, std::move(text)));
}

std::optional<VendorFile> open_vendor_gds(VendorIo& io, const db::CellDef& def, const std::string& name) {
  const std::optional<platform::Path> path = find_vendor_path(io, def, name);
  if (!path) {
    return std::nullopt;
  }
  diag::Outcome<std::unique_ptr<ByteSource>> source = open_source(io.files(), *path);
  if (!source.ok()) {
    return std::nullopt;
  }
  return VendorFile{*path, std::move(source.value())};
}

CopyResult copy_vendor_structure(VendorIo& io, const db::CellDef& def, ByteSource& source) {
  const CopySpan span = copy_span(def);
  if (!sane(io, def, source, span)) {
    return CopyResult::fallback;
  }
  ElementWriter out(io.records, io.context(def), &io.sink);
  if (!span.old_style) {
    const GdsSettings& settings = io.prepared.options.settings;
    const std::array<std::int16_t, 12> dates = structure_dates(settings.datestamp.value_or(def.timestamp()), io.now,
                                                               io.prepared.compat, io.prepared.options.time_zone);
    if (diag::Status s = io.records.write_int2(RecordType::bgnstr, dates); !s.ok()) {
      io.error = s;
      return CopyResult::failed;
    }
    if (diag::Status s = io.names.write(out, RecordType::strname, def, io.id_of(def)); !s.ok()) {
      io.error = s;
      return CopyResult::failed;
    }
  }
  if (diag::Status s = copy_body(io, def, source, span, out); !s.ok()) {
    io.report(codes::kVendorWrite,
              "Calma output error:  Failed to write cell \"" + def.name() + "\" from vendor GDS.  Output is invalid!");
    if (s.error().code != codes::kVendorWrite) {
      io.error = s;  // the output stream failed
    }
    return CopyResult::failed;
  }
  return CopyResult::copied;
}

}  // namespace hefesto::gds::detail
