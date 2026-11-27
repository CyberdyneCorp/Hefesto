// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `descend`, `ascend`, `hierarchy`, `undo` and `redo` (capability
// schematic-editing "Hierarchy navigation", "Hierarchy path", "Undo and
// redo", "Read-only libraries").
//
// Descending opens the child (or activates it when it is already open) and
// keeps the parent open with its unsaved changes; ascending activates the
// parent again. The descents are Session::edit.hierarchy; when the active
// document is not the last one descended into (another document was opened
// or activated since), the stack is stale and the active document is the
// new top. A PDK cell can be descended into: read-only is checked by the
// editing operations, not here. Navigation creates no undo step, and every
// document keeps its own history, so "Undo after ascending" undoes the edit
// made before descending.
//
// Owner: EDIT.

#include <algorithm>
#include <array>
#include <string>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/schem/edit/hier_path.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/sym/types.hpp"
#include "internal.hpp"

namespace hefesto::schem {

std::string path_text(const HierPath& path) {
  std::string text = ".";
  for (const std::string& element : path.elements) {
    text += element;
    text += '.';
  }
  return text;
}

std::string at_path(const HierPath& path) {
  return path_text(path).substr(1);
}

}  // namespace hefesto::schem

namespace hefesto::schem::editimpl {

namespace {

using cmdimpl::FlagSpec;

// The descents that lead to the active document `active`, for reading:
// empty when the stack is stale, which is left as it is (review of PR #101:
// a query made while another document was active dropped the descent path).
std::span<const HierFrame> frames(const Session& session, DocumentId active) {
  const std::vector<HierFrame>& stack = session.edit.hierarchy;
  if (!stack.empty() && stack.back().child != active) return {};
  return stack;
}

// The descents that lead to `active`, for descending and ascending, which
// start a new stack when it is stale.
std::vector<HierFrame>& frames_to_change(Session& session, DocumentId active) {
  std::vector<HierFrame>& stack = session.edit.hierarchy;
  if (!stack.empty() && stack.back().child != active) stack.clear();
  return stack;
}

HierPath current_path(std::span<const HierFrame> stack) {
  HierPath path;
  for (const HierFrame& frame : stack) path.elements.push_back(frame.element);
  return path;
}

platform::Path folder_of(const Document& doc) {
  return doc.path.empty() ? cmdimpl::absolute_path(".") : doc.path.parent_path();
}

platform::Path with_sch_extension(const platform::Path& file) {
  platform::Path sch = file;
  sch.replace_extension(".sch");
  return sch;
}

// "Schematic resolution" (schematic-netlisting) for the editor: the
// instance's `schematic`, else the symbol's, resolved like a symbol
// reference; else, or when that names no file, the symbol file with `.sch`.
platform::Path schematic_file(EditContext& ec, const InstanceRecord& instance, const Symbol& symbol) {
  platform::FileSystem& fs = ec.ctx().services().files;
  std::string value = get_token(instance.props, "schematic").value_or(std::string{});
  if (value.empty()) value = get_token(symbol.property(), "schematic").value_or(std::string{});
  if (!value.empty()) {
    const ResolvedReference found =
        resolve_reference(value, folder_of(ec.doc()), ec.session().search_path, fs);
    if (!found.file.empty() && fs.exists(found.file)) return found.file;
  }
  return symbol.file.empty() ? platform::Path{} : with_sch_extension(symbol.file);
}

// "When descending into a vector instance the user SHALL choose which
// element; elements are numbered 1 from the left, negative numbers count
// from the right, and an invalid number SHALL select element 1." An
// element may also be given by name.
std::string choose_element(const std::vector<std::string>& bits, const std::optional<std::string>& choice) {
  if (!choice || bits.size() <= 1) return bits.front();
  if (std::find(bits.begin(), bits.end(), *choice) != bits.end()) return *choice;
  const auto n = int_value(*choice);
  const auto size = static_cast<long long>(bits.size());
  const long long index = !n ? 0 : (*n > 0 ? *n - 1 : size + *n);
  return (index < 0 || index >= size) ? bits.front() : bits[static_cast<std::size_t>(index)];
}

OpenDocument* find_by_path(Session& session, const platform::Path& path) {
  for (OpenDocument& open : session.documents) {
    if (open.document.path == path) return &open;
  }
  return nullptr;
}

void activate(Session& session, DocumentId id) {
  auto it = std::find_if(session.documents.begin(), session.documents.end(),
                         [id](const OpenDocument& open) { return open.id == id; });
  if (it != session.documents.end()) std::rotate(it, it + 1, session.documents.end());
}

// The open document of `file`, loaded when it is not open yet.
diag::Outcome<DocumentId> open_child(EditContext& ec, const platform::Path& file) {
  Session& session = ec.session();
  if (OpenDocument* open = find_by_path(session, file)) return open->id;
  ReadOptions options;
  options.xschem_compat = ec.ctx().project().compat.xschem_compat;
  options.warnings = &ec.ctx().sink();
  options.symbol_file_exists = symbol_file_finder(file.parent_path(), session.search_path, ec.ctx().services().files);
  auto loaded = load_document(ec.ctx().services().files, file, options);
  if (!loaded) return diag::make(kNoSchematic, "schematic: cannot open " + file.generic_string());
  const DocumentId id = session.next_id++;
  session.documents.push_back(OpenDocument{id, std::move(loaded).value()});
  return id;
}

struct DescendRequest {
  std::string instance;
  std::optional<std::string> element;
  bool symbol = false;
};

diag::Outcome<platform::Path> descend_target(EditContext& ec, const InstanceRecord& instance, bool to_symbol) {
  const Symbol& symbol = ec.symbol(instance);
  if (to_symbol) {
    if (symbol.placeholder || symbol.file.empty()) {
      return diag::make(kNoSchematic, "schematic: no symbol file for " + instance.symbol);
    }
    return symbol.file;
  }
  if (!can_descend(symbol_type(symbol))) {
    return diag::make(kNoDescend, "schematic: cannot descend into " + instance.symbol +
                                      ": its type is not subcircuit or primitive");
  }
  platform::Path file = schematic_file(ec, instance, symbol);
  if (file.empty() || !ec.ctx().services().files.exists(file)) {
    return diag::make(kNoSchematic, "schematic: no schematic " + file.generic_string() + " for " + instance.symbol);
  }
  return file;
}

cmd::CommandResult descend(EditContext& ec, const DescendRequest& request) {
  Session& session = ec.session();
  std::vector<HierFrame>& stack = frames_to_change(session, ec.id());
  if (stack.size() >= kMaxHierarchyDepth) {
    return fail(kDepthLimit, "schematic: descending is limited to " + std::to_string(kMaxHierarchyDepth) + " levels");
  }
  const auto index = cmdimpl::find_instance(ec.doc(), request.instance);
  if (!index) return fail(cmdimpl::kNoObject, "schematic: no instance \"" + request.instance + "\"");
  const InstanceRecord instance = ec.doc().instances[*index];
  const auto target = descend_target(ec, instance, request.symbol);
  if (!target) return fail(target.error());
  const std::string name = get_token(instance.props, "name").value_or(request.instance);
  const std::vector<std::string> bits = expand_bus(name, &session.bus_budget);
  const std::string element = choose_element(bits, request.element);
  const DocumentId parent = ec.id();
  const auto child = open_child(ec, target.value());
  if (!child) return fail(child.error());
  stack.push_back(HierFrame{parent, child.value(), name, element});
  activate(session, child.value());  // `ec` refers to the parent no more
  return cmd::CommandResult::success(path_text(current_path(stack)));
}

cmd::CommandResult descend_body(EditContext& ec, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-symbol"}};
  const auto parsed = cmdimpl::parse_args(args, kFlags, "descend");
  if (!parsed) return fail(parsed.error());
  const auto& positional = parsed.value().positional;
  const bool symbol = parsed.value().has(0);
  if (positional.empty() || positional.size() > (symbol ? 1U : 2U)) return usage("descend");
  DescendRequest request{positional[0], std::nullopt, symbol};
  if (positional.size() == 2) request.element = positional[1];
  return descend(ec, request);
}

cmd::CommandResult ascend_body(EditContext& ec, std::span<const std::string> args) {
  if (!args.empty()) return usage("ascend");
  std::vector<HierFrame>& stack = frames_to_change(ec.session(), ec.id());
  if (stack.empty()) return fail(kAtTop, "schematic: already at the top of the hierarchy");
  const DocumentId parent = stack.back().parent;
  stack.pop_back();
  activate(ec.session(), parent);
  if (!stack.empty() && stack.back().child != parent) stack.clear();
  return cmd::CommandResult::success(path_text(current_path(stack)));
}

const Document* document_by_id(const Session& session, DocumentId id) {
  for (const OpenDocument& open : session.documents) {
    if (open.id == id) return &open.document;
  }
  return nullptr;
}

std::string file_text(const Session& session, DocumentId id) {
  const Document* doc = document_by_id(session, id);
  return doc == nullptr ? std::string{} : doc->path.generic_string();
}

cmd::CommandResult hierarchy_body(EditContext& ec, std::span<const std::string> args) {
  static constexpr std::array<std::string_view, 2> kWhat{"path", "list"};
  if (args.size() > 1) return usage("hierarchy");
  std::size_t what = 0;
  if (args.size() == 1) {
    const auto found = cmdimpl::match_keyword(args[0], kWhat);
    if (!found) return fail(found.error());
    what = found.value();
  }
  const std::span<const HierFrame> stack = frames(ec.session(), ec.id());
  if (what == 0) return cmd::CommandResult::success(path_text(current_path(stack)));
  cmd::Value::List list;
  list.emplace_back(file_text(ec.session(), stack.empty() ? ec.id() : stack.front().parent));
  for (const HierFrame& frame : stack) list.emplace_back(file_text(ec.session(), frame.child));
  return cmd::CommandResult::success(std::move(list));
}

cmd::CommandResult history_body(EditContext& ec, std::span<const std::string> args, bool redo) {
  const std::string_view option = redo ? "redo" : "undo";
  if (args.size() > 1) return usage(option);
  std::size_t steps = 1;
  if (args.size() == 1) {
    const auto n = index_value(args[0]);
    if (!n) return usage(option);
    steps = *n;
  }
  const std::size_t done =
      redo ? redo_steps(ec.session(), ec.id(), steps) : undo_steps(ec.session(), ec.id(), steps);
  return cmd::CommandResult::success(static_cast<std::int64_t>(done));
}

}  // namespace

HierPath hierarchy_path(Session& session) {
  const OpenDocument* active = active_document(session);
  return active == nullptr ? HierPath{} : current_path(frames(session, active->id));
}

cmd::CommandResult op_descend(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_view(ctx, inv, descend_body);
}

cmd::CommandResult op_ascend(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_view(ctx, inv, ascend_body);
}

cmd::CommandResult op_hierarchy(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_view(ctx, inv, hierarchy_body);
}

cmd::CommandResult op_undo(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_view(ctx, inv, [](EditContext& ec, auto args) { return history_body(ec, args, false); });
}

cmd::CommandResult op_redo(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_view(ctx, inv, [](EditContext& ec, auto args) { return history_body(ec, args, true); });
}

}  // namespace hefesto::schem::editimpl
