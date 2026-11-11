// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Where an option keyword or command-table entry ends (capability
// scripting-api "Option keyword matching"; Q-SCRIPT-14, resolution: compat).
// Magic's Lookup() compares a word with the text of each table entry up to
// its first space, so an entry such as "id\t..." (whose keyword ends at a
// tab) is only a prefix of itself and `id` is ambiguous beside `identify`.
// With magic-compat ON (the default) Hefesto keeps that rule; with it OFF
// any whitespace ends the keyword.
//
// The rule is per thread, scoped: the code that runs a command for a
// project (the script binding, the dispatcher) opens a KeywordTerminatorScope
// with that project's magic-compat; every keyword lookup on the thread
// (cmd::lookup, cmd::match_name, ui::magic_lookup_ambiguous) then reads it.
// Without a scope the magic-compat ON rule applies.
//
// Implemented in src/core/src/cmd/keywords.cpp (BINDING's decided change to
// M1-M3 code, openspec/changes/m4-scripting/design.md D2/D14).
//
// Owner: BINDING (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hefesto::cmd {

enum class KeywordTerminator : std::uint8_t {
  space,       // magic-compat ON: only a space ends a keyword (Magic's Lookup)
  whitespace,  // magic-compat OFF: any whitespace character ends it
};

/// The rule in effect on this thread.
[[nodiscard]] KeywordTerminator keyword_terminator() noexcept;

/// True when `c` ends a keyword under the rule in effect.
[[nodiscard]] bool ends_keyword(char c) noexcept;

/// The significant part of a table entry: its text up to the first
/// terminator under the rule in effect.
[[nodiscard]] std::string_view keyword_of(std::string_view entry) noexcept;

/// Sets the rule for this thread until destroyed (nestable).
class KeywordTerminatorScope {
 public:
  explicit KeywordTerminatorScope(bool magic_compat) noexcept;
  ~KeywordTerminatorScope();
  KeywordTerminatorScope(const KeywordTerminatorScope&) = delete;
  KeywordTerminatorScope& operator=(const KeywordTerminatorScope&) = delete;

 private:
  KeywordTerminator previous_;
};

}  // namespace hefesto::cmd
