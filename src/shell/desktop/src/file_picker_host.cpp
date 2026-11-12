// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop FilePicker host (see file_picker_host.hpp).

#include "hefesto/shell/file_picker_host.hpp"

#include <algorithm>
#include <utility>

namespace hefesto::shell {

namespace {

constexpr std::string_view kAllFiles = "All files";

// "*.spice *.sp" → "*.spice;*.sp" (Tk's list of patterns → Raven's).
std::string native_patterns(std::string_view patterns) {
  std::string out;
  std::size_t start = 0;
  while (start < patterns.size()) {
    const std::size_t end = std::min(patterns.find_first_of(" \t;", start), patterns.size());
    if (end > start) {
      if (!out.empty()) out += ';';
      out += patterns.substr(start, end - start);
    }
    start = end + 1;
  }
  return out.empty() ? std::string{"*"} : out;
}

// The extension of a pattern "*.mag" → "mag"; empty for "*" or "*.*".
std::string pattern_extension(std::string_view patterns) {
  const std::string_view first = patterns.substr(0, patterns.find(';'));
  if (!first.starts_with("*.") || first.size() <= 2 || first.find_first_of("*?[", 2) != std::string_view::npos) {
    return {};
  }
  return std::string{first.substr(2)};
}

}  // namespace

NativeFileDialogRequest native_request(const platform::FilePickerRequest& request) {
  NativeFileDialogRequest out;
  out.save = request.mode == platform::FilePickerRequest::Mode::save;
  out.title = request.title;
  out.directory = request.initial_directory.empty() ? std::string{} : request.initial_directory.string();
  out.file_name = request.initial_file;
  for (const auto& [name, patterns] : request.file_types) {
    out.filters.push_back(NativeFileFilter{name, native_patterns(patterns)});
  }
  const bool has_all = std::ranges::any_of(out.filters, [](const NativeFileFilter& f) { return f.patterns == "*"; });
  if (!has_all) out.filters.push_back(NativeFileFilter{std::string{kAllFiles}, "*"});
  if (out.save && !out.filters.empty()) out.default_extension = pattern_extension(out.filters.front().patterns);
  return out;
}

std::optional<platform::Path> DesktopFilePicker::pick(const platform::FilePickerRequest& request) {
  ++asked_;
  if (!dialog_) return std::nullopt;
  const auto chosen = dialog_(native_request(request));
  if (!chosen || chosen->empty()) return std::nullopt;
  return platform::Path{*chosen};
}

}  // namespace hefesto::shell
