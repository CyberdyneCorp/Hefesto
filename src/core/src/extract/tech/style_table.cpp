// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// load_styles and StyleTable::select (capability extract-tech-section
// "Extraction styles and variants", "Non-electrical types";
// openspec/changes/m7-extraction/design.md D5, D7, tasks.md 4.6).
//
// Every style of the section is read once, with its messages kept, by the
// one reader (style_reader.cpp) in its full mode. Magic's allExtractTypes
// is reset only when a technology loads and grows with every style
// ExtLoadStyle reads, so with magic-compat ON a type an earlier selection
// mentioned stays electrical in a later one; with magic-compat OFF each
// style uses its own mentions (an unreported Magic behaviour; the standing
// rule).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c ExtTechInit,
// ExtLoadStyle and ExtTechFinal (:3556-3620), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH.

#include <algorithm>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/extract/tech/style.hpp"
#include "rule_context.hpp"
#include "style_reader.hpp"

namespace hefesto::extract {

namespace detail {

struct StyleEntry {
  std::string name;
  ExtStyle style;              // active before the non-electrical types are removed
  tech::TypeMask mentioned;    // the style's allExtractTypes, connected types included
  std::vector<diag::Diagnostic> messages;
};

struct StyleCache {
  bool magic_compat = true;
  tech::TileType first_user = tech::kFirstUserType;
  std::vector<std::string> type_names;  // long names, index = tech::TileType (user types)
  std::vector<StyleEntry> entries;
  std::mutex mutex;
  tech::TypeMask accumulated;  // allExtractTypes since the tech load (magic-compat ON)
};

}  // namespace detail

namespace {

detail::StyleEntry read_entry(const tech::RawSection& section, const tech::Technology& technology,
                              std::optional<std::string_view> name, const compat::CompatSettings& compat) {
  diag::CollectingSink collect;
  detail::ReadOptions options;
  options.style = name;
  options.report = detail::Subset::all;
  options.magic_compat = compat.magic_compat;
  std::optional<detail::StyleTables> tables = detail::read_style_tables(section, technology, collect, options);
  detail::StyleEntry entry;
  if (tables) {
    entry.name = tables->style.name;
    entry.style = std::move(tables->style);
    entry.mentioned = tables->mentioned;
  }
  entry.messages = collect.take();
  return entry;
}

// The user types `mentioned` leaves out (ExtTechFinal's non-electrical set).
tech::TypeMask unhandled(const detail::StyleCache& cache, const tech::TypeMask& mentioned) {
  tech::TypeMask out;
  for (std::size_t t = cache.first_user; t < cache.type_names.size(); ++t) {
    if (!mentioned.test(static_cast<tech::TileType>(t))) {
      out.set(static_cast<tech::TileType>(t));
    }
  }
  return out;
}

void report_unhandled(const detail::StyleCache& cache, const tech::TypeMask& types, diag::DiagnosticSink& sink) {
  if (types.empty()) {
    return;
  }
  std::string text = "The following types are not handled by extraction and will be treated as non-electrical "
                     "types:\n    ";
  for (std::size_t t = cache.first_user; t < cache.type_names.size(); ++t) {
    if (types.test(static_cast<tech::TileType>(t))) {
      text += cache.type_names[t] + " ";
    }
  }
  sink.emit(diag::make(detail::kCodeNonElectrical, std::move(text)));
}

std::shared_ptr<const ExtStyle> select_entry(detail::StyleCache& cache, std::string_view name,
                                             diag::DiagnosticSink* sink) {
  const auto it = std::find_if(cache.entries.begin(), cache.entries.end(),
                               [&](const detail::StyleEntry& e) { return e.name == name; });
  if (it == cache.entries.end()) {
    return nullptr;
  }
  tech::TypeMask mentioned = it->mentioned;
  if (cache.magic_compat) {
    const std::scoped_lock lock{cache.mutex};
    cache.accumulated |= it->mentioned;
    mentioned = cache.accumulated;
  }
  ExtStyle style = it->style;
  style.non_electrical = unhandled(cache, mentioned);
  style.active &= ~style.non_electrical;
  if (sink != nullptr) {
    for (const diag::Diagnostic& d : it->messages) {
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
  cache->magic_compat = compat.magic_compat;
  cache->type_names.resize(technology.user_type_end());
  for (std::size_t t = tech::kFirstUserType; t < technology.user_type_end(); ++t) {
    cache->type_names[t] = std::string{technology.type_name(static_cast<tech::TileType>(t))};
  }
  std::vector<std::string> names = detail::declared_styles(section);
  // The style the technology loads (the first declared, or "default").
  cache->entries.push_back(read_entry(section, technology, std::nullopt, compat));
  for (std::size_t i = 1; i < names.size(); ++i) {
    cache->entries.push_back(read_entry(section, technology, names[i], compat));
  }
  if (names.empty()) {
    names.push_back(cache->entries.front().name);
  }
  StyleTable table{std::move(names)};
  table.cache_ = cache;
  // The tech load reports the first style's messages and the
  // non-electrical list (ExtTechFinal), with allExtractTypes starting empty.
  (void)select_entry(*cache, table.names().front(), &sink);
  return table;
}

}  // namespace hefesto::extract
