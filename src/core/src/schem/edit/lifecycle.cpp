// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The editing state that ends with a document's content (capability
// schematic-command "Subcommand table" `reload` and `close`; schematic-editing
// "Selection", "Net highlighting", "Undo and redo"; security review S8-06).
// A reload replaces the document under the same id, so nothing that refers
// to the old content may survive it: the selection (indices), the undo
// history (snapshots of the old file), the highlights shown in it and the
// launcher confirmation given for the old file.
//
// Owner: EDIT.

#include <set>
#include <string>

#include "hefesto/schem/edit/edit_state.hpp"
#include "hefesto/schem/edit/hier_path.hpp"
#include "hefesto/schem/tcl/permission.hpp"
#include "internal.hpp"

namespace hefesto::schem {

namespace {

// The hierarchy paths whose schematic is document `id`: along the descents
// when they lead to the active document, else `.` for the active document.
std::set<std::string> paths_showing(Session& session, DocumentId id) {
  std::set<std::string> paths;
  const OpenDocument* active = active_document(session);
  const std::vector<HierFrame>& stack = session.edit.hierarchy;
  if (stack.empty() || active == nullptr || stack.back().child != active->id) {
    if (active != nullptr && active->id == id) paths.insert(".");
    return paths;
  }
  if (stack.front().parent == id) paths.insert(".");
  HierPath walked;
  for (const HierFrame& frame : stack) {
    walked.elements.push_back(frame.element);
    if (frame.child == id) paths.insert(path_text(walked));
  }
  return paths;
}

}  // namespace

void forget_document_state(Session& session, DocumentId id) {
  EditSession& edit = session.edit;
  const std::set<std::string> paths = paths_showing(session, id);
  std::erase_if(edit.highlights, [&paths](const Highlight& h) { return paths.contains(h.path); });
  edit.selections.erase(id);
  edit.histories.erase(id);  // the session undo log's steps refer to them weakly and now do nothing
  edit.width_warnings.erase(id);
  if (edit.launchers != nullptr) edit.launchers->forget(id);
}

}  // namespace hefesto::schem
