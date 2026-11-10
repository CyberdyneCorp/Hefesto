// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The post-command update (capability ui-command-dispatch, requirement
// "Command counter and post-command update"; design.md D4). After each
// dispatched command:
//   1. increment editor::EditorState::command_count, except for `*bypass`
//      and Origin::internal commands;
//   2. close the open undo step (undo::UndoLog::close_step);
//   3. run every client's update handler (ui::run_client_updates);
//   4. redraw the highlights (view::HighlightLayer::redraw(editor): runs the
//      clients, builds each window's highlight frame, applies the `*watch`
//      full-redraw rule; design.md D6);
//   5. perform the pending window redisplay (WindowManager::update_display).
// The dispatcher calls it after every command of every front end.
// (Magic ref: textio/txCommands.c:1491-1594, dbwind/DBWprocs.c:984-1010)
//
// Owner: INTEGRATION (shared).

#pragma once

#include "hefesto/cmd/command.hpp"

namespace hefesto::editor {

struct EditorState;

/// Runs the post-command update for `invocation` (whatever its result).
void post_command(EditorState& editor, const cmd::Invocation& invocation);

}  // namespace hefesto::editor
