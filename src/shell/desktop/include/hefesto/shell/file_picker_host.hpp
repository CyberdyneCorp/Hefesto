// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop host of platform::FilePicker (openspec/changes/m4-scripting
// design.md D4; capability ui-gui-managers "File prompts"; PDKINT's
// `tk_getOpenFile` interception, Q-SCRIPT-42): `promptload`, `promptsave`
// and intercepted PDK menu callbacks ask through
// editor::EditorState::file_picker, and the shell answers with the
// platform's native open or save dialog (Raven's Rfile_dialog_open /
// Rfile_dialog_save, as the M1 Open button). Toolkit-free: the dialog is a
// NativeFileDialog function, so the request mapping is unit-tested.
//
// Owner: SHELL (task 10.5).

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/platform/file_picker.hpp"

namespace hefesto::shell {

/// One filter of a native file dialog: a name and ';'-separated patterns.
struct NativeFileFilter {
  std::string name;
  std::string patterns;  // "*.spice;*.sp"

  friend bool operator==(const NativeFileFilter&, const NativeFileFilter&) = default;
};

/// What the shell asks the native dialog.
struct NativeFileDialogRequest {
  bool save = false;
  std::string title;
  std::string directory;  // empty: the dialog's own choice
  std::string file_name;  // save dialogs
  std::vector<NativeFileFilter> filters;  // ends with "All files" (*)
  std::string default_extension;          // save: added to a name typed without one

  friend bool operator==(const NativeFileDialogRequest&, const NativeFileDialogRequest&) = default;
};

/// The native dialog request for a FilePicker request: space-separated
/// patterns become ';'-separated, "All files" is added once, and a save
/// dialog proposes `initial_file` with the first filter's extension as
/// default (`*.mag` → "mag").
[[nodiscard]] NativeFileDialogRequest native_request(const platform::FilePickerRequest& request);

class DesktopFilePicker final : public platform::FilePicker {
 public:
  /// Shows the dialog; nullopt when the user cancelled.
  using NativeFileDialog = std::function<std::optional<std::string>(const NativeFileDialogRequest&)>;

  explicit DesktopFilePicker(NativeFileDialog dialog) : dialog_(std::move(dialog)) {}

  [[nodiscard]] std::optional<platform::Path> pick(const platform::FilePickerRequest& request) override;
  /// Requests answered so far (diagnostics, tests).
  [[nodiscard]] std::size_t asked() const noexcept { return asked_; }

 private:
  NativeFileDialog dialog_;
  std::size_t asked_ = 0;
};

}  // namespace hefesto::shell
