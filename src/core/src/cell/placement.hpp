// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the CELL commands: the placement reference syntax of
// `getcell`/`get` and `dump` (cmd-hierarchy "Placement reference syntax for
// getcell and dump"; Magic's cmdDumpParseArgs after the cell is read), and
// `flatten` registration. Owner: HIER.

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "db/command_util.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::cell::detail {

/// The usage text cmdDumpParseArgs prints for `command`.
[[nodiscard]] std::string placement_usage(std::string_view command);

/// The transform from `child`'s coordinates to root coordinates that
/// `inv.args[1..]` (`[child REF] [parent REF] [ORIENT [REF]]`) describe,
/// relative to the box lower-left corner by default.
[[nodiscard]] diag::Outcome<db::Transform> parse_placement(const db::cmdutil::Session& s, const db::CellDef& child,
                                                           const cmd::Invocation& inv);

/// The location of the label `name` (an instance path may precede it) in
/// `def`: its rectangle's lower-left corner in `def`'s coordinates
/// (DBSrLabelLoc + cmdDumpFunc). nullopt when there is no such label.
[[nodiscard]] std::optional<db::Point> label_location(const db::CellDef& def, std::string_view name);

/// `flatten` (flatten_command.cpp).
bool register_flatten_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::cell::detail
