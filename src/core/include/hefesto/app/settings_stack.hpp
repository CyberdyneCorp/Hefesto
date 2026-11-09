// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layered settings (capability hefesto-settings "Layered settings
// resolution", decision X-4): built-in defaults < PDK startup data < user
// preferences < project < command line / session. Each effective value
// reports the layer that supplied it. The M0 project settings
// (settings::ProjectSettings: magic-compat, netgen-compat, script-strict)
// are the project layer's typed core and stay the source of truth for them.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::app {

enum class SettingLayer : std::uint8_t { builtin, pdk, user, project, session };

inline constexpr std::size_t kSettingLayerCount = 5;

[[nodiscard]] std::string_view layer_name(SettingLayer layer) noexcept;

/// A setting's effective value and where it came from.
struct EffectiveSetting {
  std::string value;
  SettingLayer layer = SettingLayer::builtin;
};

/// String-valued settings keyed by name, one map per layer. Typed access
/// (units, snap, theme, density, ...) is layered on top by the owners of
/// each setting; names are the spec names (e.g. "units", "magic-compat").
class SettingsStack {
 public:
  SettingsStack();

  void set(SettingLayer layer, std::string_view name, std::string value);
  void clear(SettingLayer layer, std::string_view name);
  void clear_layer(SettingLayer layer);

  /// Highest-precedence value of `name`; std::nullopt if no layer has it.
  [[nodiscard]] std::optional<EffectiveSetting> effective(std::string_view name) const;
  /// The value `name` has in one layer, if any.
  [[nodiscard]] std::optional<std::string> value_in(SettingLayer layer, std::string_view name) const;
  /// Every name with a value in any layer, sorted.
  [[nodiscard]] std::vector<std::string> names() const;

 private:
  std::array<std::map<std::string, std::string, std::less<>>, kSettingLayerCount> layers_;
};

}  // namespace hefesto::app
