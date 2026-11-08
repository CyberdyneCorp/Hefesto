// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The "Magic string hash table" of capability hefesto-output-determinism
// (requirement "Magic string hash table", compatibility: exact; Magic ref
// utils/hash.c). Iteration order is a function of the requested size, the key
// bytes and the insertion/removal history only, identical on every platform.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::util {

/// Magic's string hash: unsigned 64-bit accumulator, each byte taken as a
/// signed 8-bit value sign-extended to 64 bits (on every host), then
/// h = c + (h<<6) + (h<<16) - h  (mod 2^64).
[[nodiscard]] constexpr std::uint64_t magic_string_hash(std::string_view key) noexcept {
  std::uint64_t h = 0;
  for (const char ch : key) {
    const auto byte = static_cast<std::uint8_t>(ch);
    const auto signed_byte = static_cast<std::int8_t>(byte >= 0x80 ? byte - 0x100 : byte);
    const auto c = static_cast<std::uint64_t>(static_cast<std::int64_t>(signed_byte));
    h = c + (h << 6U) + (h << 16U) - h;
  }
  return h;
}

/// A client-supplied signed 32-bit hash, sign-extended to 64 bits.
[[nodiscard]] constexpr std::uint64_t magic_client_hash(std::int32_t client_hash) noexcept {
  return static_cast<std::uint64_t>(static_cast<std::int64_t>(client_hash));
}

/// Largest bucket count: shift = 30 - m reaches 0 at size 2^30, and a larger
/// table would need a negative shift (undefined in Magic's formula too).
inline constexpr std::size_t kMagicMaxTableSize = std::size_t{1} << 30U;

/// Smallest power of two >= max(2, requested), clamped to kMagicMaxTableSize.
[[nodiscard]] constexpr std::size_t magic_table_size(std::size_t requested) noexcept {
  std::size_t size = 2;
  while (size < requested && size < kMagicMaxTableSize) {
    size *= 2;
  }
  return size;
}

/// shift = 30 - m where size = 2^m (0 for sizes at or above 2^30).
[[nodiscard]] constexpr unsigned magic_table_shift(std::size_t size) noexcept {
  unsigned m = 0;
  while (m < 30U && (std::size_t{1} << m) < size) {
    ++m;
  }
  return 30U - m;
}

/// bucket = (((h * 1103515245 + 12345) mod 2^64) >> shift) & (size - 1).
[[nodiscard]] constexpr std::size_t magic_bucket(std::uint64_t h, unsigned shift, std::size_t size) noexcept {
  static_assert(sizeof(std::size_t) == sizeof(std::uint64_t), "Hefesto targets 64-bit platforms only");
  const std::uint64_t mixed = h * 1103515245ULL + 12345ULL;
  const std::uint64_t mask = size - 1;
  const std::size_t bucket = (mixed >> shift) & mask;
  return bucket;
}

/// Key traits for string keys hashed with magic_string_hash.
struct MagicStringKey {
  using Key = std::string;
  [[nodiscard]] static std::uint64_t hash(const Key& key) noexcept { return magic_string_hash(key); }
};

/// Key traits for a client-hashed key: `ClientHash::operator()(const Key&)`
/// returns the client's signed 32-bit hash (e.g. hierarchical names).
template <typename K, typename ClientHash>
struct MagicClientKey {
  using Key = K;
  [[nodiscard]] static std::uint64_t hash(const Key& key) noexcept { return magic_client_hash(ClientHash{}(key)); }
};

/// The chained hash table itself. `Traits` supplies `Key` and `hash(Key)`;
/// keys compare with operator==.
///
/// - The requested size is rounded up to a power of two and clamped to
///   kMagicMaxTableSize (2^30 buckets).
/// - Insertion puts a new key at the head of its bucket. Before inserting, if
///   the entry count is >= 3 * size, the table grows to 4 * size by visiting
///   old buckets in ascending order, each chain head to tail, and pushing each
///   entry onto the head of its new bucket; the entry count is then recounted.
///   A table at kMagicMaxTableSize stops growing (its chains get longer).
/// - remove() unlinks an entry; the entry count used by the growth test is not
///   decreased until the next growth recounts it.
/// - clear_value() is the citing-capability variant that keeps the entry and
///   its position and only clears the value.
/// - for_each() visits buckets 0 .. size-1, each chain head to tail.
///
/// Entry references and pointers are invalidated by the next insertion.
/// Not thread-safe; guard it like the rest of the database.
template <typename Value, typename Traits = MagicStringKey>
class MagicHashTable {
 public:
  using Key = typename Traits::Key;

  struct Entry {
    Key key;
    std::optional<Value> value;
  };

  explicit MagicHashTable(std::size_t requested_size = 32)
      : buckets_(magic_table_size(requested_size), kNil), shift_(magic_table_shift(buckets_.size())) {}

  /// Returns the entry for `key`, inserting it (with no value) when absent.
  Entry& find_or_insert(const Key& key) {
    const std::uint64_t h = Traits::hash(key);
    if (Entry* found = find_hashed(key, h)) {
      return *found;
    }
    if (entry_count_ >= 3 * buckets_.size() && buckets_.size() * 4 <= kMagicMaxTableSize) {
      grow();
    }
    const std::size_t index = allocate(Entry{key, std::nullopt});
    push_head(index, bucket_of(h));
    ++entry_count_;
    return nodes_[index].entry;
  }

  /// Inserts or overwrites the value for `key`.
  Entry& insert(const Key& key, Value value) {
    Entry& entry = find_or_insert(key);
    entry.value = std::move(value);
    return entry;
  }

  [[nodiscard]] Entry* find(const Key& key) { return find_hashed(key, Traits::hash(key)); }
  [[nodiscard]] const Entry* find(const Key& key) const {
    return const_cast<MagicHashTable*>(this)->find_hashed(key, Traits::hash(key));
  }

  /// Unlinks `key`; returns false when it is absent.
  bool remove(const Key& key) {
    std::size_t* link = &buckets_[bucket_of(Traits::hash(key))];
    while (*link != kNil && !(nodes_[*link].entry.key == key)) {
      link = &nodes_[*link].next;
    }
    if (*link == kNil) {
      return false;
    }
    const std::size_t index = *link;
    *link = nodes_[index].next;
    release(index);
    return true;
  }

  /// Clears the value of `key` but keeps the entry and its position.
  bool clear_value(const Key& key) {
    Entry* entry = find(key);
    if (entry == nullptr) {
      return false;
    }
    entry->value.reset();
    return true;
  }

  /// Visits every entry in Magic's iteration order.
  template <typename F>
  void for_each(F&& visit) const {
    for (const std::size_t head : buckets_) {
      for (std::size_t i = head; i != kNil; i = nodes_[i].next) {
        visit(nodes_[i].entry);
      }
    }
  }

  /// Keys in iteration order.
  [[nodiscard]] std::vector<Key> keys() const {
    std::vector<Key> out;
    for_each([&](const Entry& e) { out.push_back(e.key); });
    return out;
  }

  /// Bucket that `key` falls in at the current size.
  [[nodiscard]] std::size_t bucket_of_key(const Key& key) const noexcept { return bucket_of(Traits::hash(key)); }

  [[nodiscard]] std::size_t bucket_count() const noexcept { return buckets_.size(); }
  [[nodiscard]] unsigned shift() const noexcept { return shift_; }
  /// Live entries.
  [[nodiscard]] std::size_t size() const noexcept { return nodes_.size() - free_.size(); }
  /// The count the growth test uses (not decreased by remove()).
  [[nodiscard]] std::size_t growth_count() const noexcept { return entry_count_; }

 private:
  static constexpr std::size_t kNil = std::numeric_limits<std::size_t>::max();

  struct Node {
    Entry entry;
    std::size_t next = kNil;
  };

  [[nodiscard]] std::size_t bucket_of(std::uint64_t h) const noexcept { return magic_bucket(h, shift_, buckets_.size()); }

  Entry* find_hashed(const Key& key, std::uint64_t h) {
    for (std::size_t i = buckets_[bucket_of(h)]; i != kNil; i = nodes_[i].next) {
      if (nodes_[i].entry.key == key) {
        return &nodes_[i].entry;
      }
    }
    return nullptr;
  }

  void push_head(std::size_t index, std::size_t bucket) {
    nodes_[index].next = buckets_[bucket];
    buckets_[bucket] = index;
  }

  void grow() {
    std::vector<std::size_t> old = std::exchange(buckets_, std::vector<std::size_t>(buckets_.size() * 4, kNil));
    shift_ -= 2;
    entry_count_ = 0;
    for (const std::size_t head : old) {
      std::size_t i = head;
      while (i != kNil) {
        const std::size_t next = nodes_[i].next;
        push_head(i, bucket_of(Traits::hash(nodes_[i].entry.key)));
        ++entry_count_;
        i = next;
      }
    }
  }

  std::size_t allocate(Entry entry) {
    if (!free_.empty()) {
      const std::size_t index = free_.back();
      free_.pop_back();
      nodes_[index] = Node{std::move(entry), kNil};
      return index;
    }
    nodes_.push_back(Node{std::move(entry), kNil});
    return nodes_.size() - 1;
  }

  void release(std::size_t index) {
    nodes_[index].entry.value.reset();
    nodes_[index].next = kNil;
    free_.push_back(index);
  }

  std::vector<std::size_t> buckets_;  // head node index per bucket
  unsigned shift_;
  std::vector<Node> nodes_;           // node pool; links are indices
  std::vector<std::size_t> free_;     // released node slots
  std::size_t entry_count_ = 0;
};

}  // namespace hefesto::util
