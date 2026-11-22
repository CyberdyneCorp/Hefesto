// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of the extraction run driver (extract/command/run.hpp):
// CORE-NOTIMPL-008 until EXTCMD lands them (openspec/changes/m7-extraction
// tasks.md 3.5, EXTCMD). EXTCMD replaces this file with its own sources.
//
// Owner: EXTCMD (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extract/command/run.hpp"

namespace hefesto::extract {

cmd::CommandResult run(editor::EditorState& /*editor*/, const RunRequest& /*request*/,
                       const mask::Progress* /*progress*/) {
  return cmd::m7_not_implemented("extract run");
}

diag::Outcome<RunReport> ensure_extracted(editor::EditorState& /*editor*/, std::string_view /*cell*/,
                                          const mask::Progress* /*progress*/) {
  return cmd::m7_not_implemented_diagnostic("extract ensure_extracted");
}

}  // namespace hefesto::extract
