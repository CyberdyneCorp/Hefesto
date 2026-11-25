// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2spice` run and render() (capability netlist-spice-options "Run
// arguments and output file", "File header and scale card"; design D18):
// the run flags (spcParseArgs through extflat::parse_common_args), the
// root cell, the read and flatten (extflat::read, extflat::flatten with
// EFFlatBuild's flags), the writer's input from the current extraction
// style (ExtGetDevInfo through ExtStyle::device_info()), and the output
// file. render() writes no file and leaves EditorState::netlist untouched.
// Hierarchical output (`hierarchy on`) is hier_run.cpp's.
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:821-1100
// (runexttospice) and 1416-1508 (spcParseArgs), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <strings.h>

#include "netlist/spice/run_impl.hpp"

#include "hefesto/app/settings_layers.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/netlist/session.hpp"
#include "hefesto/netlist/spice/run_args.hpp"
#include "hefesto/netlist/spice/spice.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::netlist::spice {

namespace {

// NETLIST-SPICE-001: no cell argument and no window.
constexpr std::string_view kNoCell = "NETLIST-SPICE-001";
// EXTFLAT-READ-001: EFReadFile found no `.ext` file.
constexpr std::string_view kCannotRead = "EXTFLAT-READ-001";

constexpr std::string_view kUsage =
    "Usage: ext2spice [-B] [-o spicefile] [-M|-m] [-J flat|hier]\n"
    "[-f spice2|spice3|hspice|ngspice|cdl] [-M] [-m] [file]\n";

// ArgStr: the rest of the flag word, else the next word.
std::optional<std::string> arg_str(std::span<const std::string> args, std::size_t& index) {
  const std::string& flag = args[index];
  if (flag.size() > 2) {
    return flag.substr(2);
  }
  if (index + 1 >= args.size()) {
    return std::nullopt;
  }
  ++index;
  return args[index];
}

std::optional<Dialect> dialect_named(const std::string& name) {
  constexpr std::array<std::string_view, 5> kNames{"SPICE2", "SPICE3", "HSPICE", "NGSPICE", "CDL"};
  for (std::size_t i = 0; i < kNames.size(); ++i) {
    if (strcasecmp(name.c_str(), std::string{kNames[i]}.c_str()) == 0) {
      return static_cast<Dialect>(i);
    }
  }
  return std::nullopt;
}

bool format_flag(std::span<const std::string> args, std::size_t& i, RunArgs& run) {
  const auto value = arg_str(args, i);
  const auto format = value ? dialect_named(*value) : std::nullopt;
  if (!format) {
    return false;
  }
  run.settings.format = *format;
  if (*format == Dialect::hspice) {
    run.settings.scale = true;
  }
  return true;
}

bool hier_flag(std::span<const std::string> args, std::size_t& i, RunArgs& run) {
  const auto value = arg_str(args, i);
  if (value && strcasecmp(value->c_str(), "HIER") == 0) {
    run.settings.hier_ap = true;
    return true;
  }
  if (value && strcasecmp(value->c_str(), "FLAT") == 0) {
    run.settings.hier_ap = false;
    return true;
  }
  return false;
}

// spcParseArgs' flags with a value. `-o` is EFArgs' (ArgOptions::output),
// so it never reaches here.
bool value_flag(std::span<const std::string> args, std::size_t& i, RunArgs& run, char flag) {
  switch (flag) {
    case 'f':
      return format_flag(args, i, run);
    case 'J':
      return hier_flag(args, i, run);
    case 'y':
      if (!arg_str(args, i)) {
        return false;
      }
      run.messages.push_back({false, "The cap accuracy flag 'y' is deprecated."});
      return true;
    default:
      break;
  }
  run.messages.push_back({true, "Unrecognized flag: " + args[i]});
  return false;
}

// -M / -m set Magic's two merge booleans; aggressive wins a query.
void merge_flag(RunArgs& run, bool aggressive) {
  if (aggressive) {
    run.settings.merge = MergeMode::aggressive;
  } else if (run.settings.merge != MergeMode::aggressive) {
    run.settings.merge = MergeMode::conservative;
  }
}

bool switch_flag(RunArgs& run, char flag) {
  switch (flag) {
    case 'd':
      run.settings.distributed_junctions = true;
      return true;
    case 'M':
    case 'm':
      merge_flag(run, flag == 'M');
      return true;
    case 'B':
      run.settings.no_attributes = true;
      return true;
    case 'F':
      run.flags.device_nodes_only = true;
      return true;
    case 'h':
      run.messages.push_back({false, std::string{kUsage.substr(0, kUsage.size() - 1)}});
      return true;
    default:
      return false;
  }
}

bool finite_cap(double cthresh) {
  return std::isfinite(cthresh) && static_cast<float>(cthresh) != 1.0E38F;
}

compat::CompatSettings session_compat(const editor::EditorState& editor) {
  compat::CompatSettings compat;
  const auto value = editor.settings.effective(settings::kMagicCompat);
  compat.magic_compat = !value || value->value != "off";
  return compat;
}

std::vector<std::string> global_nets(const editor::EditorState& editor) {
  std::vector<std::string> names;
  if (const auto value = editor.settings.effective(app::kGlobalNets)) {
    std::string word;
    for (const char c : value->value + " ") {
      if (c == ' ') {
        if (!word.empty()) {
          names.push_back(word);
        }
        word.clear();
      } else {
        word += c;
      }
    }
  }
  return names;
}

}  // namespace

namespace detail {

extflat::FlatFlags flat_flags(const SpiceSettings& settings) {
  extflat::FlatFlags flags = extflat::FlatFlags::flat_nodes;
  if (!settings.global) {
    flags = flags | extflat::FlatFlags::no_name_merge;
  }
  if (finite_cap(settings.cthresh)) {
    flags = flags | extflat::FlatFlags::flat_caps;
  }
  return flags;
}

void apply_style(SpiceInput& input, const editor::EditorState& editor) {
  const auto& style = editor.extract.current;
  if (style == nullptr) {
    input.style_matches = false;
    return;
  }
  input.resist_classes = style->resist_classes.size();
  apply_device_info(input, style->device_info());
  if (const auto gnd = editor.settings.effective(app::kGnd); gnd && !gnd->value.empty()) {
    input.gnd_names.insert(input.gnd_names.begin(), gnd->value);
  }
}

}  // namespace detail

namespace {

// The style lookups the read takes from the session: extGetDevType and
// DBNumTypes, which key the device table, and ExtCompareStyle.
void add_style_lookups(extflat::ReadOptions& options, const editor::EditorState& editor) {
  if (editor.technology) {
    options.tile_type_count = static_cast<std::int32_t>(editor.technology->type_count());
  }
  if (const auto style = editor.extract.current) {
    options.device_tile_type = [style](std::string_view name) -> std::optional<std::int32_t> {
      for (const extract::DeviceInfo& info : style->device_info()) {
        if (info.model == name) {
          return static_cast<std::int32_t>(info.type);
        }
      }
      return std::nullopt;
    };
  }
  options.style_units = [&editor](std::string_view name) -> std::optional<float> {
    const auto style = editor.extract.styles.select(name);
    return style ? std::optional<float>{style->units_per_lambda} : std::nullopt;
  };
}

}  // namespace

namespace detail {

// EFReadFile(inName, esDoHierarchy, esDoExtResis, FALSE, TRUE)
// (ext2spice.c:888) with EFOutputFlags' short mode, and the style lookups
// the read takes from the session: extGetDevType/DBNumTypes, which key the
// device table, and ExtCompareStyle for a `style` record.
extflat::ReadOptions read_options(const editor::EditorState& editor, const SpiceSettings& settings,
                                  const compat::CompatSettings& compat) {
  extflat::ReadOptions options;
  options.magic_compat = compat.magic_compat;
  options.limits = editor.netlist.read_limits;
  options.tech_name = editor.technology ? editor.technology->name() : std::string{};
  options.global_nets = global_nets(editor);
  options.search_path = settings.search_path;
  options.cell_directory = [&editor](std::string_view name) -> std::optional<std::string> {
    const db::CellDef* def = editor.cells.find(name);
    if (def == nullptr || def->file_path().empty()) {
      return std::nullopt;
    }
    return def->file_path().parent_path().string();
  };
  if (editor.extract.current != nullptr) {
    options.units_per_lambda = editor.extract.current->units_per_lambda;
  }
  options.subcircuits = settings.hierarchy;
  options.resist_overlay = settings.extresist;
  options.noscale = false;
  options.spice = true;
  options.shorts = static_cast<extflat::ReadOptions::PortShorts>(settings.shorts);
  add_style_lookups(options, editor);
  return options;
}

// The flattening settings of a run: efFlatSingleCap's EFCapThreshold is
// the run's threshold (the option level, then -c/-C).
extflat::FlattenOptions flatten_options(const editor::EditorState& editor, const SpiceSettings& settings,
                                        const extflat::SymbolTable& symbols) {
  extflat::FlattenOptions options;
  options.cap_threshold = settings.cthresh;
  if (const auto gnd = editor.settings.effective(app::kGnd)) {
    options.gnd = gnd->value;
  }
  options.symbols = symbols;
  return options;
}

}  // namespace detail

std::optional<std::size_t> parse_spice_flag(std::span<const std::string> args, std::size_t flag, RunArgs& run) {
  const std::string& word = args[flag];
  const char letter = word.size() > 1 ? word[1] : '\0';
  std::size_t i = flag;
  if (switch_flag(run, letter) || value_flag(args, i, run, letter)) {
    return i + 1;
  }
  run.messages.push_back({true, std::string{kUsage.substr(0, kUsage.size() - 1)}});
  return std::nullopt;
}

namespace detail {

diag::Outcome<SpiceInput> read_input(editor::EditorState& editor, std::string_view cell,
                                     const SpiceSettings& settings, const compat::CompatSettings& compat,
                                     platform::FileSystem& fs, diag::DiagnosticSink& sink,
                                     const extflat::SymbolTable& symbols) {
  cell::LookupContext lookup{fs, editor.search_paths, editor.path_variables, sink};
  auto hierarchy = extflat::read(lookup, cell, read_options(editor, settings, compat));
  if (!hierarchy.ok()) {
    return hierarchy.error();
  }
  // EFFlatBuild with its messages (split globals, unresolved names), the
  // run's EFCapThreshold, $GND and the -s/-S symbols.
  auto netlist = extflat::flatten(hierarchy.value(), flat_flags(settings), flatten_options(editor, settings, symbols),
                                  sink);
  if (!netlist.ok()) {
    return netlist.error();
  }
  SpiceInput input = from_netlist(netlist.value(), cell);
  apply_style(input, editor);
  return input;
}

std::vector<diag::Diagnostic> to_diagnostics(const std::vector<WriteMessage>& messages) {
  std::vector<diag::Diagnostic> out;
  for (const WriteMessage& m : messages) {
    out.push_back(diag::make(m.warning ? db::codes::kWarning : db::codes::kReport, m.text));
  }
  return out;
}

namespace {

// EFArgs' `-t`: trim and conversion characters added to the run's flags.
void apply_trim(extflat::NameFlags& names, const std::string& chars) {
  names.trim_global = names.trim_global || chars.find('!') != std::string::npos;
  names.trim_local = names.trim_local || chars.find('#') != std::string::npos;
  names.convert_comma = names.convert_comma || chars.find(',') != std::string::npos;
  names.convert_equal = names.convert_equal || chars.find('=') != std::string::npos;
  names.convert_brackets = names.convert_brackets || chars.find_first_of("[]") != std::string::npos;
}

}  // namespace

// The netlist of one run: flat, or hierarchical with `hierarchy on`.
// EFArgs' -t is applied to the name flags before the read, in both paths,
// as EFArgs sets EFOutputFlags while it parses: with magic-compat ON (the
// session's flags) a `-t` survives a failed read.
diag::Outcome<WriteResult> write_run(editor::EditorState& editor, std::string_view cell, const RunRequest& request,
                                     platform::FileSystem& fs, diag::DiagnosticSink& sink, extflat::NameFlags& names) {
  apply_trim(names, request.trim_chars);
  if (request.settings.hierarchy) {
    return write_hierarchy(editor, cell, request, fs, sink, names);
  }
  auto input = read_input(editor, cell, request.settings, request.compat, fs, sink, request.symbols);
  if (!input.ok()) {
    return input.error();
  }
  return write_flat(input.value(), request.settings, request.flags, request.compat, names);
}

namespace {

// The run's cell: the argument, else the current window's root cell.
std::optional<std::string> run_cell(const editor::EditorState& editor, const extflat::CommonArgs& args) {
  if (args.cell) {
    return args.cell;
  }
  if (const view::LayoutWindow* w = editor.windows.current(); w != nullptr && w->root() != nullptr) {
    return w->root()->name();
  }
  return std::nullopt;
}

// The run's copy of the settings: EFArgs' -c, -C apply to this run only.
SpiceSettings run_settings(const SpiceSettings& session, const extflat::CommonArgs& args) {
  SpiceSettings local = session;
  if (args.cap_threshold) {
    local.cthresh = static_cast<double>(static_cast<float>(*args.cap_threshold));  // EFCapValue
  }
  if (args.cap_infinite) {
    local.cthresh = std::numeric_limits<double>::infinity();
  }
  return local;
}

cmd::CommandResult with_messages(cmd::CommandResult result, std::vector<diag::Diagnostic> messages) {
  messages.insert(messages.end(), std::make_move_iterator(result.diagnostics.begin()),
                  std::make_move_iterator(result.diagnostics.end()));
  result.diagnostics = std::move(messages);
  return result;
}

}  // namespace

cmd::CommandResult run_session(editor::EditorState& editor, const std::vector<std::string>& args,
                               const compat::CompatSettings& compat, platform::FileSystem& fs) {
  // Q-NETLIST-54 (compat): spcParseArgs' -M -m -f -B -d -J set Magic's
  // globals, so they hold for later runs with magic-compat ON; with it OFF
  // they change a copy for this run only.
  SpiceSettings run_copy = editor.netlist.spice;
  SpiceSettings& session = compat.magic_compat ? editor.netlist.spice : run_copy;
  RunArgs run{session, {}, {}};
  auto parsed = extflat::parse_common_args(
      args, [&run](std::span<const std::string> words, std::size_t flag) { return parse_spice_flag(words, flag, run); },
      extflat::ArgOptions{true, false, extflat::file_reader(fs)});
  std::vector<diag::Diagnostic> messages = to_diagnostics(run.messages);
  if (!parsed.ok()) {
    editor.netlist.search_path.clear();  // EFDone
    return with_messages(cmd::CommandResult::failure(parsed.error()), std::move(messages));
  }
  const extflat::CommonArgs& common = parsed.value();
  messages.insert(messages.end(), common.messages.begin(), common.messages.end());
  // Q-NETLIST-61: -p holds for this run only (EFDone frees EFSearchPath).
  const std::vector<std::string> search_path = take_search_path(editor.netlist, common.search_path);
  const auto cell = run_cell(editor, common);
  if (!cell) {
    if (compat.magic_compat) {
      editor.netlist.search_path = search_path;  // Magic returns here without EFDone
    }
    return with_messages(cmd::CommandResult::failure(diag::make(kNoCell, "Point to a window or specify a cell name.")),
                         std::move(messages));
  }
  // Q-NETLIST-42 (fix): `-o` names this run's file only. EFArgs takes it
  // for ext2spice (ArgOptions::output), into this run's CommonArgs.
  const std::string output = common.output.value_or(*cell + (session.format == Dialect::cdl ? ".cdl" : ".spice"));
  SpiceSettings local = run_settings(session, common);
  local.search_path = search_path;
  // Q-NETLIST-16: the session's flags with magic-compat ON, fresh ones per
  // run with it OFF.
  extflat::NameFlags fresh;
  extflat::NameFlags& names = compat.magic_compat ? editor.netlist.names : fresh;
  diag::CollectingSink sink;
  auto result = write_run(editor, *cell, RunRequest{local, run.flags, compat, common.symbols, common.trim_chars},
                          fs, sink, names);
  for (diag::Diagnostic& d : sink.take()) {
    messages.push_back(std::move(d));
  }
  if (!result.ok()) {
    // EFReadFile sets `Cannot read extract file <cell>.ext` as the Tcl
    // result and the command succeeds: copied with magic-compat ON; with
    // it OFF the run fails with the diagnostic.
    if (compat.magic_compat && result.error().code == kCannotRead) {
      return with_messages(cmd::CommandResult::success(result.error().text + "\n"), std::move(messages));
    }
    return with_messages(cmd::CommandResult::failure(result.error()), std::move(messages));
  }
  const WriteResult& written = result.value();
  if (!fs.write_atomic(output, written.text).ok()) {
    // A Tcl result in Magic, not a printed line.
    return with_messages(
        cmd::CommandResult::success(std::string{"exttospice: Unable to open file " + output + " for writing\n"}),
        std::move(messages));
  }
  for (diag::Diagnostic& d : to_diagnostics(written.messages)) {
    messages.push_back(std::move(d));
  }
  messages.push_back(diag::make(db::codes::kReport, "exttospice finished."));
  return with_messages(cmd::CommandResult::success(), std::move(messages));
}

}  // namespace detail

diag::Outcome<std::string> render(editor::EditorState& editor, std::string_view cell,
                                  const SpiceSettings& settings) {
  platform::LocalFileSystem fs;
  diag::CollectingSink sink;
  const compat::CompatSettings compat = session_compat(editor);
  // A fresh copy of the name flags: render never changes the session.
  extflat::NameFlags names = compat.magic_compat ? editor.netlist.names : extflat::NameFlags{};
  auto written = detail::write_run(editor, cell, detail::RunRequest{settings, RunFlags{}, compat, {}, {}}, fs, sink,
                                   names);
  if (!written.ok()) {
    return written.error();
  }
  return written.value().text;
}

cmd::CommandResult run(editor::EditorState& editor, const SpiceRun& request) {
  platform::LocalFileSystem fs;
  return detail::run_session(editor, request.args, session_compat(editor), fs);
}

}  // namespace hefesto::netlist::spice
