// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The read-only and netlisting options of `schematic` that CMD implements:
// getprop, instances, netlist and check (capability schematic-command
// "Subcommand table", "Return values and errors in scripts").
//
// Owner: CMD.

#include <array>
#include <charconv>
#include <string>
#include <utility>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/netlist.hpp"
#include "internal.hpp"

namespace hefesto::schem::cmdimpl {

namespace {

constexpr std::array<std::string_view, 4> kGetpropKinds{"instance", "symbol", "wire", "schematic"};
constexpr std::array<std::string_view, 6> kGlobalTags{"K", "G", "V", "S", "F", "E"};

std::optional<std::string> key_at(std::span<const std::string> args, std::size_t index) {
  return index < args.size() ? std::optional<std::string>{args[index]} : std::nullopt;
}

std::optional<std::size_t> parse_index(std::string_view text) {
  std::size_t value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size()) return std::nullopt;
  return value;
}

cmd::CommandResult getprop_instance(const Document& document, std::span<const std::string> args) {
  const auto index = find_instance(document, args[0]);
  if (!index) return fail(kNoObject, "schematic: no instance \"" + args[0] + "\"");
  return cmd::CommandResult::success(property_value(document.instances[*index].props, key_at(args, 1)));
}

cmd::CommandResult getprop_symbol(cmd::CommandContext& ctx, Session& session, const Document& document,
                                  std::span<const std::string> args) {
  const ResolveContext context{session.search_path, ctx.services().files, &ctx.sink(),
                               ctx.project().compat.xschem_compat};
  const Symbol& symbol = session.symbols.resolve(Reference{args[0]}, document, context);
  if (symbol.placeholder) return fail(kNoObject, "schematic: no symbol \"" + args[0] + "\"");
  return cmd::CommandResult::success(property_value(symbol.property(), key_at(args, 1)));
}

cmd::CommandResult getprop_wire(const Document& document, std::span<const std::string> args) {
  const auto index = parse_index(args[0]);
  if (!index || *index >= document.wires.size()) return fail(kNoObject, "schematic: no wire \"" + args[0] + "\"");
  return cmd::CommandResult::success(property_value(document.wires[*index].props, key_at(args, 1)));
}

cmd::CommandResult getprop_schematic(const Document& document, std::span<const std::string> args) {
  if (args.empty()) return cmd::CommandResult::success(document.header_text);
  const auto tag = match_keyword(args[0], kGlobalTags);
  if (!tag) return fail(tag.error());
  const GlobalProperties& g = document.globals;
  const std::array<const std::string*, 6> texts{&g.k, &g.g, &g.v, &g.s, &g.f, &g.e};
  return cmd::CommandResult::success(*texts.at(tag.value()));
}

// `-hier`, `-top`, `-flat` and the other flags of `netlist`, in this order.
constexpr std::array kNetlistFlags{FlagSpec{"-hier"},     FlagSpec{"-top"},      FlagSpec{"-flat"},
                                   FlagSpec{"-lvs"},      FlagSpec{"-topsubckt"}, FlagSpec{"-lvsignore"},
                                   FlagSpec{"-noprefix"}, FlagSpec{"-buschar", true}, FlagSpec{"-uppercase"},
                                   FlagSpec{"-force"}};
enum NetlistFlag : std::size_t { kHier, kTop, kFlat, kLvs, kTopSubckt, kLvsIgnore, kNoPrefix, kBusChar, kUpper, kForce };

std::optional<NetlistOptions> netlist_options(cmd::CommandContext& ctx, const ParsedArgs& parsed) {
  const int modes = int{parsed.has(kHier)} + int{parsed.has(kTop)} + int{parsed.has(kFlat)};
  if (modes > 1 || (parsed.has(kBusChar) && parsed.flags[kBusChar]->size() != 2)) return std::nullopt;
  NetlistOptions options;
  if (parsed.has(kTop)) options.hierarchy = Hierarchy::top;
  if (parsed.has(kFlat)) options.hierarchy = Hierarchy::flat;
  options.lvs_netlist = parsed.has(kLvs);
  options.top_is_subckt = parsed.has(kTopSubckt);
  options.lvs_ignore = parsed.has(kLvsIgnore);
  options.spiceprefix = !parsed.has(kNoPrefix);
  options.bus_replacement_char = parsed.flags[kBusChar].value_or(std::string{});
  options.uppercase_subckt = parsed.has(kUpper);
  options.portable_paths = ctx.project().portable_netlist_paths;
  return options;
}

// Stores the computed `lab` values (Q-SCHEM-29) in the open documents they
// belong to, without marking them modified ("Document and session state").
void apply_labels(Session& session, const std::vector<ComputedLabel>& labels) {
  for (const ComputedLabel& label : labels) {
    for (OpenDocument& open : session.documents) {
      if (open.document.path != label.schematic || label.instance >= open.document.instances.size()) continue;
      std::string& props = open.document.instances[label.instance].props;
      props = set_token(props, "lab", label.lab);
    }
  }
}

std::string error_list(const std::vector<diag::Diagnostic>& diagnostics) {
  std::string text;
  for (const diag::Diagnostic& d : diagnostics) {
    if (d.severity != diag::Severity::error) continue;
    if (!text.empty()) text += '\n';
    text += d.text;
  }
  return text.empty() ? std::string{"schematic: the netlist run failed"} : text;
}

// Runs the netlister on the active document and publishes its diagnostics.
NetlistResult run_netlist(cmd::CommandContext& ctx, Session& session, const NetlistOptions& options) {
  NetlistResult result = netlist(session, active_document(session)->document, options, ctx.project().compat);
  for (const diag::Diagnostic& d : result.diagnostics) ctx.emit(d);
  apply_labels(session, result.labels);
  return result;
}

}  // namespace

cmd::CommandResult option_getprop(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  if (args.empty()) return usage("getprop");
  const auto kind = match_keyword(args[0], kGetpropKinds);
  if (!kind) return fail(kind.error());
  const std::span<const std::string> rest = args.subspan(1);
  const Document& document = active_document(session)->document;
  if (kind.value() == 3) return rest.size() > 1 ? usage("getprop") : getprop_schematic(document, rest);
  if (rest.empty() || rest.size() > 2) return usage("getprop");
  if (kind.value() == 0) return getprop_instance(document, rest);
  if (kind.value() == 1) return getprop_symbol(ctx, session, document, rest);
  return getprop_wire(document, rest);
}

cmd::CommandResult option_instances(cmd::CommandContext& /*ctx*/, Session& session,
                                    std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-pattern", true}};
  const auto parsed = parse_args(args, kFlags, "instances");
  if (!parsed || !parsed.value().positional.empty()) return parsed ? usage("instances") : fail(parsed.error());
  const std::string pattern = parsed.value().flags[0].value_or("*");
  cmd::Value::List list;
  for (const InstanceRecord& instance : active_document(session)->document.instances) {
    const std::string name = get_token(instance.props, "name").value_or(std::string{});
    if (!glob_match(pattern, name)) continue;
    list.emplace_back(cmd::Value::List{name, instance.symbol, instance.origin.x, instance.origin.y,
                                       instance.rotation, instance.flip});
  }
  return cmd::CommandResult::success(std::move(list));
}

cmd::CommandResult option_netlist(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  const auto parsed = parse_args(args, kNetlistFlags, "netlist");
  if (!parsed) return fail(parsed.error());
  const auto options = netlist_options(ctx, parsed.value());
  if (!options || parsed.value().positional.size() > 1) return usage("netlist");
  const NetlistResult result = run_netlist(ctx, session, *options);
  const bool to_file = !parsed.value().positional.empty();
  if (to_file && (!result.failed || parsed.value().has(kForce))) {
    const auto written = ctx.services().files.write_atomic(absolute_path(parsed.value().positional.front()), result.text);
    if (!written) return fail(written.error());
  }
  if (result.failed) return fail(kNetlistFailed, error_list(result.diagnostics));
  return cmd::CommandResult::success(to_file ? std::string{} : result.text);
}

cmd::CommandResult option_check(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  if (!args.empty()) return usage("check");
  const NetlistResult result = run_netlist(ctx, session, NetlistOptions{});
  if (result.failed) return fail(kNetlistFailed, error_list(result.diagnostics));
  cmd::Value::List list;
  for (const diag::Diagnostic& d : result.diagnostics) list.emplace_back(d.text);
  return cmd::CommandResult::success(std::move(list));
}

}  // namespace hefesto::schem::cmdimpl
