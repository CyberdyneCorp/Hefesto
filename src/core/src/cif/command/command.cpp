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
// which follows the selection) and the
// technology's raw `cifinput` section. Every other option fails with
// CORE-NOTIMPL-005 until CIF lands it (tasks.md section 11).
//
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdCD.c CmdCif (the option
// table, list/listall, HELP, ISTYLE, OSTYLE, CIF_SCALE, CIF_LAMBDA),
// cif/CIFmain.c CIFGetOutputScale / CIFPrintStyle and cif/CIFrdtech.c
// CIFGetInputScale / CIFPrintReadStyle / the `style` and `scalefactor`
// statements, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/cif/command/command.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <optional>
#include <tuple>
#include <string>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/cmd/stubs.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
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

enum Option : std::size_t { kHelp = 5, kIstyle = 7, kLambda = 8, kOstyle = 10, kScale = 15 };

/// One parsed `cif` call: the list mode words removed (Magic's argv++).
struct CifCall {
  cu::Session s;
  std::vector<std::string> words;  // words[0] is the option
  bool list = false;
  bool listall = false;
  [[nodiscard]] std::size_t argc() const noexcept { return words.size() + 1; }
};

// A `cifinput` style as `cif istyle` sees it: its name and the first
// `scalefactor` of its body (crs_scaleFactor / crs_multiplier).
struct InputStyle {
  std::string name;
  std::int64_t scale_factor = 1;
  std::int64_t multiplier = 1;
};

// CIFParseScale: "n" or "n.d" as a reduced fraction n / expander.
std::pair<std::int64_t, std::int64_t> parse_scale(const std::string& text) {
  const std::size_t dot = text.find('.');
  if (dot == std::string::npos) {
    return {cu::c_atoi(text), 1};
  }
  std::int64_t d = 1;
  for (std::size_t i = dot + 1; i < text.size(); ++i) {
    d *= 10;
  }
  const std::int64_t n = std::int64_t{cu::c_atoi(text.substr(0, dot))} * d + cu::c_atoi(text.substr(dot + 1));
  const std::int64_t g = std::gcd(n, d);
  return g > 1 ? std::pair{n / g, d / g} : std::pair{n, d};
}

// The names a `style` statement adds (CIFReadTechLine): the name, or with
// `variants a,b` one name per variant appended to it.
std::vector<std::string> style_names(const tech::RawLine& line) {
  const auto& t = line.tokens;
  if (t.size() == 4 && t[2].starts_with("variant")) {
    std::vector<std::string> names;
    std::size_t start = 0;
    while (start <= t[3].size()) {
      const std::size_t comma = t[3].find(',', start);
      names.push_back(t[1] + t[3].substr(start, comma == std::string::npos ? std::string::npos : comma - start));
      if (comma == std::string::npos) {
        break;
      }
      start = comma + 1;
    }
    return names;
  }
  return t.size() >= 2 ? std::vector<std::string>{t[1]} : std::vector<std::string>{};
}

void apply_scalefactor(const tech::RawLine& line, InputStyle& style) {
  const auto& t = line.tokens;
  if (t.size() < 2) {
    return;
  }
  std::tie(style.scale_factor, style.multiplier) = parse_scale(t[1]);
  if (t.size() >= 3 && t.back().starts_with("nanom")) {
    style.multiplier = 10;
  } else if (t.size() >= 3 && t.back().starts_with("angstr")) {
    style.multiplier = 100;
  }
}

// Every `cifinput` style in declaration order; the first is current.
std::vector<InputStyle> input_styles(const tech::Technology& tech) {
  std::vector<InputStyle> styles;
  for (const tech::RawSection& section : tech.raw_sections()) {
    if (section.name != "cifinput") {
      continue;
    }
    std::size_t first_of_statement = styles.size();
    for (const tech::RawLine& line : section.lines) {
      if (line.tokens.empty()) {
        continue;
      }
      if (line.tokens[0] == "style") {
        first_of_statement = styles.size();
        for (std::string& name : style_names(line)) {
          styles.push_back(InputStyle{std::move(name)});
        }
      } else if (line.tokens[0] == "scalefactor" && first_of_statement < styles.size()) {
        for (std::size_t i = first_of_statement; i < styles.size(); ++i) {
          apply_scalefactor(line, styles[i]);
        }
        first_of_statement = styles.size();  // the first scalefactor of the body counts
      }
    }
  }
  return styles;
}

// An option the M5 scaffold registered before CIF implemented it.
CommandResult not_yet(std::size_t option) {
  const std::string_view entry = kOptions.at(option);
  return cmd::not_implemented("cif " + std::string{entry.substr(0, entry.find_first_of(" \t\n"))});
}

CommandResult wrong_args(const CifCall& c, std::size_t option) {
  cu::warn(c.s, "Wrong arguments in \"cif " + c.words.front() + "\" command:");
  cu::warn(c.s, "    :cif " + std::string{kOptions.at(option)});
  cu::warn(c.s, "Try \":cif help\" for more help.");
  return CommandResult::success();
}

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

// CIFGetInputScale(1000); nullopt ("Error: No style is set") without one.
std::optional<double> input_scale(const editor::EditorState& editor, bool magic_compat) {
  if (!editor.technology) {
    return std::nullopt;
  }
  const std::vector<InputStyle> styles = input_styles(*editor.technology);
  if (styles.empty()) {
    return std::nullopt;
  }
  const std::int64_t numerator = 10 * styles.front().scale_factor;
  const std::int64_t denominator = styles.front().multiplier * 1000;
  if (magic_compat) {
    return static_cast<double>(static_cast<float>(numerator) / static_cast<float>(denominator));
  }
  return static_cast<double>(numerator) / static_cast<double>(denominator);
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
  std::vector<std::string> names;
  if (c.s.editor->technology) {
    for (InputStyle& style : input_styles(*c.s.editor->technology)) {
      names.push_back(std::move(style.name));
    }
  }
  return print_styles(c, names, names.empty() ? std::nullopt : std::optional{names.front()}, "input");
}

// CIFSetStyle: `cif ostyle <name>` selects the output style (an exact name,
// else a unique prefix), and the DRC catalog follows it; on failure the
// error, then the current style and the style list (CIFPrintStyle).
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
    (void)editor.drc.cif.select(current->name);
  }
  cu::print(c.s, "CIF output style is now \"" + name + "\"");
  return CommandResult::success();
}

// `cif istyle|ostyle [style]`: istyle lists only until INPUT lands selection.
CommandResult style(const CifCall& c, std::size_t option) {
  if (c.argc() == 3) {
    return option == kOstyle ? select_output(c) : not_yet(option);
  }
  if (c.argc() != 2) {
    return wrong_args(c, option);
  }
  return option == kIstyle ? input_styles_of(c) : output_styles(c);
}

// Options that need a window (every one but help, istyle, ostyle, prefix,
// arealabels, warning, limit and polygon): "Point to a window first".
bool needs_window(std::size_t option) {
  return option == kScale || option == kLambda;
}

CommandResult run_option(const CifCall& c, std::size_t option) {
  if (needs_window(option) && c.s.editor->windows.size() == 0 && !c.s.editor->command_point) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  switch (option) {
    case kHelp:
      return help(c);
    case kScale:
    case kLambda:
      return scale(c, option);
    case kIstyle:
    case kOstyle:
      return style(c, option);
    default:
      return not_yet(option);
  }
}

CommandResult cif_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  CifCall c{session.value(), invocation.args};
  if (c.words.empty()) {
    return cmd::not_implemented("cif write");  // a bare `cif` writes the root cell
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
