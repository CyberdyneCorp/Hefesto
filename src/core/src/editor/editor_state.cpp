// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// editor::EditorState construction. Shared file owned by INTEGRATION.

#include "hefesto/editor/editor_state.hpp"

#include "hefesto/cell/io_safety.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/undo/db_recorder.hpp"

namespace hefesto::editor {

EditorState::EditorState()
    : technology(tech::builtin_technology()),
      locks(cell::make_in_process_lock_service()),
      undo_recorder(std::make_unique<undo::DbRecorder>(undo)) {
  cells.set_recorder(undo_recorder.get());
}

EditorState::~EditorState() {
  cells.set_recorder(nullptr);  // the recorder dies before the cells
}

}  // namespace hefesto::editor
