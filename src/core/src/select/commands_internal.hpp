// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private declarations shared by the select area's command files. Owner:
// SELECT.

#pragma once

#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "select/ops.hpp"

namespace hefesto::select {

/// A failed command with one diagnostic.
[[nodiscard]] inline cmd::CommandResult fail_text(std::string_view code, std::string text) {
  return cmd::CommandResult::failure(diag::make(code, std::move(text)));
}

/// `select` (select_command.cpp).
[[nodiscard]] cmd::CommandResult cmd_select(cmd::CommandContext& ctx, const cmd::Invocation& inv);
/// `select short NAME1 NAME2` (short.cpp).
[[nodiscard]] cmd::CommandResult select_short(const ops::Context& c, const std::string& name1,
                                              const std::string& name2);
/// `what` and `identify`/`id` (what.cpp).
[[nodiscard]] cmd::CommandResult cmd_what(cmd::CommandContext& ctx, const cmd::Invocation& inv);
[[nodiscard]] cmd::CommandResult cmd_identify(cmd::CommandContext& ctx, const cmd::Invocation& inv);

}  // namespace hefesto::select
