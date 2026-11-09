// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/util/guarded.hpp"
#include "hefesto/util/record_order.hpp"

using hefesto::util::RecordKey;

#define ORDER_TAGS "[cap:hefesto-output-determinism][req:Deterministic order where Magic's order is arbitrary]"

namespace {
struct Rec {
  std::string name;
  std::int64_t x;
  std::int64_t y;
  std::string type;
};
RecordKey key_of(const Rec& r) {
  return {r.name, r.x, r.y, r.type};
}
}  // namespace

TEST_CASE("records sort by name, then lower-left x and y, then type", ORDER_TAGS) {
  std::vector<Rec> recs = {
      {"b", 0, 0, "m1"}, {"a", 5, 0, "m1"}, {"a", 1, 9, "m2"}, {"a", 1, 2, "m2"}, {"a", 1, 2, "m1"}, {"B", 0, 0, "m1"},
  };
  hefesto::util::sort_records(recs, key_of);
  std::vector<std::string> got;
  for (const auto& r : recs) {
    got.push_back(r.name + "," + std::to_string(r.x) + "," + std::to_string(r.y) + "," + r.type);
  }
  CHECK(got == std::vector<std::string>{"B,0,0,m1", "a,1,2,m1", "a,1,2,m2", "a,1,9,m2", "a,5,0,m1", "b,0,0,m1"});
}

TEST_CASE("names compare byte-wise as unsigned bytes on every platform", ORDER_TAGS) {
  // 0xE9 sorts after 'z' even where char is signed.
  CHECK(hefesto::util::compare_bytes("z", "\xE9") < 0);
  CHECK(hefesto::util::compare_bytes("ab", "abc") < 0);
  CHECK(hefesto::util::compare_bytes("abc", "abc") == 0);
  CHECK(hefesto::util::compare_bytes("", "") == 0);
}

TEST_CASE("coupling terminals: smaller name first", ORDER_TAGS) {
  // Scenario "Equivalent .ext despite order": cap "c" "a" is written as cap "a" "c".
  CHECK(hefesto::util::order_terminals("c", "a") == std::pair<std::string_view, std::string_view>{"a", "c"});
  CHECK(hefesto::util::order_terminals("a", "b") == std::pair<std::string_view, std::string_view>{"a", "b"});
}

TEST_CASE("a custom name comparator plugs in", ORDER_TAGS) {
  const auto reversed = [](std::string_view a, std::string_view b) noexcept {
    return hefesto::util::compare_bytes(b, a);
  };
  CHECK(hefesto::util::record_less({"b", 0, 0, ""}, {"a", 0, 0, ""}, reversed));
}

TEST_CASE("the natural comparator orders digit runs by value and is total",
          "[cap:cell-file-format][req:Natural ordering of names]") {
  using hefesto::util::compare_natural;
  CHECK(compare_natural("a2", "a10") < 0);
  CHECK(compare_natural("a10", "a2") > 0);
  CHECK(compare_natural("A1", "a2") < 0);      // letters compare case-insensitively
  CHECK(compare_natural("a", "a1") < 0);       // a proper prefix sorts first
  CHECK(compare_natural("a01", "a1") != 0);    // equal by value, then byte-wise: a total order
  CHECK(compare_natural("a01", "a1") == -compare_natural("a1", "a01"));
  CHECK(compare_natural("x", "x") == 0);
  std::vector<std::string> names{"u10", "u2", "U1", "u1"};
  hefesto::util::sort_records(names, [](const std::string& n) { return hefesto::util::RecordKey{n, 0, 0, {}}; },
                              compare_natural);
  CHECK(names == std::vector<std::string>{"U1", "u1", "u2", "u10"});
}

TEST_CASE("single writer, concurrent readers", "[cap:hefesto-architecture][req:Concurrency model]") {
  hefesto::util::Guarded<std::vector<int>> db;
  std::atomic<bool> torn{false};
  std::vector<std::thread> readers;
  for (int r = 0; r < 4; ++r) {
    readers.emplace_back([&] {
      for (int i = 0; i < 2000; ++i) {
        db.read([&](const std::vector<int>& v) {
          // The writer always appends pairs, so a reader never sees an odd size.
          if (v.size() % 2 != 0) {
            torn = true;
          }
        });
      }
    });
  }
  for (int i = 0; i < 2000; ++i) {
    db.write([&](std::vector<int>& v) {
      v.push_back(i);
      v.push_back(i);
    });
  }
  for (auto& t : readers) {
    t.join();
  }
  CHECK_FALSE(torn.load());
  CHECK(db.read([](const std::vector<int>& v) { return v.size(); }) == 4000U);
}
