// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// load_styles and StyleTable::select (capability extract-tech-section
// "Extraction styles and variants", "Non-electrical types";
// openspec/changes/m7-extraction/design.md D5, D7, tasks.md 4.1, 4.6).
//
// The table parses lazily, as ExtLoadStyle does: the tech load reads the
// section for the first style only, and a style is read by the one reader
// (style_reader.cpp) in its full mode the first time it is selected. The
// parsed style and its messages are kept, so a later selection replays the
// messages Magic prints when it re-reads the section. Magic's
// allExtractTypes is reset only when a technology loads and grows with
// every style ExtLoadStyle reads, so with magic-compat ON a type an earlier
// selection mentioned stays electrical in a later one; with magic-compat
// OFF each style uses its own mentions (an unreported Magic behaviour; the
// standing rule).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c ExtTechInit,
// ExtLoadStyle and ExtTechFinal (:3556-3620), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH.

#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/extract/tech/style.hpp"
#include "rule_context.hpp"
#include "style_reader.hpp"

namespace hefesto::extract {

namespace detail {

struct StyleEntry {
  bool parsed = false;
  ExtStyle style;            // active before the non-electrical types are removed
  tech::TypeMask mentioned;  // the style's allExtractTypes, connected types included
  std::vector<diag::Diagnostic> messages;
};

struct StyleCache {
  compat::CompatSettings compat;
  tech::Technology technology;  // a handle on the loaded technology's data
  tech::RawSection section;     // the `extract` section the styles are read from
  std::vector<std::string> names;
  std::vector<std::string> type_names;  // long names, index = tech::TileType (user types)
  std::mutex mutex;
  std::vector<StyleEntry> entries;  // index = names; guarded by mutex
  tech::TypeMask accumulated;       // allExtractTypes since the tech load (magic-compat ON); guarded by mutex
};

}  // namespace detail

namespace {

// ExtLoadStyle's read of style `index` (the first one as the tech load
// reads it: the first declared style, or "default").
void parse_entry(detail::StyleCache& cache, std::size_t index) {
  detail::StyleEntry& entry = cache.entries[index];
  if (entry.parsed) {
    return;
  }
  diag::CollectingSink collect;
  detail::ReadOptions options;
  if (index > 0) {
    options.style = cache.names[index];
  }
  options.report = detail::Subset::all;
  options.magic_compat = cache.compat.magic_compat;
  std::optional<detail::StyleTables> tables =
      detail::read_style_tables(cache.section, cache.technology, collect, options);
  if (tables) {
    entry.style = std::move(tables->style);
    entry.mentioned = tables->mentioned;
  }
  entry.messages = collect.take();
  entry.parsed = true;
}

void report_unhandled(const detail::StyleCache& cache, const tech::TypeMask& types, diag::DiagnosticSink& sink) {
  if (types.empty()) {
    return;
  }
  std::string text = "The following types are not handled by extraction and will be treated as non-electrical "
                     "types:\n    ";
  for (std::size_t t = tech::kFirstUserType; t < cache.type_names.size(); ++t) {
    if (types.test(static_cast<tech::TileType>(t))) {
      text += cache.type_names[t] + " ";
    }
  }
  sink.emit(diag::make(detail::kCodeNonElectrical, std::move(text)));
}

std::shared_ptr<const ExtStyle> select_entry(detail::StyleCache& cache, std::string_view name,
                                             diag::DiagnosticSink* sink) {
  const auto it = std::find(cache.names.begin(), cache.names.end(), name);
  if (it == cache.names.end()) {
    return nullptr;
  }
  const auto index = static_cast<std::size_t>(it - cache.names.begin());
  const std::scoped_lock lock{cache.mutex};
  parse_entry(cache, index);
  const detail::StyleEntry& entry = cache.entries[index];
  tech::TypeMask mentioned = entry.mentioned;
  if (cache.compat.magic_compat) {
    cache.accumulated |= entry.mentioned;
    mentioned = cache.accumulated;
  }
  ExtStyle style = entry.style;
  detail::apply_non_electrical(style, mentioned, cache.type_names.size());
  if (sink != nullptr) {
    for (const diag::Diagnostic& d : entry.messages) {
      sink->emit(d);
    }
    report_unhandled(cache, style.non_electrical, *sink);
  }
  return std::make_shared<const ExtStyle>(std::move(style));
}

}  // namespace

std::shared_ptr<const ExtStyle> StyleTable::select(std::string_view name) const {
  return cache_ == nullptr ? nullptr : select_entry(*cache_, name, nullptr);
}

std::shared_ptr<const ExtStyle> StyleTable::select(std::string_view name, diag::DiagnosticSink& sink) const {
  return cache_ == nullptr ? nullptr : select_entry(*cache_, name, &sink);
}

StyleTable load_styles(const tech::RawSection& section, const tech::Technology& technology,
                       diag::DiagnosticSink& sink, const compat::CompatSettings& compat) {
  auto cache = std::make_shared<detail::StyleCache>();
  cache->compat = compat;
  cache->technology = technology;
  cache->section = section;
  cache->type_names.resize(technology.user_type_end());
  for (std::size_t t = tech::kFirstUserType; t < technology.user_type_end(); ++t) {
    cache->type_names[t] = std::string{technology.type_name(static_cast<tech::TileType>(t))};
  }
  cache->names = detail::declared_styles(section);
  if (cache->names.empty()) {
    cache->names.emplace_back("default");
  }
  cache->entries.resize(cache->names.size());
  StyleTable table{cache->names};
  table.cache_ = cache;
  // The tech load reads the first style and reports its messages and the
  // non-electrical list (ExtTechFinal), with allExtractTypes starting
  // empty; the other styles are read when selected.
  (void)select_entry(*cache, cache->names.front(), &sink);
  return table;
}

}  // namespace hefesto::extract
