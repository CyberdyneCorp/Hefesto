// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command tags (openspec/changes/m4-scripting/design.md D6; capability
// scripting-api "Command tag callbacks", "Tag callback execution and
// substitution"; Q-SCRIPT-3): `tag <command> ?script?` stores a Tcl script
// per command; after the command succeeds the script runs with Magic's
// %-substitutions, done in a single pass with each substituted value
// list-quoted (so "%r" of a result with spaces stays one word).
//
// Ported from Magic 8.3.694 (fc5daa9e) tcltk/tclmagic.c AddCommandTag,
// TagCallback (Magic ref: tcltk/tclmagic.c:85-101,107-383), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md), with Q-SCRIPT-3's fix in both magic-compat
// settings.
//
// Owner: BINDING (design D2).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"

namespace hefesto::script {

/// The tag scripts of one interpreter, keyed by command name.
class TagTable {
 public:
  /// Stores (or, with an empty script, removes) the tag of `command`.
  void set(std::string_view command, std::string script);
  [[nodiscard]] std::optional<std::string> get(std::string_view command) const;
  void remove(std::string_view command);
  [[nodiscard]] bool empty() const noexcept { return scripts_.empty(); }
  /// Every (command, script) pair in command-name order.
  [[nodiscard]] const std::map<std::string, std::string, std::less<>>& entries() const noexcept { return scripts_; }

 private:
  std::map<std::string, std::string, std::less<>> scripts_;
};

/// The tag-table key of an invoked command name: a leading `::`, then a
/// leading `magic::`, removed (TagVerify / TagCallback).
[[nodiscard]] std::string_view tag_key(std::string_view invoked) noexcept;

/// `tag <name> ?script?` and `tag add|replace <name> <script>` on `tags`
/// (AddCommandTag). `args` are the words after `tag`. A query returns the
/// script (empty when none); a set returns an empty value. Fails with an
/// empty message for a wrong word count and with Tcl's index message for a
/// keyword other than a unique prefix of `add` or `replace`; the failure is
/// CommandResult::raises (a Tcl error in both strict settings).
[[nodiscard]] cmd::CommandResult tag_command(TagTable& tags, const std::vector<std::string>& args);

/// What a tag script's %-escapes expand to (scripting-api "Tag callback
/// execution and substitution").
struct TagContext {
  std::string window;                // %W: the window path of the command ("" = unknown)
  std::string result;                // %R / %r: the command's result
  std::vector<std::string> args;     // %0 .. %5: the command words, %0 the command name
  std::uint64_t command_count = 0;   // the post-command counter (restored after the tag)
};

/// The tag script with its %-escapes substituted in one left-to-right pass
/// (never rescanned), each value inserted as one Tcl list element:
/// %W, %r, %R, %0..%5 (`{}` for an empty value or a missing argument),
/// `%%` a single `%`; any other `%x` and a final `%` are copied unchanged.
[[nodiscard]] std::string substitute_tag(std::string_view script, const TagContext& context);

/// True when the single-pass scan of `script` meets `%R`: the tag's own
/// result then replaces the command's result.
[[nodiscard]] bool tag_resets_result(std::string_view script) noexcept;

/// `value` quoted as Tcl's `list` quotes a single element (`{}` when empty).
[[nodiscard]] std::string quote_list_element(std::string_view value);

}  // namespace hefesto::script
