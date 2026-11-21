// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 5.2): every reference resolves to the placeholder.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/symbol_cache.hpp"

namespace hefesto::schem {

SymbolCache::SymbolCache() {
  placeholder_.placeholder = true;
  placeholder_.document.kind = FileKind::symbol;
}

const Symbol& SymbolCache::resolve(const Reference& /*reference*/, const Document& /*from*/,
                                   const ResolveContext& /*context*/) {
  return placeholder_;
}

}  // namespace hefesto::schem
