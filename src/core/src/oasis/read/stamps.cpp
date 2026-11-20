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
// The pass streams: the file is not held in memory, nor the decompressed
// stream. Raw bytes are kept only from the last record reached to what the
// decoder has read ahead, one CBLOCK's data at a time, and each cell's hash
// runs as its bytes go by (a hostile file of many small CBLOCKs, each
// inflating 256 times, used to be held whole).
//
// The stamp procedure is gds-read's calmaStructStamp (memoized, cycles
// cut, each child stamp folded as 4 big-endian bytes, the final mix and
// mask), shared through gds::stamp_entries (gds/read/stamps.hpp, design
// D17), and the hash is gds::stamp_hash.
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/oasis/read/stamps.hpp"

#include <algorithm>
#include <functional>

#include "hefesto/gds/read/stamps.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/cblock.hpp"
#include "hefesto/oasis/codec/primitives.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

namespace {

using read_detail::NameRef;

constexpr std::uint64_t kFnvOffset = gds::kStampHashOffset;

std::uint64_t fnv(std::uint64_t h, std::span<const std::byte> bytes) {
  return gds::stamp_hash(h, bytes);
}

/// The source the decoder reads through, keeping the bytes it has read
/// from the last position the stamp pass still needs.
class KeepingSource final : public gds::ByteSource {
 public:
  explicit KeepingSource(gds::ByteSource& inner) : inner_(inner), base_(inner.position()) {}

  diag::Outcome<std::size_t> read(std::span<std::byte> out) override {
    auto got = inner_.read(out);
    if (got.ok()) {
      kept_.insert(kept_.end(), out.begin(), out.begin() + static_cast<std::ptrdiff_t>(got.value()));
    }
    return got;
  }
  [[nodiscard]] std::uint64_t position() const noexcept override { return inner_.position(); }
  diag::Status seek(std::uint64_t offset) override {
    kept_.clear();
    base_ = offset;
    return inner_.seek(offset);
  }

  /// The kept bytes of [from, to) (the part that is kept).
  [[nodiscard]] std::span<const std::byte> bytes(std::uint64_t from, std::uint64_t to) const {
    const std::uint64_t end = base_ + kept_.size();
    from = std::clamp(from, base_, end);
    to = std::clamp(to, from, end);
    return std::span<const std::byte>{kept_}.subspan(from - base_, to - from);
  }

  /// The bytes before `offset` are no longer needed.
  void drop_before(std::uint64_t offset) {
    const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(offset - std::min(offset, base_), kept_.size()));
    if (n >= kCompactAt && 2 * n >= kept_.size()) {
      kept_.erase(kept_.begin(), kept_.begin() + static_cast<std::ptrdiff_t>(n));
      base_ += n;
    }
  }

 private:
  static constexpr std::size_t kCompactAt = 1U << 16U;

  gds::ByteSource& inner_;
  std::vector<std::byte> kept_;
  std::uint64_t base_;  // the stream offset of kept_[0]
};

using ByteFeed = std::function<void(std::span<const std::byte>)>;

/// The decompressed record stream (each CBLOCK replaced by its inflated
/// data), produced in order: `start` feeds every byte before a record and
/// returns the record's position in it.
class LogicalStream {
 public:
  LogicalStream(KeepingSource& raw, ByteFeed feed) : raw_(raw), feed_(std::move(feed)) {}

  std::uint64_t start(const Record& r) {
    if (r.in_cblock) {
      feed_block(static_cast<std::size_t>(std::min<std::uint64_t>(r.cblock_position, block_.size())));
      return block_start_ + r.cblock_position;
    }
    feed_block(block_.size());
    block_.clear();
    block_fed_ = 0;
    feed(raw_.bytes(raw_pos_, r.offset));
    raw_pos_ = std::max(raw_pos_, r.offset);
    raw_.drop_before(raw_pos_);
    if (r.id == RecordId::cblock) {
      inflate(r);
    }
    return pos_;
  }

 private:
  void feed(std::span<const std::byte> bytes) {
    pos_ += bytes.size();
    feed_(bytes);
  }

  void feed_block(std::size_t to) {
    if (to > block_fed_) {
      feed(std::span<const std::byte>{block_}.subspan(block_fed_, to - block_fed_));
      block_fed_ = to;
    }
  }

  /// The CBLOCK at r.offset: its data becomes the current block (the
  /// decoder has checked its header and sizes, and has read it all).
  void inflate(const Record& r) {
    const std::span<const std::byte> raw = raw_.bytes(r.offset, raw_.position());
    ByteCursor c{raw, 0};
    (void)read_unsigned(c);  // record id 34
    (void)read_unsigned(c);  // compression type 0
    const auto uncompressed = read_unsigned(c);
    const auto compressed = read_unsigned(c);
    block_start_ = pos_;
    if (!uncompressed.ok() || !compressed.ok() || compressed.value() > raw.size() - c.offset) {
      raw_pos_ = raw_.position();
      return;
    }
    auto data = inflate_cblock(raw.subspan(c.offset, compressed.value()), uncompressed.value(), uncompressed.value());
    if (data.ok()) {
      block_ = std::move(data).value();
    }
    raw_pos_ = r.offset + c.offset + compressed.value();
  }

  KeepingSource& raw_;
  ByteFeed feed_;
  std::uint64_t raw_pos_ = 0;  // the raw bytes before it have been fed (or replaced by a block)
  std::uint64_t pos_ = 0;      // the bytes fed so far
  std::vector<std::byte> block_;
  std::uint64_t block_start_ = 0;
  std::size_t block_fed_ = 0;
};

bool ends_cell(RecordId id) {
  return id == RecordId::end || id == RecordId::cell_by_reference || id == RecordId::cell_by_name ||
         id == RecordId::xname_implicit || id == RecordId::xname ||
         (id >= RecordId::cellname_implicit && id <= RecordId::layername_text);
}

bool same(const NameRef& a, const NameRef& b) {
  return a.name == b.name && a.reference == b.reference;
}

/// A cell whose name is still a reference (CELLNAME records may come last).
struct CellRecord {
  NameRef name;
  std::uint64_t start = 0;
  std::uint64_t end = 0;
  std::uint64_t hash = kFnvOffset;
  std::vector<NameRef> children;
};

/// Follows the records and the bytes between them, hashing the open cell's
/// content as it goes.
class CellHasher {
 public:
  void bytes(std::span<const std::byte> b) {
    if (open_ && hashing_) {
      running_ = fnv(running_, b);
    }
  }

  void record(const Record& r, std::uint64_t at) {
    if (open_) {
      reach(at);
    }
    if (open_ && ends_cell(r.id)) {
      cells_.push_back(*std::move(open_));
      open_.reset();
    }
    if (r.id == RecordId::cell_by_reference || r.id == RecordId::cell_by_name) {
      open_ = CellRecord{NameRef{r.name, r.reference}, at, at, kFnvOffset, {}};
      hashing_ = false;
    } else if (open_ && r.element && r.element->kind == ElementKind::placement) {
      add_child(NameRef{r.element->name, r.element->reference});
    }
    after_content_ = r.id != RecordId::pad && r.id != RecordId::cblock && !ends_cell(r.id);
  }

  [[nodiscard]] std::vector<CellRecord>& cells() noexcept { return cells_; }

 private:
  /// A record starts at `at`: the cell's bytes start at the first record
  /// after CELL and end where the record after a content record starts.
  void reach(std::uint64_t at) {
    if (!hashing_) {
      hashing_ = true;
      running_ = kFnvOffset;
      open_->end = at;
    } else if (after_content_) {
      open_->hash = running_;
      open_->end = at;
    }
  }

  void add_child(NameRef child) {
    if (open_->children.empty() || !same(open_->children.back(), child)) {
      open_->children.push_back(std::move(child));
    }
  }

  std::optional<CellRecord> open_;
  bool hashing_ = false;
  bool after_content_ = false;
  std::uint64_t running_ = kFnvOffset;
  std::vector<CellRecord> cells_;
};

// --- the cells ---------------------------------------------------------------------

/// The stamp entry of `c`, its names resolved; consecutive children that
/// resolve to the same name are one.
gds::StampEntry entry_of(CellRecord& c, const NameTables& names) {
  gds::StampEntry e;
  e.name = read_detail::cell_name(names, c.name);
  e.start = c.start;
  e.end = c.end;
  e.hash = c.hash;
  for (const NameRef& ref : c.children) {
    std::string child = read_detail::cell_name(names, ref);
    if (e.children.empty() || e.children.back() != child) {
      e.children.push_back(std::move(child));
    }
  }
  c.children = {};
  return e;
}

}  // namespace

gds::StampTable content_stamps(gds::ByteSource& source) {
  if (!source.seek(0).ok()) {
    return {};
  }
  KeepingSource input{source};
  Decoder decoder(input);
  CellHasher cells;
  LogicalStream stream{input, [&cells](std::span<const std::byte> b) { cells.bytes(b); }};
  for (;;) {
    auto record = decoder.next();
    if (!record.ok() || !record.value()) {
      break;
    }
    const Record& r = *record.value();
    cells.record(r, stream.start(r));
  }
  std::vector<gds::StampEntry> entries;
  entries.reserve(cells.cells().size());
  for (CellRecord& c : cells.cells()) {
    entries.push_back(entry_of(c, decoder.names()));
  }
  return gds::stamp_entries(std::move(entries));
}

}  // namespace hefesto::oasis
