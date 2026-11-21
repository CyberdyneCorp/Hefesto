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
#include <set>
#include <string>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"
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
  /// `xschem-compat`, passed to the reader of symbol files.
  bool xschem_compat = true;
};

/// Nesting depth at which a schematic used as a symbol stops loading the
/// symbols it places ("Schematic used as symbol").
inline constexpr int kMaxSymbolDepth = 40;

/// Loaded symbols keyed by resolved absolute path. A returned reference
/// stays valid until clear(), forget() of its file, or the cache is
/// destroyed.
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

  /// Drops every loaded symbol and the record of the warnings given
  /// ("Symbol cache and reload"); the next resolve() reads the files again.
  void clear() noexcept;

  /// Drops the symbol loaded from `file`, so the next resolve() reads it
  /// again (a symbol edited and saved in Hefesto), and the embedded symbols
  /// of the document saved as `file`.
  void forget(const platform::Path& file);

  [[nodiscard]] std::size_t size() const noexcept { return symbols_.size(); }

 private:
  const Symbol& resolve_at(const Reference& reference, const Document& from, const ResolveContext& context, int depth);
  const Symbol& load(const platform::Path& file, const std::string& reference, const ResolveContext& context, int depth);
  const Symbol* embedded(const Reference& reference, const Document& from);
  void warn_once(const ResolveContext& context, const std::string& key, diag::Diagnostic diagnostic);

  std::map<platform::Path, Symbol> symbols_;
  // Embedded blocks by (document path, reference); one entry per distinct
  // block content, so unsaved documents (empty path) and edited blocks never
  // share a symbol. Map nodes keep returned references valid.
  std::multimap<std::pair<platform::Path, std::string>, Symbol> embedded_;
  std::set<platform::Path> loading_;  // files being loaded, to stop cycles
  std::set<std::string> warned_;      // warnings already given, by key
  Symbol placeholder_;
};

}  // namespace hefesto::schem
