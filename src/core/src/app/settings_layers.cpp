// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Settings layers (hefesto-settings).

#include "hefesto/app/settings_layers.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>

#include <nlohmann/json.hpp>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/diag/codes.hpp"

namespace hefesto::app {

namespace {

std::string on_off(bool value) {
  return value ? "on" : "off";
}

std::string join_sorted(std::vector<std::string> names) {
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  std::string out;
  for (const auto& name : names) {
    out += (out.empty() ? "" : " ") + name;
  }
  return out;
}

// One Tcl-like word: "quoted", {braced} or bare; nullopt when it uses
// substitution ($, [) or is malformed.
std::optional<std::string> literal_word(std::string_view word) {
  if (word.size() >= 2 && ((word.front() == '"' && word.back() == '"') || (word.front() == '{' && word.back() == '}'))) {
    word = word.substr(1, word.size() - 2);
  }
  if (word.empty() || word.find_first_of("$[]\\\"{}") != std::string_view::npos) {
    return std::nullopt;
  }
  return std::string{word};
}

// End of the word starting at `i`: the closing quote or brace, else the next blank or ';'.
std::size_t word_end(std::string_view line, std::size_t i) {
  const char close = line[i] == '"' ? '"' : (line[i] == '{' ? '}' : '\0');
  if (close == '\0') {
    const std::size_t end = line.find_first_of(" \t;", i);
    return end == std::string_view::npos ? line.size() : end;
  }
  const std::size_t end = line.find(close, i + 1);
  return end == std::string_view::npos ? line.size() : end + 1;
}

// The words of one command line, up to `;` or a `#` comment.
std::vector<std::string_view> split_words(std::string_view line) {
  std::vector<std::string_view> words;
  std::size_t i = line.find_first_not_of(" \t");
  while (i != std::string_view::npos && line[i] != ';' && line[i] != '#') {
    const std::size_t end = word_end(line, i);
    words.push_back(line.substr(i, end - i));
    i = line.find_first_not_of(" \t", end);
  }
  return words;
}

int brace_delta(std::string_view line) {
  int delta = 0;
  bool escaped = false;
  for (const char c : line) {
    if (escaped) {
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == '{') {
      ++delta;
    } else if (c == '}') {
      --delta;
    }
  }
  return delta;
}

// A recognized `set` line: VDD/GND/SUB value, or a globals(...) element name.
struct SetLine {
  std::string name;   // "vdd", "gnd", "sub" or "globals"
  std::string value;  // the value, or the globals element name
};

std::optional<SetLine> parse_set(std::string_view line) {
  const auto words = split_words(line);
  if (words.size() != 3 || words[0] != "set") {
    return std::nullopt;
  }
  const auto value = literal_word(words[2]);
  if (!value) {
    return std::nullopt;
  }
  const std::string_view var = words[1];
  if (var == "VDD" || var == "GND" || var == "SUB") {
    std::string name{var};
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return SetLine{name, *value};
  }
  if (var.starts_with("globals(") && var.ends_with(")") && var.size() > 9) {
    if (const auto element = literal_word(var.substr(8, var.size() - 9))) {
      return SetLine{"globals", *element};
    }
  }
  return std::nullopt;
}

}  // namespace

void install_builtin_defaults(SettingsStack& stack) {
  const settings::ProjectSettings defaults;
  stack.set(SettingLayer::builtin, settings::kMagicCompat, on_off(defaults.compat.magic_compat));
  stack.set(SettingLayer::builtin, settings::kNetgenCompat, on_off(defaults.compat.netgen_compat));
  stack.set(SettingLayer::builtin, settings::kScriptStrict, on_off(defaults.script_strict));
  stack.set(SettingLayer::builtin, kVdd, "vdd!");
  stack.set(SettingLayer::builtin, kGnd, "gnd!");
  stack.set(SettingLayer::builtin, kGlobalNets, "");
  stack.set(SettingLayer::builtin, kPdkUpdateCheck, "on");
}

void apply_project(SettingsStack& stack, const ProjectFile& project) {
  for (const auto& [name, value] : project.extra_settings) {
    stack.set(SettingLayer::project, name, value);
  }
  stack.set(SettingLayer::project, settings::kMagicCompat, on_off(project.settings.compat.magic_compat));
  stack.set(SettingLayer::project, settings::kNetgenCompat, on_off(project.settings.compat.netgen_compat));
  stack.set(SettingLayer::project, settings::kScriptStrict, on_off(project.settings.script_strict));
  if (project.nets.global_nets) {
    stack.set(SettingLayer::project, kGlobalNets, join_sorted(*project.nets.global_nets));
  }
  const auto put = [&](std::string_view name, const std::optional<std::string>& value) {
    if (value) {
      stack.set(SettingLayer::project, name, *value);
    }
  };
  put(kVdd, project.nets.vdd);
  put(kGnd, project.nets.gnd);
  put(kSub, project.nets.sub);
}

void apply_effective(const SettingsStack& stack, settings::ProjectSettings& typed) {
  for (const auto name : settings::setting_names()) {
    if (const auto effective = stack.effective(name)) {
      (void)settings::set(typed, name, effective->value);
    }
  }
}

std::vector<std::pair<std::string, std::string>> startup_file_settings(std::string_view text) {
  std::vector<std::pair<std::string, std::string>> out;
  std::vector<std::string> globals;
  int depth = 0;
  while (!text.empty()) {
    const auto newline = text.find('\n');
    std::string_view line = text.substr(0, newline);
    text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
    if (depth == 0) {
      if (const auto set = parse_set(line); set && set->name == "globals") {
        globals.push_back(set->value);
      } else if (set) {
        out.emplace_back(set->name, set->value);
      }
    }
    depth = std::max(0, depth + brace_delta(line));
  }
  if (!globals.empty()) {
    out.emplace_back(std::string{kGlobalNets}, join_sorted(std::move(globals)));
  }
  return out;
}

diag::Status load_preferences(platform::FileSystem& fs, const platform::Path& path, SettingsStack& stack) {
  if (!fs.exists(path)) {
    return diag::Done{};
  }
  auto text = fs.read(path);
  if (!text) {
    return diag::make(codes::kSettingsFile, "cannot read " + path.string() + ": " + text.error().text);
  }
  const auto doc = nlohmann::json::parse(text.value(), nullptr, false);
  if (doc.is_discarded() || !doc.is_object()) {
    return diag::make(codes::kSettingsFile, path.string() + ": not a JSON object");
  }
  std::map<std::string, std::string> values;
  for (const auto& [name, value] : doc.items()) {
    if (!value.is_string()) {
      return diag::make(codes::kSettingsFile, path.string() + ": setting " + name + " is not a string");
    }
    values[name] = value.get<std::string>();
  }
  stack.clear_layer(SettingLayer::user);
  for (const auto& [name, value] : values) {
    stack.set(SettingLayer::user, name, value);
  }
  return diag::Done{};
}

diag::Status save_preferences(platform::FileSystem& fs, const platform::Path& path, const SettingsStack& stack) {
  nlohmann::json doc = nlohmann::json::object();
  for (const auto& name : stack.names()) {
    if (const auto value = stack.value_in(SettingLayer::user, name)) {
      doc[name] = *value;
    }
  }
  return fs.write_atomic(path, doc.dump(2) + "\n");
}

}  // namespace hefesto::app
