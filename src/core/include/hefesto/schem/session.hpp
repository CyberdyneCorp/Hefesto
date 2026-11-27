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

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/edit/edit_state.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/bus.hpp"

namespace hefesto::cmd {
struct ScriptSession;
}

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

  // CMD: what the `xschem` command and the `abs_sym_path`/`rel_sym_path`
  // procedures read with, outside a `schematic` command: the project's
  // compat settings (xschem-compat) and file services. Every `schematic`
  // command points them at its context (as cmd::Dispatcher::set_editor does
  // for the undo recorder); nullptr until the first one.
  const compat::CompatSettings* compat = nullptr;
  platform::FileSystem* files = nullptr;
  // CMD: the script session of the last `schematic` command, whose exit
  // status a failed `xschem netlist` sets to 10 with xschem-compat ON
  // (Q-SCHEM-61); nullptr until the first one.
  cmd::ScriptSession* script = nullptr;

  // EDIT: selection, snap, per-document undo history, hierarchy and
  // highlights (schem/edit/edit_state.hpp); never saved.
  EditSession edit;
};

}  // namespace hefesto::schem
