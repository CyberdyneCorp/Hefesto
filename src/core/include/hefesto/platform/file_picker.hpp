// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// platform::FilePicker: open/save file requests that the core cannot answer
// itself (openspec/changes/m4-scripting/design.md D4): `tk_getOpenFile` in
// an intercepted PDK menu callback (Q-SCRIPT-42), the file prompts of the
// GUI managers (ui-gui-managers "File prompts"). The desktop shell answers
// with the native dialog (SHELL), tests with ScriptedFilePicker.
//
// Owner: INTEGRATION (shared interface); SHELL implements the desktop host.

#pragma once

#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/platform/file_system.hpp"

namespace hefesto::platform {

struct FilePickerRequest {
  enum class Mode : unsigned char { open, save };
  Mode mode = Mode::open;
  std::string title;
  Path initial_directory;  // empty: the host's choice
  std::string initial_file;
  /// (description, patterns) pairs, e.g. {"SPICE netlists", "*.spice *.sp *.cir"}.
  std::vector<std::pair<std::string, std::string>> file_types;

  friend bool operator==(const FilePickerRequest&, const FilePickerRequest&) = default;
};

class FilePicker {
 public:
  FilePicker() = default;
  FilePicker(const FilePicker&) = delete;
  FilePicker& operator=(const FilePicker&) = delete;
  virtual ~FilePicker() = default;

  /// The chosen file, or nullopt when the user cancelled.
  [[nodiscard]] virtual std::optional<Path> pick(const FilePickerRequest& request) = 0;
};

/// A test host: answers from a queue (empty queue = cancel) and records
/// every request.
class ScriptedFilePicker final : public FilePicker {
 public:
  void push_answer(std::optional<Path> answer) { answers_.push_back(std::move(answer)); }
  [[nodiscard]] std::optional<Path> pick(const FilePickerRequest& request) override;
  [[nodiscard]] const std::vector<FilePickerRequest>& requests() const noexcept { return requests_; }

 private:
  std::deque<std::optional<Path>> answers_;
  std::vector<FilePickerRequest> requests_;
};

}  // namespace hefesto::platform
