// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cif::read fails with CORE-NOTIMPL-005 until the read engine (INPUT,
// tasks.md 7.3) and the `cif read` option (tasks.md 11.4) land; the parser
// behind it is parser.cpp. Owner: CIF.

#include "hefesto/cif/read/read.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::cif {

cmd::CommandResult read(editor::EditorState& /*editor*/, const platform::Path& /*path*/,
                        const CifReadOptions& /*options*/, const mask::Progress* /*progress*/) {
  return cmd::not_implemented("cif read");
}

}  // namespace hefesto::cif
