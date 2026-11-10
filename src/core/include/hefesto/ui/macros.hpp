// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Macro tables and key names (capability ui-command-dispatch, requirements
// "Macro tables", "Macro key names", "macro and imacro commands";
// ui-tools-and-input "Default key bindings", "Key event normalization";
// design.md D7). One table per tool for the `layout` client and per client
// otherwise. Key names follow X11 keysym names with `Meta_`/`Control_`/
// `Capslock_`/`Shift_` prefixes on every platform, through an embedded
// keysym table (data from X11 keysymdef.h, MIT/X11 license; Q-UI-12); the
// shell maps native key codes to keysyms.
//
// A key is Magic's extended key code: the keysym (or the character code)
// masked to 16 bits, with the X11 modifier mask in the upper bits. Tables
// keep Magic's hash order (a word-key utils/hash.c table of 32 buckets,
// MacroClients a string-key table of 4), so listings come out in Magic's
// order, including its quirk that a deleted entry stays in the table with
// no macro and ends a listing.
// (Magic ref: utils/macros.c, textio/txCommands.c:1199-1363)
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::ui {

/// Modifier bits of a key (X11's ShiftMask, LockMask, ControlMask, Mod1Mask).
enum Modifier : std::uint8_t { kShift = 1U, kLock = 2U, kControl = 4U, kMeta = 8U };

/// A normalized key or pointer button: a keysym or character code (16 bits)
/// plus modifier bits.
struct Key {
  std::uint32_t code = 0;
  std::uint8_t modifiers = 0;

  /// Magic's extended key code: code | modifiers << 16.
  [[nodiscard]] constexpr std::uint32_t packed() const noexcept {
    return (code & 0xFFFFU) | (std::uint32_t{modifiers} << 16U);
  }
  friend auto operator<=>(const Key&, const Key&) = default;
};

/// XK_Pointer_Button1 (masked to 16 bits); `ButtonN` is kPointerButton1 + N - 1
/// (1 left, 2 middle, 3 right, 4 wheel up, 5 wheel down).
inline constexpr std::uint32_t kPointerButton1 = 0xFEE9U;
[[nodiscard]] constexpr Key button_key(int button, std::uint8_t modifiers = 0) noexcept {
  return Key{kPointerButton1 + static_cast<std::uint32_t>(button - 1), modifiers};
}

/// The X11 keysym of a name of the embedded table (`F1`, `KP_Left`, `space`,
/// `Pointer_Button1`, ...), masked to 16 bits; std::nullopt when unknown.
[[nodiscard]] std::optional<std::uint32_t> keysym_from_name(std::string_view name) noexcept;
/// The first name of a keysym in the table (XKeysymToString); empty if none.
[[nodiscard]] std::string_view keysym_name(std::uint32_t keysym) noexcept;

/// "Macro key names": parses a key name with its modifier prefixes and
/// normalizes it. Fails with UI-MACRO-001 `Unrecognized macro name K`.
[[nodiscard]] diag::Outcome<Key> parse_key_name(std::string_view name);
/// The display name of a key (`macro` listings, "Unknown macro" errors).
[[nodiscard]] std::string key_display_name(const Key& key);

/// One binding.
struct Macro {
  std::string text;
  std::string help;  // empty: none
  bool interactive = false;  // `imacro`
};

class MacroTables {
 public:
  MacroTables();

  /// MacroDefine: replaces text and flag of `key` in `table_name` (created on
  /// first use) and sets the help to `help`, clearing it when nullopt.
  void define(std::string_view table_name, const Key& key, std::string text, std::optional<std::string> help,
              bool interactive);
  /// define() from a Macro (an empty help clears it).
  void set(std::string_view table_name, const Key& key, Macro macro);
  /// MacroDefineHelp: sets the help of an existing binding. As in Magic, a
  /// missing key gets an empty entry (which ends listings). False when the
  /// key has no binding.
  bool set_help(std::string_view table_name, const Key& key, std::optional<std::string> help);
  [[nodiscard]] const Macro* find(std::string_view table_name, const Key& key) const;
  /// MacroDelete: removes the binding; the entry stays without a macro.
  bool erase(std::string_view table_name, const Key& key);
  /// MacroCopy: defines every binding of `from` in `to`, in `from`'s order.
  void copy(std::string_view from, std::string_view to);

  [[nodiscard]] bool has_table(std::string_view name) const;
  /// The table names in Magic's MacroClients order.
  [[nodiscard]] std::vector<std::string> table_names() const;
  /// The entries of a table in Magic's order; a deleted entry has a null
  /// macro. Empty for an unknown table.
  [[nodiscard]] std::vector<std::pair<Key, const Macro*>> entries(std::string_view table_name) const;

 private:
  struct WordKey {
    using Key = std::uint32_t;
    [[nodiscard]] static std::uint64_t hash(const Key& key) noexcept { return key; }
  };
  using Table = util::MagicHashTable<Macro, WordKey>;

  [[nodiscard]] const Table* table(std::string_view name) const;
  Table& table_for_update(std::string_view name);

  util::MagicHashTable<Table> tables_;
};

}  // namespace hefesto::ui
