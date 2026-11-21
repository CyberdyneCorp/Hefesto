// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 5.2): from_xschemrc reads nothing and reports
// CORE-NOTIMPL-007.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/search_path.hpp"

#include "hefesto/schem/not_implemented.hpp"

namespace hefesto::schem {

XschemrcResult SearchPath::from_xschemrc(platform::FileSystem& /*fs*/, const XschemrcInput& /*input*/) {
  XschemrcResult result;
  result.diagnostics.push_back(not_implemented("schem::SearchPath::from_xschemrc"));
  return result;
}

}  // namespace hefesto::schem
