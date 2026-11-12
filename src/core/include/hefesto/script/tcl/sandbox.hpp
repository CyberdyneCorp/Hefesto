// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The script sandbox (decision X-3; openspec/changes/m4-scripting/design.md
// D5; capabilities hefesto-scripting "Embedded sandboxed Tcl on all
// platforms" and hefesto-security "Script sandbox enforcement").
//
//   SandboxPolicy      what one project interpreter may do: the granted
//                      filesystem roots (project folder, PDK store, session
//                      temporary area, explicitly opened documents), the
//                      removed commands, the evaluation limits and the
//                      synthesized `env` array (D-31).
//   SandboxFilesystem  the path decision of the Tcl_Filesystem layer that
//                      sits in front of Tcl's native filesystem: a path is
//                      allowed only when, with `.`/`..` removed and symbolic
//                      links resolved, it lies under a granted root.
//
// How the policy is enforced (src/core/src/script/tcl/):
//   - removed commands (`exec`, `socket`, `load`, `unload`, `exit`) are
//     replaced by commands that raise a Tcl error naming the operation;
//   - `open |…`, `interp create` without `-safe`, `interp invokehidden`,
//     `expose`, `marktrusted` and `package require` of a package that Tcl's
//     embedded library does not provide raise the same kind of error;
//   - every path operation of Tcl (source, open, file, glob, cd, encoding
//     files, ...) passes the filesystem layer: a path outside the granted
//     roots fails with EACCES and the command reports
//     "<operation>: ... is outside the sandbox's granted roots";
//   - each denial posts a SCRIPT-SANDBOX-* diagnostic with the script file
//     and line and a script::DeniedOperation event, also when the script
//     absorbs the Tcl error with `catch`.
// The embedded library root (kVirtualLibraryRoot) is served read-only from
// memory and is not a granted root.
//
// Owner: TCL (design D2).

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/platform/file_system.hpp"

namespace hefesto::script {

/// The commands removed from every project interpreter (design D5). `open`
/// stays but refuses `|` pipelines, `interp create` stays for `-safe` only
/// and `package require` for Tcl's own pure-Tcl packages only; those three
/// are restricted, not removed, and are not listed here.
[[nodiscard]] std::vector<std::string> default_removed_commands();

struct SandboxLimits {
  /// Commands one top-level evaluation may run (Tcl `interp limit
  /// -command`); 0 = unlimited.
  std::uint64_t command_limit = 0;
  /// Wall time of one top-level evaluation; zero = unlimited.
  std::chrono::milliseconds time_limit{0};
};

struct SandboxPolicy {
  /// Absolute directories under which path operations are allowed
  /// (hefesto-files-and-pdks granted roots). They are normalized and their
  /// symbolic links resolved when the policy is applied.
  std::vector<platform::Path> granted_roots;
  /// Commands replaced by a denial (default_removed_commands()).
  std::vector<std::string> removed_commands = default_removed_commands();
  SandboxLimits limits;
  /// The synthesized `env` array (D-31): PDK_ROOT, PDK, CAD_ROOT from the
  /// PDK store and settings. It is the whole `env` array; writes stay local
  /// to the interpreter and never reach the process environment.
  std::map<std::string, std::string, std::less<>> environment;
};

/// The verdict on one path operation.
struct PathDecision {
  bool allowed = false;
  /// The absolute path that was checked: lexically normalized, with the
  /// symbolic links of its existing prefix resolved.
  platform::Path normalized;
};

/// The path decision of one interpreter's filesystem layer.
class SandboxFilesystem {
 public:
  /// Normalizes the granted roots (relative or empty roots are dropped).
  explicit SandboxFilesystem(SandboxPolicy policy);

  /// Whether `operation` ("open", "source", "file delete", "glob", "cd",
  /// ...) may touch `path`, resolved against `cwd` when relative. A path
  /// starting with `~` (Tcl's home-directory form, which Tcl expands before
  /// the layer sees it) is refused here.
  [[nodiscard]] PathDecision check(std::string_view operation, std::string_view path,
                                   const platform::Path& cwd) const;

  /// True when `normalized` (absolute, normalized) is a directory above a
  /// granted root, such as "/" or "/home" for /home/u/proj. The layer
  /// reveals the metadata of these (stat, access), which Tcl reads while
  /// creating directories under a root, and refuses every other access.
  [[nodiscard]] bool is_root_ancestor(std::string_view normalized) const;

  [[nodiscard]] const SandboxPolicy& policy() const noexcept { return policy_; }
  /// The granted roots after normalization.
  [[nodiscard]] const std::vector<platform::Path>& roots() const noexcept { return roots_; }

 private:
  SandboxPolicy policy_;
  std::vector<platform::Path> roots_;
};

/// True when `path` is the embedded library's virtual root or lies under it
/// (lexically; the embedded tree has no links).
[[nodiscard]] bool is_embedded_path(std::string_view path) noexcept;

/// One decision of the filesystem layer, for the sandbox audit of the fuzz
/// target and the security tests (design D19).
struct FilesystemAccess {
  std::string path;  // the normalized path Tcl asked about
  enum class Verdict { embedded, allowed, denied } verdict = Verdict::denied;
};

/// Installs a process-wide observer of every filesystem-layer decision
/// (empty function: none). For tests and fuzzing only; called from the
/// thread that evaluates scripts.
void set_filesystem_audit(std::function<void(const FilesystemAccess&)> audit);

}  // namespace hefesto::script
