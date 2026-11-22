// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// schem::Session: the schematic state of one open project,
// editor::EditorState::schem (openspec/changes/m8-schematic design.md D4).
// Schematics and layouts are separate documents; nothing here refers to
// the layout database. Netlisting reads documents and the cache and writes
// only the computed `lab` values it returns (Q-SCHEM-29), never the undo
// log or a document's modified flag.
//
// Owner: SCAFFOLD owns the struct; each member belongs to the named
// workstream.

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/bus.hpp"

namespace hefesto::schem {

/// Identifies an open document for the session's lifetime (never reused).
using DocumentId = std::uint32_t;

struct OpenDocument {
  DocumentId id = 0;
  Document document;
};

struct Session {
  // EDIT: the open schematic and symbol documents, in opening order, and
  // the next id to hand out.
  std::vector<OpenDocument> documents;
  DocumentId next_id = 1;

  // SYMLIB: the symbol cache (keyed by resolved absolute path), the library
  // search path, and the active PDK's `xschemrc` result (nullopt until one
  // is read).
  SymbolCache symbols;
  SearchPath search_path;
  std::optional<XschemrcResult> xschemrc;

  // CMD: the expanded bus text the session's own expansions (the `xschem`
  // subcommands) may still produce (Q-SCHEM-41); each netlist run has its
  // own budget.
  BusBudget bus_budget;
};

}  // namespace hefesto::schem
