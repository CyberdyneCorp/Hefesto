// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Worker confinement (openspec/changes/m9-lvs-simulation design D9
// "Confinement"; capability simulation-engine-embedding "Worker
// confinement"). hefesto-simd calls confine() before it loads the engine
// adapter. Once confined the worker cannot open network sockets, create
// processes, or write outside the job directory; it may read anything,
// including the read-only roots (the PDK store, the installed libraries).
//
//   macOS  sandbox_init with a Hefesto profile (sandbox_macos.cpp)
//   Linux  PR_SET_NO_NEW_PRIVS, a seccomp filter (sockets other than
//          AF_UNIX, fork, vfork, clone without CLONE_THREAD, clone3, execve,
//          execveat), and a Landlock ruleset for writes where the kernel has
//          Landlock; otherwise the seccomp filter alone, noted in the job log
//          (sandbox_linux.cpp)
//
// Owner: SIMD.

#pragma once

#include <string>
#include <vector>

namespace hefesto::simd {

struct Confinement {
  std::string job_dir;                      // the only writable directory (may be empty: no writes)
  std::vector<std::string> readonly_roots;  // also denied for writing, even inside job_dir
};

struct ConfinementResult {
  bool ok = false;
  std::string error;               // why confinement failed (the worker then exits)
  std::vector<std::string> notes;  // job-log lines: what was applied, what is unavailable
};

[[nodiscard]] ConfinementResult confine(const Confinement& confinement);

}  // namespace hefesto::simd
