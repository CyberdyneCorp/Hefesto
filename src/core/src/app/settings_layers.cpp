// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Settings layers (hefesto-settings).

#include "hefesto/app/settings_layers.hpp"

#include <algorithm>
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
