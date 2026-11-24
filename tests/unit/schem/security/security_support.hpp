// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Helpers of the schematic script security review (task 12.1 of
// openspec/changes/m8-schematic, design D14): a temporary folder outside
// every granted root, the hostile Tcl snippets the review runs on every
// script path, and diagnostic filters.
//
// Owner: SECURITY.

#pragma once

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::test::security {

inline constexpr const char* kPermissionModel = "[cap:hefesto-security][req:Schematic script permission model]";
inline constexpr const char* kSandbox = "[cap:hefesto-security][req:Script sandbox enforcement]";

/// A fresh folder under the system temporary folder, removed afterwards.
class TempFolder {
 public:
  TempFolder() {
    static std::atomic<int> counter{0};
    path_ = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
            ("hefesto-schem-security-" + std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1)));
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
  }
  ~TempFolder() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  TempFolder(const TempFolder&) = delete;
  TempFolder& operator=(const TempFolder&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
  [[nodiscard]] std::string str(const std::filesystem::path& relative = {}) const {
    return (relative.empty() ? path_ : path_ / relative).generic_string();
  }
  void write(const std::filesystem::path& relative, std::string_view text) const {
    std::filesystem::create_directories((path_ / relative).parent_path());
    std::ofstream(path_ / relative, std::ios::binary) << text;
  }

 private:
  std::filesystem::path path_;
};

/// One hostile operation and the sandbox code that must refuse it.
struct Hostile {
  std::string name;
  std::string script;  // Tcl; `@OUT@` stands for a path outside every root
  std::string code;    // SCRIPT-SANDBOX-00n
};

/// The operations of hefesto-security "Schematic script permission model":
/// process execution, sockets, native extensions, file access (read, write,
/// `source`) outside the granted folders and nested-interpreter escapes.
inline std::vector<Hostile> hostile_operations() {
  return {
      {"exec", "exec cat /etc/passwd", "SCRIPT-SANDBOX-001"},
      {"open pipeline", "open |id", "SCRIPT-SANDBOX-001"},
      {"socket", "socket example.com 80", "SCRIPT-SANDBOX-002"},
      {"server socket", "socket -server accept 8080", "SCRIPT-SANDBOX-002"},
      {"load", "load @OUT@/evil.dylib", "SCRIPT-SANDBOX-003"},
      {"read outside", "open /etc/hosts r", "SCRIPT-SANDBOX-004"},
      {"source outside", "source @OUT@/evil.tcl", "SCRIPT-SANDBOX-004"},
      {"write outside", "open @OUT@/written w", "SCRIPT-SANDBOX-004"},
      {"delete outside", "file delete @OUT@/evil.tcl", "SCRIPT-SANDBOX-004"},
      {"mkdir outside", "file mkdir @OUT@/made", "SCRIPT-SANDBOX-004"},
      {"glob outside", "glob -directory / *", "SCRIPT-SANDBOX-004"},
      {"cd outside", "cd /", "SCRIPT-SANDBOX-004"},
      {"unsafe interpreter", "interp create", "SCRIPT-SANDBOX-005"},
      {"hidden command", "interp create -safe s; interp invokehidden s exec id", "SCRIPT-SANDBOX-005"},
      {"child object", "interp create -safe s; s invokehidden exec id", "SCRIPT-SANDBOX-005"},
      {"expose", "interp create -safe s; interp expose s exec", "SCRIPT-SANDBOX-005"},
      {"alias into the child", "interp create -safe s; interp alias s run {} exec; s eval run id",
       "SCRIPT-SANDBOX-001"},
      {"package", "package require Tk", "SCRIPT-SANDBOX-006"},
      {"exit", "exit 3", "SCRIPT-SANDBOX-007"},
  };
}

/// `script` with `@OUT@` replaced by `outside`.
inline std::string with_outside(std::string script, const std::string& outside) {
  for (std::size_t at = script.find("@OUT@"); at != std::string::npos; at = script.find("@OUT@", at)) {
    script.replace(at, 5, outside);
  }
  return script;
}

inline std::vector<diag::Diagnostic> with_code_prefix(const std::vector<diag::Diagnostic>& all,
                                                      std::string_view prefix) {
  std::vector<diag::Diagnostic> out;
  std::copy_if(all.begin(), all.end(), std::back_inserter(out),
               [prefix](const diag::Diagnostic& d) { return d.code.starts_with(prefix); });
  return out;
}

}  // namespace hefesto::test::security
