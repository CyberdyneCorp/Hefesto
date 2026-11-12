// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/platform/file_picker.hpp"

namespace hefesto::platform {

std::optional<Path> ScriptedFilePicker::pick(const FilePickerRequest& request) {
  requests_.push_back(request);
  if (answers_.empty()) {
    return std::nullopt;
  }
  std::optional<Path> answer = std::move(answers_.front());
  answers_.pop_front();
  return answer;
}

}  // namespace hefesto::platform
