// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The auxiliary scripting tools (see auxiliary.hpp).
// (Magic ref: tcltk/strip_reflibs.tcl:21-41, tcltk/toolkit_rev0.tcl,
// tcltk/console.tcl, tcltk/magic.sh.in, tcltk/magicdnull.c)

#include "hefesto/tools/auxiliary.hpp"

#include <algorithm>
#include <array>

#include "hefesto/tools/diag_codes.hpp"

namespace hefesto::tools {

namespace {

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}

// socketcmd.tcl (no command; it opens the listener when sourced),
// bsitools.tcl, toolkit_rev0.tcl, techbuilder.tcl.
constexpr std::array<std::string_view, 8> kNotLoaded = {
    "add_menu",        "add_menu_command", "add_menu_separator", "gen_params",
    "techbuilder",     "techwizard",       "socketcmd",          "strip_reflibs"};

}  // namespace

std::string strip_reflib_lines(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  // Lines end at \n, \r\n or \r (Tcl's `gets` in auto translation).
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t end = text.find_first_of("\r\n", pos);
    const std::string_view line = text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
    if (end == std::string_view::npos) {
      pos = text.size();
    } else {
      pos = end + 1 + ((text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n') ? 1U : 0U);
    }
    if (starts_with(line, "string GDS_START") || starts_with(line, "string GDS_END")) {
      continue;
    }
    out += line;
    out += '\n';
  }
  return out;
}

std::vector<diag::Diagnostic> strip_reflibs(platform::FileSystem& files, const platform::Path& directory,
                                            std::vector<std::string>* stripped) {
  std::vector<diag::Diagnostic> out;
  auto entries = files.list(directory);
  if (!entries.ok()) {
    return out;  // `glob *.mag` matches nothing
  }
  for (const platform::Path& path : entries.value()) {
    if (path.extension() != ".mag") {
      continue;
    }
    const std::string name = path.filename().string();
    auto text = files.read(path);
    if (!text.ok() || !files.write_atomic(path, strip_reflib_lines(text.value())).ok()) {
      out.push_back(diag::make(codes::kStripOpen, "Error: can't open file " + name));
      continue;
    }
    if (stripped != nullptr) {
      stripped->push_back(name);
    }
  }
  return out;
}

std::span<const std::string_view> not_loaded_commands() noexcept {
  return kNotLoaded;
}

std::string rev0_cell_name(std::string_view type, std::span<const std::string> existing) {
  for (std::size_t n = 1;; ++n) {
    std::string name = std::string{type} + "_" + std::to_string(n);
    if (std::find(existing.begin(), existing.end(), name) == existing.end()) {
      return name;
    }
  }
}

char console_prompt(bool drc_busy, bool long_command) noexcept {
  if (long_command) {
    return ':';
  }
  return drc_busy ? ']' : '%';
}

bool launcher_batch_mode(bool noconsole, bool dnull) noexcept {
  return noconsole && dnull;
}

}  // namespace hefesto::tools
