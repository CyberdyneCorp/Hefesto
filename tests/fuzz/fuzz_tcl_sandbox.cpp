// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the script sandbox (openspec/changes/m4-scripting
// design.md D5 and D19, tasks.md 4.5; capabilities hefesto-scripting
// "Embedded sandboxed Tcl on all platforms" and hefesto-security "Script
// sandbox enforcement"). Each input is a Tcl script evaluated in a fresh
// project interpreter whose only granted root is a temporary directory,
// with a command and a time limit so loops and sleeps end.
//
// Besides crashes, hangs and memory errors (ASan/UBSan), it checks through
// the filesystem layer's audit (script::set_filesystem_audit, no
// LD_PRELOAD) that every path the layer let through to the host
// filesystem lies under the temporary root: a path outside it with the
// verdict `allowed` aborts. Paths the layer refuses or serves from the
// embedded library are fine whatever they are. Security review finding
// S-13: an allowed path must also stay under the root once its symbolic
// links are followed (dangling ones included), and the root's private
// parent directory must hold nothing but the root after each script, so a
// write through a link that left the root (S-04, S-05) aborts too.

#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#include "fuzz_common.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace {

namespace fs = std::filesystem;

// The granted root: a fresh directory `root` in a private directory under
// the system temporary directory; the process working directory while a
// script runs.
const fs::path& sandbox_root() {
  static const fs::path root = [] {
    const fs::path base = fs::canonical(fs::temp_directory_path());
    const fs::path parent = base / ("hefesto-fuzz-tcl-" + std::to_string(::getpid()));
    fs::remove_all(parent);
    fs::create_directories(parent / "root");
    return parent / "root";
  }();
  return root;
}

// `path` with every symbolic link followed, dangling ones from their
// directory (as the kernel would on open); the path itself when unreadable.
fs::path followed(const fs::path& path) {
  fs::path current = path;
  std::error_code error;
  for (int hop = 0; hop < 40 && fs::is_symlink(fs::symlink_status(current, error)); ++hop) {
    const fs::path target = fs::read_symlink(current, error);
    if (error) {
      break;
    }
    current = target.is_absolute() ? target : current.parent_path() / target;
  }
  const fs::path resolved = fs::weakly_canonical(current, error);
  return error ? current.lexically_normal() : resolved;
}

bool under_root(std::string_view path) {
  const std::string root = sandbox_root().string();
  return path.substr(0, root.size()) == root && (path.size() == root.size() || path[root.size()] == '/');
}

void audit(const hefesto::script::FilesystemAccess& access) {
  if (access.verdict != hefesto::script::FilesystemAccess::Verdict::allowed) {
    return;
  }
  if (!under_root(access.path) || !under_root(followed(access.path).string())) {
    std::fprintf(stderr, "sandbox escape: %s reached the host filesystem\n", access.path.c_str());
    std::abort();
  }
}

// Nothing but the root may appear next to it (a write through a link out of
// the root lands there first).
void check_parent() {
  std::error_code ignored;
  for (const auto& entry : fs::directory_iterator(sandbox_root().parent_path(), ignored)) {
    if (entry.path() != sandbox_root()) {
      std::fprintf(stderr, "sandbox escape: %s was created outside the root\n", entry.path().c_str());
      std::abort();
    }
  }
}

// Empties the root and makes it the working directory again.
void reset_root() {
  std::error_code ignored;
  fs::current_path(sandbox_root(), ignored);
  for (const auto& entry : fs::directory_iterator(sandbox_root(), ignored)) {
    fs::remove_all(entry.path(), ignored);
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  static const bool installed = [] {
    hefesto::script::set_filesystem_audit(audit);
    return true;
  }();
  static_cast<void>(installed);
  reset_root();

  hefesto::script::SandboxPolicy policy;
  policy.granted_roots = {sandbox_root()};
  policy.environment = {{"PDK_ROOT", sandbox_root().string()}, {"PDK", "sky130A"}};
  policy.limits.command_limit = 20000;
  policy.limits.time_limit = std::chrono::milliseconds(250);
  hefesto::editor::EditorState state;
  auto& interp = hefesto::script::open_project_interpreter(state, policy);
  static_cast<void>(interp.evaluate(hefesto::fuzz::as_text(data, size)));
  hefesto::script::close_project_interpreter(state);
  check_parent();
  return 0;
}
