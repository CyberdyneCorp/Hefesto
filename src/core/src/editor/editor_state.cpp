// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// editor::EditorState construction. Shared file owned by INTEGRATION.

#include "hefesto/editor/editor_state.hpp"

#include "hefesto/cell/io_safety.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::editor {

EditorState::EditorState() : technology(tech::builtin_technology()), locks(cell::make_in_process_lock_service()) {}

EditorState::~EditorState() = default;

}  // namespace hefesto::editor
