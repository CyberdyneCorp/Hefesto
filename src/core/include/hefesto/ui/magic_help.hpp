// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Magic's help strings (ui-command-dispatch "Command registration and help
// text", "help command"): the text each window client of Magic 8.3.694's
// Tcl build registers for its commands with WindAddCommand. `help` prints
// these texts, in strcmp order, for the registered commands that Magic has.
// Owner: DISPATCH.

#pragma once

#include <span>
#include <string_view>

namespace hefesto::ui {

struct MagicHelpEntry {
  std::string_view client;  // `*window`, `layout` or `color`
  std::string_view name;    // the first word of `text`
  std::string_view text;    // the whole help string, as Magic prints it
};

/// Every entry, grouped by client and in ascending byte order of `text`
/// within a client.
[[nodiscard]] std::span<const MagicHelpEntry> magic_help_entries() noexcept;

/// Magic's help string for `name` in `client`'s table; empty when Magic's
/// table has no such command.
[[nodiscard]] std::string_view magic_help_text(std::string_view client, std::string_view name) noexcept;

/// True when Magic's Lookup() (utils/lookup.c) finds `word` ambiguous in
/// `client`'s table. Lookup ends an entry's name at a space only, so the
/// tab-terminated entries `id\tOR` and `get\tOR` are no exact names: `id`
/// is a prefix of `id\tOR` and `identify ...`, and Magic's quiet Tcl
/// dispatch makes it fail with an empty result (golden UI-G-002).
[[nodiscard]] bool magic_lookup_ambiguous(std::string_view client, std::string_view word) noexcept;

}  // namespace hefesto::ui
