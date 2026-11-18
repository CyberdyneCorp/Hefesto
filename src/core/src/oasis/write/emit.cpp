// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS write's emit phase, transcoders and repetition detection, scaffold
// stubs (openspec/changes/m6-lef-def-oasis/design.md D13).
//
// Owner: OASW (design D2).

#include "hefesto/oasis/write/emit.hpp"
#include "hefesto/oasis/write/repetitions.hpp"
#include "hefesto/oasis/write/transcode.hpp"

#include "hefesto/cmd/stubs.hpp"
#include "hefesto/gds/stream/io.hpp"

namespace hefesto::oasis {

cmd::CommandResult emit(const gds::PreparedWrite& /*prepared*/, const OasisSettings& /*settings*/,
                        const platform::Path& /*path*/, const mask::Progress* /*progress*/) {
  return cmd::not_implemented("oasis write");
}

diag::Status transcode_gds_cell(gds::ByteSource& /*gds_stream*/, std::uint64_t /*offset*/, std::string_view /*name*/,
                                Encoder& /*out*/) {
  return cmd::not_implemented_status("oasis write");
}

diag::Status reencode_oasis_cell(gds::ByteSource& /*oasis_stream*/, std::uint64_t /*offset*/,
                                 std::string_view /*name*/, Encoder& /*out*/) {
  return cmd::not_implemented_status("oasis write");
}

std::vector<RepetitionGroup> detect_repetitions(std::span<const db::Point> positions) {
  std::vector<RepetitionGroup> groups;
  groups.reserve(positions.size());
  for (const db::Point& p : positions) {
    groups.push_back(RepetitionGroup{p, {}});
  }
  return groups;
}

}  // namespace hefesto::oasis
