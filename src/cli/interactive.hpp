// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-cli's interactive prompt (m3-drc design D8 "Hosts"): commands
// are read line by line from a terminal (or any descriptor) and the
// background DRC checker runs while the prompt waits for input, as Magic
// runs DRCContinuous from its idle loop (textio/txCommands.c
// TxDispatch: "Give the DRC a chance to work"). Scripts and -c commands
// never get here: the CLI runs no worker in batch mode.
//
// The prompt is Magic's console prompt (drc::BackgroundHost::prompt_char):
// "% " when idle, "] " while the checker is busy; a busy/idle transition
// while waiting rewrites it in place (TxUnPrompt's backspaces, then
// TxPrompt). Each complete command is Tcl, evaluated in the project's
// interpreter (m4-scripting tasks.md 13.2): a line that leaves a brace
// group, a quote, a bracket or a backslash-newline open continues on the
// next line (`info complete`).

#pragma once

#include <cstdio>
#include <functional>

#include "tcl_lines.hpp"

namespace hefesto::cmd {
class Dispatcher;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cli {

struct InteractiveRun {
  bool ok = true;     // no command failed
  bool quit = false;  // ended by `quit` (otherwise by end of input)
};

/// Evaluates one complete command typed at the prompt and prints its result
/// or error; false when it raised a Tcl error.
using ChunkEvaluator = std::function<bool(const TclChunk&)>;

/// Runs the interactive prompt on `input_fd` until end of input or `quit`.
/// The prompt goes to `prompt_out` when `show_prompt` is set. Each complete
/// command goes to `evaluate`; without one, it is evaluated in
/// `editor.script.interp` (which must be open) and its result dropped.
/// Hosts the background checker over `editor` for its duration.
[[nodiscard]] InteractiveRun run_interactive(cmd::Dispatcher& dispatcher, editor::EditorState& editor, int input_fd,
                                             std::FILE* prompt_out, bool show_prompt, ChunkEvaluator evaluate = {});

}  // namespace hefesto::cli
