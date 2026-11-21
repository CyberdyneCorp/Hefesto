// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for property strings and brace strings (capability
// schematic-file-format "Property strings", "Brace strings", "Boolean
// values"; hefesto-testing-strategy "Parser fuzzing"; openspec/changes/
// m8-schematic design D19).
//
// Input: mode byte, then `props`, `key` and `value` separated by NUL bytes
// (missing parts are empty). Every lookup and update runs on them, and two
// properties are checked: a value written as a brace string reads back as
// the value without its CRs, and in a string with no quotes, backslashes or
// trailing `=`, setting a non-empty value then looking the key up finds an
// attribute.
//
// Mode byte: bit 0 removes the key instead of setting `value`.
//
// Owner: FORMAT.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace {

std::string_view next_part(std::string_view& rest) {
  const auto nul = rest.find('\0');
  const std::string_view part = rest.substr(0, nul);
  rest = nul == std::string_view::npos ? std::string_view{} : rest.substr(nul + 1);
  return part;
}

void check_brace_string(std::string_view value) {
  namespace schem = hefesto::schem;
  const auto read = schem::read_document("S " + schem::brace_string(value) + "\n", {});
  std::string expected;
  for (const char c : value) {
    if (c != '\r') {
      expected.push_back(c);
    }
  }
  if (!read.ok() || read.value().globals.s != expected) {
    __builtin_trap();
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace schem = hefesto::schem;
  const auto split = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  std::string_view rest = split.rest;
  const std::string_view props = next_part(rest);
  const std::string_view key = next_part(rest);
  const std::string_view value = next_part(rest);

  (void)schem::parse_properties(props);
  const auto decoded = schem::get_token(props, key);
  (void)schem::get_token(props, key, schem::ValueForm::raw);
  (void)schem::decode_value(props);
  (void)schem::is_true(decoded.value_or(""));
  (void)schem::is_false(value);
  const bool remove = (split.mode & 1U) != 0;
  const std::string updated = schem::set_token(props, key, remove ? std::nullopt : std::optional{value});
  (void)schem::parse_properties(updated);
  // Quotes and backslashes in `props`, or a trailing `=` (whose value may
  // start after separators, LF included), may swallow an appended attribute,
  // as in xschem; other strings must always show the key that was set.
  const auto last = props.find_last_not_of(" \t\n;");
  const bool plain = props.find_first_of("\"\\") == std::string_view::npos &&
                     (last == std::string_view::npos || props[last] != '=') &&
                     key.find_first_of(" \t\n;=\"\\") == std::string_view::npos;
  if (!remove && plain && !value.empty() && !key.empty() && !schem::get_token(updated, key).has_value()) {
    __builtin_trap();
  }
  check_brace_string(props);
  return 0;
}
