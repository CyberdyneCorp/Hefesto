// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol references and the symbol cache (openspec/changes/m8-schematic
// design.md D4, D7, "Area contracts" SYMLIB; capability
// schematic-symbols-and-libraries "Symbol references", "Missing symbols",
// "Symbol cache and reload", "Schematic used as symbol").
//
// Owner: SYMLIB.

#pragma once

#include <map>
#include <string>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::schem {

/// A symbol reference as written in a `C` record.
struct Reference {
  std::string text;

  friend bool operator==(const Reference&, const Reference&) = default;
};

/// What resolution reads: the search path, the files, and where warnings
/// (missing symbol, `http` reference, SCHEM-LIB-001) go.
struct ResolveContext {
  const SearchPath& search_path;
  platform::FileSystem& fs;
  diag::DiagnosticSink* warnings = nullptr;  // not owned; nullptr drops them
};

/// Loaded symbols keyed by resolved absolute path. A returned reference
/// stays valid until clear() or the cache is destroyed.
class SymbolCache {
 public:
  SymbolCache();

  /// The symbol `reference` names, resolved from the document `from` (its
  /// folder for relative references, its embedded blocks first). Never
  /// fails: an unresolvable or unreadable reference gives placeholder().
  [[nodiscard]] const Symbol& resolve(const Reference& reference, const Document& from,
                                      const ResolveContext& context);

  /// The missing-symbol placeholder ("Missing symbols").
  [[nodiscard]] const Symbol& placeholder() const noexcept { return placeholder_; }

  /// Drops every loaded symbol ("Symbol cache and reload").
  void clear() noexcept { symbols_.clear(); }

  [[nodiscard]] std::size_t size() const noexcept { return symbols_.size(); }

 private:
  std::map<platform::Path, Symbol> symbols_;
  Symbol placeholder_;
};

}  // namespace hefesto::schem
