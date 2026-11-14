// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Content-hash timestamps (see hefesto/gds/read/stamps.hpp). Ported from
// Magic 8.3.694 calma/CalmaRead.c (calmaFNVHash, calmaStampTableRead,
// calmaStructStamp); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// The scan reads one record at a time, from a byte span or a ByteSource
// (a gzip file is never inflated whole, as calmaStampTableRead streams it),
// and the table grows with the number of structures and SNAME records,
// each at least four bytes of input. calmaStructStamp's recursion is an
// explicit stack, so a deep or cyclic hierarchy cannot exhaust the call
// stack; the visiting order, and so every stamp, is the recursive one.
// Owner: GDSR.

#include "hefesto/gds/read/stamps.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "hefesto/gds/stream/io.hpp"

namespace hefesto::gds {

namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
constexpr std::size_t kHeader = 4;
constexpr std::uint8_t kEndlib = 0x04;
constexpr std::uint8_t kBgnstr = 0x05;
constexpr std::uint8_t kStrname = 0x06;
constexpr std::uint8_t kEndstr = 0x07;
constexpr std::uint8_t kSname = 0x12;

std::uint64_t fnv(std::uint64_t h, std::span<const std::byte> bytes) {
  for (const std::byte b : bytes) {
    h ^= std::to_integer<std::uint64_t>(b);
    h *= kFnvPrime;
  }
  return h;
}

/// The name of a payload as Magic's C string: up to the first NUL.
std::string c_name(std::span<const std::byte> data) {
  const auto* chars = reinterpret_cast<const char*>(data.data());
  const std::string_view all{chars, data.size()};
  return std::string{all.substr(0, all.find('\0'))};
}

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

/// One record of the scan; nullopt at the end (EOF, L < 4, short data).
/// The spans stay valid until the next record is read.
struct RawRecord {
  std::uint8_t type = 0;
  std::span<const std::byte> whole;  // header and data
  std::span<const std::byte> data;
};

std::size_t record_length(std::span<const std::byte> header) {
  return (std::to_integer<std::size_t>(header[0]) << 8U) | std::to_integer<std::size_t>(header[1]);
}

RawRecord raw_record(std::span<const std::byte> whole) {
  return RawRecord{std::to_integer<std::uint8_t>(whole[2]), whole, whole.subspan(kHeader)};
}

/// The records of a byte span, in place.
class SpanRecords {
 public:
  explicit SpanRecords(std::span<const std::byte> bytes) : bytes_(bytes) {}

  std::optional<RawRecord> next() {
    if (bytes_.size() - offset_ < kHeader) {
      return std::nullopt;
    }
    const std::size_t length = record_length(bytes_.subspan(offset_));
    if (length < kHeader || bytes_.size() - offset_ < length) {
      return std::nullopt;
    }
    const auto whole = bytes_.subspan(offset_, length);
    offset_ += length;
    return raw_record(whole);
  }

 private:
  std::span<const std::byte> bytes_;
  std::size_t offset_ = 0;
};

/// The records of a ByteSource, one at a time (at most 65535 bytes held).
/// A read error ends the scan as the end of the stream does.
class SourceRecords {
 public:
  explicit SourceRecords(ByteSource& source) : source_(source), buffer_(0xffff) {}

  std::optional<RawRecord> next() {
    const std::span<std::byte> all{buffer_};
    if (!fill(all.first(kHeader))) {
      return std::nullopt;
    }
    const std::size_t length = record_length(all);
    if (length < kHeader || !fill(all.subspan(kHeader, length - kHeader))) {
      return std::nullopt;
    }
    return raw_record(all.first(length));
  }

 private:
  bool fill(std::span<std::byte> out) {
    while (!out.empty()) {
      const auto got = source_.read(out);
      if (!got.ok() || got.value() == 0) {
        return false;
      }
      out = out.subspan(got.value());
    }
    return true;
  }

  ByteSource& source_;
  std::vector<std::byte> buffer_;
};

void add_record(Entry& entry, const RawRecord& r, std::uint64_t end) {
  entry.hash = fnv(entry.hash, r.whole);
  if (r.type == kSname) {
    std::string child = c_name(r.data);
    if (entry.children.empty() || entry.children.back() != child) {
      entry.children.push_back(std::move(child));
    }
  } else if (r.type == kEndstr) {
    entry.end = end;
  }
}

/// calmaStampTableRead over the records of `records`.
template <typename Records>
Table scan(Records& records) {
  Table table;
  bool need_name = false;
  std::optional<std::size_t> current;
  std::uint64_t offset = 0;
  while (const auto r = records.next()) {
    offset += r->whole.size();
    if (r->type == kEndlib) {
      break;
    }
    if (r->type == kBgnstr) {
      current.reset();
      need_name = true;
      continue;
    }
    if (need_name) {
      need_name = false;
      if (r->type == kStrname) {
        Entry entry;
        entry.name = c_name(r->data);
        entry.start = offset;
        table.by_name.try_emplace(entry.name, table.entries.size());
        current = table.entries.size();
        table.entries.push_back(std::move(entry));
      }
      continue;
    }
    if (current) {
      add_record(table.entries[*current], *r, offset);
      if (r->type == kEndstr) {
        current.reset();
      }
    }
  }
  return table;
}

std::uint32_t fold(std::uint64_t h) {
  h ^= (h >> 31U) ^ (h >> 62U);
  const auto stamp = static_cast<std::uint32_t>(h & 0x7fffffffU);
  return stamp == 0 ? 1 : stamp;
}

void finish(Entry& e, std::uint64_t h) {
  e.stamp = fold(h);
  e.state = State::done;
}

/// One active calmaStructStamp call.
struct Frame {
  std::size_t entry;
  std::size_t next_child = 0;
  std::uint64_t hash;
};

/// Starts calmaStructStamp(entry): pushes a frame, or answers at once.
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

/// The next child of the top frame that names a tabulated structure.
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

/// calmaStructStamp(root), iteratively.
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

/// Every entry of `table` stamped, in file order.
StampTable stamps_of(Table& table) {
  StampTable out;
  out.reserve(table.entries.size());
  for (std::size_t i = 0; i < table.entries.size(); ++i) {
    stamp(table, i);
    const Entry& e = table.entries[i];
    out.push_back(StructureStamp{e.name, e.start, e.end, e.stamp});
  }
  return out;
}

}  // namespace

StampTable content_stamps(std::span<const std::byte> bytes) {
  SpanRecords records{bytes};
  Table table = scan(records);
  return stamps_of(table);
}

StampTable content_stamps(ByteSource& source) {
  SourceRecords records{source};
  Table table = scan(records);
  return stamps_of(table);
}

std::uint32_t lookup_stamp(const StampTable& table, std::uint64_t start) {
  const auto found = std::lower_bound(table.begin(), table.end(), start,
                                      [](const StructureStamp& s, std::uint64_t v) { return s.start < v; });
  return found != table.end() && found->start == start ? found->stamp : 0;
}

}  // namespace hefesto::gds
