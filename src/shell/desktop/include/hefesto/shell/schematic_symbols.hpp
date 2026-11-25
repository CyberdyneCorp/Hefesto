// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol resolution for the schematic canvas off the UI thread
// (openspec/changes/m8-schematic design D15, task 13.2; capability
// schematic-symbols-and-libraries "Symbol generators", "Missing symbols").
//
// The canvas never resolves a symbol on the UI thread: a Tcl generator
// reference may run for up to kGeneratorTimeLimit (10 s) and all of them
// for kGeneratorTotalTimeLimit (60 s), and a schematic used as a symbol is
// flattened. SchematicSymbolResolver resolves the references of the
// document shown on one worker thread, into a SymbolCache of its own over
// a file system of its own (the session's SymbolCache, which netlisting
// and the `schematic` commands use, is never touched from two threads):
//
// - show() (UI thread) records the document shown and, when some of its
//   references are not resolved yet, hands a snapshot (the references, the
//   document's folder and embedded blocks, the search path, xschem-compat)
//   to the worker; it never blocks;
// - lookup() (UI thread) gives build_scene the resolved symbol of a
//   reference, or pending_symbol() while it is not resolved yet (drawn as a
//   placeholder; SceneInstance::pending);
// - when the worker finishes, `notify` runs on the worker thread (the app
//   posts a task to the UI thread with it); take_finished() (UI thread)
//   then merges the symbols, and the view rebuilds its scene.
//
// A result is stale, and dropped, when the document shown changed context
// meanwhile: another document (closed, or another one activated), its file
// or its on-disk content (a reload of a changed file, Document::disk_digest),
// the search path or xschem-compat, or the symbols were reloaded or a file
// saved; and a reference the document no longer uses (an edit, a reload) is
// dropped from a result that is merged.
//
// Destroying the resolver cancels the job between references and joins the
// worker: a generator already running finishes first (at most
// kGeneratorTimeLimit).
//
// Raven-free and unit-tested in tests/unit/shell/schematic_symbols_test.cpp.
// Owner: SHELL (openspec/changes/m8-schematic design D3, D15; task 13.2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/schem/sym/symbol.hpp"
#include "hefesto/shell/schematic_canvas.hpp"

namespace hefesto::shell {

/// What one job resolves: the references and what resolution reads.
struct SymbolRequest {
  /// The document's path and embedded blocks only ("Symbol references":
  /// its folder for relative references, its `[` ... `]` blocks first).
  schem::Document from;
  schem::SearchPath search_path;
  bool xschem_compat = true;
  /// Distinct references, in first-use order.
  std::vector<std::string> references;
};

/// The distinct symbol references of `document`'s instances, in first-use order.
[[nodiscard]] std::vector<std::string> document_references(const schem::Document& document);

class SchematicSymbolResolver {
 public:
  /// Resolves one reference on the worker thread (warnings to `warnings`).
  /// The default resolves through a SymbolCache owned by the worker over
  /// the resolver's file system; tests replace it.
  using ResolveFn = std::function<schem::Symbol(const SymbolRequest& request, const std::string& reference,
                                                std::vector<diag::Diagnostic>& warnings)>;
  /// Called on the worker thread when a job ends (the app posts a UI task).
  using Notify = std::function<void()>;

  struct Options {
    /// The worker's file system (the app: its own LocalFileSystem). Required
    /// unless `resolve` is set.
    std::shared_ptr<platform::FileSystem> files;
    ResolveFn resolve;
    Notify notify;
  };

  explicit SchematicSymbolResolver(Options options);
  ~SchematicSymbolResolver();  // cancels between references and joins
  SchematicSymbolResolver(const SchematicSymbolResolver&) = delete;
  SchematicSymbolResolver& operator=(const SchematicSymbolResolver&) = delete;

  /// The document shown (UI thread). Drops every symbol of another context
  /// (see the file comment) and starts a job for the references not
  /// resolved yet when no job is running. Returns true when a job started.
  bool show(schem::DocumentId id, const schem::Document& document, const schem::SearchPath& search_path,
            bool xschem_compat);
  /// Nothing shown: drops every symbol and any running job's result.
  void forget();
  /// `schematic reload_symbols` ("Symbol cache and reload"): every symbol
  /// is resolved again, the worker's cache starts again (and with it the
  /// generators' total time).
  void reload_symbols();
  /// `schematic save` wrote `file`: its symbol and the embedded ones are
  /// resolved again (the session cache's forget()).
  void forget_file(const platform::Path& file);

  /// Merges a finished job's symbols (UI thread); true when a job had
  /// finished (the scene should be rebuilt, and show() called again for
  /// references added meanwhile).
  bool take_finished();
  /// The warnings of the jobs merged since the last call (missing symbols,
  /// SCHEM-LIB-012), for the messages panel.
  [[nodiscard]] std::vector<diag::Diagnostic> take_warnings();

  /// For build_scene (UI thread; never resolves). Valid until the next
  /// show(), forget() or take_finished().
  [[nodiscard]] SymbolLookup lookup() const;
  /// What lookup() gives for a reference not resolved yet.
  [[nodiscard]] const schem::Symbol& pending_symbol() const noexcept { return pending_; }

  [[nodiscard]] bool running() const;
  /// References of the document shown that are not resolved yet.
  [[nodiscard]] std::size_t pending_count() const;
  /// Blocks until the running job ends (tests, shutdown).
  void wait();

 private:
  struct Context {
    schem::DocumentId id = 0;
    platform::Path path;
    std::string disk_digest;
    std::vector<platform::Path> directories;
    std::vector<schem::LibraryAlias> aliases;
    bool xschem_compat = true;

    friend bool operator==(const Context&, const Context&) = default;
  };
  struct Worker;

  bool start_job(const schem::Document& document, const schem::SearchPath& search_path);

  std::shared_ptr<Worker> worker_;
  Context context_;
  bool has_context_ = false;
  std::uint64_t generation_ = 0;
  std::vector<std::string> references_;  // of the document shown
  std::map<std::string, schem::Symbol, std::less<>> symbols_;
  std::vector<diag::Diagnostic> warnings_;
  schem::Symbol pending_;
};

}  // namespace hefesto::shell
