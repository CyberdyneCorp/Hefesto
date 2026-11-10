// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `units` state and command (capability cmd-coordinates-and-units,
// requirement "Units command"; decision Q-CMDEDIT-6). Since M2 the units are
// editor state (editor::EditorState::units), not a settings-layer key: a
// project file that stored the key keeps loading (it is read once at startup
// and then ignored; design.md "Migration Plan").
// (Magic ref: commands/CmdTZ.c:862-1004, dbwind/dbwind.h:179-205)
//
// Owner: EDIT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstdint>
#include <string_view>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::cmd {

/// The unit of un-suffixed input and of reported values. `default_units`:
/// input follows `snap` and reports are plain internal-unit integers.
/// `user` is also reached through the `grid` synonym.
enum class InputUnit : std::uint8_t { default_units, internal, lambda, user, microns };

struct UnitsState {
  InputUnit unit = InputUnit::default_units;
  /// Reported values carry a unit suffix ("Reported value formatting");
  /// never set while `unit` is default_units.
  bool print = false;

  friend bool operator==(const UnitsState&, const UnitsState&) = default;
};

/// The name `units` reports for `unit` (`default`, `internal`, `lambda`,
/// `user`, `microns`).
[[nodiscard]] std::string_view units_name(InputUnit unit) noexcept;

/// Registers `units`. Returns false if the name was taken.
bool register_units_commands(CommandRegistry& registry);

}  // namespace hefesto::cmd
