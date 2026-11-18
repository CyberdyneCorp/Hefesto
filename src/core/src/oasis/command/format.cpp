// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Format recognition on open, scaffold stub
// (openspec/changes/m6-lef-def-oasis/design.md D15).
//
// Owner: OASCMD (design D2).

#include "hefesto/oasis/command/format.hpp"

#include "hefesto/gds/stream/io.hpp"

namespace hefesto::oasis {

FileFormat sniff_format(gds::ByteSource& /*source*/) {
  return FileFormat::unknown;
}

}  // namespace hefesto::oasis
