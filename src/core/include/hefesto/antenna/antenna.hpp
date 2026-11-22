// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The antenna check and the shared antenna rules (capability antenna-check;
// openspec/changes/m7-extraction/design.md D5, D17). RuleView is the view
// of an ExtStyle's antenna rules and tie-down types that `antennacheck` and
// `lef write` (ANTENNAGATEAREA, ANTENNADIFFAREA) read. check() is the
// command's run path: EFantenna.c over EXTFLAT's flattened devices and
// connect::search_hierarchical. The command matches its first word with
// Magic's table and the first-word rule of netlist/first_word.hpp.
//
// Scaffold state (tasks.md 3.3, 3.5): RuleView answers the style's rules;
// check() and every option but `help` fail with CORE-NOTIMPL-008 until
// ANTENNA lands them.
//
// Owner: INTEGRATION (this header and the registration); ANTENNA (the
// implementation).

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::db {
class CellDef;
}
namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::antenna {

/// The antenna rules of one extraction style.
class RuleView {
 public:
  RuleView() = default;
  [[nodiscard]] static RuleView from_style(const extract::ExtStyle& style);

  /// The rule of `type`, nullopt when the style declares none.
  [[nodiscard]] std::optional<extract::AntennaRule> rules(tech::TileType type) const;
  /// The `tiedown` types.
  [[nodiscard]] tech::TypeMask tiedown_types() const noexcept { return tiedown_; }

 private:
  std::vector<extract::AntennaRule> rules_;
  tech::TypeMask tiedown_;
};

/// One run's options.
struct AntennaOptions {
  std::vector<std::string> args;  // the run arguments (EFArgs)
  bool debug = false;             // armed by `antennacheck debug`
};

[[nodiscard]] cmd::CommandResult check(editor::EditorState& editor, db::CellDef& root,
                                       const AntennaOptions& options);

/// cmdAntennaCheckOption, in Magic's order: [run], debug, help.
[[nodiscard]] std::span<const std::string_view> antennacheck_options() noexcept;

/// Registers `antennacheck` in the layout client.
bool register_antenna_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::antenna
