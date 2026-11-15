// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `cif` command (capability cif-format-io "cif command dispatch";
// openspec/changes/m5-mask-gds-cif/design.md D3, D13), moved here from
// script/binding/ by the M5 scaffold with Magic's whole option table. The
// M4 answers keep working (capability scripting-api "Result formats of
// commands used by scripts"; Q-SCRIPT-7, resolution: compat):
// `cif scale|lambda input|output`, `cif [list|listall] ostyle|istyle` and
// `cif help`, backed by the session's output styles (EditorState::mask,
// selected by `cif ostyle <name>`; `cif scale out` reads drc::CifCatalog,
// which follows the selection) and the session's input styles
// (EditorState::mask.input_styles, read at tech load by
// mask::read_input_styles and selected by `cif istyle <name>`). The window
// check, a bare `cif` (write the root cell) and the dispatch are here; the
// session options are options.cpp's, the output options output.cpp's
// (command_impl.hpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdCD.c CmdCif (the option
// table, list/listall, the window check, HELP, ISTYLE, OSTYLE, CIF_SCALE,
// CIF_LAMBDA),
// cif/CIFmain.c CIFGetOutputScale / CIFPrintStyle and cif/CIFrdtech.c
// CIFGetInputScale / CIFPrintReadStyle / the `style` and `scalefactor`
// statements, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/cif/command/command.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "command_impl.hpp"
#include "db/command_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::cif {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;
using cmd::Value;

constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";

// cmdCifOption, in Magic's order (Lookup() reads each entry up to its
// first space; the help prints the whole entry).
constexpr std::array<std::string_view, 23> kOptions{
    "*array layer\t\tdisplay CIF layer under box (array only)",
    "*hier layer\t\tdisplay CIF layer under box (hier only)",
    "arealabels yes|no\tenable/disable us of area label extension",
    "coverage layer\t\tprint area coverage of indicated layer",
    "drccheck [yes|no]\tmark all cells as needing DRC checking",
    "help\t\tprint this help information",
    "idcell yes|no\tenable/disable use of cell ID extension",
    "istyle [style]\tchange style for reading CIF to style",
    "lambda in|out\tshow microns per lambda for the current style",
    "limit [value]\tlimit internal grid subdivision by scalefactor value",
    "ostyle [style]\tchange style for writing CIF to style",
    "paint ciflayer magiclayer [cellname]\n\t\t\tgenerate CIF and paint into cell",
    "prefix [path]\tprepend path to cell names in CIF output",
    "read file\t\tread CIF from \"file\" into edit cell",
    "rescale [yes|no]\tallow/disallow rescaling of internal grid",
    "scale in|out\tshow microns per internal units for the current style",
    "see layer\t\tdisplay CIF layer under box",
    "statistics\t\tprint out statistics for CIF generator",
    "warning [option]\tset warning display options",
    "write file\t\toutput CIF for the window's root cell to \"file\"",
    "flat file\t\toutput flattened CIF for the window's root cell to \"file\"",
    "polygon subcells [yes|no]\n\t\t\tput non-Manhattan polygons in subcells",
    "unfracture [yes|no]\n\t\t\toptimize non-Manhattan geometry",
};

}  // namespace

namespace command_detail {

std::string_view option_entry(std::size_t option) {
  return kOptions.at(option);
}

CommandResult wrong_args(const CifCall& c, std::size_t option) {
  cu::warn(c.s, "Wrong arguments in \"cif " + c.words.front() + "\" command:");
  cu::warn(c.s, "    :cif " + std::string{kOptions.at(option)});
  cu::warn(c.s, "Try \":cif help\" for more help.");
  return CommandResult::success();
}

}  // namespace command_detail

namespace {

using namespace command_detail;  // NOLINT(google-build-using-namespace): this file's own helpers

CommandResult help(const CifCall& c) {
  const bool wizard = c.argc() == 3 && c.words[1] == "wizard";
  std::string text = "CIF commands have the form \":cif option\", where option is one of:\n";
  for (const std::string_view entry : kOptions) {
    if (entry.front() != '*' || wizard) {
      text += "    " + std::string{entry} + "\n";
    }
  }
  text += "If no option is given, CIF is output for the root cell.\n";
  cu::print_block(c.s, std::move(text));
  return CommandResult::success();
}

// CIFGetOutputScale(1000): (float)(10 * scalefactor) / (float)(expander *
// 1000), widened to double with magic-compat ON; the same quotient in
// double precision with it OFF (Q-SCRIPT-7). 1.0 without an output style.
double output_scale(const editor::EditorState& editor, bool magic_compat) {
  const drc::CifStyleInfo* style = editor.drc.cif.current();
  if (style == nullptr) {
    return 1.0;
  }
  const std::int64_t numerator = std::int64_t{10} * style->scale_factor;
  const std::int64_t denominator = std::int64_t{style->expander} * 1000;
  if (magic_compat) {
    return static_cast<double>(static_cast<float>(numerator) / static_cast<float>(denominator));
  }
  return static_cast<double>(numerator) / static_cast<double>(denominator);
}

// CIFGetInputScale(1000) of the session's input style (mask-input-styles
// "Input-scale reporting"): the float quotient widened with magic-compat
// ON, the same quotient in double precision OFF (Q-SCRIPT-7); nullopt
// ("Error: No style is set") without one.
std::optional<double> input_scale(const editor::EditorState& editor, bool magic_compat) {
  const mask::InputStyle* style = editor.mask.input_styles.current_style();
  if (style == nullptr) {
    return std::nullopt;
  }
  if (magic_compat) {
    return static_cast<double>(style->microns_per_unit());
  }
  return static_cast<double>(10 * style->scale_factor) / static_cast<double>(style->multiplier * 1000);
}

CommandResult scale(const CifCall& c, std::size_t option) {
  if (c.argc() != 3) {
    return wrong_args(c, option);
  }
  const int direction = cu::lookup(c.words[1], {"input", "output"});
  if (direction < 0) {
    return wrong_args(c, option);
  }
  std::optional<double> value = direction == 0 ? input_scale(*c.s.editor, c.s.magic_compat)
                                               : std::optional{output_scale(*c.s.editor, c.s.magic_compat)};
  if (!value) {
    cu::warn(c.s, "Error: No style is set");
    value = 0.0;
  }
  if (option == kLambda) {
    cu::print(c.s, "Warning:  \"cif lambda\" is deprecated;  use \"cif scale\" instead.  Units are not in lambda.");
  }
  return CommandResult::success(Value{*value});
}

// CIFPrintStyle / CIFPrintReadStyle.
CommandResult print_styles(const CifCall& c, const std::vector<std::string>& names,
                           const std::optional<std::string>& current, std::string_view kind) {
  if (!c.listall) {
    if (!current) {
      cu::warn(c.s, "Error: No style is set");
      return CommandResult::success();
    }
    if (c.list) {
      return CommandResult::success(Value{*current});
    }
    cu::print(c.s, "The current style is \"" + *current + "\".");
    return CommandResult::success();
  }
  if (c.list) {
    Value::List all;
    for (const std::string& name : names) {
      all.emplace_back(name);
    }
    return CommandResult::success(Value{std::move(all)});
  }
  std::string text = "The CIF " + std::string{kind} + " styles are: ";
  for (std::size_t i = 0; i < names.size(); ++i) {
    text += (i == 0 ? "" : ", ") + names[i];
  }
  cu::print(c.s, text + ".");
  return CommandResult::success();
}

CommandResult output_styles(const CifCall& c) {
  const mask::OutputStyleSet& set = c.s.editor->mask.output_styles;
  const mask::OutputStyle* current = set.current_style();
  return print_styles(c, set.names(), current != nullptr ? std::optional{current->name} : std::nullopt, "output");
}

CommandResult input_styles_of(const CifCall& c) {
  const mask::InputStyleSet& set = c.s.editor->mask.input_styles;
  const mask::InputStyle* current = set.current_style();
  return print_styles(c, set.names(), current != nullptr ? std::optional{current->name} : std::nullopt, "input");
}

// CIFSetReadStyle: `cif istyle <name>` (mask-input-styles "Input style
// selection"; Q-MASK-303: an exact name wins); on failure the error, then
// the current style and the style list (CIFPrintReadStyle).
CommandResult select_input(const CifCall& c) {
  editor::EditorState& editor = *c.s.editor;
  const std::string& name = c.words[1];
  const compat::CompatSettings compat{c.s.magic_compat, c.s.ctx->project().compat.netgen_compat};
  const diag::Status selected = mask::select_input_style(editor.mask.input_styles, name, editor.grid.lambda,
                                                         editor.grid.internal, compat, c.s.ctx->sink());
  if (!selected.ok()) {
    c.s.ctx->emit(selected.error());
    CifCall listing = c;
    listing.list = false;
    listing.listall = false;
    (void)input_styles_of(listing);
    listing.listall = true;
    return input_styles_of(listing);
  }
  cu::print(c.s, "CIF input style is now \"" + name + "\"");
  return CommandResult::success();
}

// CIFSetStyle: `cif ostyle <name>` selects the output style (an exact name,
// else a unique prefix), and the DRC catalog follows it; on failure the
// error, then the current style and the style list (CIFPrintStyle).
// A DRC style that binds a cifstyle is reloaded (drc::select_cif_style).
CommandResult select_output(const CifCall& c) {
  editor::EditorState& editor = *c.s.editor;
  const std::string& name = c.words[1];
  const compat::CompatSettings compat{c.s.magic_compat, c.s.ctx->project().compat.netgen_compat};
  const auto selected = mask::select_output_style(editor.mask.output_styles, name,
                                                  static_cast<std::int32_t>(editor.grid.lambda),
                                                  static_cast<std::int32_t>(editor.grid.internal), compat,
                                                  c.s.ctx->sink());
  if (!selected.ok()) {
    c.s.ctx->emit(selected.error());
    CifCall listing = c;
    listing.list = false;
    listing.listall = false;
    (void)output_styles(listing);
    listing.listall = true;
    return output_styles(listing);
  }
  if (const mask::OutputStyle* current = editor.mask.output_styles.current_style()) {
    // CIFLoadStyle: DRCReloadCurStyle when the DRC section names a cifstyle
    // (cif/CIFtech.c:2297-2298).
    (void)drc::select_cif_style(editor, current->name, c.s.ctx->sink());
  }
  cu::print(c.s, "CIF output style is now \"" + name + "\"");
  return CommandResult::success();
}

// `cif istyle|ostyle [style]`.
CommandResult style(const CifCall& c, std::size_t option) {
  if (c.argc() == 3) {
    return option == kOstyle ? select_output(c) : select_input(c);
  }
  if (c.argc() != 2) {
    return wrong_args(c, option);
  }
  return option == kIstyle ? input_styles_of(c) : output_styles(c);
}

// Options that need a window (every one but help, istyle, ostyle, prefix,
// arealabels, warning, limit and polygon): "Point to a window first".
bool needs_window(std::size_t option) {
  switch (option) {
    case kHelp:
    case kIstyle:
    case kOstyle:
    case kPrefix:
    case kAreaLabels:
    case kWarning:
    case kLimit:
    case kPolygon:
      return false;
    default:
      return true;
  }
}

bool has_window(const CifCall& c) {
  return c.s.editor->windows.size() != 0 || c.s.editor->command_point.has_value();
}

// `*array` and `*hier`: the `write` switch with four words, else a preview.
CommandResult wizard_option(const CifCall& c, std::size_t option) {
  return c.argc() == 4 ? settings_option(c, option) : see_option(c, option);
}

// `write` and `flat` with their file name.
CommandResult output_option(const CifCall& c, std::size_t option) {
  if (c.argc() != 3) {
    return wrong_args(c, option);
  }
  return write_option(c, c.words[1], option == kFlat);
}

CommandResult run_option(const CifCall& c, std::size_t option) {
  switch (option) {
    case kHelp:
      return help(c);
    case kScale:
    case kLambda:
      return scale(c, option);
    case kIstyle:
    case kOstyle:
      return style(c, option);
    case kArray:
    case kHier:
      return wizard_option(c, option);
    case kWrite:
    case kFlat:
      return output_option(c, option);
    case kRead:
      return read_option(c);
    case kSee:
      return see_option(c, option);
    case kPaint:
      return paint_option(c);
    case kCoverage:
      return coverage_option(c);
    case kStats:
      return statistics_option(c);
    default:
      return settings_option(c, option);
  }
}

// A bare `cif`: CIF of the window's root cell to <name after its last '/'>.
CommandResult bare_write(const CifCall& c) {
  if (!has_window(c) || c.s.editor->windows.current() == nullptr) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  const std::string& name = c.s.editor->windows.current()->root()->name();
  const std::size_t slash = name.rfind('/');
  return write_option(c, slash == std::string::npos ? name : name.substr(slash + 1), false);
}

CommandResult cif_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  CifCall c{session.value(), invocation.args};
  if (c.words.empty()) {
    return bare_write(c);
  }
  if (c.words.front().starts_with("list")) {
    c.list = true;
    c.listall = c.words.front().starts_with("listall");
    c.words.erase(c.words.begin());
    if (c.words.empty()) {
      c.words.emplace_back("help");
    }
  }
  const cmd::LookupResult found = cmd::lookup(c.words.front(), kOptions);
  if (found.match != cmd::Match::found) {
    cu::warn(c.s, "\"" + c.words.front() + "\" isn't a valid cif option.");
    c.words = {"help"};
    return help(c);
  }
  if (needs_window(found.index) && !has_window(c)) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  return run_option(c, found.index);
}

}  // namespace

bool register_cif_command(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"cif", "cif option",
                                    "CIF processor; type \"cif help\" for information on options.", cif_command},
                   cmd::kLayoutClient)
              .has_value();
}

}  // namespace hefesto::cif
