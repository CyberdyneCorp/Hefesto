// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Creating objects: `place`, `label`, `wire` and the graphics options
// `line`, `box`, `arc`, `polygon` and `text` (capability schematic-editing
// "Document model", "Wires", "Placing instances", "Labels and ports",
// "Graphics and text"; schematic-command "Subcommand table"). New objects
// are appended to their list ("Created objects appended"). Coordinates are
// taken as given: the pointer snap of "Snap and grid" is applied by the
// front end before it issues the command, so a replayed log gives the same
// file.
//
// Owner: EDIT.

#include <array>
#include <string>
#include <utility>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/sym/instance.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

using cmdimpl::FlagSpec;
using cmdimpl::ParsedArgs;

/// The layer of new graphics when `-layer` is not given (xschem's default
/// current layer is not specified; see the as-landed notes of tasks.md 9.2).
constexpr int kDefaultLayer = 4;

struct Orientation {
  int rotation = 0;
  int flip = 0;
};

std::optional<Orientation> orientation(const ParsedArgs& parsed, std::size_t rot_flag, std::size_t flip_flag) {
  Orientation o;
  if (parsed.has(rot_flag)) {
    const auto r = int_value(*parsed.flags[rot_flag]);
    if (!r || *r < 0 || *r > 3) return std::nullopt;
    o.rotation = *r;
  }
  if (parsed.has(flip_flag)) {
    const auto f = int_value(*parsed.flags[flip_flag]);
    if (!f || (*f != 0 && *f != 1)) return std::nullopt;
    o.flip = *f;
  }
  return o;
}

std::optional<int> layer(const ParsedArgs& parsed, std::size_t flag) {
  if (!parsed.has(flag)) return kDefaultLayer;
  const auto value = int_value(*parsed.flags[flag]);
  if (!value || *value < 0 || *value >= kLayerCount) return std::nullopt;
  return value;
}

std::string props_flag(const ParsedArgs& parsed, std::size_t flag) {
  return parsed.has(flag) ? *parsed.flags[flag] : std::string{};
}

platform::Path folder_of(const Document& doc) {
  return doc.path.empty() ? cmdimpl::absolute_path(".") : doc.path.parent_path();
}

// The reference written for a symbol chosen by `typed` ("Symbol
// references": the shortest form); as typed when no file was found.
std::string written_reference(EditContext& ec, const Symbol& symbol, const std::string& typed) {
  if (symbol.placeholder || symbol.file.empty()) return typed;
  return shortest_reference(symbol.file, folder_of(ec.doc()), ec.session().search_path);
}

struct NewInstance {
  std::string reference;
  Point origin;
  Orientation orientation;
  std::optional<std::string> props;  // nullopt: the symbol template
  std::optional<std::string> lab;
};

// Appends an instance with a unique name (InstanceNamer over the session's
// bus budget, Q-SCHEM-41) and returns its name.
std::string add_instance(EditContext& ec, const NewInstance& request) {
  Document& doc = ec.doc();
  const Symbol& symbol = ec.session().symbols.resolve(Reference{request.reference}, doc, ec.resolve_context());
  InstanceNamer names(doc, &ec.session().bus_budget);
  std::string props =
      request.props ? names.unique_properties(*request.props) : new_instance_properties(symbol, names);
  if (request.lab) props = set_token(props, "lab", *request.lab);
  InstanceRecord record;
  record.symbol = written_reference(ec, symbol, request.reference);
  record.origin = request.origin;
  record.rotation = request.orientation.rotation;
  record.flip = request.orientation.flip;
  record.props = std::move(props);
  doc.instances.push_back(std::move(record));
  ec.changed();
  return get_token(doc.instances.back().props, "name").value_or(std::string{});
}

cmd::CommandResult index_result(std::size_t size) {
  return cmd::CommandResult::success(static_cast<std::int64_t>(size - 1));
}

cmd::CommandResult place_body(EditContext& ec, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-rot", true}, FlagSpec{"-flip", true}, FlagSpec{"-props", true}};
  const auto parsed = cmdimpl::parse_args(args, kFlags, "place");
  if (!parsed) return fail(parsed.error());
  const auto& p = parsed.value();
  const auto xy = numbers(p.positional, 1, 2);
  const auto o = orientation(p, 0, 1);
  if (p.positional.size() != 3 || !xy || !o) return usage("place");
  NewInstance request{p.positional[0], {(*xy)[0], (*xy)[1]}, *o, std::nullopt, std::nullopt};
  if (p.has(2)) request.props = *p.flags[2];
  return cmd::CommandResult::success(add_instance(ec, request));
}

// `-kind` of `label`: the system-library symbol of each kind.
constexpr std::array<std::string_view, 5> kLabelKinds{"pin", "wire", "ipin", "opin", "iopin"};
constexpr std::array<std::string_view, 5> kLabelSymbols{"lab_pin.sym", "lab_wire.sym", "ipin.sym", "opin.sym",
                                                        "iopin.sym"};

cmd::CommandResult label_body(EditContext& ec, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-kind", true}, FlagSpec{"-rot", true}, FlagSpec{"-flip", true}};
  const auto parsed = cmdimpl::parse_args(args, kFlags, "label");
  if (!parsed) return fail(parsed.error());
  const auto& p = parsed.value();
  const auto xy = numbers(p.positional, 0, 2);
  const auto o = orientation(p, 1, 2);
  if (p.positional.size() != 3 || !xy || !o) return usage("label");
  std::size_t kind = 0;
  if (p.has(0)) {
    const auto found = cmdimpl::match_keyword(*p.flags[0], kLabelKinds);
    if (!found) return fail(found.error());
    kind = found.value();
  }
  const NewInstance request{std::string{kLabelSymbols.at(kind)}, {(*xy)[0], (*xy)[1]}, *o, std::nullopt,
                            p.positional[2]};
  return cmd::CommandResult::success(add_instance(ec, request));
}

cmd::CommandResult wire_body(EditContext& ec, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-props", true}};
  const auto parsed = cmdimpl::parse_args(args, kFlags, "wire");
  if (!parsed) return fail(parsed.error());
  const auto c = numbers(parsed.value().positional, 0, 4);
  if (parsed.value().positional.size() != 4 || !c) return usage("wire");
  ec.doc().wires.push_back({{(*c)[0], (*c)[1]}, {(*c)[2], (*c)[3]}, props_flag(parsed.value(), 0)});
  ec.changed();
  return index_result(ec.doc().wires.size());
}

constexpr std::array kGraphicFlags{FlagSpec{"-layer", true}, FlagSpec{"-props", true}};

// `line` and `box`: two corners.
cmd::CommandResult segment_body(EditContext& ec, std::span<const std::string> args, ObjectKind kind) {
  const std::string_view option = kind == ObjectKind::line ? "line" : "box";
  const auto parsed = cmdimpl::parse_args(args, kGraphicFlags, option);
  if (!parsed) return fail(parsed.error());
  const auto c = numbers(parsed.value().positional, 0, 4);
  const auto l = layer(parsed.value(), 0);
  if (parsed.value().positional.size() != 4 || !c || !l) return usage(option);
  std::vector<LineRecord>& list = kind == ObjectKind::line ? ec.doc().lines : ec.doc().boxes;
  list.push_back({*l, {(*c)[0], (*c)[1]}, {(*c)[2], (*c)[3]}, props_flag(parsed.value(), 1)});
  ec.changed();
  return index_result(list.size());
}

cmd::CommandResult arc_body(EditContext& ec, std::span<const std::string> args) {
  const auto parsed = cmdimpl::parse_args(args, kGraphicFlags, "arc");
  if (!parsed) return fail(parsed.error());
  const auto c = numbers(parsed.value().positional, 0, 5);
  const auto l = layer(parsed.value(), 0);
  if (parsed.value().positional.size() != 5 || !c || !l) return usage("arc");
  ec.doc().arcs.push_back({*l, {(*c)[0], (*c)[1]}, (*c)[2], (*c)[3], (*c)[4], props_flag(parsed.value(), 1)});
  ec.changed();
  return index_result(ec.doc().arcs.size());
}

cmd::CommandResult polygon_body(EditContext& ec, std::span<const std::string> args) {
  const auto parsed = cmdimpl::parse_args(args, kGraphicFlags, "polygon");
  if (!parsed) return fail(parsed.error());
  const auto& positional = parsed.value().positional;
  const auto c = numbers(positional, 0, positional.size());
  const auto l = layer(parsed.value(), 0);
  if (positional.size() < 4 || positional.size() % 2 != 0 || !c || !l) return usage("polygon");
  PolygonRecord polygon{*l, {}, props_flag(parsed.value(), 1)};
  for (std::size_t i = 0; i < c->size(); i += 2) polygon.points.push_back({(*c)[i], (*c)[i + 1]});
  ec.doc().polygons.push_back(std::move(polygon));
  ec.changed();
  return index_result(ec.doc().polygons.size());
}

cmd::CommandResult text_body(EditContext& ec, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-props", true}};
  const auto parsed = cmdimpl::parse_args(args, kFlags, "text");
  if (!parsed) return fail(parsed.error());
  const auto& positional = parsed.value().positional;
  const auto xy = numbers(positional, 0, 2);
  const auto scale = numbers(positional, 4, 2);
  const auto rot = positional.size() == 7 ? int_value(positional[2]) : std::nullopt;
  const auto flip = positional.size() == 7 ? int_value(positional[3]) : std::nullopt;
  if (positional.size() != 7 || !xy || !scale || !rot || !flip) return usage("text");
  TextRecord text{positional[6], {(*xy)[0], (*xy)[1]}, *rot, *flip, (*scale)[0], (*scale)[1],
                  props_flag(parsed.value(), 0)};
  ec.doc().texts.push_back(std::move(text));
  ec.changed();
  return index_result(ec.doc().texts.size());
}

}  // namespace

cmd::CommandResult op_place(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, place_body);
}

cmd::CommandResult op_label(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, label_body);
}

cmd::CommandResult op_wire(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, wire_body);
}

cmd::CommandResult op_line(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, [](EditContext& ec, auto args) { return segment_body(ec, args, ObjectKind::line); });
}

cmd::CommandResult op_box(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, [](EditContext& ec, auto args) { return segment_body(ec, args, ObjectKind::box); });
}

cmd::CommandResult op_arc(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, arc_body);
}

cmd::CommandResult op_polygon(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, polygon_body);
}

cmd::CommandResult op_text(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, text_body);
}

}  // namespace hefesto::schem::editimpl
