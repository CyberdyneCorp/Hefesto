// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `clipboard`, `paste` and `merge` (capability schematic-editing
// "Clipboard"; schematic-command "Subcommand table").
//
// The clipboard is session text (EditSession::clipboard) in the record
// format of schematic-file-format: `clipboard copy` writes a document made
// of the active document's global records and the selected objects (with
// the embedded symbols of the selected instances), moved so that the
// selection's anchor (the lower-left corner of its bounding box, snapped) is
// the origin. The front end exchanges the text with the system clipboard
// (`clipboard` reads it, `clipboard set` replaces it), so text copied by
// another program that writes these records pastes too. `paste <x> <y>`
// reads the text and appends its objects moved by (x, y), with unique
// instance names, and selects them; `merge <file> [<x> <y>]` does the same
// with a file's objects. The global records of the pasted text or merged
// file are not taken, and symbol references are kept as written. Pasting
// and merging are one undo step each and are refused on a read-only
// document (run_edit); copying is allowed on any document.
//
// Owner: EDIT.

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/sym/instance.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

constexpr std::array<ObjectKind, 7> kKinds{ObjectKind::line, ObjectKind::box,  ObjectKind::arc,
                                           ObjectKind::polygon, ObjectKind::text, ObjectKind::wire,
                                           ObjectKind::instance};
constexpr std::array<std::string_view, 2> kActions{"copy", "set"};

template <typename T>
ObjectRef append_from(const std::vector<T>& from, std::vector<T>& to, ObjectRef ref) {
  to.push_back(from[ref.index]);
  return {ref.kind, to.size() - 1};
}

// Appends object `ref` of `from` to `to`; the reference of the copy.
ObjectRef append_copy(const Document& from, ObjectRef ref, Document& to) {
  switch (ref.kind) {
    case ObjectKind::line:
      return append_from(from.lines, to.lines, ref);
    case ObjectKind::box:
      return append_from(from.boxes, to.boxes, ref);
    case ObjectKind::arc:
      return append_from(from.arcs, to.arcs, ref);
    case ObjectKind::polygon:
      return append_from(from.polygons, to.polygons, ref);
    case ObjectKind::text:
      return append_from(from.texts, to.texts, ref);
    case ObjectKind::wire:
      return append_from(from.wires, to.wires, ref);
    case ObjectKind::instance:
      return append_from(from.instances, to.instances, ref);
  }
  return ref;
}

// Adds to `to` the embedded symbol of `from` that `reference` names, unless
// `to` already has one for it.
void add_embedded(const Document& from, const std::string& reference, Document& to) {
  const auto has = [&reference](const EmbeddedSymbol& e) { return e.reference == reference; };
  if (std::any_of(to.embedded.begin(), to.embedded.end(), has)) return;
  const auto found = std::find_if(from.embedded.begin(), from.embedded.end(), has);
  if (found != from.embedded.end()) to.embedded.push_back(*found);
}

// The selection's anchor: the lower-left corner of its bounding box,
// snapped to the session's snap.
Point selection_anchor(EditContext& ec) {
  Rect r;
  for (const ObjectRef& ref : ec.selection()) {
    const Symbol* symbol = ref.kind == ObjectKind::instance ? &ec.symbol(ec.doc().instances[ref.index]) : nullptr;
    r.add(bounds(ec.doc(), ref, symbol));
  }
  return snap_point(r.min, ec.session().edit.snap.snap);
}

std::string clipboard_text(EditContext& ec) {
  const Document& doc = ec.doc();
  Document clip;
  clip.kind = doc.kind;
  clip.file_version = "1.3";
  clip.globals = doc.globals;
  const Point anchor = selection_anchor(ec);
  const Motion motion{Motion::Kind::translate, {-anchor.x, -anchor.y}, {}};
  for (const ObjectRef& ref : ec.selection()) {
    const ObjectRef copy = append_copy(doc, ref, clip);
    apply(motion, clip, copy);
    if (ref.kind == ObjectKind::instance) add_embedded(doc, doc.instances[ref.index].symbol, clip);
  }
  return write_document(clip);
}

std::size_t object_total(const Document& doc) {
  std::size_t total = 0;
  for (const ObjectKind kind : kKinds) total += object_count(doc, kind);
  return total;
}

// The symbol references the instances of `doc` use.
std::set<std::string, std::less<>> used_references(const Document& doc) {
  std::set<std::string, std::less<>> used;
  for (const InstanceRecord& instance : doc.instances) used.insert(instance.symbol);
  return used;
}

// Appends the objects of `source` to the active document moved by `at`,
// with unique instance names, and selects them. An embedded block of
// `source` is added only for a reference no instance of the document used
// before: an embedded block resolves its reference document-wide, so it
// would redefine the existing instances' symbol (proposed Q-SCHEM-51).
cmd::CommandResult insert_objects(EditContext& ec, const Document& source, Point at) {
  Document& doc = ec.doc();
  const std::set<std::string, std::less<>> used = used_references(doc);
  const Motion motion{Motion::Kind::translate, at, {}};
  InstanceNamer names(doc, &ec.session().bus_budget);
  Selection inserted;
  for (const ObjectKind kind : kKinds) {
    for (std::size_t i = 0; i < object_count(source, kind); ++i) {
      const ObjectRef ref = append_copy(source, ObjectRef{kind, i}, doc);
      apply(motion, doc, ref);
      if (kind == ObjectKind::instance) {
        InstanceRecord& instance = doc.instances[ref.index];
        instance.props = names.unique_properties(instance.props);
        if (!used.contains(instance.symbol)) add_embedded(source, instance.symbol, doc);
      }
      inserted.insert(ref);
    }
  }
  ec.selection() = std::move(inserted);
  if (!ec.selection().empty()) ec.changed();
  return cmd::CommandResult::success(selected_objects(ec));
}

ReadOptions read_options(EditContext& ec) {
  ReadOptions options;
  options.kind = ec.doc().kind;
  options.xschem_compat = ec.ctx().project().compat.xschem_compat;
  options.warnings = &ec.ctx().sink();
  return options;
}

cmd::CommandResult copy_body(EditContext& ec, std::span<const std::string> args) {
  if (args.size() != 1) return usage("clipboard");
  if (ec.selection().empty()) return cmd::CommandResult::success(std::string{});  // as `copy`: nothing to do
  std::string& clipboard = ec.session().edit.clipboard;
  clipboard = clipboard_text(ec);
  return cmd::CommandResult::success(clipboard);
}

cmd::CommandResult paste_body(EditContext& ec, std::span<const std::string> args) {
  const auto xy = numbers(args, 0, 2);
  if (args.size() != 2 || !xy) return usage("paste");
  ReadOptions options = read_options(ec);
  options.source_name = "clipboard";
  auto clip = read_document(ec.session().edit.clipboard, options);
  if (!clip) return fail(clip.error());
  if (object_total(clip.value()) == 0) {
    return fail(kEmptyClipboard, "schematic: the clipboard holds no schematic objects");
  }
  return insert_objects(ec, clip.value(), {(*xy)[0], (*xy)[1]});
}

cmd::CommandResult merge_body(EditContext& ec, std::span<const std::string> args) {
  const auto xy = args.size() == 3 ? numbers(args, 1, 2) : std::optional{std::vector<double>{0.0, 0.0}};
  if ((args.size() != 1 && args.size() != 3) || !xy) return usage("merge");
  const platform::Path file = cmdimpl::absolute_path(args[0]);
  ReadOptions options = read_options(ec);
  options.symbol_file_exists =
      symbol_file_finder(file.parent_path(), ec.session().search_path, ec.ctx().services().files);
  auto merged = load_document(ec.ctx().services().files, file, options);
  if (!merged) return fail(merged.error());
  return insert_objects(ec, merged.value(), {(*xy)[0], (*xy)[1]});
}

}  // namespace

cmd::CommandResult op_clipboard(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  std::string& clipboard = ctx.editor()->schem.edit.clipboard;
  if (inv.args.size() < 2) return cmd::CommandResult::success(clipboard);
  const auto action = cmdimpl::match_keyword(inv.args[1], kActions);
  if (!action) return fail(action.error());
  if (action.value() == 0) return run_view(ctx, inv, copy_body);
  if (inv.args.size() != 3) return usage("clipboard");
  clipboard = inv.args[2];
  return cmd::CommandResult::success(clipboard);
}

cmd::CommandResult op_paste(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, paste_body);
}

cmd::CommandResult op_merge(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, merge_body);
}

}  // namespace hefesto::schem::editimpl
