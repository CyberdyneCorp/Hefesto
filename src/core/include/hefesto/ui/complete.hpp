// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Completion of command, cell and layer names for every command-entry front
// end (capability hefesto-input-and-touch, requirement "Command entry on
// every platform"; design.md D7). The desktop command bar's Tab completion
// and the iPad keyboard (M14) share it.
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).
// Fails with UI-USAGE-001 when the context lacks what the kind needs.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::ui {

enum class CompletionKind : std::uint8_t { command, cell, layer };

struct CompletionContext {
  CompletionKind kind = CompletionKind::command;
  const cmd::CommandRegistry* registry = nullptr;   // command names
  const editor::EditorState* editor = nullptr;      // cell and layer names
};

/// The names starting with `prefix`, sorted, without duplicates.
[[nodiscard]] diag::Outcome<std::vector<std::string>> complete(std::string_view prefix,
                                                               const CompletionContext& context);

}  // namespace hefesto::ui
