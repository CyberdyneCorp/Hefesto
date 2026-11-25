// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The editing state of a schematic session, Session::edit
// (openspec/changes/m8-schematic design.md D11; capability
// schematic-editing "Snap and grid", "Selection", "Undo and redo",
// "Hierarchy navigation", "Hierarchy path", "Net highlighting").
//
// Everything here is session state, never document data: nothing in it is
// saved, and changing it creates no undo step.
//
// Owner: EDIT.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "hefesto/schem/edit/hier_path.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

/// The same type as session.hpp's DocumentId (an alias may be repeated).
using DocumentId = std::uint32_t;

struct Session;            // hefesto/schem/session.hpp
class LauncherPermission;  // hefesto/schem/tcl/permission.hpp

// ---- Snap and grid -----------------------------------------------------------

/// "Snap and grid": the snap (default 10) and the visible grid (default 20).
struct SnapSettings {
  double snap = 10.0;
  double grid = 20.0;
};

/// `value` rounded to the nearest multiple of `snap` (halves away from
/// zero); `value` itself when `snap` is not positive.
[[nodiscard]] double snap_value(double value, double snap) noexcept;
[[nodiscard]] Point snap_point(Point p, double snap) noexcept;

/// Halves, doubles or sets the snap; set_snap refuses a value that is not a
/// positive finite number (returns false, the snap is unchanged).
void halve_snap(SnapSettings& settings) noexcept;
void double_snap(SnapSettings& settings) noexcept;
bool set_snap(SnapSettings& settings, double value) noexcept;

// ---- Selection ---------------------------------------------------------------

/// The object lists of a Document ("Document model").
enum class ObjectKind : std::uint8_t { line, box, arc, polygon, text, wire, instance };

/// One object: its kind and its index in that kind's list.
struct ObjectRef {
  ObjectKind kind = ObjectKind::wire;
  std::size_t index = 0;

  friend auto operator<=>(const ObjectRef&, const ObjectRef&) = default;
};

/// The selected objects of one document, ordered by kind and index.
using Selection = std::set<ObjectRef>;

// ---- Undo and redo -----------------------------------------------------------

/// A state of a document kept by the undo history. Snapshots are shared:
/// the `after` of one step is the `before` of the next, so a history of n
/// steps holds n + 1 documents, not 2n.
using DocumentSnapshot = std::shared_ptr<const Document>;

/// One undo step of one document: the document before and after the
/// command, and their sizes (document_bytes). Shared by the per-document
/// history and the session undo log (undo::UndoLog), so either can undo it.
struct UndoRecord {
  DocumentId document = 0;
  DocumentSnapshot before;
  DocumentSnapshot after;
  std::size_t before_bytes = 0;
  std::size_t after_bytes = 0;
};

/// "Undo and redo": the per-document history. `records[0, position)` are
/// done, the rest undone; at most kUndoDepth records are kept, and the
/// oldest are dropped while the snapshots take more than
/// EditSession::undo_max_bytes (the newest step is always kept).
struct DocumentHistory {
  std::vector<std::shared_ptr<UndoRecord>> records;
  std::size_t position = 0;
};

/// The steps kept per document (at least xschem's 80).
inline constexpr std::size_t kUndoDepth = 200;

/// The memory the snapshots of one document's history may take (Hefesto's
/// bound; xschem keeps its 80 copies whatever their size).
inline constexpr std::size_t kUndoMaxBytes = std::size_t{512} << 20U;

/// An estimate of the memory `document` takes: its records, strings and
/// embedded symbols.
[[nodiscard]] std::size_t document_bytes(const Document& document) noexcept;

// ---- Hierarchy ---------------------------------------------------------------

/// "Hierarchy navigation": descents are limited to 40 levels.
inline constexpr std::size_t kMaxHierarchyDepth = 40;

/// One descent: the parent document and the chosen instance element.
struct HierFrame {
  DocumentId parent = 0;
  DocumentId child = 0;
  std::string instance;  // the instance name as written (e.g. `xinv[3:0]`)
  std::string element;   // the chosen element (e.g. `xinv[0]`)
};

// ---- Highlighting ------------------------------------------------------------

/// "Net highlighting": a highlighted net or instance of the schematic shown
/// at one hierarchy path, with its colour index.
struct Highlight {
  std::string path;  // the hierarchy path text, e.g. `.x1.`
  std::string name;  // net name, or instance name when `instance`
  bool instance = false;
  int colour = 0;

  friend auto operator<=>(const Highlight&, const Highlight&) = default;
};

/// The highlight colours cycled through ("increment highlight colour").
inline constexpr int kHighlightColours = 8;

// ---- Session -----------------------------------------------------------------

struct EditSession {
  SnapSettings snap;
  /// "Selection": right-to-left rectangles also select touched objects.
  bool select_touched = true;
  /// "Net highlighting": each new highlight takes the next colour.
  bool increment_highlight_colour = true;

  std::map<DocumentId, Selection> selections;
  std::map<DocumentId, DocumentHistory> histories;
  /// The byte bound of each document's history (kUndoMaxBytes; tests lower it).
  std::size_t undo_max_bytes = kUndoMaxBytes;

  /// The descents from the top schematic to the active one, top first.
  std::vector<HierFrame> hierarchy;

  std::set<Highlight> highlights;
  int next_colour = 0;

  /// The width warnings ("Bus editing") last reported per document, so an
  /// edit reports only new ones.
  std::map<DocumentId, std::set<std::string>> width_warnings;

  /// "Clipboard": the text `schematic clipboard copy` made or `schematic
  /// clipboard set` stored (records of schematic-file-format), which
  /// `schematic paste` inserts. The front end exchanges it with the system
  /// clipboard.
  std::string clipboard;

  /// The launcher confirmations of the front end that runs launchers
  /// (schem/tcl/permission.hpp); a reload or close of a document ends its
  /// confirmation (security review S8-06). Not owned; nullptr when no front
  /// end runs launchers.
  LauncherPermission* launchers = nullptr;
};

/// Document `id` was re-read from disk or is being closed (`schematic
/// reload`, `schematic close`): drops its selection, its undo and redo
/// history, the highlights made in the schematic it shows, its width
/// warnings and its launcher confirmation (EditSession::launchers). The
/// highlights dropped are those stored at the hierarchy paths that show
/// document `id` (the active document's path when the descents do not lead
/// to it).
void forget_document_state(Session& session, DocumentId id);

}  // namespace hefesto::schem
