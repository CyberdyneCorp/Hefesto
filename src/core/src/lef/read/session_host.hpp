// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The editor side of a LEF read (private to lef/read; capability lef-read
// "MACRO and SITE cell creation", layout-undo "Undoable LEF and DEF reads";
// design D6, Q-LEFDEF-22 platform):
// - SessionMacroHost: lef::MacroHost over the editor session. It creates
//   the macro cells, redisplays each macro and queues the DRC checks, and
//   keeps the state of every cell it creates or annotates;
// - the read's undo step, one native event built from that state: undo
//   removes the created cells and restores the annotated ones, redo puts
//   them back. The event names cells, not pointers, so a cell deleted or
//   re-created since is found again or skipped.
//
// Owner: LEFR (design D2).

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/lef/read/macro.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::lef::detail {

class SessionMacroHost final : public MacroHost {
 public:
  explicit SessionMacroHost(editor::EditorState& editor);
  ~SessionMacroHost() override;

  [[nodiscard]] db::CellDef* find(std::string_view name) override;
  [[nodiscard]] db::CellDef* create(std::string_view name) override;
  void annotating(db::CellDef& cell) override;
  void finished(db::CellDef& cell, std::optional<db::Rect> drc_area) override;

  /// The read's undo step (nullptr when the read changed no cell), named
  /// `description`. Call once, after the read.
  [[nodiscard]] std::unique_ptr<undo::Event> take_event(std::string description);

 private:
  struct Kept;

  editor::EditorState& editor_;
  std::vector<std::string> created_;
  std::vector<std::unique_ptr<Kept>> annotated_;
};

}  // namespace hefesto::lef::detail
