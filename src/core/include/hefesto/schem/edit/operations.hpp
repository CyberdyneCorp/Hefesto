// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Editing operations behind the `schematic` command
// (openspec/changes/m8-schematic design.md D11, "Area contracts" EDIT;
// capability schematic-editing "Document model": every editing operation
// goes through the command API so that it is scripted, logged and
// replayed, one undo step each).
//
// EDIT implements one handler per editing option of schematic-command
// "Subcommand table"; CMD's `schematic` dispatcher looks options up here.
// A handler reaches the session through CommandContext::editor()->schem.
//
// Owner: EDIT.

#pragma once

#include <span>
#include <string_view>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::schem {

struct EditOperation {
  std::string_view option;  // the `schematic` option, e.g. "rotate"
  cmd::Handler handler;
};

/// The editing operations, sorted by option name.
[[nodiscard]] std::span<const EditOperation> edit_operations();

}  // namespace hefesto::schem
