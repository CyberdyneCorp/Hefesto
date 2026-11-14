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
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/prompt.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/editor/coordinate_units.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/wiring/wiring.hpp"
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
    "version\t\tShow current technology version",
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

// CmdTech's wrongNumArgs: the first line is TxPrintf (stdout), the usage
// hint TxError (Magic ref: commands/CmdTZ.c:635-645).
CommandResult wrong_args() {
  CommandResult result = fail("TECH-CMD-001", "  Type \":tech help\" for help.");
  result.diagnostics.insert(result.diagnostics.begin(),
                            diag::make("TECH-CMD-007", "wrong number of arguments to command \"tech\""));
  return result;
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
  rep.locate_after_load(tech.data().file_name, tech.data().end_line);
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

/// `tech drc <rule> <layer> [<layer2>]` (CmdTech TECH_DRC): the value of the
/// session's compiled rule set in display units (TECH-CMD-005 is retired).
CommandResult tech_drc(cmd::CommandContext& ctx, const editor::EditorState& ed, const cmd::Invocation& inv) {
  if (inv.args.size() < 3) {
    return wrong_args();
  }
  const std::span<const std::string> query{inv.args.data() + 1, inv.args.size() - 1};
  const auto value = drc::tech_drc_query(*ed.drc.rules, *ed.technology, query);
  if (!value.ok()) {
    return CommandResult::failure(value.error());
  }
  const auto* internal = std::get_if<std::int64_t>(&value.value().data());
  if (internal == nullptr) {
    return CommandResult::success();  // an unknown rule keyword returns nothing
  }
  const bool magic_compat = ctx.project().compat.magic_compat;
  return CommandResult::success(cmd::format_value(*internal, ed.units, editor::coordinate_units(ed, magic_compat)));
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
  bool override_default = false;  // `-override`: lifts the -T override
  std::size_t words = 0;          // Magic's locargc after the flag scan
};

LoadArgs scan_load_flags(const cmd::Invocation& inv) {
  // Magic counts "tech" itself: argv = tech load <file> [flags].
  LoadArgs out{false, false, inv.args.size() + 1};
  while (out.words > 3 && inv.args[out.words - 2].starts_with("-")) {
    const std::string& flag = inv.args[out.words - 2];
    if (flag == "-noprompt") {
      out.noprompt = true;
    } else if (flag == "-override") {
      out.override_default = true;
    } else if (flag != "-nooverride") {
      break;
    }
    --out.words;
  }
  return out;
}

/// The confirmation of a reload over painted layout (Magic's TxDialog; the
/// command stops only on `no`). A session that cannot ask goes ahead with
/// magic-compat ON, as Magic's -1 does, and is refused with TECH-CMD-003
/// with it OFF (Q-SCRIPT-48). nullopt to go ahead.
std::optional<CommandResult> confirm_reload(const editor::EditorState& ed, bool magic_compat) {
  static constexpr std::string_view kQuestion =
      "Technology file (re)loading may invalidate the existing layout.  Continue? ";
  const std::optional<bool> answer = cmd::try_confirm(ed.prompt, kQuestion);
  if (answer.value_or(magic_compat)) {
    return std::nullopt;
  }
  if (answer) {
    return CommandResult::success();  // answered `no`: nothing is loaded, nothing reported
  }
  return fail("TECH-CMD-003", std::string{kQuestion} + "(no: this session cannot ask; use -noprompt)");
}

CommandResult tech_load(cmd::CommandContext& ctx, editor::EditorState& ed, const cmd::Invocation& inv) {
  const LoadArgs args = scan_load_flags(inv);
  if (args.override_default) {
    ed.technology_override = false;  // Magic clears TechOverridesDefault while scanning
  }
  if (args.words != 3) {
    return fail("TECH-CMD-002", "Usage: tech load <filename> [-noprompt] [-override]");
  }
  // A technology given with -T holds until startup ends: `tech load` in a
  // startup script is then a silent no-op (Magic ref: commands/CmdTZ.c:574-579).
  if (ed.technology_override) {
    return CommandResult::success();
  }
  const bool magic_compat = ctx.project().compat.magic_compat;
  if (!args.noprompt && layout_has_paint(ed.cells, ed.technology->plane_count())) {
    if (auto declined = confirm_reload(ed, magic_compat)) {
      return *declined;
    }
  }
  SessionLoadOptions options;
  options.magic_compat = magic_compat;
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
    case kDrc: return tech_drc(ctx, ed, inv);
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

db::Rect scale_rect_copy(const db::Rect& r, std::int64_t n, std::int64_t d) {
  return db::Rect{static_cast<db::Coord>(scale_value(r.x0, n, d)), static_cast<db::Coord>(scale_value(r.y0, n, d)),
                  static_cast<db::Coord>(scale_value(r.x1, n, d)), static_cast<db::Coord>(scale_value(r.y1, n, d))};
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
    if (auto limit = check_grid_limit(*ed.technology, a, b, ed.grid); !limit) {
      return CommandResult::failure(limit.error());
    }
    (void)scale_layout(ed, b, a);  // DBScaleEverything, which also scales the box (ToolScaleBox)
    ed.grid = reduce(GridScale{ed.grid.internal * b, ed.grid.lambda * a});
    // CmdScalegrid's DRCTechScale(a, b): rules, halo and step size (Q-DRC-1).
    drc::rescale_rules(ed, static_cast<std::int32_t>(a), static_cast<std::int32_t>(b));
    (void)mask::rescale_output_styles(ed.mask.output_styles, static_cast<std::int32_t>(a),
                                      static_cast<std::int32_t>(b), ed.drc.compat_settings());
    if (ctx.project().compat.magic_compat && ed.box) {
      // CmdScalegrid then rescales the box once more ("Rescale cursor box"),
      // so Magic scales the box by (b/a)^2 (magic-compat; see step 8 of
      // tech-loading-and-scaling "scalegrid command").
      ed.box = scale_rect_copy(*ed.box, b, a);
    }
  }
  ctx.emit(diag::make("TECH-GRID-003", scale_report(ed.grid)));
  return CommandResult::success();
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

// TechLoad's PaOpen(..., SysLibPath): the session's system path (`path
// sys`) is searched after ".", and printed as written when nothing is found.
void set_system_path(const editor::EditorState& ed, LoadOptions& load) {
  std::string text;
  for (const std::string& entry : ed.search_paths.system) {
    text += (text.empty() ? "" : " ") + entry;
    if (entry == ".") {
      continue;
    }
    if (const auto dir = cell::expand_entry(entry, ed.path_variables); dir && !dir->empty()) {
      load.search_path.emplace_back(*dir);
    }
  }
  load.search_path_text = std::move(text);
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

/// "Style loading triggered by styletype" after a technology load, where the
/// startup looked (Magic reloads the styles of the new styletype, so its long
/// names resolve, UI-G-067). Missing files keep the previous styles and
/// never fail the load (Q-TECH-24).
void reload_display_styles(editor::EditorState& editor, platform::FileSystem& files, bool magic_compat) {
  display::StyleLoadOptions styles;
  styles.magic_compat = magic_compat;
  styles.library_dirs = editor.style_dirs;
  if (editor.styles) {
    styles.display_class = editor.styles->display_class;
  }
  diag::NullSink quiet;
  auto loaded = display::load_style_set(files, *editor.technology, styles, quiet);
  if (loaded.ok()) {
    editor.styles = std::move(loaded).value();
  }
}

namespace {

/// The steps after a successful load that make `tech` the session's
/// technology: "Post-load adjustments" (grid), "Effect on existing layout",
/// the locked types, the DRC rules, the wiring section's report, the plane
/// count of every cell and the display styles.
void install_technology(editor::EditorState& editor, platform::FileSystem& files,
                        std::shared_ptr<const Technology> tech, drc::LoadedRules rules,
                        mask::OutputStyleSet output_styles, bool magic_compat, diag::DiagnosticSink& sink) {
  const std::size_t old_planes = editor.technology->plane_count();
  const std::size_t new_planes = tech->plane_count();
  editor.grid = apply_post_load(*tech, editor.grid, sink);
  check_existing_layout(editor, old_planes, new_planes, sink);
  set_technology(editor, std::move(tech));
  drc::install_rules(editor, std::move(rules));
  // TechLoad's post-load CIFTechOutputScale(DBLambda[0], DBLambda[1]).
  editor.mask.output_styles = std::move(output_styles);
  if (editor.grid.lambda != editor.grid.internal) {
    (void)mask::rescale_output_styles(editor.mask.output_styles, static_cast<std::int32_t>(editor.grid.lambda),
                                      static_cast<std::int32_t>(editor.grid.internal), editor.drc.compat_settings());
  }
  // The `wiring` section is kept raw and read by the wiring area; reading it
  // here reports its line errors (WIRE-TECH-001) at load, as Magic does.
  (void)wiring::parse_wiring_section(*editor.technology, editor.grid, &sink);
  if (new_planes != old_planes) {
    editor.cells.for_each([new_planes](db::CellDef& def) { def.set_plane_count(new_planes); });
  }
  reload_display_styles(editor, files, magic_compat);
}

}  // namespace

std::string layer_names_text(const Technology& tech) {
  return print_types_text(tech, all_but_space_and_drc());
}

namespace {

/// TechLoad's report of the sections whose line handler failed, and the
/// failed load.
diag::Status section_failure(const std::string& file, bool cifoutput, bool drc, diag::DiagnosticSink& sink) {
  std::string sections;
  for (const auto& [failed, name] : {std::pair{cifoutput, "cifoutput"}, std::pair{drc, "drc"}}) {
    if (failed) {
      sections += std::string{"\n    "} + name;
    }
  }
  sink.emit(diag::make("TECH-SECTION-007", "The following sections of " + file + " contained errors:" + sections,
                       diag::SourceLocation{file, 0}));
  return diag::make("TECH-LOAD-001", "Error in loading technology file", diag::SourceLocation{file, 0});
}

}  // namespace

diag::Status load_session_technology(editor::EditorState& editor, platform::PlatformServices& services,
                                     std::string_view name, const SessionLoadOptions& options,
                                     diag::DiagnosticSink& sink) {
  LoadOptions load;
  load.magic_compat = options.magic_compat;
  set_system_path(editor, load);
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
  // The cifoutput and drc sections are read while the technology is read
  // (m3-drc design D4/D5): Magic reads them, in this order, before the
  // wiring section and before the post-load scaling, so their messages come
  // first, and a failed section (a statement before the first cifoutput
  // `style`, Q-MASK-17, or a rejected rule line) fails the load with the
  // previous technology kept (TECH-LOAD-001).
  mask::OutputStyleSet output_styles = mask::read_output_styles(*tech.value(), sink);
  drc::LoadedRules rules = drc::read_rules(*tech.value(), editor.drc.compat_settings(), sink);
  if (output_styles.section_failed || !rules.ok()) {
    return section_failure(path.value().string(), output_styles.section_failed, !rules.ok(), sink);
  }
  install_technology(editor, services.files, std::move(tech.value()), std::move(rules), std::move(output_styles),
                     options.magic_compat, sink);
  return diag::Done{};
}

diag::Status load_builtin_technology(editor::EditorState& editor, platform::PlatformServices& services,
                                     const SessionLoadOptions& options, diag::DiagnosticSink& sink) {
  std::shared_ptr<const Technology> minimum = builtin_technology();
  if (editor.technology == minimum) {
    return diag::Done{};  // already current: nothing is reloaded
  }
  drc::LoadedRules rules = drc::read_rules(*minimum, editor.drc.compat_settings(), sink);
  if (!rules.ok()) {  // the built-in text has no drc section; kept as a guard
    return diag::make("TECH-LOAD-001", "Error in loading technology file");
  }
  diag::NullSink quiet;  // the built-in text has no cifoutput section
  mask::OutputStyleSet output_styles = mask::read_output_styles(*minimum, quiet);
  install_technology(editor, services.files, std::move(minimum), std::move(rules), std::move(output_styles),
                     options.magic_compat, sink);
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
