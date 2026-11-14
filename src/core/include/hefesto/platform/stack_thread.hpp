// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A thread with a chosen stack size, for work whose recursion follows the
// cell hierarchy (capability hefesto-architecture "Concurrency model";
// openspec/changes/m5-mask-gds-cif/design.md D16). The mask generators and
// the GDS/CIF writers recurse once per hierarchy level, up to their depth
// limits (gds::kMaxSubtreeDepth, the generators' 1024 levels); a
// std::thread's default stack (512 KiB for a secondary thread on macOS) is
// too small for that, so a host that runs such work off the command thread
// (task 12.2) runs it through run_on_stack with at least
// kDeepRecursionStackBytes.
//
// The new thread does not hold the caller's database lock (the lock is
// re-entrant only on the thread that took it) and starts with fresh
// thread_local state; the caller blocks until it finishes.
//
// Owner: INTEGRATION (shared platform code).

#pragma once

#include <cstddef>
#include <functional>

namespace hefesto::platform {

/// The stack that recursion over a hierarchy at the writers' and
/// generators' depth limits needs, with room for sanitizer builds.
inline constexpr std::size_t kDeepRecursionStackBytes = std::size_t{64} << 20U;

/// Runs `work` on a new thread whose stack has at least `stack_bytes`
/// bytes and waits for it; an exception thrown by `work` is rethrown here.
void run_on_stack(std::size_t stack_bytes, const std::function<void()>& work);

}  // namespace hefesto::platform
