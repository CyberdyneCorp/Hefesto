// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol cache (capability schematic-symbols-and-libraries "Symbol
// references", "Missing symbols", "Schematic used as symbol", "Symbol cache
// and reload"; Q-SCHEM-9, Q-SCHEM-30, Q-SCHEM-32, Q-SCHEM-40).
//
// Misses are not cached, so a later change of the search path re-resolves a
// missing symbol; each warning is given once per reference (or file) until
// clear(). A schematic used as a symbol is flattened by schematic_symbol():
// the schematics it places are read once per load and opened in place, and
// the `.sym` files they place come from this cache. There is no cycle check:
// a schematic that places itself nests until kMaxSymbolDepth, as in xschem.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/symbol_cache.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/lib/generator.hpp"
#include "hefesto/schem/lib/reference.hpp"

namespace hefesto::schem {

namespace {

bool is_schematic_file(const platform::Path& file) {
  return file.extension() == ".sch";
}

std::string relative_warning(const std::string& reference, const platform::Path& folder) {
  return "Relative symbol reference " + reference + " resolved against the schematic folder " +
         folder.generic_string() + "; xschem resolves it against its start folder";
}

// Whether a cached embedded symbol was built from `block`: a document's
// path does not identify a block (unsaved documents share the empty path,
// and a block can be edited), so the records are compared.
bool same_block(const Document& cached, const Document& block) {
  return cached.kind == block.kind && cached.file_version == block.file_version &&
         cached.header_text == block.header_text && cached.globals == block.globals && cached.lines == block.lines &&
         cached.boxes == block.boxes && cached.arcs == block.arcs && cached.polygons == block.polygons &&
         cached.texts == block.texts && cached.wires == block.wires && cached.instances == block.instances;
}

// M8-D5..D7 (approved 2026-11-23), both modes: a generator file over
// kMaxGeneratorScriptBytes is not read, not even for its `#!` line, and not
// run, whatever its name (SCHEM-LIB-012; it was SCHEM-LIB-011 without `.tcl`).
bool oversized_generator(const platform::Path& script, const ResolveContext& context) {
  const auto size = context.fs.size(script);
  return size.ok() && size.value() > kMaxGeneratorScriptBytes;
}

}  // namespace

// The nested resolver of one schematic-as-symbol load: embedded blocks and
// `.sym` files from the cache, nested schematics read once and kept until
// the load ends (their Document addresses key the flattener's memo).
class SymbolCache::SchematicLoad {
 public:
  SchematicLoad(SymbolCache& cache, const ResolveContext& context) : cache_(cache), context_(context) {}

  NestedTarget operator()(const InstanceRecord& instance, const Document& from) {
    const Reference reference{instance.symbol};
    if (const Symbol* inline_symbol = cache_.embedded(reference, from)) return {nullptr, inline_symbol};
    if (is_generator_reference(reference.text)) return {nullptr, &cache_.generated(reference, from, context_)};
    const platform::Path file = cache_.locate(reference, from, context_);
    if (file.empty()) return {nullptr, &cache_.placeholder_};
    if (!is_schematic_file(file)) return {nullptr, &cache_.load(file, instance.symbol, context_)};
    if (const auto it = schematics_.find(file); it != schematics_.end()) return {&it->second, nullptr};
    auto document = cache_.read(file, instance.symbol, context_);
    if (!document) return {nullptr, &cache_.placeholder_};
    return {&schematics_.emplace(file, std::move(*document)).first->second, nullptr};
  }

 private:
  SymbolCache& cache_;
  const ResolveContext& context_;
  std::map<platform::Path, Document> schematics_;  // map nodes keep addresses
};

SymbolCache::SymbolCache(FlattenLimits limits, std::chrono::milliseconds generator_time)
    : limits_(limits), generator_time_(generator_time), generator_left_(generator_time) {
  placeholder_.placeholder = true;
  placeholder_.document.kind = FileKind::symbol;
}

void SymbolCache::clear() noexcept {
  symbols_.clear();
  embedded_.clear();
  generated_.clear();
  warned_.clear();
  generator_left_ = generator_time_;
}

void SymbolCache::forget(const platform::Path& file) {
  symbols_.erase(file);
  std::erase_if(embedded_, [&](const auto& entry) { return entry.first.first == file; });
  std::erase_if(generated_, [&](const auto& entry) { return entry.first.first == file; });
  // A schematic used as a symbol holds copies of the graphics it places.
  std::erase_if(symbols_, [](const auto& entry) { return is_schematic_file(entry.first); });
}

const Symbol& SymbolCache::resolve(const Reference& reference, const Document& from, const ResolveContext& context) {
  if (const Symbol* inline_symbol = embedded(reference, from)) return *inline_symbol;
  if (is_generator_reference(reference.text)) return generated(reference, from, context);
  const platform::Path file = locate(reference, from, context);
  return file.empty() ? placeholder_ : load(file, reference.text, context);
}

// "Symbol generators" (Q-SCHEM-11): the program is looked up like a symbol
// file; only a Tcl script runs, and what it prints is read as a `.sym`.
const Symbol& SymbolCache::generated(const Reference& reference, const Document& from,
                                     const ResolveContext& context) {
  const std::string& text = reference.text;
  const GeneratorCall call = parse_generator(text).value_or(GeneratorCall{});
  const platform::Path folder = from.path.parent_path();
  const platform::Path script = resolve_reference(call.program, folder, context.search_path, context.fs).file;
  if (!script.is_absolute() || !context.fs.exists(script)) {  // relative: an unsaved document's fallback
    warn_once(context, "missing|" + text, diag::make("SCHEM-LIB-003", "Symbol not found: " + text));
    return placeholder_;
  }
  auto key = std::make_pair(script, text);
  if (const auto it = generated_.find(key); it != generated_.end()) return it->second;
  Symbol& symbol = generated_.emplace(std::move(key), placeholder_).first->second;
  if (oversized_generator(script, context)) {
    warn_once(context, "oversized|" + script.generic_string(),
              diag::make("SCHEM-LIB-012", "Symbol " + text + ": the generator " + script.generic_string() +
                                              " is larger than " + std::to_string(kMaxGeneratorScriptBytes) +
                                              " bytes; it is neither read nor run"));
    return symbol;
  }
  if (!is_tcl_generator(script, context.fs)) {
    warn_once(context, "external|" + script.generic_string(),
              diag::make("SCHEM-LIB-011", "Symbol " + text + ": " + std::string{kExternalGeneratorMessage}));
    return symbol;
  }
  const std::optional<std::string> output = run_generator(text, script, call.args, folder, context);
  if (!output) return symbol;
  const ReadOptions options{FileKind::symbol, context.xschem_compat, context.warnings, text};
  auto document = read_document(*output, options);
  if (!document.ok()) {
    warn_once(context, "read|" + text,
              diag::make("SCHEM-LIB-004", "Symbol " + text + " could not be read from the output of " +
                                              script.generic_string() + ": " + document.error().text));
    return symbol;
  }
  symbol = make_symbol(text, script, std::move(document.value()));
  return symbol;
}

// One generator run under what is left of the cache's total time
// (kGeneratorTotalTimeLimit): each run gets at most kGeneratorTimeLimit, and
// once the total is spent no generator runs until clear(). What it printed,
// or nullopt after SCHEM-LIB-012.
std::optional<std::string> SymbolCache::run_generator(const std::string& reference, const platform::Path& script,
                                                      std::vector<std::string> args, const platform::Path& folder,
                                                      const ResolveContext& context) {
  using std::chrono::milliseconds;
  if (generator_left_ <= milliseconds{0}) {
    warn_once(context, "generator-time",
              diag::make("SCHEM-LIB-012", "Symbol " + reference + ": symbol generators ran for their total of " +
                                              std::to_string(generator_time_.count()) +
                                              " ms; no further generator runs until the symbols are reloaded"));
    return std::nullopt;
  }
  GeneratorInput input{script, std::move(args), folder};
  input.time_limit = std::min(kGeneratorTimeLimit, generator_left_);
  const auto start = std::chrono::steady_clock::now();
  GeneratorOutput output = run_tcl_generator(context.fs, input);
  generator_left_ -= std::chrono::duration_cast<milliseconds>(std::chrono::steady_clock::now() - start);
  for (const diag::Diagnostic& diagnostic : output.diagnostics) {
    if (context.warnings != nullptr) context.warnings->emit(diagnostic);
  }
  return std::move(output.text);
}

void SymbolCache::warn_once(const ResolveContext& context, const std::string& key, diag::Diagnostic diagnostic) {
  if (!warned_.insert(key).second || context.warnings == nullptr) return;
  context.warnings->emit(diagnostic);
}

const Symbol* SymbolCache::embedded(const Reference& reference, const Document& from) {
  const auto block = std::find_if(from.embedded.begin(), from.embedded.end(),
                                  [&](const EmbeddedSymbol& b) { return b.reference == reference.text; });
  if (block == from.embedded.end()) return nullptr;
  auto key = std::make_pair(from.path, reference.text);
  const auto [first, last] = embedded_.equal_range(key);
  for (auto it = first; it != last; ++it) {
    if (same_block(it->second.document, block->symbol)) return &it->second;
  }
  Symbol symbol = make_symbol(reference.text, {}, block->symbol);
  symbol.embedded = true;
  return &embedded_.emplace(std::move(key), std::move(symbol))->second;
}

platform::Path SymbolCache::locate(const Reference& reference, const Document& from, const ResolveContext& context) {
  const std::string& text = reference.text;
  const ResolvedReference resolved =
      resolve_reference(text, from.path.parent_path(), context.search_path, context.fs);
  switch (resolved.via) {
    case ResolvedVia::none:
      return {};
    case ResolvedVia::web:
      warn_once(context, "web|" + text,
                diag::make("SCHEM-LIB-002", "Web symbol reference " + text +
                                                " is not fetched; the instance is shown as a missing symbol"));
      return {};
    case ResolvedVia::relative:
    case ResolvedVia::document_folder:
      warn_once(context, "relative|" + text + "|" + resolved.folder.generic_string(),
                diag::make("SCHEM-LIB-001", relative_warning(text, resolved.folder)));
      break;
    default:
      break;
  }
  // A relative result is the fallback of an unsaved document (no folder):
  // missing, never looked up in the folder Hefesto was started in.
  if (!resolved.file.is_absolute() || !context.fs.exists(resolved.file)) {
    warn_once(context, "missing|" + text, diag::make("SCHEM-LIB-003", "Symbol not found: " + text));
    return {};
  }
  return resolved.file;
}

std::optional<Document> SymbolCache::read(const platform::Path& file, const std::string& reference,
                                          const ResolveContext& context) {
  auto bytes = context.fs.read(file);
  const ReadOptions options{is_schematic_file(file) ? FileKind::schematic : FileKind::symbol, context.xschem_compat,
                            context.warnings, file.generic_string()};
  auto document = bytes.ok() ? read_document(bytes.value(), options) : diag::Outcome<Document>{bytes.error()};
  if (!document.ok()) {
    warn_once(context, "read|" + file.generic_string(),
              diag::make("SCHEM-LIB-004", "Symbol " + reference + " could not be read from " + file.generic_string() +
                                              ": " + document.error().text));
    return std::nullopt;
  }
  document.value().path = file;
  return std::move(document.value());
}

const Symbol& SymbolCache::load(const platform::Path& file, const std::string& reference,
                                const ResolveContext& context) {
  if (const auto it = symbols_.find(file); it != symbols_.end()) return it->second;
  auto document = read(file, reference, context);
  if (!document) return placeholder_;
  if (is_schematic_file(file)) return load_schematic(file, reference, std::move(*document), context);
  return symbols_.emplace(file, make_symbol(reference, file, std::move(*document))).first->second;
}

const Symbol& SymbolCache::load_schematic(const platform::Path& file, const std::string& reference, Document document,
                                          const ResolveContext& context) {
  platform::Path sibling = file;
  sibling.replace_extension(".sym");
  const Symbol* order_from = context.fs.exists(sibling) ? &load(sibling, reference, context) : nullptr;
  SchematicLoad nested(*this, context);
  FlattenReport report;
  Symbol symbol = schematic_symbol(reference, file, document, std::ref(nested), order_from, &report, limits_);
  if (report.depth_reached) {
    warn_once(context, "depth|" + file.generic_string(),
              diag::make("SCHEM-LIB-005", "Symbol recursively instantiating symbol: max depth reached, skipping"));
  }
  if (report.size_reached || report.bytes_reached) {
    const std::string limit = report.size_reached ? std::to_string(limits_.max_records) + " records"
                                                  : std::to_string(limits_.max_bytes) + " bytes";
    warn_once(context, "size|" + file.generic_string(),
              diag::make("SCHEM-LIB-009", "Symbol " + reference + ": more than " + limit +
                                              " when flattened; the rest is not loaded"));
  }
  return symbols_.insert_or_assign(file, std::move(symbol)).first->second;
}

}  // namespace hefesto::schem
