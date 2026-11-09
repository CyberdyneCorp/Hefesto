// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `tech` command (capability tech-command), `scalegrid`
// (tech-loading-and-scaling, "scalegrid command") and the session-level
// technology load shared with startup.
//
// Ported from Magic VLSI 8.3.694 commands/CmdTZ.c (CmdTech),
// commands/CmdRS.c (CmdScaleGrid) and database/DBtechname.c
// (DBTechPrintTypes), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md). Results follow Magic's Tcl build.

#include "hefesto/tech/commands.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/tech/loader.hpp"
#include "tech/build_state.hpp"

namespace hefesto::tech {

namespace {

using cmd::CommandResult;

enum TechOption : int { kLoad, kHelp, kName, kFilename, kVersion, kLambda, kPlanes, kLayers, kDrc, kLock, kUnlock, kRevert };

const std::vector<std::string_view>& option_names() {
  static const std::vector<std::string_view> kNames = {"load",   "help",   "name", "filename", "version", "lambda",
                                                       "planes", "layers", "drc",  "locked",   "unlocked", "revert"};
  return kNames;
}

constexpr std::array<std::string_view, 12> kOptionHelp = {
    "load filename [-noprompt][-[no]override]\n\t\t\t\tLoad a new technology",
    "help\t\t\tDisplay techinfo command options",
    "name\t\t\tShow current technology name",
    "filename\t\tShow current technology filename",
    "version\t\t\tShow current technology version",
    "lambda\t\t\tShow internal units per lambda",
    "planes\t\t\tShow defined planes",
    "layers\t[<layer...>]\tShow defined layers",
    "drc <rule> <layer...>\tQuery DRC ruleset",
    "locked [<layer...>]\tLock (make uneditable) layer <layer>",
    "unlocked [<layer...>]\tUnlock (make editable) layer <layer>",
    "revert [<layer...>]\tRevert lock state of layer <layer> to default",
};

CommandResult fail(std::string_view code, std::string text) {
  return CommandResult::failure(diag::make(code, std::move(text)));
}

CommandResult wrong_args() {
  return fail("TECH-CMD-001", "wrong number of arguments to command \"tech\"\n  Type \":tech help\" for help.");
}

TypeMask range_mask(std::size_t from, std::size_t to) {
  TypeMask mask;
  for (std::size_t t = from; t < to; ++t) {
    mask.set(static_cast<TileType>(t));
  }
  return mask;
}

TypeMask all_but_space_and_drc() {
  return range_mask(kMagnet, kMaxTileTypes);
}

// DBUserLayerBits: space and the user types.
TypeMask user_layer_bits(const Technology& tech) {
  return range_mask(kFirstUserType, tech.user_type_end()) | TypeMask::of(kSpace);
}

// --- DBTechPrintTypes ------------------------------------------------------

std::vector<std::string> names_for_listing(const Technology& tech, TileType type, bool builtin) {
  return tech.type_names(type, builtin);  // aliases are skipped for user types only
}

std::string longest_name(const std::vector<std::string>& names) {
  std::string keep;
  for (const auto& name : names) {
    if (keep.empty() || name.size() > keep.size()) {
      keep = name;
    }
  }
  return keep;
}

std::vector<TileType> listed_types(const Technology& tech, const TypeMask& mask) {
  std::vector<TileType> types;
  for (std::size_t t = kFirstUserType; t < tech.user_type_end(); ++t) {
    if (mask.test(static_cast<TileType>(t))) {
      types.push_back(static_cast<TileType>(t));
    }
  }
  for (const TileType t : {kMagnet, kFence, kRotate}) {
    if (mask.test(t)) {
      types.push_back(t);
    }
  }
  return types;
}

/// `tech layers <list>` form: the longest name per type, space separated.
std::string print_types_list(const Technology& tech, const TypeMask& mask) {
  std::string out;
  for (const TileType t : listed_types(tech, mask)) {
    if (!out.empty()) {
      out += ' ';
    }
    out += longest_name(names_for_listing(tech, t, t < kFirstUserType));
  }
  return out;
}

/// `tech layers` form: "Layer names are:" and one line per type.
std::string print_types_text(const Technology& tech, const TypeMask& mask) {
  std::string out = "Layer names are:\n";
  for (const TileType t : listed_types(tech, mask)) {
    std::string line;
    for (const auto& name : names_for_listing(tech, t, t < kFirstUserType)) {
      line += line.empty() ? "    " + name : " or " + name;
    }
    out += line + "\n";
  }
  return out;
}

// --- options -------------------------------------------------------------------

CommandResult tech_help() {
  std::string text = "Tech commands have the form \"tech option\",\nwhere option is one of:\n";
  for (const auto help : kOptionHelp) {
    text += "    " + std::string{help} + "\n";
  }
  return CommandResult::success(std::move(text));
}

CommandResult tech_lambda(editor::EditorState& ed, const cmd::Invocation& inv, bool magic_compat) {
  const auto& a = inv.args;
  if (a.size() > 1 && str_is_int(a[1])) {
    if (!magic_compat) {
      return fail("TECH-CMD-004", "tech lambda N [D] does not rescale anything; use scalegrid to change the grid");
    }
    GridScale grid = ed.grid;
    grid.internal = str_to_int(a[1]);
    if (a.size() == 2) {
      grid.lambda = 1;
    } else if (str_is_int(a[2])) {
      grid.lambda = str_to_int(a[2]);
    }
    ed.grid = reduce(grid);
  }
  return CommandResult::success(cmd::Value::List{cmd::Value{ed.grid.lambda}, cmd::Value{ed.grid.internal}});
}

CommandResult tech_planes(const Technology& tech) {
  cmd::Value::List names;
  for (std::size_t p = 0; p < tech.plane_count(); ++p) {
    names.emplace_back(std::string{tech.plane_name(static_cast<PlaneId>(p))});
  }
  return CommandResult::success(std::move(names));
}

/// A `tech` type-list argument; `star` is the set used for `*`.
TypeMask parse_layers_arg(const Technology& tech, const std::string& arg, const TypeMask& star,
                          diag::DiagnosticSink& sink) {
  if (arg == "*") {
    return star;
  }
  Reporter rep(sink);
  return name_mask(tech.data(), arg, &rep, Noise::warning).types;
}

CommandResult tech_layers(const Technology& tech, const cmd::Invocation& inv, diag::DiagnosticSink& sink) {
  if (inv.args.size() == 1) {
    return CommandResult::success(print_types_text(tech, all_but_space_and_drc()));
  }
  if (inv.args.size() != 2) {
    return wrong_args();
  }
  const TypeMask mask = parse_layers_arg(tech, inv.args[1], all_but_space_and_drc(), sink);
  return CommandResult::success(print_types_list(tech, mask));
}

void set_technology(editor::EditorState& ed, std::shared_ptr<const Technology> tech) {
  ed.technology = std::move(tech);
  ed.locked_types = all_but_space_and_drc() & ~ed.technology->active_types();
}

CommandResult tech_lock(editor::EditorState& ed, const cmd::Invocation& inv, int option, diag::DiagnosticSink& sink) {
  const Technology& tech = *ed.technology;
  if (inv.args.size() == 1) {
    const TypeMask active = tech.active_types() & all_but_space_and_drc();
    const TypeMask listed = option == kLock ? all_but_space_and_drc() & ~active : active;
    return CommandResult::success(print_types_list(tech, listed));
  }
  if (inv.args.size() != 2) {
    return wrong_args();
  }
  const TypeMask mask = parse_layers_arg(tech, inv.args[1], user_layer_bits(tech), sink);
  set_technology(ed, option == kLock ? lock_types(tech, mask) : unlock_types(tech, mask));
  return CommandResult::success();
}

CommandResult tech_revert(editor::EditorState& ed, const cmd::Invocation& inv, diag::DiagnosticSink& sink) {
  const Technology& tech = *ed.technology;
  if (inv.args.size() > 2) {
    return wrong_args();
  }
  std::optional<TypeMask> mask;
  if (inv.args.size() == 2 && inv.args[1] != "*") {  // Q-TECH-25: `*` re-locks like no argument
    mask = parse_layers_arg(tech, inv.args[1], TypeMask{}, sink);
  }
  set_technology(ed, revert_types(tech, mask));
  return CommandResult::success();
}

CommandResult tech_drc(const Technology& tech, const cmd::Invocation& inv) {
  if (inv.args.size() < 3) {
    return wrong_args();
  }
  for (std::size_t i = 2; i < inv.args.size(); ++i) {
    if (!tech.find_type(inv.args[i]).ok()) {
      return fail("TECH-CMD-006", "Unrecognized layer (type) name \"" + inv.args[i] + "\"\nNo such layer " +
                                      inv.args[i]);
    }
  }
  return fail("TECH-CMD-005", "tech drc: the DRC rule set is not available in this version (drc-* capabilities, M3)");
}

/// The Tcl result Magic's `tech load` sets when the load fails (CmdTZ.c:602-619,
/// MAGIC_WRAPPER): no period, a trailing LF, and no TxError line. Empty for
/// failures that are reported as diagnostics only.
cmd::Value load_failure_result(std::string_view code) {
  if (code == "TECH-FILE-002") {
    return cmd::Value{std::string{"Technology file does not exist or is not readable\n"}};
  }
  if (code == "TECH-LOAD-001") {
    return cmd::Value{std::string{"Error in loading technology file\n"}};
  }
  return {};
}

struct LoadArgs {
  bool noprompt = false;
  std::size_t words = 0;  // Magic's locargc after the flag scan
};

LoadArgs scan_load_flags(const cmd::Invocation& inv) {
  // Magic counts "tech" itself: argv = tech load <file> [flags].
  LoadArgs out{false, inv.args.size() + 1};
  while (out.words > 3 && inv.args[out.words - 2].starts_with("-")) {
    const std::string& flag = inv.args[out.words - 2];
    if (flag == "-noprompt") {
      out.noprompt = true;
    } else if (flag != "-override" && flag != "-nooverride") {
      break;
    }
    --out.words;
  }
  return out;
}

CommandResult tech_load(cmd::CommandContext& ctx, editor::EditorState& ed, const cmd::Invocation& inv) {
  const LoadArgs args = scan_load_flags(inv);
  if (args.words != 3) {
    return fail("TECH-CMD-002", "Usage: tech load <filename> [-noprompt] [-override]");
  }
  if (!args.noprompt && layout_has_paint(ed.cells, ed.technology->plane_count())) {
    return fail("TECH-CMD-003",
                "Technology file (re)loading may invalidate the existing layout.  Continue? (no: this session cannot "
                "ask; use -noprompt)");
  }
  SessionLoadOptions options;
  options.magic_compat = ctx.project().compat.magic_compat;
  auto status = load_session_technology(ed, ctx.services(), inv.args[1], options, ctx.sink());
  if (status) {
    return CommandResult::success();
  }
  CommandResult result = CommandResult::failure(status.error());
  result.value = load_failure_result(status.error().code);
  return result;
}

/// `tech version`. Magic (CmdTZ.c:221-224) sets the raw version string and
/// appends the description with Tcl_AppendElement, which quotes only the
/// description, so a multi-word version (words joined by LF) is not a proper
/// list element. magic-compat OFF returns a proper two-element list.
CommandResult tech_version(const Technology& tech, bool magic_compat) {
  const auto& v = tech.version();
  std::string version = v.version_lines.empty() ? std::string{} : v.version_lines.back();
  std::string description = v.description_lines.empty() ? std::string{} : v.description_lines.back();
  if (!magic_compat) {
    return CommandResult::success(cmd::Value::List{cmd::Value{std::move(version)}, cmd::Value{std::move(description)}});
  }
  // TclNeedSpace: a separator unless the result is empty or ends in a space.
  if (!version.empty() && version.back() != ' ' && version.back() != '\n' && version.back() != '\t') {
    version += ' ';
  }
  return CommandResult::success(version + cmd::Value{cmd::Value::List{cmd::Value{std::move(description)}}}.to_string());
}

CommandResult bad_option(const std::string& option, int lookup) {
  if (lookup == kAmbiguous) {
    return fail("TECH-CMD-001", "Ambiguous techinfo option: \"" + option + "\"\n  Type \":tech help\" for help.");
  }
  return fail("TECH-CMD-001", "\"" + option + "\" isn't a valid tech option.  Type \":tech help\" for help.");
}

CommandResult run_option(cmd::CommandContext& ctx, editor::EditorState& ed, const cmd::Invocation& inv, int option) {
  const Technology& tech = *ed.technology;
  switch (option) {
    case kName: return CommandResult::success(tech.name());
    case kFilename: return CommandResult::success(tech.file_name());
    case kVersion: return tech_version(tech, ctx.project().compat.magic_compat);
    case kLambda: return tech_lambda(ed, inv, ctx.project().compat.magic_compat);
    case kPlanes: return tech_planes(tech);
    case kLayers: return tech_layers(tech, inv, ctx.sink());
    case kDrc: return tech_drc(tech, inv);
    case kLock:
    case kUnlock: return tech_lock(ed, inv, option, ctx.sink());
    case kRevert: return tech_revert(ed, inv, ctx.sink());
    case kLoad: return tech_load(ctx, ed, inv);
    default: return tech_help();
  }
}

CommandResult tech_command(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const int option = inv.args.empty() ? kHelp : keyword_lookup(inv.args[0], option_names(), true);
  if (option < 0) {
    return bad_option(inv.args[0], option);
  }
  if (option == kHelp) {
    return tech_help();
  }
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  return run_option(ctx, *editor.value(), inv, option);
}

// --- scalegrid ---------------------------------------------------------------

std::optional<std::pair<std::int64_t, std::int64_t>> parse_scalegrid(const std::vector<std::string>& args) {
  if (args.empty() || args.size() > 2) {
    return std::nullopt;
  }
  std::string first = args[0];
  std::string second;
  if (args.size() == 2) {
    second = args[1];
  } else {
    auto sep = first.find(':');
    if (sep == std::string::npos) {
      sep = first.find('/');
    }
    if (sep == std::string::npos) {
      return std::nullopt;
    }
    second = first.substr(sep + 1);
    first.resize(sep);
  }
  if (!str_is_int(first) || !str_is_int(second)) {
    return std::nullopt;
  }
  const std::int64_t n = str_to_int(first);
  const std::int64_t d = str_to_int(second);
  if (n <= 0 || d <= 0) {
    return std::nullopt;
  }
  return std::make_pair(n, d);
}

std::string scale_report(const GridScale& grid) {
  return std::to_string(grid.internal) + " Magic internal unit" + (grid.internal == 1 ? "" : "s") + " = " +
         std::to_string(grid.lambda) + " Lambda";
}

CommandResult scalegrid_command(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const auto parsed = parse_scalegrid(inv.args);
  if (!parsed) {
    return fail("TECH-GRID-001", "Usage:  scalegrid a b, where a and b are strictly positive integers");
  }
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return CommandResult::failure(editor.error());
  }
  editor::EditorState& ed = *editor.value();
  if (parsed->first != parsed->second) {
    const GridScale factor = reduce(GridScale{parsed->first, parsed->second});
    const std::int64_t a = factor.internal;
    const std::int64_t b = factor.lambda;
    if (auto limit = check_grid_limit(*ed.technology, a, b); !limit) {
      return CommandResult::failure(limit.error());
    }
    (void)scale_layout(ed, b, a);
    ed.grid = reduce(GridScale{ed.grid.internal * b, ed.grid.lambda * a});
  }
  return CommandResult::success(scale_report(ed.grid));
}

// --- session load ------------------------------------------------------------

diag::Outcome<platform::Path> locate(platform::PlatformServices& services, std::string_view name,
                                     LoadOptions& options, diag::DiagnosticSink& sink) {
  auto path = resolve_tech_file(services.files, name, options, &options.include_suffix);
  if (path) {
    return path;
  }
  if (auto pdk = services.pdks.find_technology(name)) {
    return pdk;
  }
  sink.emit(path.error());
  return path.error();
}

void check_existing_layout(const editor::EditorState& ed, std::size_t old_planes, std::size_t new_planes,
                           diag::DiagnosticSink& sink) {
  if (!layout_has_paint(ed.cells, old_planes)) {
    return;
  }
  const std::string changed = old_planes != new_planes ? "Warning:  Number of planes has changed.  " : "";
  sink.emit(diag::make("TECH-LOAD-004", changed + "Existing layout may be invalid."));
}

}  // namespace

diag::Status load_session_technology(editor::EditorState& editor, platform::PlatformServices& services,
                                     std::string_view name, const SessionLoadOptions& options,
                                     diag::DiagnosticSink& sink) {
  LoadOptions load;
  load.magic_compat = options.magic_compat;
  auto path = locate(services, name, load, sink);
  if (!path) {
    if (options.precheck) {
      return diag::make("TECH-FILE-002", "Technology file does not exist or is not readable.");
    }
    return path.error();
  }
  if (options.precheck) {
    if (auto pre = precheck_tech_file(services.files, path.value()); !pre) {
      return pre.error();
    }
  }
  auto tech = load_technology(services.files, path.value(), load, sink);
  if (!tech) {
    return tech.error();
  }
  const std::size_t old_planes = editor.technology->plane_count();
  const std::size_t new_planes = tech.value()->plane_count();
  editor.grid = apply_post_load(*tech.value(), editor.grid, sink);
  check_existing_layout(editor, old_planes, new_planes, sink);
  set_technology(editor, std::move(tech.value()));
  if (new_planes != old_planes) {
    editor.cells.for_each([new_planes](db::CellDef& def) { def.set_plane_count(new_planes); });
  }
  return diag::Done{};
}

bool register_tech_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"tech", "tech option ?args?",
                                       "Queries or reloads the technology: load, help, name, filename, version, "
                                       "lambda, planes, layers, drc, locked, unlocked, revert (tech-command).",
                                       tech_command})
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"scalegrid", "scalegrid a b",
                                  "Changes the internal grid so that the internal unit becomes a/b of its former size, "
                                  "rescaling all layout (tech-loading-and-scaling).",
                                  scalegrid_command})
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::tech
