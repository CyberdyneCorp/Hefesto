// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The read side of the `gds`/`calma` command (capability gds-read "Read
// option syntax and defaults", "Text magnification scale option", "Grid
// subdivision option", "Read command and file lookup", "Read
// diagnostics"; openspec/changes/m5-mask-gds-cif/design.md D15): drccheck,
// flatten, flatglob, magscale, maskhints, noduplicates, ordering, readonly,
// rescale (Q-MASK-130), savepaths, warning, polygon subcells, path
// subcells, unique and `read`. Queries answer as Magic's Tcl build does
// (Tcl_SetObjResult); `warning` prints, as Magic does in every build.
//
// `read` looks the file up (gds::find_gds_file), makes an empty edit cell
// when there is none (DBWloadWindow(w, NULL, DBW_LOAD_IGNORE_TECH)), reads
// through gds::read, whose cells are queued for DRC there unless `drccheck
// no` or `readonly yes` (CIFReadCellCleanup), and loads the library cell
// into the window (DBWloadWindow(mw, libname, 0)). GDS_FILE is the name the
// file was found as with magic-compat ON and the absolute canonical path
// with it OFF (Q-MASK-129), then substituted (Q-MASK-128).
//
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdCD.c CmdCalma,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/read/read.hpp"
#include "hefesto/gds/read/vendor.hpp"
#include "options.hpp"

namespace hefesto::gds::command_detail {

namespace {

using cmd::CommandResult;

constexpr std::array<std::string_view, 3> kPolygonTypes{"none", "temporary", "keep"};
constexpr std::array<std::string_view, 6> kWarnOptions{"default", "none", "align", "limit", "redirect", "help"};
constexpr std::size_t kWarnHelp = 5;

CommandResult magscale_option(const cmd::Invocation& invocation, GdsSettings& g) {
  if (invocation.args.size() == 1) {
    return CommandResult::success(cmd::Value{g.magscale});
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  const std::string& value = invocation.args[1];
  if (cu::str_is_numeric(value)) {
    // CalmaMagScale is a float.
    g.magscale = static_cast<double>(static_cast<float>(std::atof(value.c_str())));
  } else if (value == "default") {
    g.magscale = 1.0;
  } else {
    return wrong_args(invocation);
  }
  return CommandResult::success();
}

CommandResult flatten_option(const cmd::Invocation& invocation, GdsSettings& g) {
  if (invocation.args.size() == 1) {
    return boolean(g.flatten);
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  if (const auto index = lookup_index(invocation.args[1], kYesNo)) {
    g.flatten = *index >= 4;
    return CommandResult::success();
  }
  if (!cu::str_is_int(invocation.args[1])) {
    return wrong_args(invocation);
  }
  g.flatten_limit = cu::c_atoi(invocation.args[1]);
  g.flatten = g.flatten_limit > 0;
  return CommandResult::success();
}

bool equals_ignoring_case(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto lower = [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); };
    if (lower(a[i]) != lower(b[i])) {
      return false;
    }
  }
  return true;
}

CommandResult flatglob_option(const cmd::Invocation& invocation, GdsSettings& g) {
  if (invocation.args.size() == 1) {
    if (g.flatglob.empty()) {
      return CommandResult::success();
    }
    cmd::Value::List list;
    for (const std::string& pattern : g.flatglob) {
      list.emplace_back(pattern);
    }
    return CommandResult::success(cmd::Value{std::move(list)});
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  if (equals_ignoring_case(invocation.args[1], "none")) {
    g.flatglob.clear();
  } else {
    g.flatglob.push_back(invocation.args[1]);
  }
  return CommandResult::success();
}

// The names of a layer list as CIFParseReadLayers splits it: a run of
// commas separates two names, a leading comma gives an empty name.
std::vector<std::string_view> split_layer_list(std::string_view list) {
  std::vector<std::string_view> names;
  std::size_t from = 0;
  while (from < list.size()) {
    const std::size_t comma = list.find(',', from);
    names.push_back(list.substr(from, comma == std::string_view::npos ? std::string_view::npos : comma - from));
    if (comma == std::string_view::npos) {
      break;
    }
    from = list.find_first_not_of(',', comma);
  }
  return names;
}

// CIFParseReadLayers(list, mask, FALSE): the layers of the current input
// style, an error for each unknown name.
std::string known_read_layers(const cu::Session& s, std::string_view list) {
  const mask::InputStyle* style = s.editor->mask.input_styles.current_style();
  std::string known;
  for (const std::string_view name : split_layer_list(list)) {
    if (style != nullptr && std::ranges::find(style->cif_layers, name) != style->cif_layers.end()) {
      known += (known.empty() ? "" : ",") + std::string{name};
    } else {
      cu::warn(s, "Error:  CIF layer \"" + std::string{name} + "\" is unknown.");
    }
  }
  return known;
}

CommandResult maskhints_option(const cu::Session& s, const cmd::Invocation& invocation, GdsSettings& g) {
  if (invocation.args.size() == 1) {
    return boolean(!g.maskhints.empty());
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  if (const auto index = lookup_index(invocation.args[1], kYesNo)) {
    g.maskhints = *index >= 4 ? "yes" : "";
  } else {
    g.maskhints = known_read_layers(s, invocation.args[1]);
  }
  return CommandResult::success();
}

CommandResult polygons_option(const cmd::Invocation& invocation, GdsSettings& g) {
  if (invocation.args.size() == 2) {
    return CommandResult::success(
        cmd::Value{std::string{kPolygonTypes.at(static_cast<std::size_t>(g.polygon_subcells))}});
  }
  if (invocation.args.size() != 3) {
    return wrong_args(invocation);
  }
  if (const auto type = lookup_index(invocation.args[2], kPolygonTypes)) {
    g.polygon_subcells = static_cast<PolygonSubcells>(*type);
    return CommandResult::success();
  }
  const auto index = lookup_index(invocation.args[2], kYesNo);
  if (!index) {
    return wrong_args(invocation);
  }
  g.polygon_subcells = *index >= 4 ? PolygonSubcells::temporary : PolygonSubcells::none;
  return CommandResult::success();
}

// Q-MASK-130: `rescale no` also sets warning level limit with
// magic-compat ON.
CommandResult rescale_option(const cu::Session& s, const cmd::Invocation& invocation) {
  GdsSettings& g = s.editor->mask.gds;
  CommandResult result = yes_no_option(invocation, g.rescale);
  if (invocation.args.size() == 2 && result.ok() && !g.rescale && s.magic_compat) {
    s.editor->mask.warnings.level = mask::WarningLevel::limit;
  }
  return result;
}

CommandResult warning_option(const cu::Session& s, const cmd::Invocation& invocation) {
  mask::WarningSettings& w = s.editor->mask.warnings;
  if (invocation.args.size() == 1) {
    cu::print(s, "Warning display options: " + std::string{kWarnOptions.at(static_cast<std::size_t>(w.level))});
    return CommandResult::success();
  }
  std::size_t option = kWarnHelp;
  if (const auto index = lookup_index(invocation.args[1], kWarnOptions)) {
    option = *index;
  } else {
    cu::warn(s, "\"" + invocation.args[1] + "\" isn't a valid gds warning option.");
  }
  if (option == kWarnHelp) {
    cu::warn(s, "Valid options are: default, align, limit, redirect, and none.");
    return CommandResult::success();
  }
  w.level = static_cast<mask::WarningLevel>(option);
  if (w.level == mask::WarningLevel::redirect) {
    w.redirect_file = invocation.args.size() == 3 ? invocation.args[2] : std::string{};
  }
  return CommandResult::success();
}

// A command of the layout client run from this one (DBWloadWindow through
// `load`), its messages passed on.
CommandResult run_nested(cmd::CommandContext& ctx, std::vector<std::string> words) {
  const cmd::CommandLookup found = ctx.registry().resolve(words.front(), cmd::kLayoutClient);
  if (found.status != cmd::LookupStatus::found) {
    return CommandResult::success();
  }
  const cmd::Invocation invocation{found.spec->name, {words.begin() + 1, words.end()}, cmd::Origin::script, {}};
  CommandResult result = found.spec->handler(ctx, invocation);
  for (const diag::Diagnostic& d : result.diagnostics) {
    ctx.emit(d);
  }
  result.diagnostics.clear();
  return result;
}

// Q-MASK-129: the name found on the search path (ON) or the absolute
// canonical path (OFF), then Q-MASK-128's substitutions.
std::string gds_file_property(const cu::Session& s, const platform::Path& found) {
  std::string path = found.generic_string();
  if (!s.magic_compat) {
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(found, ec);
    const std::filesystem::path canonical = ec ? found : std::filesystem::weakly_canonical(absolute, ec);
    path = (ec ? absolute.lexically_normal() : canonical).generic_string();
  }
  return gds_file_value(path, s.editor->path_variables);
}

}  // namespace

std::optional<CommandResult> read_settings_option(const cu::Session& s, std::size_t option,
                                                  const cmd::Invocation& invocation) {
  GdsSettings& g = s.editor->mask.gds;
  switch (option) {
    case kDrcCheck:
      return yes_no_option(invocation, g.drccheck);
    case kNoDuplicates:
      return yes_no_option(invocation, g.noduplicates);
    case kOrdering:
      return yes_no_option(invocation, g.ordering);
    case kReadOnly:
      return yes_no_option(invocation, g.readonly);
    case kSavePaths:
      return yes_no_option(invocation, g.savepaths);
    case kUnique:
      return yes_no_option(invocation, g.unique);
    case kPaths:
      return yes_no_option(invocation, g.path_subcells, 2);
    case kMagScale:
      return magscale_option(invocation, g);
    case kFlatten:
      return flatten_option(invocation, g);
    case kFlatGlob:
      return flatglob_option(invocation, g);
    case kMaskHints:
      return maskhints_option(s, invocation, g);
    case kPolygons:
      return polygons_option(invocation, g);
    case kRescale:
      return rescale_option(s, invocation);
    case kWarning:
      return warning_option(s, invocation);
    default:
      return std::nullopt;
  }
}

CommandResult read_option(cmd::CommandContext& ctx, const cu::Session& s, const cmd::Invocation& invocation) {
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  editor::EditorState& editor = *s.editor;
  const std::string& name = invocation.args[1];
  cell::LookupContext lookup{ctx.services().files, editor.search_paths, editor.path_variables, ctx.sink()};
  const std::optional<cell::FoundFile> found = find_gds_file(lookup, name);
  if (!found) {
    return CommandResult::failure(diag::make(kCannotOpen, "Cannot open " + name + ".gds, " + name + ".strm or " +
                                                              name + " to read GDS-II stream input."));
  }
  if (editor.edit_cell == nullptr) {
    (void)run_nested(ctx, {"load"});
  }
  GdsReadOptions options;
  options.settings = editor.mask.gds;
  options.warnings = editor.mask.warnings;
  options.typed_name = name;
  options.files = &ctx.services().files;
  options.time_zone = editor.mask.time_zone;
  options.gds_file = gds_file_property(s, found->path);
  CommandResult result = read(editor, found->path, options);
  const std::string* library = std::get_if<std::string>(&result.value.data());
  if (library != nullptr && editor.cells.find(*library) != nullptr) {
    const std::string cell = *library;
    for (const diag::Diagnostic& d : result.diagnostics) {
      ctx.emit(d);
    }
    result.diagnostics.clear();
    result.value = cmd::Value{};
    (void)run_nested(ctx, {"load", cell});
  }
  return result;
}

}  // namespace hefesto::gds::command_detail
