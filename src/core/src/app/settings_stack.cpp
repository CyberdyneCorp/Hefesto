// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layered settings (hefesto-settings "Layered settings resolution").

#include "hefesto/app/settings_stack.hpp"

#include <algorithm>

namespace hefesto::app {

std::string_view layer_name(SettingLayer layer) noexcept {
  switch (layer) {
    case SettingLayer::builtin:
      return "builtin";
    case SettingLayer::pdk:
      return "pdk";
    case SettingLayer::user:
      return "user";
    case SettingLayer::project:
      return "project";
    case SettingLayer::session:
      return "session";
  }
  return "builtin";
}

SettingsStack::SettingsStack() = default;

void SettingsStack::set(SettingLayer layer, std::string_view name, std::string value) {
  layers_.at(static_cast<std::size_t>(layer)).insert_or_assign(std::string{name}, std::move(value));
}

void SettingsStack::clear(SettingLayer layer, std::string_view name) {
  auto& map = layers_.at(static_cast<std::size_t>(layer));
  if (const auto it = map.find(name); it != map.end()) {
    map.erase(it);
  }
}

void SettingsStack::clear_layer(SettingLayer layer) {
  layers_.at(static_cast<std::size_t>(layer)).clear();
}

std::optional<EffectiveSetting> SettingsStack::effective(std::string_view name) const {
  for (std::size_t i = kSettingLayerCount; i-- > 0;) {
    if (const auto it = layers_[i].find(name); it != layers_[i].end()) {
      return EffectiveSetting{it->second, static_cast<SettingLayer>(i)};
    }
  }
  return std::nullopt;
}

std::optional<std::string> SettingsStack::value_in(SettingLayer layer, std::string_view name) const {
  const auto& map = layers_.at(static_cast<std::size_t>(layer));
  if (const auto it = map.find(name); it != map.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<std::string> SettingsStack::names() const {
  std::vector<std::string> out;
  for (const auto& layer : layers_) {
    for (const auto& entry : layer) {
      out.push_back(entry.first);
    }
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

}  // namespace hefesto::app
