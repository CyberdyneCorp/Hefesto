// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2sim` run (netlist/sim/sim.hpp run(); capability netlist-sim
// "Command options and defaults", "File structure and order", "Name
// conversion state"; openspec/changes/m7-extraction/design.md D15).
//
// The decisions applied here:
// - Q-NETLIST-18 (compat): coupling capacitors and resistors are flattened
//   when the option-level thresholds are finite with magic-compat ON, when
//   the run's thresholds (after -c, -C, -r, -R) are with it OFF.
// - Q-NETLIST-16 (compat): the name-conversion flags are the session's
//   (EditorState::netlist.names, -t adds to them) with magic-compat ON, and
//   start empty for each run with it OFF.
// - Q-NETLIST-52 (fix, both modes): each run writes `.al` only when alias is
//   on and `.nodes` only when labels is on, and keeps no file of its own
//   past its end, so a later run never writes through or closes it.
// - simParseArgs keeps -A, -B, -F, -L, -M, -m, -f and -J in globals, so they
//   stay set for later runs: with magic-compat ON they change the session's
//   settings, with it OFF they apply to the run only. The names of -o, -a and
//   -l apply to the run only in both modes (Magic keeps pointers into the
//   freed argument words; the rule of Q-NETLIST-42).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2sim/ext2sim.c (CmdExtToSim's run
// path, simParseArgs), Copyright (C) 1985, 1990 Regents of the University
// of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SIM (design D3).

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/app/settings_layers.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/netlist/session.hpp"
#include "hefesto/netlist/sim/sim.hpp"
#include "hefesto/netlist/sim/writer.hpp"
#include "hefesto/platform/file_system.hpp"
#include "netlist/sim/sim_codes.hpp"

namespace hefesto::netlist::sim {

namespace {

using cmd::CommandResult;
using console::message;
using console::report;

constexpr std::string_view kCannotRead = "EXTFLAT-READ-001";

constexpr const char* kUsage =
    "Usage: ext2sim [-a aliasfile] [-A] [-B] [-l labelfile] [-L]\n"
    "[-o simfile] [-J flat|hier] [-y cap_digits]\n"
    "[-f mit|lbl|su] [file]";

// The run's own flags (simParseArgs) and the file names.
struct RunFlags {
  std::optional<std::string> alias_file;  // -a
  std::optional<std::string> label_file;  // -l
};

// ArgStr: the value of a flag, attached (`-ofile`, leading blanks skipped)
// or the next word; nullopt with Magic's message when there is none.
std::optional<std::string> arg_str(std::span<const std::string> args, std::size_t& i, const char* what,
                                   const console::Emit& emit) {
  const std::string& flag = args[i];
  if (flag.size() > 2) {
    const std::size_t start = flag.find_first_not_of(" \t\n\v\f\r", 2);
    return start == std::string::npos ? std::string{} : flag.substr(start);
  }
  if (i + 1 < args.size()) {
    return args[++i];
  }
  message(emit, std::string{"-"} + flag[1] + " requires a following " + what);
  return std::nullopt;
}

bool set_format(std::string_view value, SimSettings& settings) {
  const auto is = [&](std::string_view name) {
    return value.size() == name.size() &&
           std::equal(value.begin(), value.end(), name.begin(), [](char a, char b) { return std::toupper(a) == b; });
  };
  if (is("MIT")) {
    settings.format = SimFormat::mit;
  } else if (is("LBL")) {
    settings.format = SimFormat::lbl;
  } else if (is("SU")) {
    settings.format = SimFormat::su;
  } else {
    return false;
  }
  return true;
}

bool set_hier_ap(std::string_view value, SimSettings& settings) {
  const auto is = [&](std::string_view name) {
    return value.size() == name.size() &&
           std::equal(value.begin(), value.end(), name.begin(), [](char a, char b) { return std::toupper(a) == b; });
  };
  if (!is("HIER") && !is("FLAT")) {
    return false;
  }
  settings.hier_ap = is("HIER");
  return true;
}

// The switch flags of simParseArgs; false when `c` is not one of them.
bool switch_flag(char c, SimSettings& settings) {
  switch (c) {
    case 'A':
      settings.alias = false;
      return true;
    case 'B':
      settings.no_attrs = true;
      return true;
    case 'F':
      settings.dev_nodes_only = true;
      return true;
    case 'L':
      settings.labels = false;
      return true;
    case 'M':
      settings.merge = SimMerge::aggressive;
      return true;
    case 'm':
      if (settings.merge == SimMerge::none) {
        settings.merge = SimMerge::conservative;  // esMergeDevsA still wins
      }
      return true;
    default:
      return false;
  }
}

// One flag with a value; false on a usage error.
bool value_flag(std::span<const std::string> args, std::size_t& i, SimSettings& settings, RunFlags& run,
                const console::Emit& emit) {
  const char c = args[i][1];
  const char* what = c == 'f' ? "format" : c == 'y' ? "cap-accuracy" : c == 'J' ? "hierAP_SD" : "filename";
  const std::optional<std::string> value = arg_str(args, i, what, emit);
  if (!value) {
    return false;
  }
  switch (c) {
    case 'a':
      run.alias_file = value;
      return true;
    case 'l':
      run.label_file = value;
      return true;
    case 'f':
      return set_format(*value, settings);
    case 'J':
      return set_hier_ap(*value, settings);
    default:  // 'y'
      report(emit, "Cap accuracy option -y is deprecated.");
      return true;
  }
}

// simParseArgs as the ExtraFlags of extflat::parse_common_args.
extflat::ExtraFlags sim_flags(SimSettings& settings, RunFlags& run, const console::Emit& emit) {
  return [&settings, &run, emit](std::span<const std::string> args, std::size_t flag) -> std::optional<std::size_t> {
    std::size_t i = flag;
    const std::string& word = args[i];
    const char c = word.size() > 1 ? word[1] : '\0';
    if (switch_flag(c, settings)) {
      return i + 1;
    }
    if (std::string_view{"alfyJ"}.find(c) != std::string_view::npos && c != '\0') {
      if (value_flag(args, i, settings, run, emit)) {
        return i + 1;
      }
    } else {
      message(emit, "Unrecognized flag: " + word);
    }
    message(emit, kUsage);
    return std::nullopt;
  };
}

void add_trim_chars(std::string_view chars, extflat::NameFlags& flags) {
  flags.trim_global = flags.trim_global || chars.find('!') != std::string_view::npos;
  flags.trim_local = flags.trim_local || chars.find('#') != std::string_view::npos;
  flags.convert_comma = flags.convert_comma || chars.find(',') != std::string_view::npos;
  flags.convert_equal = flags.convert_equal || chars.find('=') != std::string_view::npos;
  flags.convert_brackets =
      flags.convert_brackets || chars.find_first_of("[]") != std::string_view::npos;
}

float cap_value(double v) {
  return std::isinf(v) ? kInfiniteCap : static_cast<float>(v);
}

// The thresholds of the run: the option level, then -c/-C and -r/-R.
void run_thresholds(const extflat::CommonArgs& args, WriteOptions& write) {
  if (args.cap_infinite) {
    write.cap_threshold = kInfiniteCap;
  } else if (args.cap_threshold) {
    write.cap_threshold = static_cast<float>(*args.cap_threshold);
  }
  if (args.resist_infinite) {
    write.resist_threshold = kInfiniteResist;
  } else if (args.resist_threshold) {
    write.resist_threshold = *args.resist_threshold;
  }
}

extflat::FlatFlags flatten_flags(float cap, std::int64_t resist) {
  extflat::FlatFlags flags = extflat::FlatFlags::flat_nodes;
  if (cap != kInfiniteCap) {
    flags = flags | extflat::FlatFlags::flat_caps;
  }
  if (resist != kInfiniteResist) {
    flags = flags | extflat::FlatFlags::flat_resists;
  }
  return flags;
}

std::vector<std::string> global_nets(const editor::EditorState& editor) {
  std::vector<std::string> names;
  const auto value = editor.settings.effective(app::kGlobalNets);
  if (value) {
    std::istringstream in(value->value);
    for (std::string name; in >> name;) {
      names.push_back(name);
    }
  }
  return names;
}

// EFReadFile(inName, FALSE, esDoSimExtResis, FALSE, FALSE) (ext2sim.c:618):
// no subcircuit marking, `.res.ext` overlays with extresist on, scaled.
void ext2sim_read(extflat::ReadOptions& options, bool extresist) {
  options.subcircuits = false;
  options.resist_overlay = extresist;
  options.noscale = false;
}

// extGetDevType and DBNumTypes, which key the device table and so decide
// the device visit order (EFbuild.c efBuildDevice): the lowest tile type
// whose device list in the current style names the device, as
// ExtGetDevInfo lists them by type.
void device_types(extflat::ReadOptions& options, const editor::EditorState& editor) {
  if (editor.technology) {
    options.tile_type_count = static_cast<std::int32_t>(editor.technology->type_count());
  }
  if (!editor.extract.current) {
    return;
  }
  options.device_tile_type = [style = editor.extract.current](std::string_view name) -> std::optional<std::int32_t> {
    for (const extract::DeviceInfo& info : style->device_info()) {
      if (info.model == name) {
        return static_cast<std::int32_t>(info.type);
      }
    }
    return std::nullopt;
  };
}

extflat::ReadOptions read_options(const editor::EditorState& editor, const extflat::CommonArgs& args,
                                  bool extresist, bool magic_compat) {
  extflat::ReadOptions options;
  ext2sim_read(options, extresist);
  device_types(options, editor);
  options.magic_compat = magic_compat;
  if (editor.extract.current) {
    options.units_per_lambda = editor.extract.current->units_per_lambda;  // ExtCurStyle (EFread.c:252-259)
  }
  // ExtCompareStyle: a `style` record naming a style of the technology.
  options.style_units = [&editor](std::string_view name) -> std::optional<float> {
    const auto style = editor.extract.styles.select(name);
    return style ? std::optional<float>{style->units_per_lambda} : std::nullopt;
  };
  options.search_path = args.search_path;
  options.tech_name = editor.technology ? editor.technology->name() : std::string{};
  options.global_nets = global_nets(editor);
  options.cell_directory = [&editor](std::string_view name) -> std::optional<std::string> {
    const db::CellDef* def = editor.cells.find(name);
    if (def == nullptr || def->file_path().empty()) {
      return std::nullopt;
    }
    return def->file_path().parent_path().string();
  };
  return options;
}

// ExtGetDevInfo through the current extraction style (ext2sim.c:647).
void device_table(const editor::EditorState& editor, WriteOptions& write) {
  if (!editor.extract.current) {
    return;
  }
  for (const extract::DeviceInfo& info : editor.extract.current->device_info()) {
    write.device_info.push_back(info);
    write.type_long_names.emplace_back(editor.technology ? editor.technology->type_name(info.type) : "space");
  }
}

// The default source: EFReadFile and EFFlatBuild.
SimSource default_source(editor::EditorState& editor, platform::FileSystem& fs, const console::Emit& emit) {
  return [&editor, &fs, emit](const ReadRequest& request) -> diag::Outcome<SimInput> {
    diag::CollectingSink sink;
    cell::LookupContext lookup{fs, editor.search_paths, editor.path_variables, sink};
    auto hierarchy = extflat::read(lookup, request.cell, request.options);
    for (const diag::Diagnostic& d : sink.take()) {
      if (emit) {
        emit(d);
      }
    }
    if (!hierarchy.ok()) {
      return hierarchy.error();
    }
    // EFFlatBuild with its messages (split globals, unresolved names); a
    // use cycle or a reached limit fails the run, writing no file.
    auto netlist = extflat::flatten(hierarchy.value(), request.flags, request.flatten, sink);
    for (const diag::Diagnostic& d : sink.take()) {
      if (emit) {
        emit(d);
      }
    }
    if (!netlist.ok()) {
      return netlist.error();
    }
    return from_netlist(netlist.value());
  };
}

// The file names: `<cell>[.ext].sim`, `.al`, `.nodes`, or the run's names.
struct FileNames {
  std::string sim;
  std::string alias;
  std::string labels;
};

FileNames file_names(const std::string& cell, bool extresist, const extflat::CommonArgs& args,
                     const RunFlags& run) {
  const std::string base = cell + (extresist ? ".ext" : "");
  return FileNames{args.output.value_or(base + ".sim"), run.alias_file.value_or(base + ".al"),
                   run.label_file.value_or(base + ".nodes")};
}

// Writes every file of the run, or none (Q-NETLIST-52: the run owns them).
CommandResult write_files(platform::FileSystem& fs, const FileNames& names, const SimOutput& output) {
  std::vector<std::pair<const std::string*, const std::string*>> files{{&names.sim, &output.sim}};
  if (output.alias) {
    files.emplace_back(&names.alias, &*output.alias);
  }
  if (output.labels) {
    files.emplace_back(&names.labels, &*output.labels);
  }
  for (const auto& [name, text] : files) {
    const diag::Status st = fs.write_atomic(platform::Path{*name}, *text);
    if (!st.ok()) {
      return CommandResult::failure(
          diag::make(console::kWriteFailed, "exttosim: Unable to open file " + *name + " for writing"));
    }
  }
  return CommandResult::success();
}

// Q-NETLIST-59 (compat): EFReadFile sets `Cannot read extract file
// <cell>.ext` as the Tcl result and the command succeeds; copied with
// magic-compat ON. With it OFF the run fails with the diagnostic.
CommandResult read_failure(const diag::Diagnostic& error, bool magic_compat) {
  if (magic_compat && error.code == kCannotRead) {
    return CommandResult::success(error.text + "\n");
  }
  return CommandResult::failure(error);
}

struct Parsed {
  extflat::CommonArgs args;
  RunFlags run;
};

struct RunSetup {
  WriteOptions write;
  extflat::FlatFlags flags = extflat::FlatFlags::flat_nodes;
};

RunSetup run_setup(const editor::EditorState& editor, const SimSettings& settings, const extflat::CommonArgs& args,
                   bool magic_compat) {
  RunSetup setup;
  setup.write.settings = settings;
  setup.write.cap_threshold = cap_value(settings.cthresh);
  setup.write.resist_threshold = settings.rthresh;
  run_thresholds(args, setup.write);
  // Q-NETLIST-18: the option-level thresholds with magic-compat ON, the run's with it OFF.
  setup.flags = magic_compat ? flatten_flags(cap_value(settings.cthresh), settings.rthresh)
                             : flatten_flags(setup.write.cap_threshold, setup.write.resist_threshold);
  setup.write.flat_caps = extflat::has(setup.flags, extflat::FlatFlags::flat_caps);
  device_table(editor, setup.write);
  return setup;
}

// The flattening settings: efFlatSingleCap's EFCapThreshold is the run's
// (the option level, then -c/-C), with $GND and the -s/-S symbols.
extflat::FlattenOptions flatten_options(const editor::EditorState& editor, const WriteOptions& write,
                                        const extflat::CommonArgs& args) {
  extflat::FlattenOptions options;
  options.cap_threshold = static_cast<double>(write.cap_threshold);
  if (const auto gnd = editor.settings.effective(app::kGnd)) {
    options.gnd = gnd->value;
  }
  options.symbols = args.symbols;
  return options;
}

void emit_all(const console::Emit& emit, const std::vector<diag::Diagnostic>& messages) {
  for (const diag::Diagnostic& d : messages) {
    if (emit) {
      emit(d);
    }
  }
}

// After the read: the style warning, the writer, its console lines and the files.
CommandResult write_run(const editor::EditorState& editor, const SimRun& request, const WriteOptions& write,
                        SimInput& input, extflat::NameFlags& names, const FileNames& files) {
  const compat::CompatSettings compat{request.magic_compat};
  if (input.tech.empty() && editor.technology) {
    input.tech = editor.technology->name();
  }
  WriteOptions options = write;
  if (!input.style_matched) {
    options.device_info.clear();  // fetInfo keeps its defaults when EFStyle is NULL (ext2sim.c:657)
    options.type_long_names.clear();
    if (write.settings.format == SimFormat::su) {
      message(request.emit, "Warning:  Current extraction style does not match .ext file!");
      message(request.emit, "Area/Perimeter values will be zero.");
    }
  }
  const SimOutput output = write_sim(input, options, names, compat);
  for (const ConsoleLine& line : output.console) {
    if (line.error) {
      message(request.emit, line.text);
    } else {
      report(request.emit, line.text);
    }
  }
  platform::LocalFileSystem local;
  platform::FileSystem& fs = request.files != nullptr ? *request.files : local;
  CommandResult written = write_files(fs, files, output);
  if (written.ok()) {
    report(request.emit, "exttosim finished.");
  }
  return written;
}

}  // namespace

CommandResult run(editor::EditorState& editor, const SimRun& request) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = request.files != nullptr ? *request.files : local;
  return run(editor, request, default_source(editor, fs, request.emit));
}

CommandResult run(editor::EditorState& editor, const SimRun& request, const SimSource& source) {
  // simParseArgs' flags are globals in Magic: the session's settings with
  // magic-compat ON, a copy for this run with it OFF.
  SimSettings run_copy = editor.netlist.sim;
  SimSettings& settings = request.magic_compat ? editor.netlist.sim : run_copy;
  platform::LocalFileSystem local;
  platform::FileSystem& fs = request.files != nullptr ? *request.files : local;
  Parsed parsed;
  auto args = extflat::parse_common_args(request.args, sim_flags(settings, parsed.run, request.emit),
                                         extflat::ArgOptions{true, false, extflat::file_reader(fs)});
  if (!args.ok()) {
    editor.netlist.search_path.clear();  // EFDone
    return CommandResult::failure(args.error());
  }
  parsed.args = std::move(args).value();
  emit_all(request.emit, parsed.args.messages);
  // Q-NETLIST-61: -p holds for this run only (EFDone frees EFSearchPath,
  // also when no cell is named: ext2sim.c:548-552), or the path an
  // ext2spice run without a cell left.
  parsed.args.search_path = take_search_path(editor.netlist, parsed.args.search_path);
  const std::string cell = parsed.args.cell.value_or(request.window_cell);
  if (cell.empty()) {
    message(request.emit, "Point to a window or specify a cell name.");
    return CommandResult::success();
  }
  // Q-NETLIST-16: the session's flags with magic-compat ON, none with it OFF.
  extflat::NameFlags run_names{false, false, false, false, false};
  extflat::NameFlags& names = request.magic_compat ? editor.netlist.names : run_names;
  add_trim_chars(parsed.args.trim_chars, names);
  const RunSetup setup = run_setup(editor, settings, parsed.args, request.magic_compat);
  auto input = source(ReadRequest{cell, read_options(editor, parsed.args, settings.extresist, request.magic_compat),
                                  setup.flags, flatten_options(editor, setup.write, parsed.args)});
  if (!input.ok()) {
    return read_failure(input.error(), request.magic_compat);
  }
  return write_run(editor, request, setup.write, input.value(), names,
                   file_names(cell, settings.extresist, parsed.args, parsed.run));
}

}  // namespace hefesto::netlist::sim
