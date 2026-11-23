// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The substring index of the `.subckt` header template (Q-SCHEM-19 keys
// looked up in `extra`; capability schematic-netlisting "Subcircuit
// definitions").
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "schem/net/substring_index.hpp"

using hefesto::schem::detail::SubstringIndex;

namespace {

constexpr const char* kTag = "[cap:schematic-netlisting][req:Subcircuit definitions]";

}  // namespace

// Regression (review of PR #88): the index was a suffix automaton built for
// every definition, about 100 bytes per byte of `extra`, with 32-bit counters
// that overflow past 2 GiB. The first kDirectQueries keys now use find()
// and build nothing; only more keys build a suffix array (4 bytes per byte
// kept), and a text over kMaxIndexedBytes is never indexed.
TEST_CASE("substring index: few keys build nothing, many keys give the same answers", kTag) {
  std::string text;
  for (int i = 0; i < 2000; ++i) text += "w" + std::to_string(i) + " ";
  text += "abcabcabd banana";
  SubstringIndex index{text};
  for (std::size_t i = 0; i < SubstringIndex::kDirectQueries; ++i) {
    CHECK(index.contains("w1999"));
  }
  CHECK_FALSE(index.indexed());
  for (const char* key : {"", "w0 ", "abcabd", "anana", "a", "d ban", "w1999 abc", "banana"}) {
    CHECK(index.contains(key));
  }
  for (const char* key : {"abcabe", "bananas", "w2000", "x", "nanab", "  "}) {
    CHECK_FALSE(index.contains(key));
  }
  CHECK(index.indexed());
}

TEST_CASE("substring index: repetitive and empty texts", kTag) {
  const std::string same(5000, 'a');
  SubstringIndex index{same};
  for (std::size_t i = 0; i <= SubstringIndex::kDirectQueries; ++i) (void)index.contains("b");
  CHECK(index.indexed());
  CHECK(index.contains(std::string(5000, 'a')));
  CHECK_FALSE(index.contains(std::string(5001, 'a')));
  CHECK_FALSE(index.contains("ab"));
  SubstringIndex empty{""};
  for (std::size_t i = 0; i <= SubstringIndex::kDirectQueries; ++i) CHECK_FALSE(empty.contains("a"));
  CHECK(empty.contains(""));
  CHECK(SubstringIndex::kMaxIndexedBytes <= UINT32_MAX / 2);
}
