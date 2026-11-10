// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DbRecorder: the db::ChangeRecorder that turns database changes into undo
// events of an UndoLog (layout-undo "Recorded event kinds"; design.md D4/D5).
// A session installs one with db::CellLibrary::set_recorder.
//
//   - Tile changes: consecutive changes of one cell form one paint event
//     (Magic's "open cell"/"close cell" interleaving: a change in another
//     cell starts a new event). Undo paints the old type back over each
//     recorded rectangle (or triangle), newest first, without image
//     resolution (Magic's dbUndoPaintBack); redo paints the new type.
//   - Split fracture/join: recorded for `undo print`; replaying them is a
//     no-op because Hefesto's planes re-merge split tiles on every paint,
//     so they never change the geometry.
//   - Labels, uses (place/delete, identifier, lock): full contents, so
//     replay re-creates them exactly. An identifier change is one event
//     (Magic's "clear id" + "set id" pair).
//   - The modified flag is not recorded: every replay marks the cells it
//     changes modified (Q-MODEL-12).
//   - Property changes are recorded only with magic-compat OFF (D-10).
// Nothing is recorded while the log is suspended or replaying.
//
// Owner: UNDO (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/change_recorder.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::undo {

class DbRecorder final : public db::ChangeRecorder {
 public:
  explicit DbRecorder(UndoLog& log) : log_(log) {}

  /// magic-compat (D-10): ON records no property change. A compat source,
  /// when set, is read at each change instead (the project's live
  /// settings::ProjectSettings::compat), so `setting set magic-compat`
  /// takes effect at once.
  void set_magic_compat(bool on) noexcept { magic_compat_ = on; }
  void set_compat_source(const compat::CompatSettings* source) noexcept { compat_source_ = source; }

  void tile_changed(db::CellDef& def, const db::TileChange& change) override;
  void split_changed(db::CellDef& def, const db::SplitChange& change) override;
  void label_put(db::CellDef& def, const db::Label& label) override;
  void label_erased(db::CellDef& def, const db::Label& label) override;
  void use_placed(db::CellDef& parent, const db::CellUse& use) override;
  void use_deleted(db::CellDef& parent, const db::CellUse& use) override;
  void use_id_cleared(db::CellDef& parent, const db::CellUse& use, std::string_view old_id) override;
  void use_id_set(db::CellDef& parent, const db::CellUse& use, std::string_view new_id) override;
  void use_locked(db::CellDef& parent, const db::CellUse& use, bool locked) override;
  void modified_changed(db::CellDef& def, bool was_modified) override;
  void property_changed(db::CellDef& def, std::string_view name, const std::optional<db::PropertyValue>& old_value,
                        const std::optional<db::PropertyValue>& new_value) override;

  /// A cell definition is about to be destroyed (db::CellLibrary::remove):
  /// clears the history when it holds events of that cell ("Non-recorded
  /// operations").
  void cell_removed(db::CellDef& def) override;

  [[nodiscard]] UndoLog& log() noexcept { return log_; }
  [[nodiscard]] bool magic_compat() const noexcept {
    return compat_source_ != nullptr ? compat_source_->magic_compat : magic_compat_;
  }

 private:
  UndoLog& log_;
  bool magic_compat_ = true;
  const compat::CompatSettings* compat_source_ = nullptr;
  // use_id_cleared waits for its use_id_set to make one rename event.
  const db::CellDef* pending_parent_ = nullptr;
  std::string pending_old_id_;
};

}  // namespace hefesto::undo
