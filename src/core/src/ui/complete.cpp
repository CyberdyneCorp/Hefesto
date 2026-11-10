// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Name completion (hefesto-input-and-touch "Command entry on every
// platform"). Owner: DISPATCH.

#include "hefesto/ui/complete.hpp"

#include <algorithm>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::ui {

namespace {

void keep_matching(std::vector<std::string>& out, std::string_view prefix, std::string name) {
  if (std::string_view{name}.starts_with(prefix)) {
    out.push_back(std::move(name));
  }
}

std::vector<std::string> command_names(std::string_view prefix, const cmd::CommandRegistry& registry) {
  std::vector<std::string> out;
  for (std::string& name : registry.names()) {
    // Wizard commands only when asked for (help hides them too).
    if (!name.starts_with('*') || prefix.starts_with('*')) {
      keep_matching(out, prefix, std::move(name));
    }
  }
  return out;
}

std::vector<std::string> cell_names(std::string_view prefix, const editor::EditorState& editor) {
  std::vector<std::string> out;
  editor.cells.for_each([&](db::CellDef& def) {
    if (!def.flags().internal) {
      keep_matching(out, prefix, def.name());
    }
  });
  return out;
}

std::vector<std::string> layer_names(std::string_view prefix, const editor::EditorState& editor) {
  std::vector<std::string> out;
  const tech::Technology& tech = *editor.technology;
  for (std::size_t t = 1; t < tech.type_count(); ++t) {
    for (std::string& name : tech.type_names(static_cast<tech::TileType>(t), true)) {
      keep_matching(out, prefix, std::move(name));
    }
  }
  for (auto& [name, mask] : tech.aliases()) {
    keep_matching(out, prefix, name);
  }
  return out;
}

}  // namespace

diag::Outcome<std::vector<std::string>> complete(std::string_view prefix, const CompletionContext& context) {
  std::vector<std::string> out;
  if (context.kind == CompletionKind::command) {
    if (context.registry == nullptr) {
      return diag::make(codes::kUsage, "command completion needs a command registry");
    }
    out = command_names(prefix, *context.registry);
  } else {
    if (context.editor == nullptr) {
      return diag::make(codes::kUsage, "cell and layer completion need an editor session");
    }
    out = context.kind == CompletionKind::cell ? cell_names(prefix, *context.editor)
                                               : layer_names(prefix, *context.editor);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

}  // namespace hefesto::ui
