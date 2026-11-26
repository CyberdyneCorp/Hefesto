// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The resource caps of an OASIS read (capability oasis-read "Input
// limits"; design D21; approved as D1 of the user decisions of 2026-11-19,
// security review M6-S3): oasis::read transcodes every element instance
// into an in-memory GDS-equivalent stream (design D12), which holds at
// most `stream_bytes` bytes and `instances` element instances and uses.
// A file that needs more is read up to the cap (OASIS-READ-007). oasis::read
// uses the defaults; tests and the fuzz target may pass smaller caps.
//
// Owner: OASR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#pragma once

#include <cstdint>

#include "hefesto/oasis/read/read.hpp"

namespace hefesto::oasis {

struct ReadLimits {
  /// The GDS-equivalent stream: 2 GiB.
  std::uint64_t stream_bytes = std::uint64_t{1} << 31;
  /// Element instances and uses, including dropped ones: 2^28.
  std::uint64_t instances = std::uint64_t{1} << 28;
};

/// oasis::read with the caps `limits`.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const OasisReadOptions& options, const ReadLimits& limits,
                                      const mask::Progress* progress = nullptr);

}  // namespace hefesto::oasis
