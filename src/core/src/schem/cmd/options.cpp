// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `schematic` option table and the matching, argument and error helpers
// (capability schematic-command "Command and option matching", "Subcommand
// table").
//
// Owner: CMD.

#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>

#include "hefesto/cmd/keywords.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "internal.hpp"

namespace hefesto::schem {

namespace {

// "Subcommand table", one row per option, in the table's order. Rows that
// the table writes as one (`line` / `box` / ...) are one option each.
constexpr std::array kOptions{
    SchematicOption{"open", "open <file> [-window new|tab|current]", false},
    SchematicOption{"new", "new schematic|symbol [<file>]", false},
    SchematicOption{"save", "save [<file>] [-force]"},
    SchematicOption{"close", "close [-discard]"},
    SchematicOption{"reload", "reload"},
    SchematicOption{"reload_symbols", "reload_symbols", false},
    SchematicOption{"path", "path [list|set <paths>|prepend <dir>]", false},
    SchematicOption{"resolve", "resolve <reference>", false},
    SchematicOption{"place", "place <symbol> <x> <y> [-rot 0..3] [-flip 0|1] [-props <string>]"},
    SchematicOption{"wire", "wire <x1> <y1> <x2> <y2> [-props <string>]"},
    SchematicOption{"line", "line <x1> <y1> <x2> <y2> [-layer <n>] [-props <string>]"},
    SchematicOption{"box", "box <x1> <y1> <x2> <y2> [-layer <n>] [-props <string>]"},
    SchematicOption{"arc", "arc <x> <y> <r> <a> <b> [-layer <n>] [-props <string>]"},
    SchematicOption{"polygon", "polygon <x1> <y1> <x2> <y2> ... [-layer <n>] [-props <string>]"},
    SchematicOption{"text", "text <x> <y> <rot> <flip> <xscale> <yscale> <text> [-props <string>]"},
    SchematicOption{"label", "label <x> <y> <net> [-kind pin|wire|ipin|opin|iopin] [-rot r] [-flip f]"},
    SchematicOption{"select", "select instance|wire|text|all|connected|area <args> | select clear"},
    SchematicOption{"selection", "selection"},
    SchematicOption{"delete", "delete"},
    SchematicOption{"move", "move <dx> <dy> [-stretch] [-insertwires]"},
    SchematicOption{"copy", "copy <dx> <dy>"},
    SchematicOption{"rotate", "rotate [-inplace] [<x> <y>]"},
    SchematicOption{"flip", "flip [-inplace] [<x> <y>]"},
    SchematicOption{"flipv", "flipv [-inplace] [<x> <y>]"},
    SchematicOption{"align", "align"},
    SchematicOption{"order", "order <index>"},
    SchematicOption{"clipboard", "clipboard [copy|set <text>]", false},
    SchematicOption{"paste", "paste <x> <y>"},
    SchematicOption{"merge", "merge <file> [<x> <y>]"},
    SchematicOption{"getprop", "getprop instance|symbol <name> [<key>] | getprop wire <n> [<key>] | "
                               "getprop schematic [K|G|V|S|F|E]"},
    SchematicOption{"setprop", "setprop instance <name> <key> [<value>] | setprop wire <n> <key> [<value>] | "
                               "setprop schematic <tag> <text>"},
    SchematicOption{"replace", "replace <instance> <symbol>"},
    SchematicOption{"instances", "instances [-pattern <glob>]"},
    SchematicOption{"nets", "nets"},
    SchematicOption{"net", "net <instance> <pin>"},
    SchematicOption{"descend", "descend <instance> [<element>] | descend -symbol <instance>"},
    SchematicOption{"ascend", "ascend"},
    SchematicOption{"hierarchy", "hierarchy [path|list]"},
    SchematicOption{"hilight", "hilight net <name>|selection|instance <name> | hilight clear [all] | hilight list"},
    SchematicOption{"undo", "undo [<n>]"},
    SchematicOption{"redo", "redo [<n>]"},
    SchematicOption{"netlist", "netlist [<file>] [-hier|-top|-flat] [-lvs] [-topsubckt] [-lvsignore] [-noprefix] "
                               "[-buschar <xy>] [-uppercase]"},
    SchematicOption{"check", "check"},
    SchematicOption{"symbol", "symbol make [-force] | symbol pins | symbol fromschematic | symbol toschematic"},
    SchematicOption{"simulate", "simulate [-wait] [-corner <name>]"},
    SchematicOption{"annotate", "annotate on [<job>]|off"},
    SchematicOption{"probe", "probe [<net>|-selection]"},
    SchematicOption{"export", "export svg|pdf|png <file> [-selection]"},
    SchematicOption{"pdk", "pdk [<name>]", false},
    SchematicOption{"help", "help", false},
};

bool is_flag_word(std::string_view word) {
  return word.size() >= 2 && word.front() == '-' && std::isalpha(static_cast<unsigned char>(word[1])) != 0;
}

std::vector<std::string_view> flag_names(std::span<const cmdimpl::FlagSpec> flags) {
  std::vector<std::string_view> names;
  names.reserve(flags.size());
  for (const auto& flag : flags) names.push_back(flag.name);
  return names;
}

// One glob item at the front of `pattern` against `c`: whether it matches
// and how many pattern characters it used. `[...]` sets and `\x` escapes.
std::pair<bool, std::size_t> match_set(std::string_view pattern, char c) {
  std::size_t i = 1;
  bool matched = false;
  while (i < pattern.size() && pattern[i] != ']') {
    const char low = pattern[i];
    char high = low;
    if (i + 2 < pattern.size() && pattern[i + 1] == '-' && pattern[i + 2] != ']') {
      high = pattern[i + 2];
      i += 2;
    }
    matched = matched || (c >= std::min(low, high) && c <= std::max(low, high));
    ++i;
  }
  return {matched, std::min(i + 1, pattern.size())};
}

std::pair<bool, std::size_t> match_one(std::string_view pattern, char c) {
  switch (pattern.front()) {
    case '?':
      return {true, 1};
    case '[':
      return match_set(pattern, c);
    case '\\':
      return pattern.size() > 1 ? std::pair{pattern[1] == c, std::size_t{2}} : std::pair{c == '\\', std::size_t{1}};
    default:
      return {pattern.front() == c, 1};
  }
}

}  // namespace

std::span<const SchematicOption> schematic_options() {
  return kOptions;
}

OpenDocument* active_document(Session& session) {
  return session.documents.empty() ? nullptr : &session.documents.back();
}

namespace cmdimpl {

cmd::CommandResult fail(std::string_view code, std::string text) {
  return cmd::CommandResult::raised(diag::make(code, std::move(text)));
}

cmd::CommandResult fail(diag::Diagnostic error) {
  return cmd::CommandResult::raised(std::move(error));
}

cmd::CommandResult usage(std::string_view option) {
  const SchematicOption* entry = find_option(option);
  const std::string syntax = entry != nullptr ? std::string{entry->syntax} : std::string{option};
  return fail(kUsage, "Usage: schematic " + syntax);
}

cmd::CommandResult no_document() {
  return fail(kNoDocument, "schematic: no schematic is open");
}

diag::Outcome<std::size_t> match_keyword(std::string_view word, std::span<const std::string_view> names) {
  const cmd::LookupResult found = cmd::lookup(word, names);
  switch (found.match) {
    case cmd::Match::found:
      return found.index;
    case cmd::Match::ambiguous:
      return diag::make(kBadOption, "schematic: ambiguous option \"" + std::string{word} + "\"");
    case cmd::Match::unrecognized:
      break;
  }
  return diag::make(kBadOption,
                    "schematic: unknown option \"" + std::string{word} + "\". Use \"schematic help\".");
}

const SchematicOption* find_option(std::string_view name) {
  for (const SchematicOption& option : kOptions) {
    if (option.name == name) return &option;
  }
  return nullptr;
}

diag::Outcome<ParsedArgs> parse_args(std::span<const std::string> args, std::span<const FlagSpec> flags,
                                     std::string_view option) {
  ParsedArgs parsed;
  parsed.flags.resize(flags.size());
  const std::vector<std::string_view> names = flag_names(flags);
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (!is_flag_word(args[i])) {
      parsed.positional.push_back(args[i]);
      continue;
    }
    const cmd::LookupResult found = cmd::lookup(args[i], names);
    if (found.match == cmd::Match::ambiguous) {
      return diag::make(kBadOption, "schematic: ambiguous option \"" + args[i] + "\"");
    }
    const bool takes_value = found.match == cmd::Match::found && flags[found.index].takes_value;
    if (found.match != cmd::Match::found || (takes_value && i + 1 >= args.size())) {
      return usage(option).diagnostics.front();
    }
    parsed.flags[found.index] = takes_value ? args[++i] : std::string{};
  }
  return parsed;
}

std::optional<double> parse_number(std::string_view text) {
  // Decimal only (no hex, inf or nan), read by strtod in the "C" locale,
  // as FORMAT reads numbers (Hefesto never calls setlocale).
  constexpr std::string_view kDecimal = "0123456789+-.eE";
  if (text.empty() || text.find_first_not_of(kDecimal) != std::string_view::npos) return std::nullopt;
  const std::string copy{text};
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  if (end != copy.c_str() + copy.size()) return std::nullopt;
  return value;
}

bool glob_match(std::string_view pattern, std::string_view text) {
  // Iterative with one backtrack point for the last `*` (linear in practice,
  // never recursive, so hostile patterns cannot exhaust the stack).
  std::size_t p = 0;
  std::size_t t = 0;
  std::optional<std::pair<std::size_t, std::size_t>> star;
  while (t < text.size()) {
    if (p < pattern.size() && pattern[p] == '*') {
      star = std::pair{++p, t};
      continue;
    }
    if (p < pattern.size()) {
      const auto [ok, used] = match_one(pattern.substr(p), text[t]);
      if (ok) {
        p += used;
        ++t;
        continue;
      }
    }
    if (!star) return false;
    p = star->first;
    t = ++star->second;
  }
  while (p < pattern.size() && pattern[p] == '*') ++p;
  return p == pattern.size();
}

std::optional<std::size_t> find_instance(const Document& document, std::string_view name) {
  for (std::size_t i = 0; i < document.instances.size(); ++i) {
    if (get_token(document.instances[i].props, "name") == name) return i;
  }
  return std::nullopt;
}

platform::Path absolute_path(const platform::Path& path) {
  std::error_code ec;
  const platform::Path absolute = std::filesystem::absolute(path, ec);
  return (ec ? path : absolute).lexically_normal();
}

std::string property_value(std::string_view props, const std::optional<std::string>& key) {
  if (!key) return std::string{props};
  return get_token(props, *key).value_or(std::string{});
}

std::string encoded_value(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  for (const char c : value) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  if (value.find_first_of(" \t\n;") != std::string_view::npos) out = '"' + out + '"';
  return out;
}

}  // namespace cmdimpl

}  // namespace hefesto::schem
