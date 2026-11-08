// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/settings/project_settings.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <utility>

#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::settings {

namespace {

namespace codes = diag::codes;
using Json = nlohmann::ordered_json;

constexpr std::array<std::string_view, 3> kNames = {kMagicCompat, kNetgenCompat, kScriptStrict};

// The one place that maps a setting name to its field.
bool* field(ProjectSettings& settings, std::string_view name) noexcept {
  if (name == kMagicCompat) {
    return &settings.compat.magic_compat;
  }
  if (name == kNetgenCompat) {
    return &settings.compat.netgen_compat;
  }
  if (name == kScriptStrict) {
    return &settings.script_strict;
  }
  return nullptr;
}

diag::Diagnostic unknown_name(std::string_view name) {
  return diag::make(codes::kSettingsName, "unknown project setting '" + std::string{name} +
                                              "' (known: magic-compat, netgen-compat, script-strict)");
}

diag::Diagnostic file_error(std::string_view source_name, std::string text) {
  diag::Location location;
  if (!source_name.empty()) {
    location = diag::SourceLocation{std::string{source_name}, 0};
  }
  return diag::make(codes::kSettingsFile, std::move(text), std::move(location));
}

}  // namespace

std::span<const std::string_view> setting_names() noexcept {
  return kNames;
}

std::optional<bool> parse_bool(std::string_view text) noexcept {
  constexpr std::array<std::string_view, 4> kTrue = {"on", "true", "yes", "1"};
  constexpr std::array<std::string_view, 4> kFalse = {"off", "false", "no", "0"};
  for (std::size_t i = 0; i < kTrue.size(); ++i) {
    if (text == kTrue[i]) {
      return true;
    }
    if (text == kFalse[i]) {
      return false;
    }
  }
  return std::nullopt;
}

diag::Outcome<bool> get(const ProjectSettings& settings, std::string_view name) {
  ProjectSettings copy = settings;
  const bool* value = field(copy, name);
  if (value == nullptr) {
    return unknown_name(name);
  }
  return *value;
}

diag::Status set(ProjectSettings& settings, std::string_view name, std::string_view value) {
  bool* target = field(settings, name);
  if (target == nullptr) {
    return unknown_name(name);
  }
  const auto parsed = parse_bool(value);
  if (!parsed) {
    return diag::make(codes::kSettingsValue,
                      "setting '" + std::string{name} + "' expects on or off, got '" + std::string{value} + "'");
  }
  *target = *parsed;
  return diag::Done{};
}

std::string to_json(const ProjectSettings& settings) {
  ProjectSettings copy = settings;
  Json j = Json::object();
  for (const auto name : kNames) {
    j[std::string{name}] = *field(copy, name);
  }
  return j.dump(2) + "\n";
}

diag::Outcome<ProjectSettings> from_json(std::string_view text, std::string_view source_name) {
  const Json j = Json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded() || !j.is_object()) {
    return file_error(source_name, "project settings must be a JSON object");
  }
  ProjectSettings result;
  for (const auto& [key, value] : j.items()) {
    bool* target = field(result, key);
    if (target == nullptr || !value.is_boolean()) {
      return file_error(source_name, "invalid project setting '" + key + "'");
    }
    *target = value.get<bool>();
  }
  return result;
}

diag::Outcome<ProjectSettings> load(platform::FileSystem& fs, const std::filesystem::path& path) {
  auto text = fs.read(path);
  if (!text) {
    return text.error();
  }
  return from_json(text.value(), path.generic_string());
}

diag::Status save(platform::FileSystem& fs, const std::filesystem::path& path, const ProjectSettings& settings) {
  return fs.write_atomic(path, to_json(settings));
}

}  // namespace hefesto::settings
