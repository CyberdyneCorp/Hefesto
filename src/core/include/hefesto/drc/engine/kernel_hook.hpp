// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A test seam: replaces the basic-check kernel (drc::basic_check, CHECK)
// the engine calls, so engine tests can observe the check and clip
// rectangles of every call and report errors of their own without
// depending on the rule semantics. Production code never installs one.
//
// The replacement is process-wide and thread-safe to read (the background
// worker calls the kernel); install it only while no worker runs.
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#pragma once

#include "hefesto/db/geometry.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/types.hpp"

namespace hefesto::drc {

using KernelFunction = CheckOutcome (*)(const CheckInput&, const db::Rect&, const db::Rect&, ErrorSink&);

/// Installs `kernel` for its lifetime (nullptr = drc::basic_check) and
/// restores the previous one on destruction.
class ScopedKernel {
 public:
  explicit ScopedKernel(KernelFunction kernel) noexcept;
  ~ScopedKernel();
  ScopedKernel(const ScopedKernel&) = delete;
  ScopedKernel& operator=(const ScopedKernel&) = delete;

 private:
  KernelFunction previous_;
};

}  // namespace hefesto::drc
