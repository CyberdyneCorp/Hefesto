// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Macro key names and tables (ui-command-dispatch "Macro tables", "Macro key
// names"). Owner: DISPATCH.
// (Magic ref: utils/macros.c:96-195, 405-461, 514-761)

#include "hefesto/ui/macros.hpp"

#include <array>
#include <cctype>
#include <string>
#include <utility>

#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::ui {

namespace {

struct Prefix {
  std::string_view text;
  std::uint8_t modifier;
};

constexpr std::array kPrefixes{Prefix{"Meta_", kMeta}, Prefix{"Alt_", kMeta}, Prefix{"Control_", kControl},
                               Prefix{"Capslock_", kLock}, Prefix{"Shift_", kShift}};

// Consumes one modifier prefix (or a pair of single quotes around the rest)
// from the front of `rest`; false when none applies.
bool take_prefix(std::string_view& rest, std::uint8_t& modifiers) {
  for (const Prefix& p : kPrefixes) {
    if (rest.starts_with(p.text)) {
      modifiers = static_cast<std::uint8_t>(modifiers | p.modifier);
      rest.remove_prefix(p.text.size());
      return true;
    }
  }
  if (rest.size() > 1 && rest.front() == '^') {
    modifiers = static_cast<std::uint8_t>(modifiers | kControl);
    rest.remove_prefix(1);
    return true;
  }
  if (!rest.empty() && rest.front() == '\'') {
    const std::size_t close = rest.rfind('\'');
    if (close == 0) {
      return false;  // an unmatched quote ends the prefixes
    }
    rest = rest.substr(1, close - 1);
    return true;
  }
  return false;
}

// "A single character with Control or Shift SHALL be normalized".
Key single_character(char c, std::uint8_t modifiers) {
  const auto ch = static_cast<unsigned char>(c);
  if ((modifiers & (kControl | kShift)) == 0) {
    return Key{ch, modifiers};
  }
  const auto upper = static_cast<std::uint32_t>(std::toupper(ch));
  const std::uint32_t code = (modifiers & kShift) != 0 ? upper : (upper - 'A' + 1U) & 0xFFFFU;
  const bool keep = (modifiers & (kMeta | kLock)) != 0 || ((modifiers & kShift) != 0 && (modifiers & kControl) != 0);
  return Key{code, keep ? modifiers : std::uint8_t{0}};
}

std::optional<std::uint32_t> named_key(std::string_view name) {
  if (name.starts_with("Button")) {
    return keysym_from_name("Pointer_" + std::string{name});
  }
  return keysym_from_name(name);
}

std::string hex_name(const Key& key) {
  static constexpr std::string_view kHex = "0123456789ABCDEF";
  std::string out = "0x";
  out += kHex[key.modifiers & 0xFU];
  for (int shift = 12; shift >= 0; shift -= 4) {
    out += kHex[(key.code >> static_cast<unsigned>(shift)) & 0xFU];
  }
  return out;
}

}  // namespace

diag::Outcome<Key> parse_key_name(std::string_view name) {
  std::uint8_t modifiers = 0;
  std::string_view rest = name;
  while (!rest.empty() && take_prefix(rest, modifiers)) {
  }
  if (rest.starts_with("XK_")) {
    rest.remove_prefix(3);
  }
  if (rest.size() == 1) {
    return single_character(rest.front(), modifiers);
  }
  if (rest.starts_with("<del>")) {
    return Key{0x7FU, modifiers};
  }
  if (const auto keysym = named_key(rest); keysym && *keysym != 0) {
    return Key{*keysym, modifiers};
  }
  return diag::make(codes::kMacroName, "Unrecognized macro name " + std::string{name});
}

std::string key_display_name(const Key& key) {
  if (key.modifiers == 0 && key.code < 0x80U) {
    if (key.code < 0x20U) {
      return std::string{'^', static_cast<char>(key.code + '@')};
    }
    if (key.code == 0x7FU) {
      return "<del>";
    }
    return std::string(1, static_cast<char>(key.code));
  }
  const std::string_view name = keysym_name(key.code);
  if (name.empty()) {
    return hex_name(key);
  }
  std::string out;
  if ((key.modifiers & kMeta) != 0) {
    out += "Meta_";
  }
  if ((key.modifiers & kControl) != 0) {
    out += "Control_";
  }
  if ((key.modifiers & kLock) != 0) {
    out += "Capslock_";
  }
  if ((key.modifiers & kShift) != 0) {
    out += "Shift_";
  }
  return out + "XK_" + std::string{name};
}

// --- MacroTables ----------------------------------------------------------------

MacroTables::MacroTables() : tables_(4) {}

const MacroTables::Table* MacroTables::table(std::string_view name) const {
  const auto* entry = tables_.find(std::string{name});
  return entry == nullptr || !entry->value ? nullptr : &*entry->value;
}

MacroTables::Table& MacroTables::table_for_update(std::string_view name) {
  auto& entry = tables_.find_or_insert(std::string{name});
  if (!entry.value) {
    entry.value.emplace(32);
  }
  return *entry.value;
}

void MacroTables::define(std::string_view table_name, const Key& key, std::string text,
                         std::optional<std::string> help, bool interactive) {
  table_for_update(table_name).insert(key.packed(), Macro{std::move(text), help.value_or(""), interactive});
}

void MacroTables::set(std::string_view table_name, const Key& key, Macro macro) {
  std::optional<std::string> help;
  if (!macro.help.empty()) {
    help = std::move(macro.help);
  }
  define(table_name, key, std::move(macro.text), std::move(help), macro.interactive);
}

bool MacroTables::set_help(std::string_view table_name, const Key& key, std::optional<std::string> help) {
  auto& entry = table_for_update(table_name).find_or_insert(key.packed());
  if (!entry.value) {
    return false;
  }
  entry.value->help = help.value_or("");
  return true;
}

const Macro* MacroTables::find(std::string_view table_name, const Key& key) const {
  const Table* t = table(table_name);
  if (t == nullptr) {
    return nullptr;
  }
  const auto* entry = t->find(key.packed());
  return entry == nullptr || !entry->value ? nullptr : &*entry->value;
}

bool MacroTables::erase(std::string_view table_name, const Key& key) {
  auto* entry = tables_.find(std::string{table_name});
  if (entry == nullptr || !entry->value) {
    return false;
  }
  auto* found = entry->value->find(key.packed());
  if (found == nullptr || !found->value) {
    return false;
  }
  found->value.reset();
  return true;
}

void MacroTables::copy(std::string_view from, std::string_view to) {
  const auto source = entries(from);
  std::vector<std::pair<Key, Macro>> copies;
  for (const auto& [key, macro] : source) {
    if (macro != nullptr) {
      copies.emplace_back(key, *macro);
    }
  }
  for (auto& [key, macro] : copies) {
    set(to, key, std::move(macro));
  }
}

bool MacroTables::has_table(std::string_view name) const {
  return table(name) != nullptr;
}

std::vector<std::string> MacroTables::table_names() const {
  std::vector<std::string> out;
  tables_.for_each([&](const auto& entry) {
    if (entry.value) {
      out.push_back(entry.key);
    }
  });
  return out;
}

std::vector<std::pair<Key, const Macro*>> MacroTables::entries(std::string_view table_name) const {
  std::vector<std::pair<Key, const Macro*>> out;
  const Table* t = table(table_name);
  if (t == nullptr) {
    return out;
  }
  t->for_each([&](const auto& entry) {
    const Key key{entry.key & 0xFFFFU, static_cast<std::uint8_t>((entry.key >> 16U) & 0xFFU)};
    out.emplace_back(key, entry.value ? &*entry.value : nullptr);
  });
  return out;
}

}  // namespace hefesto::ui
