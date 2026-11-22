// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub of extract/length (length.hpp): CORE-NOTIMPL-008 until
// EXTCMD lands it (openspec/changes/m7-extraction tasks.md, EXTCMD). EXTCMD
// replaces this file with its own sources.
//
// Owner: EXTCMD (design D3).

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/extract/length/length.hpp"

namespace hefesto::extract {

diag::Status path_lengths(ExtractContext& /*ctx*/, const LengthLists& /*lists*/, ExtWriter& /*writer*/) {
  return cmd::m7_not_implemented_status("extract length");
}

}  // namespace hefesto::extract
