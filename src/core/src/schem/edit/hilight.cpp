// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `hilight` (capability schematic-editing "Net highlighting"). Highlights
// are session state (Session::edit.highlights), never saved and never an
// undo step. Each is stored at the hierarchy path where it was made; what
// a schematic shows is computed when asked (`hilight list`):
//   - the highlights made at its own path;
//   - going down the descents from the top, the child nets whose port name
//     is a pin of the descended instance on a highlighted parent net, in
//     the parent's colour ("Highlight crosses into child");
//   - the parent nets on the pins of an instance whose schematic, one level
//     down, has a highlighted net named as the pin (highlights made in a
//     child shown when ascending);
//   - highlighted global nets, at every level.
//
// Owner: EDIT.

#include <array>
#include <map>
#include <set>
#include <string>

#include "hefesto/schem/file/properties.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

constexpr std::array<std::string_view, 5> kHilightWhat{"net", "selection", "instance", "clear", "list"};
enum HilightWhat : std::size_t { kNet, kSelection, kInstance, kClear, kList };

using Shown = std::map<std::pair<bool, std::string>, int>;  // (instance, name) -> colour

int take_colour(EditSession& edit) {
  const int colour = edit.next_colour;
  if (edit.increment_highlight_colour) edit.next_colour = (edit.next_colour + 1) % kHighlightColours;
  return colour;
}

void add(EditContext& ec, std::string name, bool instance) {
  EditSession& edit = ec.session().edit;
  const std::string path = path_text(hierarchy_path(ec.session()));
  for (const Highlight& h : edit.highlights) {
    if (h.path == path && h.name == name && h.instance == instance) return;
  }
  edit.highlights.insert(Highlight{path, std::move(name), instance, take_colour(edit)});
}

void stored_at(const EditSession& edit, const std::string& path, Shown& shown) {
  for (const Highlight& h : edit.highlights) {
    if (h.path == path) shown.try_emplace({h.instance, h.name}, h.colour);
  }
}

OpenDocument* open_by_id(Session& session, DocumentId id) {
  for (OpenDocument& open : session.documents) {
    if (open.id == id) return &open;
  }
  return nullptr;
}

// The pins and their nets of instance `name` in `d`.
std::vector<std::pair<std::string, std::string>> pin_nets_of(const DocumentNets& d, std::string_view name) {
  std::vector<std::pair<std::string, std::string>> pins;
  for (std::size_t i = 0; i < d.nets.instances.size() && i < d.naming.pin_nets.size(); ++i) {
    if (d.nets.instances[i].name != name) continue;
    for (std::size_t p = 0; p < d.nets.instances[i].pins.size() && p < d.naming.pin_nets[i].size(); ++p) {
      pins.emplace_back(d.nets.instances[i].pins[p].name, d.naming.pin_nets[i][p]);
    }
  }
  return pins;
}

// Down one descent: the child nets named as the pins on highlighted nets.
Shown propagate_down(EditContext& ec, const HierFrame& frame, const Shown& parent_shown) {
  Shown child;
  OpenDocument* parent = open_by_id(ec.session(), frame.parent);
  if (parent == nullptr) return child;
  const DocumentNets d = document_nets(ec, *parent);
  for (const auto& [pin, net] : pin_nets_of(d, frame.instance)) {
    const auto it = parent_shown.find({false, net});
    if (it != parent_shown.end()) child.try_emplace({false, pin}, it->second);
  }
  return child;
}

// Up one level: the nets of the active document on the pins of instances
// whose child path has a highlight named as the pin.
void propagate_up(EditContext& ec, const DocumentNets& d, const std::string& path, Shown& shown) {
  for (const Highlight& h : ec.session().edit.highlights) {
    if (h.instance || !h.path.starts_with(path) || h.path.size() <= path.size()) continue;
    const std::string element = h.path.substr(path.size(), h.path.size() - path.size() - 1);
    if (element.find('.') != std::string::npos) continue;  // more than one level down
    for (const auto& [pin, net] : pin_nets_of(d, element)) {
      if (pin == h.name && !net.empty()) shown.try_emplace({false, net}, h.colour);
    }
  }
}

void globals(EditContext& ec, const DocumentNets& d, Shown& shown) {
  const std::set<std::string> global(d.naming.globals.begin(), d.naming.globals.end());
  for (const Highlight& h : ec.session().edit.highlights) {
    if (!h.instance && global.contains(h.name)) shown.try_emplace({false, h.name}, h.colour);
  }
}

Shown shown_here(EditContext& ec) {
  const std::vector<HierFrame>& stack = ec.session().edit.hierarchy;
  const HierPath path = hierarchy_path(ec.session());
  Shown shown;
  HierPath walked;
  stored_at(ec.session().edit, ".", shown);
  for (std::size_t i = 0; i < path.elements.size() && i < stack.size(); ++i) {
    Shown below = propagate_down(ec, stack[i], shown);
    walked.elements.push_back(path.elements[i]);
    stored_at(ec.session().edit, path_text(walked), below);
    shown = std::move(below);
  }
  const DocumentNets d = document_nets(ec);
  propagate_up(ec, d, path_text(path), shown);
  globals(ec, d, shown);
  return shown;
}

cmd::Value shown_value(const Shown& shown) {
  cmd::Value::List list;
  for (const auto& [key, colour] : shown) {
    list.emplace_back(cmd::Value::List{cmd::Value{key.first ? "instance" : "net"}, cmd::Value{key.second},
                                       cmd::Value{colour}});
  }
  return list;
}

std::set<std::string> nets_of_selection(EditContext& ec) {
  const DocumentNets d = document_nets(ec);
  std::set<std::string> nets;
  for (const ObjectRef& ref : ec.selection()) {
    if (ref.kind == ObjectKind::wire && ref.index < d.naming.wire_nets.size()) nets.insert(d.naming.wire_nets[ref.index]);
    if (ref.kind == ObjectKind::instance && ref.index < d.naming.pin_nets.size()) {
      nets.insert(d.naming.pin_nets[ref.index].begin(), d.naming.pin_nets[ref.index].end());
    }
  }
  nets.erase(std::string{});
  return nets;
}

cmd::CommandResult clear(EditContext& ec, std::span<const std::string> rest) {
  if (rest.size() > 1 || (rest.size() == 1 && rest[0] != "all")) return usage("hilight");
  EditSession& edit = ec.session().edit;
  if (!rest.empty()) {
    edit.highlights.clear();
    edit.next_colour = 0;
    return cmd::CommandResult::success();
  }
  const std::string path = path_text(hierarchy_path(ec.session()));
  const std::set<std::string> nets = nets_of_selection(ec);
  std::erase_if(edit.highlights, [&](const Highlight& h) { return h.path == path && nets.contains(h.name); });
  return cmd::CommandResult::success();
}

cmd::CommandResult hilight_word(EditContext& ec, std::size_t what, std::span<const std::string> rest) {
  switch (what) {
    case kNet:
      if (rest.size() != 1) return usage("hilight");
      add(ec, rest[0], false);
      return cmd::CommandResult::success();
    case kInstance:
      if (rest.size() != 1) return usage("hilight");
      if (!cmdimpl::find_instance(ec.doc(), rest[0])) {
        return fail(cmdimpl::kNoObject, "schematic: no instance \"" + rest[0] + "\"");
      }
      add(ec, rest[0], true);
      return cmd::CommandResult::success();
    case kSelection:
      if (!rest.empty()) return usage("hilight");
      for (const std::string& net : nets_of_selection(ec)) add(ec, net, false);
      return cmd::CommandResult::success();
    case kClear:
      return clear(ec, rest);
    default:
      if (!rest.empty()) return usage("hilight");
      return cmd::CommandResult::success(shown_value(shown_here(ec)));
  }
}

cmd::CommandResult hilight_body(EditContext& ec, std::span<const std::string> args) {
  if (args.empty()) return usage("hilight");
  const auto what = cmdimpl::match_keyword(args[0], kHilightWhat);
  if (!what) return fail(what.error());
  return hilight_word(ec, what.value(), args.subspan(1));
}

}  // namespace

cmd::CommandResult op_hilight(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_view(ctx, inv, hilight_body);
}

}  // namespace hefesto::schem::editimpl
