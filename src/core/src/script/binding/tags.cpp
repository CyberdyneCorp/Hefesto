// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command tags (see tags.hpp). Ported from Magic 8.3.694 (fc5daa9e)
// tcltk/tclmagic.c (AddCommandTag, TagCallback), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).

#include "hefesto/script/binding/tags.hpp"

#include <array>
#include <cstddef>
#include <utility>

#include "hefesto/cmd/value.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::script {

namespace {

constexpr std::string_view kTclError = "SCRIPT-TCL-001";

// A Tcl error: raised in both strict settings, message as given.
cmd::CommandResult tcl_error(std::string message) {
  return cmd::CommandResult::raised(diag::make(kTclError, std::move(message)));
}

// Tcl_GetIndexFromObj(..., "tag options", 0, ...) over {"add", "replace"}:
// exact case, an exact name or a unique prefix; the index or the error.
std::optional<std::size_t> tag_option(std::string_view word, std::string* error) {
  constexpr std::array<std::string_view, 2> kOptions{"add", "replace"};
  std::optional<std::size_t> found;
  std::size_t matches = 0;
  for (std::size_t i = 0; i < kOptions.size(); ++i) {
    if (kOptions[i] == word) {
      return i;
    }
    if (kOptions[i].starts_with(word)) {
      found = i;
      ++matches;
    }
  }
  if (matches == 1 && !word.empty()) {
    return found;
  }
  const std::string kind = matches > 1 || word.empty() ? "ambiguous" : "bad";
  *error = kind + " tag options \"" + std::string{word} + "\": must be add or replace";
  return std::nullopt;
}

// The value one %-escape inserts, or nullopt for an escape copied as is.
std::optional<std::string> escape_value(char escape, const TagContext& context) {
  switch (escape) {
    case 'W':
      return quote_list_element(context.window);
    case 'r':
    case 'R':
      return quote_list_element(context.result);
    case '%':
      return std::string{"%"};
    default:
      break;
  }
  if (escape >= '0' && escape <= '5') {
    const auto index = static_cast<std::size_t>(escape - '0');
    return quote_list_element(index < context.args.size() ? context.args[index] : std::string{});
  }
  return std::nullopt;
}

}  // namespace

void TagTable::set(std::string_view command, std::string script) {
  if (script.empty()) {
    remove(command);
    return;
  }
  scripts_.insert_or_assign(std::string{command}, std::move(script));
}

std::optional<std::string> TagTable::get(std::string_view command) const {
  const auto it = scripts_.find(command);
  if (it == scripts_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void TagTable::remove(std::string_view command) {
  const auto it = scripts_.find(command);
  if (it != scripts_.end()) {
    scripts_.erase(it);
  }
}

std::string_view tag_key(std::string_view invoked) noexcept {
  if (invoked.starts_with("::")) {
    invoked.remove_prefix(2);
  }
  if (invoked.starts_with("magic::")) {
    invoked.remove_prefix(7);
  }
  return invoked;
}

cmd::CommandResult tag_command(TagTable& tags, const std::vector<std::string>& args) {
  // Magic's argc counts the command word.
  std::size_t argc = args.size() + 1;
  std::size_t start = 0;
  bool add = false;
  if (argc == 4) {
    std::string error;
    const auto option = tag_option(args[0], &error);
    if (!option) {
      return tcl_error(std::move(error));
    }
    add = *option == 0;
    start = 1;
    --argc;
  }
  if (argc != 2 && argc != 3) {
    return tcl_error({});
  }
  const std::string& name = args[start];
  const std::optional<std::string> existing = tags.get(name);
  if (argc == 2) {
    return cmd::CommandResult::success(existing.value_or(std::string{}));
  }
  const std::string& script = args[start + 1];
  if (add && existing) {
    if (!script.empty()) {  // appending nothing is a no-op
      tags.set(name, *existing + " ; " + script);
    }
  } else {
    tags.set(name, script);  // empty removes
  }
  return cmd::CommandResult::success();
}

std::string substitute_tag(std::string_view script, const TagContext& context) {
  std::string out;
  out.reserve(script.size());
  std::size_t i = 0;
  while (i < script.size()) {
    const std::size_t percent = script.find('%', i);
    if (percent == std::string_view::npos || percent + 1 >= script.size()) {
      out.append(script.substr(i));
      break;
    }
    out.append(script.substr(i, percent - i));
    if (const auto value = escape_value(script[percent + 1], context)) {
      out += *value;
    } else {
      out.append(script.substr(percent, 2));
    }
    i = percent + 2;
  }
  return out;
}

bool tag_resets_result(std::string_view script) noexcept {
  std::size_t i = 0;
  while ((i = script.find('%', i)) != std::string_view::npos && i + 1 < script.size()) {
    if (script[i + 1] == 'R') {
      return true;
    }
    i += 2;  // `%%` and every other escape consume two characters
  }
  return false;
}

std::string quote_list_element(std::string_view value) {
  return cmd::Value{cmd::Value::List{cmd::Value{std::string{value}}}}.to_string();
}

}  // namespace hefesto::script
