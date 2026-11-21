// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol cache (capability schematic-symbols-and-libraries "Symbol
// references", "Missing symbols", "Schematic used as symbol", "Symbol cache
// and reload"; Q-SCHEM-9, Q-SCHEM-30).
//
// Misses are not cached, so a later change of the search path re-resolves a
// missing symbol; each warning is given once per reference (or file) until
// clear(). A schematic that places itself, directly or through other
// schematics, stops at the first repetition with the max-depth warning
// rather than nesting 40 copies of itself.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/symbol_cache.hpp"

#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/lib/schematic_symbol.hpp"

namespace hefesto::schem {

namespace {

bool is_schematic_file(const platform::Path& file) {
  return file.extension() == ".sch";
}

std::string relative_warning(const std::string& reference, const platform::Path& folder) {
  return "Relative symbol reference " + reference + " resolved against the schematic folder " +
         folder.generic_string() + "; xschem resolves it against its start folder";
}

// Removes `file` from `loading` when the load ends, however it ends.
class LoadingGuard {
 public:
  LoadingGuard(std::set<platform::Path>& loading, platform::Path file) : loading_(loading), file_(std::move(file)) {
    loading_.insert(file_);
  }
  ~LoadingGuard() { loading_.erase(file_); }
  LoadingGuard(const LoadingGuard&) = delete;
  LoadingGuard& operator=(const LoadingGuard&) = delete;

 private:
  std::set<platform::Path>& loading_;
  platform::Path file_;
};

}  // namespace

SymbolCache::SymbolCache() {
  placeholder_.placeholder = true;
  placeholder_.document.kind = FileKind::symbol;
}

void SymbolCache::clear() noexcept {
  symbols_.clear();
  embedded_.clear();
  warned_.clear();
}

void SymbolCache::forget(const platform::Path& file) {
  symbols_.erase(file);
  // A schematic used as a symbol holds copies of the graphics it places.
  std::erase_if(symbols_, [](const auto& entry) { return is_schematic_file(entry.first); });
}

const Symbol& SymbolCache::resolve(const Reference& reference, const Document& from, const ResolveContext& context) {
  return resolve_at(reference, from, context, 0);
}

void SymbolCache::warn_once(const ResolveContext& context, const std::string& key, diag::Diagnostic diagnostic) {
  if (!warned_.insert(key).second || context.warnings == nullptr) return;
  context.warnings->emit(diagnostic);
}

const Symbol* SymbolCache::embedded(const Reference& reference, const Document& from) {
  auto key = std::make_pair(from.path, reference.text);
  if (const auto it = embedded_.find(key); it != embedded_.end()) return &it->second;
  for (const EmbeddedSymbol& block : from.embedded) {
    if (block.reference != reference.text) continue;
    Symbol symbol = make_symbol(reference.text, {}, block.symbol);
    symbol.embedded = true;
    return &embedded_.emplace(std::move(key), std::move(symbol)).first->second;
  }
  return nullptr;
}

const Symbol& SymbolCache::resolve_at(const Reference& reference, const Document& from, const ResolveContext& context,
                                      int depth) {
  if (const Symbol* inline_symbol = embedded(reference, from)) return *inline_symbol;
  const std::string& text = reference.text;
  const ResolvedReference resolved =
      resolve_reference(text, from.path.parent_path(), context.search_path, context.fs);
  switch (resolved.via) {
    case ResolvedVia::none:
      return placeholder_;
    case ResolvedVia::web:
      warn_once(context, "web|" + text,
                diag::make("SCHEM-LIB-002", "Web symbol reference " + text +
                                                " is not fetched; the instance is shown as a missing symbol"));
      return placeholder_;
    case ResolvedVia::relative:
    case ResolvedVia::document_folder:
      warn_once(context, "relative|" + text + "|" + resolved.folder.generic_string(),
                diag::make("SCHEM-LIB-001", relative_warning(text, resolved.folder)));
      break;
    default:
      break;
  }
  if (!context.fs.exists(resolved.file)) {
    warn_once(context, "missing|" + text, diag::make("SCHEM-LIB-003", "Symbol not found: " + text));
    return placeholder_;
  }
  return load(resolved.file, text, context, depth);
}

const Symbol& SymbolCache::load(const platform::Path& file, const std::string& reference, const ResolveContext& context,
                                int depth) {
  if (const auto it = symbols_.find(file); it != symbols_.end()) return it->second;
  if (depth >= kMaxSymbolDepth || loading_.count(file) != 0) {
    warn_once(context, "depth|" + file.generic_string(),
              diag::make("SCHEM-LIB-005", "Symbol recursively instantiating symbol: max depth reached, skipping"));
    return placeholder_;
  }
  const bool schematic = is_schematic_file(file);
  auto bytes = context.fs.read(file);
  auto document = bytes.ok() ? read_document(bytes.value(), ReadOptions{schematic ? FileKind::schematic : FileKind::symbol,
                                                                         context.xschem_compat, context.warnings})
                             : diag::Outcome<Document>{bytes.error()};
  if (!document.ok()) {
    warn_once(context, "read|" + file.generic_string(),
              diag::make("SCHEM-LIB-004", "Symbol " + reference + " could not be read from " + file.generic_string() +
                                              ": " + document.error().text));
    return placeholder_;
  }
  document.value().path = file;
  if (!schematic) return symbols_.emplace(file, make_symbol(reference, file, std::move(document.value()))).first->second;

  const LoadingGuard guard(loading_, file);
  const Document& sch = document.value();
  const NestedSymbol nested = [&](const InstanceRecord& instance) -> const Symbol& {
    return resolve_at(Reference{instance.symbol}, sch, context, depth + 1);
  };
  platform::Path sibling = file;
  sibling.replace_extension(".sym");
  const Symbol* order_from = context.fs.exists(sibling) ? &load(sibling, reference, context, depth + 1) : nullptr;
  Symbol symbol = schematic_symbol(reference, file, sch, nested, order_from);
  return symbols_.insert_or_assign(file, std::move(symbol)).first->second;
}

}  // namespace hefesto::schem
