// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The user key-binding file (design.md D7): Hefesto `macro`/`imacro`
// command lines, read with the script reader's quoting rules (so `.magicrc`
// macro lines work unchanged). Any other command is an error. A libFuzzer
// target (`fuzz_macro_file`) exercises read_macro_file().
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::ui {

struct MacroFileResult {
  std::size_t applied = 0;  // macro/imacro lines run
  /// Parse errors, non-macro lines (UI-USAGE-001) and the diagnostics the
  /// macro lines produced, in file order.
  std::vector<diag::Diagnostic> diagnostics;
};

/// Reads a key-binding file and applies its lines to `editor`'s macro
/// tables. `source_name` labels diagnostics. Never throws.
MacroFileResult read_macro_file(editor::EditorState& editor, std::string_view text,
                                std::string_view source_name = {});

}  // namespace hefesto::ui
