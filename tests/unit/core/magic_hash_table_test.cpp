// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <deque>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "hefesto/util/magic_hash_table.hpp"
#include "test_session.hpp"

using hefesto::util::magic_bucket;
using hefesto::util::kMagicMaxTableSize;
using hefesto::util::magic_client_hash;
using hefesto::util::magic_string_hash;
using hefesto::util::magic_table_shift;
using hefesto::util::magic_table_size;
using Table = hefesto::util::MagicHashTable<int>;

#define MAGIC_HASH_TAGS "[cap:hefesto-output-determinism][req:Magic string hash table]"

TEST_CASE("hash of a short key", MAGIC_HASH_TAGS) {
  CHECK(magic_string_hash("ab") == 6363201U);
  CHECK(magic_table_shift(32) == 25U);
  CHECK(magic_bucket(6363201U, 25, 32) == 1U);
  Table table(32);
  CHECK(table.bucket_of_key("ab") == 1U);
}

TEST_CASE("insertion history decides the order", MAGIC_HASH_TAGS) {
  CHECK(magic_string_hash("u1") == 7675132U);
  CHECK(magic_string_hash("u2") == 7675133U);
  CHECK(magic_string_hash("u3") == 7675134U);
  Table table(32);
  table.insert("u3", 3);
  table.insert("u1", 1);
  table.insert("u2", 2);
  CHECK(table.bucket_of_key("u1") == 3U);
  CHECK(table.bucket_of_key("u2") == 4U);
  CHECK(table.bucket_of_key("u3") == 5U);
  CHECK(table.keys() == std::vector<std::string>{"u1", "u2", "u3"});
}

TEST_CASE("same bucket, most recent first", MAGIC_HASH_TAGS) {
  Table table(32);
  table.insert("a", 1);
  table.insert("b", 2);
  REQUIRE(table.bucket_of_key("a") == 22U);
  REQUIRE(table.bucket_of_key("b") == 22U);
  CHECK(table.keys() == std::vector<std::string>{"b", "a"});
}

TEST_CASE("key bytes >= 0x80 are signed on every platform", MAGIC_HASH_TAGS) {
  // 0xE9 is -23 as a signed byte: h = 2^64 - 23, not 233.
  CHECK(magic_string_hash("\xE9") == UINT64_MAX - 22U);
  CHECK(magic_string_hash("\xE9") != 233U);
  CHECK(magic_bucket(magic_string_hash("\xE9"), 25, 32) == 11U);
  // "\xFF\x01": h = -1 then 1 + (-1)*65599 (mod 2^64).
  CHECK(magic_string_hash("\xFF\x01") == static_cast<std::uint64_t>(1 - 65599));
}

TEST_CASE("client hash is a sign-extended 32-bit value", MAGIC_HASH_TAGS) {
  CHECK(magic_client_hash(-1) == UINT64_MAX);
  CHECK(magic_client_hash(42) == 42U);

  struct ByValue {
    std::int32_t operator()(std::int32_t v) const noexcept { return v; }
  };
  hefesto::util::MagicHashTable<int, hefesto::util::MagicClientKey<std::int32_t, ByValue>> table(32);
  table.insert(98, 0);
  table.insert(97, 0);
  // 97 and 98 are the string hashes of "a" and "b": same bucket 22, newest first.
  CHECK(table.bucket_of_key(97) == 22U);
  CHECK(table.keys() == std::vector<std::int32_t>{97, 98});
}

TEST_CASE("table size is the smallest power of two >= max(2, requested)", MAGIC_HASH_TAGS) {
  CHECK(magic_table_size(0) == 2U);
  CHECK(magic_table_size(1) == 2U);
  CHECK(magic_table_size(2) == 2U);
  CHECK(magic_table_size(3) == 4U);
  CHECK(magic_table_size(32) == 32U);
  CHECK(magic_table_size(33) == 64U);
  CHECK(magic_table_shift(2) == 29U);
  CHECK(magic_table_shift(64) == 24U);
}

// Regression: sizes above 2^30 made the shift wrap around (an out-of-range shift, UB).
TEST_CASE("table size is clamped to 2^30 buckets, where the shift reaches 0", MAGIC_HASH_TAGS) {
  constexpr std::size_t kMax = std::size_t{1} << 30U;
  STATIC_REQUIRE(kMagicMaxTableSize == kMax);
  CHECK(magic_table_size(kMax) == kMax);
  CHECK(magic_table_size(kMax + 1) == kMax);
  CHECK(magic_table_size(std::size_t{1} << 31U) == kMax);
  CHECK(magic_table_size(std::numeric_limits<std::size_t>::max()) == kMax);
  CHECK(magic_table_shift(kMax) == 0U);
  CHECK(magic_table_shift(std::size_t{1} << 31U) == 0U);
  CHECK(magic_bucket(magic_string_hash("a"), magic_table_shift(kMax), kMax) < kMax);
}

TEST_CASE("growth happens before the insertion that finds 3 x size entries", MAGIC_HASH_TAGS) {
  Table table(2);
  for (int i = 0; i < 6; ++i) {
    table.insert("k" + std::to_string(i), i);
  }
  CHECK(table.bucket_count() == 2U);  // 6 = 3 x 2 entries, not grown yet
  CHECK(table.shift() == 29U);
  table.insert("k6", 6);
  CHECK(table.bucket_count() == 8U);  // grew to 4 x size
  CHECK(table.shift() == 27U);
  CHECK(table.size() == 7U);
  CHECK(table.growth_count() == 7U);
  // Looking up an existing key never grows the table.
  table.find_or_insert("k0");
  CHECK(table.growth_count() == 7U);
}

TEST_CASE("removal keeps relative order and does not decrease the growth count", MAGIC_HASH_TAGS) {
  Table table(2);
  for (const char* k : {"a", "b", "c", "d", "e", "f"}) {
    table.insert(k, 0);
  }
  const auto before = table.keys();
  CHECK(table.remove("c"));
  CHECK_FALSE(table.remove("c"));
  auto expected = before;
  std::erase(expected, "c");
  CHECK(table.keys() == expected);
  CHECK(table.size() == 5U);
  CHECK(table.growth_count() == 6U);
  // The count is still 6 = 3 x 2, so the next new key grows the table,
  // and growth recounts the live entries.
  table.insert("g", 0);
  CHECK(table.bucket_count() == 8U);
  CHECK(table.growth_count() == 6U);
}

TEST_CASE("clear-value removal keeps the entry and its position", MAGIC_HASH_TAGS) {
  Table table(32);
  table.insert("u3", 3);
  table.insert("u1", 1);
  CHECK(table.clear_value("u1"));
  CHECK_FALSE(table.clear_value("zz"));
  CHECK(table.keys() == std::vector<std::string>{"u1", "u3"});
  REQUIRE(table.find("u1") != nullptr);
  CHECK_FALSE(table.find("u1")->value.has_value());
  CHECK(table.find("u3")->value == 3);
}

namespace {

// Straight transcription of the spec, used as an oracle.
class ReferenceTable {
 public:
  explicit ReferenceTable(std::size_t requested) : buckets_(magic_table_size(requested)) {
    shift_ = magic_table_shift(buckets_.size());
  }

  void insert(const std::string& key) {
    if (contains(key)) {
      return;
    }
    if (count_ >= 3 * buckets_.size()) {
      std::vector<std::deque<std::string>> old = std::move(buckets_);
      buckets_.assign(old.size() * 4, {});
      shift_ -= 2;
      count_ = 0;
      for (const auto& chain : old) {
        for (const auto& k : chain) {
          buckets_[bucket(k)].push_front(k);
          ++count_;
        }
      }
    }
    buckets_[bucket(key)].push_front(key);
    ++count_;
  }

  void remove(const std::string& key) { std::erase(buckets_[bucket(key)], key); }

  [[nodiscard]] std::vector<std::string> keys() const {
    std::vector<std::string> out;
    for (const auto& chain : buckets_) {
      out.insert(out.end(), chain.begin(), chain.end());
    }
    return out;
  }

 private:
  [[nodiscard]] std::size_t bucket(const std::string& k) const {
    return magic_bucket(magic_string_hash(k), shift_, buckets_.size());
  }
  [[nodiscard]] bool contains(const std::string& k) const {
    const auto& chain = buckets_[bucket(k)];
    return std::find(chain.begin(), chain.end(), k) != chain.end();
  }

  std::vector<std::deque<std::string>> buckets_;
  unsigned shift_ = 0;
  std::size_t count_ = 0;
};

}  // namespace

TEST_CASE("iteration order matches the spec model over random histories", MAGIC_HASH_TAGS) {
  std::mt19937 rng(20261107);  // fixed seed: deterministic
  for (const std::size_t requested : {2U, 32U}) {
    Table table(requested);
    ReferenceTable reference(requested);
    for (int step = 0; step < 3000; ++step) {
      const std::string key = "n" + std::to_string(rng() % 700) + (rng() % 5 == 0 ? "\xC3\xA9" : "");
      if (rng() % 4 == 0) {
        table.remove(key);
        reference.remove(key);
      } else {
        table.insert(key, step);
        reference.insert(key);
      }
    }
    CHECK(table.bucket_count() > requested);
    CHECK(table.keys() == reference.keys());
  }
}

TEST_CASE("hefesto::debug::hashorder exposes the table order to scripts", MAGIC_HASH_TAGS) {
  hefesto::test::Session s;
  CHECK(s.exec("hefesto::debug::hashorder", {"32", "+u3", "+u1", "+u2"}).value == hefesto::cmd::Value{"u1 u2 u3"});
  CHECK(s.exec("hefesto::debug::hashorder", {"2", "+r0", "+r1", "+r2", "+r3", "+r4", "+r5", "-r0", "+r6"}).value ==
        hefesto::cmd::Value{"r1 r2 r6 r3 r4 r5"});
  CHECK(s.exec("hefesto::debug::hashorder", {"32"}).value == hefesto::cmd::Value{""});
  for (const auto& bad : std::vector<std::vector<std::string>>{{}, {"0"}, {"x"}, {"32", "k"}, {"32", "+"}}) {
    CHECK_FALSE(s.exec("hefesto::debug::hashorder", bad).ok());
  }
}
