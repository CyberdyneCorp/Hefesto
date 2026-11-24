// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `setprop` and `replace` (capability schematic-editing "Property editing",
// "Placing instances"; schematic-file-format "Property strings" for the
// update rules of set_token). A new instance `name` is made unique among
// the other instances ("Instance names").
//
// Owner: EDIT.

#include <array>
#include <optional>
#include <string>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/sym/instance.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

constexpr std::array<std::string_view, 3> kSetpropKinds{"instance", "wire", "schematic"};
constexpr std::array<std::string_view, 6> kGlobalTags{"K", "G", "V", "S", "F", "E"};

std::optional<std::string_view> value_at(std::span<const std::string> args, std::size_t index) {
  return index < args.size() ? std::optional<std::string_view>{args[index]} : std::nullopt;
}

// `name` unique among the instances of `doc` other than `self`.
std::string unique_name(EditContext& ec, std::size_t self, std::string_view name) {
  InstanceNamer names(&ec.session().bus_budget);
  for (std::size_t i = 0; i < ec.doc().instances.size(); ++i) {
    if (i != self) names.add(get_token(ec.doc().instances[i].props, "name").value_or(std::string{}));
  }
  return names.unique(name);
}

cmd::CommandResult set_instance(EditContext& ec, std::span<const std::string> args) {
  if (args.size() < 2 || args.size() > 3) return usage("setprop");
  const auto index = cmdimpl::find_instance(ec.doc(), args[0]);
  if (!index) return fail(cmdimpl::kNoObject, "schematic: no instance \"" + args[0] + "\"");
  std::optional<std::string_view> value = value_at(args, 2);
  std::string name;
  if (args[1] == "name" && value && !value->empty()) {
    name = unique_name(ec, *index, *value);
    value = name;
  }
  std::string& props = ec.doc().instances[*index].props;
  props = set_token(props, args[1], value);
  ec.changed();
  return cmd::CommandResult::success();
}

cmd::CommandResult set_wire(EditContext& ec, std::span<const std::string> args) {
  if (args.size() < 2 || args.size() > 3) return usage("setprop");
  const auto index = index_value(args[0]);
  if (!index || *index >= ec.doc().wires.size()) return fail(cmdimpl::kNoObject, "schematic: no wire \"" + args[0] + "\"");
  std::string& props = ec.doc().wires[*index].props;
  props = set_token(props, args[1], value_at(args, 2));
  ec.changed();
  return cmd::CommandResult::success();
}

cmd::CommandResult set_global(EditContext& ec, std::span<const std::string> args) {
  if (args.size() != 2) return usage("setprop");
  const auto tag = cmdimpl::match_keyword(args[0], kGlobalTags);
  if (!tag) return fail(tag.error());
  GlobalProperties& g = ec.doc().globals;
  const std::array<std::string*, 6> texts{&g.k, &g.g, &g.v, &g.s, &g.f, &g.e};
  *texts.at(tag.value()) = args[1];
  ec.changed();
  return cmd::CommandResult::success();
}

cmd::CommandResult setprop_body(EditContext& ec, std::span<const std::string> args) {
  if (args.empty()) return usage("setprop");
  const auto kind = cmdimpl::match_keyword(args[0], kSetpropKinds);
  if (!kind) return fail(kind.error());
  const auto rest = args.subspan(1);
  if (kind.value() == 0) return set_instance(ec, rest);
  if (kind.value() == 1) return set_wire(ec, rest);
  return set_global(ec, rest);
}

// "Replacing the symbol of selected instances SHALL change only the
// reference and keep the properties."
cmd::CommandResult replace_body(EditContext& ec, std::span<const std::string> args) {
  if (args.size() != 2) return usage("replace");
  const auto index = cmdimpl::find_instance(ec.doc(), args[0]);
  if (!index) return fail(cmdimpl::kNoObject, "schematic: no instance \"" + args[0] + "\"");
  const Symbol& symbol = ec.session().symbols.resolve(Reference{args[1]}, ec.doc(), ec.resolve_context());
  std::string reference = args[1];
  if (!symbol.placeholder && !symbol.file.empty()) {
    const platform::Path folder =
        ec.doc().path.empty() ? cmdimpl::absolute_path(".") : ec.doc().path.parent_path();
    reference = shortest_reference(symbol.file, folder, ec.session().search_path);
  }
  ec.doc().instances[*index].symbol = std::move(reference);
  ec.changed();
  return cmd::CommandResult::success();
}

}  // namespace

cmd::CommandResult op_setprop(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, setprop_body);
}

cmd::CommandResult op_replace(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, replace_body);
}

}  // namespace hefesto::schem::editimpl
