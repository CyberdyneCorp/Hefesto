// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ReadEngine: the one read engine behind both parsers (capability
// mask-input-styles; openspec/changes/m5-mask-gds-cif/design.md D9; Magic's
// CIFrdcl.c, CIFrdutils.c, CIFrdpt.c, CIFrdpoly.c). It keeps per-cell read
// planes and the scale, rescales mid-read (CIFScalePlanes,
// CIFInputRescale), paints the current cell through evaluate_layer
// (CIFPaintCurrent), and handles boundary, mask-hints, tagged and copyup.
// The read holds the exclusive database lock; undo is disabled for it.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); INPUT fills
// in the implementation in src/core/src/mask/input/engine.cpp (design D2).

#pragma once

#include <memory>
#include <string_view>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/mask/input/sink.hpp"
#include "hefesto/mask/input/style.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::mask {

class ReadEngine final : public ReadSink {
 public:
  /// A read into `editor`'s cell library with the input style `style`.
  /// Both must outlive the engine.
  ReadEngine(editor::EditorState& editor, const InputStyle& style, const compat::CompatSettings& compat);
  ~ReadEngine() override;

  [[nodiscard]] diag::Status begin_cell(std::string_view name) override;
  [[nodiscard]] diag::Status paint(std::size_t cif_layer, const ReadShape& shape) override;
  [[nodiscard]] diag::Status label(const ReadLabel& label) override;
  [[nodiscard]] diag::Status call(std::string_view child, const db::Transform& transform,
                                  const db::ArrayInfo& array) override;
  [[nodiscard]] diag::Status property(std::string_view key, std::string_view value) override;
  [[nodiscard]] diag::Status end_cell() override;

  /// Ends the read: paints pending planes, applies copyup and cleans up.
  [[nodiscard]] diag::Status finish();

 private:
  struct State;
  editor::EditorState& editor_;
  const InputStyle& style_;
  compat::CompatSettings compat_;
  std::unique_ptr<State> state_;
};

}  // namespace hefesto::mask
