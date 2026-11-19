// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Content-hash timestamps of read-only OASIS cells (see
// hefesto/oasis/read/stamps.hpp; capability oasis-read "Content-hash
// timestamps for read-only OASIS cells").
//
// The cell bytes are taken from the decompressed record stream: the file
// with each CBLOCK record replaced by its inflated data, so a cell gets the
// same hash whether its records are compressed or not. They run from the
// first byte after the CELL record through the last byte of its last
// cell-content record (trailing PAD records and the name records, CELL or
// END that end the cell are not cell content). The children are the cells
// its PLACEMENTs name, consecutive duplicates removed (a PLACEMENT that
// reuses the modal cell names the previous one's, a duplicate).
//
// The stamp procedure is gds-read's calmaStructStamp (memoized, cycles
// cut, each child stamp folded as 4 big-endian bytes, the final mix and
// mask). It mirrors gds/read/stamps.cpp until INTEGRATION shares that one
// (design D17, an OASR open item); the two must stay identical.
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/oasis/read/stamps.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <unordered_map>

#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/cblock.hpp"
#include "hefesto/oasis/codec/primitives.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

namespace {

using read_detail::NameRef;

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::uint64_t fnv(std::uint64_t h, std::span<const std::byte> bytes) {
  for (const std::byte b : bytes) {
    h ^= std::to_integer<std::uint64_t>(b);
    h *= kFnvPrime;
  }
  return h;
}

/// A ByteSource over bytes held elsewhere.
class SpanSource final : public gds::ByteSource {
 public:
  explicit SpanSource(std::span<const std::byte> bytes) : bytes_(bytes) {}
  diag::Outcome<std::size_t> read(std::span<std::byte> out) override {
    const std::size_t n = std::min(out.size(), bytes_.size() - offset_);
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_), n, out.begin());
    offset_ += n;
    return n;
  }
  [[nodiscard]] std::uint64_t position() const noexcept override { return offset_; }
  diag::Status seek(std::uint64_t offset) override {
    offset_ = static_cast<std::size_t>(std::min<std::uint64_t>(offset, bytes_.size()));
    return diag::Done{};
  }

 private:
  std::span<const std::byte> bytes_;
  std::size_t offset_ = 0;
};

std::vector<std::byte> read_all(gds::ByteSource& source) {
  std::vector<std::byte> out;
  std::array<std::byte, 65536> buffer{};
  if (!source.seek(0).ok()) {
    return out;
  }
  for (;;) {
    const auto got = source.read(buffer);
    if (!got.ok() || got.value() == 0) {
      return out;
    }
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(got.value()));
  }
}

/// One decoded record: its id, its start in the decompressed stream and,
/// for a PLACEMENT, the cell it names.
struct Mark {
  RecordId id = RecordId::pad;
  std::uint64_t start = 0;
  std::optional<NameRef> child;
  std::optional<NameRef> cell;  // CELL records
};

/// The file's records and its decompressed stream.
class Stream {
 public:
  explicit Stream(std::span<const std::byte> raw) : raw_(raw) {}

  void take(const Record& r) {
    Mark m{r.id, start_of(r), std::nullopt, std::nullopt};
    if (r.id == RecordId::cblock) {
      inflate(r);
      m.start = cblocks_[r.offset];  // where its data starts
    } else if (r.id == RecordId::cell_by_reference || r.id == RecordId::cell_by_name) {
      m.cell = NameRef{r.name, r.reference};
    } else if (r.element && r.element->kind == ElementKind::placement) {
      m.child = NameRef{r.element->name, r.element->reference};
    }
    marks_.push_back(std::move(m));
  }

  /// Appends the bytes after the last CBLOCK.
  void finish() { logical_.insert(logical_.end(), raw_.begin() + static_cast<std::ptrdiff_t>(cursor_), raw_.end()); }

  [[nodiscard]] const std::vector<Mark>& marks() const noexcept { return marks_; }
  [[nodiscard]] std::span<const std::byte> bytes(std::uint64_t from, std::uint64_t to) const {
    to = std::min<std::uint64_t>(to, logical_.size());
    from = std::min(from, to);
    return std::span<const std::byte>{logical_}.subspan(from, to - from);
  }

 private:
  [[nodiscard]] std::uint64_t start_of(const Record& r) const {
    if (r.in_cblock) {
      const auto found = cblocks_.find(r.offset);
      return found != cblocks_.end() ? found->second + r.cblock_position : logical_.size();
    }
    return logical_.size() + (r.offset - std::min<std::uint64_t>(r.offset, cursor_));
  }

  /// The CBLOCK at r.offset replaced by its data (the decoder has checked
  /// its header and sizes).
  void inflate(const Record& r) {
    logical_.insert(logical_.end(), raw_.begin() + static_cast<std::ptrdiff_t>(cursor_),
                    raw_.begin() + static_cast<std::ptrdiff_t>(r.offset));
    ByteCursor c{raw_, static_cast<std::size_t>(r.offset)};
    (void)read_unsigned(c);  // record id 34
    (void)read_unsigned(c);  // compression type 0
    const auto uncompressed = read_unsigned(c);
    const auto compressed = read_unsigned(c);
    cblocks_[r.offset] = logical_.size();
    if (!uncompressed.ok() || !compressed.ok() || compressed.value() > raw_.size() - c.offset) {
      cursor_ = raw_.size();
      return;
    }
    auto data = inflate_cblock(raw_.subspan(c.offset, compressed.value()), uncompressed.value(), uncompressed.value());
    if (data.ok()) {
      logical_.insert(logical_.end(), data.value().begin(), data.value().end());
    }
    cursor_ = c.offset + compressed.value();
  }

  std::span<const std::byte> raw_;
  std::vector<std::byte> logical_;
  std::uint64_t cursor_ = 0;                      // raw bytes before it are in logical_
  std::map<std::uint64_t, std::uint64_t> cblocks_;  // CBLOCK file offset -> start of its data in logical_
  std::vector<Mark> marks_;
};

bool ends_cell(RecordId id) {
  return id == RecordId::end || id == RecordId::cell_by_reference || id == RecordId::cell_by_name ||
         id == RecordId::xname_implicit || id == RecordId::xname ||
         (id >= RecordId::cellname_implicit && id <= RecordId::layername_text);
}

// --- calmaStructStamp (gds/read/stamps.cpp) ------------------------------------

enum class State : std::uint8_t { not_started, in_progress, done };

struct Entry {
  std::string name;
  std::uint64_t start = 0;
  std::uint64_t end = 0;
  std::uint64_t hash = kFnvOffset;
  std::vector<std::string> children;
  std::uint32_t stamp = 0;
  State state = State::not_started;
};

struct Table {
  std::vector<Entry> entries;
  std::unordered_map<std::string, std::size_t> by_name;  // first definition wins
};

std::uint32_t fold(std::uint64_t h) {
  h ^= (h >> 31U) ^ (h >> 62U);
  const auto stamp = static_cast<std::uint32_t>(h & 0x7fffffffU);
  return stamp == 0 ? 1 : stamp;
}

void finish(Entry& e, std::uint64_t h) {
  e.stamp = fold(h);
  e.state = State::done;
}

struct Frame {
  std::size_t entry;
  std::size_t next_child = 0;
  std::uint64_t hash;
};

std::optional<std::uint32_t> enter(Table& t, std::size_t index, std::vector<Frame>& stack) {
  Entry& e = t.entries[index];
  if (e.state == State::done) {
    return e.stamp;
  }
  if (e.state == State::in_progress) {
    finish(e, e.hash);  // a reference loop is not followed
    return e.stamp;
  }
  e.state = State::in_progress;
  stack.push_back(Frame{index, 0, e.hash});
  return std::nullopt;
}

std::uint64_t mix_stamp(std::uint64_t h, std::uint32_t stamp) {
  const std::array<std::byte, 4> bytes{static_cast<std::byte>((stamp >> 24U) & 0xffU),
                                       static_cast<std::byte>((stamp >> 16U) & 0xffU),
                                       static_cast<std::byte>((stamp >> 8U) & 0xffU),
                                       static_cast<std::byte>(stamp & 0xffU)};
  return fnv(h, bytes);
}

std::optional<std::size_t> next_child(const Table& t, Frame& frame) {
  const Entry& e = t.entries[frame.entry];
  while (frame.next_child < e.children.size()) {
    const auto found = t.by_name.find(e.children[frame.next_child++]);
    if (found != t.by_name.end()) {
      return found->second;
    }
  }
  return std::nullopt;
}

void stamp(Table& t, std::size_t root) {
  std::vector<Frame> stack;
  if (enter(t, root, stack)) {
    return;
  }
  while (!stack.empty()) {
    const auto child = next_child(t, stack.back());
    if (!child) {
      const Frame done = stack.back();
      stack.pop_back();
      finish(t.entries[done.entry], done.hash);
      if (!stack.empty()) {
        stack.back().hash = mix_stamp(stack.back().hash, t.entries[done.entry].stamp);
      }
      continue;
    }
    if (const auto value = enter(t, *child, stack)) {
      stack.back().hash = mix_stamp(stack.back().hash, *value);
    }
  }
}

// --- the cells ---------------------------------------------------------------------

/// The entry of the cell whose CELL record is marks[i]; nullopt when the
/// stream ends inside it.
std::optional<Entry> cell_entry(const Stream& stream, const NameTables& names, std::size_t i) {
  const std::vector<Mark>& marks = stream.marks();
  Entry e;
  e.name = read_detail::cell_name(names, *marks[i].cell);
  e.start = marks[i].start;
  const std::uint64_t from = i + 1 < marks.size() ? marks[i + 1].start : marks[i].start;
  std::uint64_t to = from;
  for (std::size_t j = i + 1; j < marks.size(); ++j) {
    const Mark& m = marks[j];
    if (ends_cell(m.id)) {
      e.end = to;
      e.hash = fnv(kFnvOffset, stream.bytes(from, to));
      return e;
    }
    if (m.id != RecordId::pad && m.id != RecordId::cblock && j + 1 < marks.size()) {
      to = marks[j + 1].start;
    }
    if (m.child) {
      std::string child = read_detail::cell_name(names, *m.child);
      if (e.children.empty() || e.children.back() != child) {
        e.children.push_back(std::move(child));
      }
    }
  }
  return std::nullopt;
}

}  // namespace

gds::StampTable content_stamps(gds::ByteSource& source) {
  const std::vector<std::byte> raw = read_all(source);
  SpanSource input{raw};
  Decoder decoder(input);
  Stream stream{raw};
  for (;;) {
    auto record = decoder.next();
    if (!record.ok() || !record.value()) {
      break;
    }
    stream.take(*record.value());
  }
  stream.finish();
  Table table;
  for (std::size_t i = 0; i < stream.marks().size(); ++i) {
    if (!stream.marks()[i].cell) {
      continue;
    }
    if (auto entry = cell_entry(stream, decoder.names(), i)) {
      table.by_name.try_emplace(entry->name, table.entries.size());
      table.entries.push_back(std::move(*entry));
    }
  }
  gds::StampTable out;
  out.reserve(table.entries.size());
  for (std::size_t i = 0; i < table.entries.size(); ++i) {
    stamp(table, i);
    const Entry& e = table.entries[i];
    out.push_back(gds::StructureStamp{e.name, e.start, e.end, e.stamp});
  }
  return out;
}

}  // namespace hefesto::oasis
