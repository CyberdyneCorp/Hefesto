// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extresist` run (extresist/command/command.hpp run(); capability
// extresist-command "Run flow", "Net selection and first cutoff", "Second
// cutoff after extraction", "Lumped resistance file"; extresist-output
// "Overlay file grammar and order"; design D16, D12).
//
// The target is the cell under the box: with magic-compat ON also for
// `extresist cell`, as Magic (ToolGetBox replaces the selected cell); with
// it OFF `cell` runs on the selected cell. Unless `blackbox` is on, the
// subcells with ports are processed first, each once, then the target.
// For each cell: ResReadExt (extresist/output/input.cpp), the nets in
// reverse order of first mention, the first cutoff, RESNET's network
// (yank_net, build_network, reduce), the second cutoff, and the
// `.res.ext` and `.res.lump` files, each written through
// extract::AtomicOutput (Q-EXTRACT-27): a run cancelled through
// mask::Progress, or a failed write, leaves the previous files and no
// temporary file.
//
// Each net goes through RESNET's R4 contract (extresist/network/
// network.hpp): yank_net from the node location (or its first drive
// point), build_network with the drive and sink points in ResNodeTable's
// hash order, then reduce, which sets maxres, the Elmore delay and
// rg_nodecap; the label sink points build_network adds stay in the input
// model for the later nets, as in Magic's node table. Terminal nodes are
// not renamed yet (tasks.md 14.3). `geometry` and `fasthenry` answer
// CORE-NOTIMPL-008 (tasks.md 14.4).
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResRex.c (CmdExtResis's run
// path, resSubcircuitFunc, ExtResisForDef, ResCheckExtNodes,
// ResProcessNode), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RESCMD.

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "extresist/command/options.hpp"
#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/file/atomic_output.hpp"
#include "hefesto/extresist/command/command.hpp"
#include "hefesto/extresist/command/cutoff.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::extresist {

namespace {

using cmd::CommandResult;
using detail::Console;

constexpr std::string_view kCancelled = "EXTRESIST-CMD-004";

// C's "%.2f" and "%f".
std::string fixed2(double value) {
  std::array<char, 64> buffer{};
  (void)std::snprintf(buffer.data(), buffer.size(), "%.2f", value);
  return buffer.data();
}

std::string fixed6(double value) {
  std::array<char, 64> buffer{};
  (void)std::snprintf(buffer.data(), buffer.size(), "%f", value);
  return buffer.data();
}

// ResWriteExtFile's delay text: femtoseconds when either value is below
// 0.01 ps, else picoseconds (`Tnew = %.2fps ; Tmin = %.2fps`).
std::string delay_text(double tdi_zs, double min_zs) {
  constexpr double kZeptoToPico = 1e-9;  // Z_TO_P
  const auto tnew = static_cast<float>(tdi_zs * kZeptoToPico);
  const auto tmin = static_cast<float>(min_zs * kZeptoToPico);
  if (tnew < 0.01F || (tmin < 0.01F && min_zs > 0)) {
    return "(Tnew = " + fixed2(static_cast<double>(tnew) * 1000) + "fs ; Tmin = " +
           fixed2(static_cast<double>(tmin) * 1000) + "fs)";
  }
  return "(Tnew = " + fixed2(static_cast<double>(tnew)) + "ps ; Tmin = " + fixed2(static_cast<double>(tmin)) + "ps)";
}

// ResWriteExtFile's `extresist debug` line for a net after its second
// cutoff: why it is withheld, or that it is added.
std::string debug_line(const std::string& net, const Extracted& measured, double node_cap, bool held,
                       const ResistOptions& options) {
  const std::string maxres = "(maxres = " + fixed2(measured.maxres * 0.001) + "ohm)";
  const double min_delay = min_delay_zs(options);
  if (held) {
    const bool by_resistance = options.threshold > 0 && measured.maxres < options.threshold;
    return by_resistance ? "Not adding " + net + " " + maxres
                         : "Not adding  " + net + "; " + delay_text(measured.tdi.value_or(0.0), min_delay);
  }
  if (min_delay == 0) {
    return "Adding " + net + " " + maxres;
  }
  const double tdi = measured.tdi.value_or(measured.maxres * node_cap);
  return "Adding " + net + "; " + delay_text(tdi, min_delay);
}

// One cell's outputs and counters.
struct CellRun {
  db::CellDef* def = nullptr;
  InputModel input;
  std::vector<OutputNet> nets;
  std::string lump;
  int total = 0;
  int extracted = 0;
  int output = 0;
};

class Runner {
 public:
  Runner(editor::EditorState& editor, const ResistRun& request, const mask::Progress* progress,
         platform::FileSystem& fs)
      : editor_(editor), request_(request), progress_(progress), fs_(fs), console_{request.emit},
        options_(editor.extresist.options) {}

  CommandResult run(db::CellDef& root);

 private:
  [[nodiscard]] std::optional<std::string> load(std::string_view cell) const;
  [[nodiscard]] std::vector<std::string> parents(const db::CellDef& def) const;
  CommandResult subcircuits(db::CellDef& def);
  CommandResult process_cell(db::CellDef& def);
  CommandResult process_net(CellRun& cell, std::size_t net);
  CommandResult extract_net(CellRun& cell, std::size_t net, const Driver& driver);
  std::optional<Network> network_of(CellRun& cell, std::size_t net, const Driver& driver);
  BuildInput build_input(const CellRun& cell, const InputNode& node, db::Point start, const Driver& driver);
  void print(std::vector<std::string>& messages) const;
  CommandResult write_outputs(db::CellDef& def, CellRun& cell);
  CommandResult commit(db::CellDef& def, std::string_view suffix, const std::string& text,
                       const std::optional<std::string>& local_path);
  void statistics(const CellRun& cell) const;

  editor::EditorState& editor_;
  const ResistRun& request_;
  const mask::Progress* progress_;
  platform::FileSystem& fs_;
  Console console_;
  ResistOptions& options_;
  std::set<std::string> processed_;
};

// ExtFileOpen for reading: `extract path`, the cell's directory, then the name.
std::optional<std::string> Runner::load(std::string_view cell) const {
  std::vector<std::string> candidates;
  const std::string file = std::string{cell} + ".ext";
  if (editor_.extract.options.path && *editor_.extract.options.path != ".") {
    candidates.push_back(*editor_.extract.options.path + "/" + file);
  }
  if (const db::CellDef* def = editor_.cells.find(cell); def != nullptr && !def->file_path().empty()) {
    candidates.push_back((def->file_path().parent_path() / file).string());
  }
  candidates.push_back(file);
  for (const std::string& path : candidates) {
    auto text = fs_.read(platform::Path{path});
    if (text.ok()) {
      return std::move(text).value();
    }
  }
  return std::nullopt;
}

// cd_parents' definitions, each once, internal cells left out.
std::vector<std::string> Runner::parents(const db::CellDef& def) const {
  std::vector<std::string> out;
  for (const db::CellUse* use : def.instances()) {
    if (use == nullptr || use->parent == nullptr || use->parent->flags().internal) {
      continue;
    }
    const std::string& name = use->parent->name();
    if (std::find(out.begin(), out.end(), name) == out.end()) {
      out.push_back(name);
    }
  }
  return out;
}

bool has_ports(const db::CellDef& def) {
  return std::any_of(def.labels().begin(), def.labels().end(), [](const db::Label& l) { return l.is_port(); });
}

// resSubcircuitFunc: each subcell with ports, then its own subcells.
CommandResult Runner::subcircuits(db::CellDef& def) {
  for (const auto& use : def.uses()) {
    db::CellDef* child = use->def;
    if (child == nullptr) {
      continue;
    }
    if (has_ports(*child) && processed_.insert(child->name()).second) {
      if (CommandResult r = process_cell(*child); !r.ok()) {
        return r;
      }
    }
    if (CommandResult r = subcircuits(*child); !r.ok()) {
      return r;
    }
  }
  return CommandResult::success();
}

CommandResult Runner::run(db::CellDef& root) {
  if (!options_.blackbox) {
    if (CommandResult r = subcircuits(root); !r.ok()) {
      return r;
    }
  }
  processed_.insert(root.name());
  return process_cell(root);
}

CommandResult Runner::process_cell(db::CellDef& def) {
  InputOptions in;
  in.tech = editor_.technology.get();
  in.style = editor_.extract.current.get();
  in.debug = options_.debug;
  in.magic_compat = request_.magic_compat;
  in.load = [this](std::string_view cell) { return load(cell); };
  in.parents = parents(def);
  in.emit = request_.emit;
  auto input = read_input(def.name(), in);
  if (!input.ok()) {
    console_.message(input.error().text);
    return CommandResult::success();  // ExtResisForDef returns; the run goes on
  }
  CellRun cell;
  cell.def = &def;
  cell.input = std::move(input).value();
  for (const std::size_t net : cell.input.net_order()) {
    if (progress_ != nullptr && progress_->cancelled()) {
      // Q-EXTRACT-27: nothing is written; the previous files stay.
      return CommandResult::failure(diag::make(kCancelled, "extresist interrupted: " + def.name() + " not written"));
    }
    if (CommandResult r = process_net(cell, net); !r.ok()) {
      return r;
    }
  }
  return write_outputs(def, cell);
}

// ResProcessNode.
CommandResult Runner::process_net(CellRun& cell, std::size_t net) {
  const InputNode& node = cell.input.nodes[net];
  if (net_skip(node, options_, options_.all) != NetSkip::none) {
    return CommandResult::success();
  }
  ++cell.total;
  const std::optional<Driver> driver = choose_driver(cell.input, net);
  if (!driver) {
    if (node.status.force) {
      console_.message("Node " + node.name + " has force label but no drive point or driving device");
    }
    return CommandResult::success();
  }
  if (!first_cutoff(node, options_, options_.all)) {
    return CommandResult::success();
  }
  if (!options_.silent) {
    std::string line = "Extracting " + node.name;
    if (options_.debug) {
      line += " (Rnode = " + fixed2(node.resistance * 0.001) + "ohm ; Rthresh = " +
              fixed2(options_.threshold * 0.001) + "ohm)";
    }
    console_.report(line);
  }
  ++cell.extracted;
  return extract_net(cell, net, *driver);
}

// ResExtractNet's start: the node location, or the first drive point (a
// node at infinity); the start types are DBConnectTbl of the node's type,
// else of the first drive point with a type.
struct NetStart {
  db::Rect area;
  db::Point point;
  tech::TileType type = tech::kSpace;
};

std::string start_error(const InputNode& node) {
  if (!node.drive_points.empty()) {
    return "Node " + node.name + ":  Did not find the net layout at any drivepoint.";
  }
  const db::Point at = node.location.value_or(db::Point{});
  return "Node " + node.name + ":  Did not find the net layout at node location (" + std::to_string(at.x) + " " +
         std::to_string(at.y) + ").";
}

std::optional<NetStart> net_start(const InputNode& node, const Console& console) {
  NetStart out;
  if (node.location) {
    out.point = *node.location;
    out.area = db::Rect{out.point.x - 2, out.point.y - 2, out.point.x + 2, out.point.y + 2};
  } else if (!node.drive_points.empty()) {
    const db::Rect& r = node.drive_points.front().rect;
    out.area = db::Rect{r.x0 - 2, r.y0 - 2, r.x1 + 2, r.y1 + 2};
    out.point = db::Point{(r.x1 + r.x0) / 2, (r.y1 + r.y0) / 2};
  } else {
    console.message("Internal error:  Node location is set to infinity.");
  }
  if (node.tile_type && *node.tile_type != tech::kSpace) {
    out.type = *node.tile_type;
    return out;
  }
  const auto typed = std::find_if(node.drive_points.begin(), node.drive_points.end(), [](const ConnectPoint& c) {
    return c.type && *c.type != tech::kSpace;
  });
  if (typed == node.drive_points.end()) {
    console.message(start_error(node));
    return std::nullopt;
  }
  out.type = *typed->type;
  return out;
}

// ResMakeDriverSinkPorts' order: the nodes in ResNodeTable's hash order
// (Magic's string table of 1024 buckets, filled in order of first mention),
// each node's drive points, then its sink points.
std::vector<NetPort> table_ports(const InputModel& input) {
  util::MagicHashTable<std::size_t> table(1024);
  for (std::size_t i = 0; i < input.nodes.size(); ++i) {
    table.insert(input.nodes[i].name, i);
  }
  std::vector<NetPort> ports;
  table.for_each([&](const util::MagicHashTable<std::size_t>::Entry& e) {
    const InputNode& node = input.nodes[*e.value];
    for (const ConnectPoint& c : node.drive_points) {
      ports.push_back(NetPort{node.name, c.rect, c.type.value_or(tech::kSpace), false});
    }
    for (const ConnectPoint& c : node.sink_points) {
      ports.push_back(NetPort{node.name, c.rect, c.type.value_or(tech::kSpace), true});
    }
  });
  return ports;
}

// ResMakeLabelPorts' sink points stay in the node table for later nets.
void keep_added_sinks(InputModel& input, const std::vector<NetPort>& sinks) {
  for (auto it = sinks.rbegin(); it != sinks.rend(); ++it) {
    std::optional<std::size_t> index = input.find(it->name);
    if (!index) {
      input.nodes.emplace_back();
      input.nodes.back().name = it->name;
      index = input.nodes.size() - 1;
    }
    std::vector<ConnectPoint>& sinks_of = input.nodes[*index].sink_points;
    sinks_of.insert(sinks_of.begin(), ConnectPoint{it->rect, it->type});
  }
}

BuildInput Runner::build_input(const CellRun& cell, const InputNode& node, db::Point start, const Driver& driver) {
  BuildInput in;
  in.technology = editor_.technology.get();
  in.cif = editor_.mask.output_styles.current_style();
  in.start = start;
  in.driver = NetDriver{driver.location, driver.type.value_or(tech::kSpace), driver.drive_only};
  in.ports = table_ports(cell.input);
  const InputModel& input = cell.input;
  in.has_drive_point = [&input](const std::string& name, const db::Rect& label) {
    const std::optional<std::size_t> i = input.find(name);
    return i && std::any_of(input.nodes[*i].drive_points.begin(), input.nodes[*i].drive_points.end(),
                            [&label](const ConnectPoint& c) { return db::touch(label, c.rect); });
  };
  for (const InputNode& n : input.nodes) {
    if (n.status.port) {
      in.port_nodes.insert(n.name);
    }
  }
  in.node_cap = node.cap;
  in.small_contact_warned = &editor_.extresist.small_contact_warned;
  in.magic_compat = request_.magic_compat;
  return in;
}

void Runner::print(std::vector<std::string>& messages) const {
  for (const std::string& m : messages) {
    console_.message(m);
  }
  messages.clear();
}

// ResExtractNet then ResDoSimplify; nullopt when the net was not extracted
// (the messages are printed).
std::optional<Network> Runner::network_of(CellRun& cell, std::size_t net, const Driver& driver) {
  const std::optional<NetStart> start = net_start(cell.input.nodes[net], console_);
  if (!start) {
    return std::nullopt;
  }
  YankRequest yank;
  yank.area = start->area;
  yank.start_types = editor_.technology->connects_to(start->type);
  yank.skip = options_.skip;
  yank.magic_compat = request_.magic_compat;
  auto geometry = yank_net(*cell.def, *editor_.technology, yank);
  if (!geometry.ok()) {
    console_.message(geometry.error().text);
    return std::nullopt;
  }
  auto built = build_network(geometry.value(), *editor_.extract.current, cell.input.annotations(net),
                             build_input(cell, cell.input.nodes[net], start->point, driver));
  if (!built.ok()) {
    console_.message(built.error().text);
    return std::nullopt;
  }
  Network network = std::move(built).value();
  print(network.messages);
  keep_added_sinks(cell.input, network.added_sinks);
  if (network.failed) {
    return std::nullopt;
  }
  const Tolerances tol{options_.min_resist, min_delay_zs(options_), options_.simplify, options_.lumped};
  const diag::Status reduced = reduce(network, tol, editor_.extresist.heap, compat::CompatSettings{request_.magic_compat});
  print(network.messages);
  if (!reduced.ok()) {
    console_.message(reduced.error().text);
    return std::nullopt;
  }
  return network;
}

CommandResult Runner::extract_net(CellRun& cell, std::size_t net, const Driver& driver) {
  std::optional<Network> network = network_of(cell, net, driver);
  const InputNode& node = cell.input.nodes[net];
  if (!network) {
    if (node.tile_type == tech::kSpace) {
      console_.report("Note:  Substrate node " + node.name + " not extracted as network.");
    } else {
      console_.message("Error in extracting node " + node.name);
    }
    return CommandResult::success();
  }
  const std::optional<double> tdi = network->tdi == -1 ? std::nullopt : std::optional<double>{network->tdi};
  const Extracted measured{network->max_resistance, tdi};
  InputNode lumped = node;
  lumped.cap = network->node_cap;  // ResWriteLumpFile reads rg_nodecap after ResDoSimplify
  if (options_.lumped) {
    cell.lump += lump_line(node.name, lumped_ohms(lumped, measured, options_));
  }
  const bool held = withheld(node, measured, options_);
  if (options_.debug) {
    console_.report(debug_line(node.name, measured, network->node_cap, held, options_));
  }
  if (held) {
    return CommandResult::success();
  }
  ++cell.output;
  cell.nets.push_back(OutputNet{net, std::move(*network), {}, lumped_ohms(lumped, measured, options_)});
  return CommandResult::success();
}

CommandResult Runner::commit(db::CellDef& def, std::string_view suffix, const std::string& text,
                             const std::optional<std::string>& local_path) {
  diag::CollectingSink sink;
  const cell::LookupContext ctx{fs_, editor_.search_paths, editor_.path_variables, sink};
  extract::OutputLocation where;
  where.local_path = local_path;
  auto out = extract::AtomicOutput::open(ctx, def, suffix, where);
  if (!out.ok()) {
    console_.message("Couldn't open output file");
    return CommandResult::failure(diag::make(detail::kWriteFailed, out.error().text));
  }
  if (diag::Status st = out.value().write(text); !st.ok()) {
    return CommandResult::failure(diag::make(detail::kWriteFailed, st.error().text));
  }
  if (diag::Status st = out.value().commit(); !st.ok()) {
    return CommandResult::failure(diag::make(detail::kWriteFailed, st.error().text));
  }
  return CommandResult::success();
}

CommandResult Runner::write_outputs(db::CellDef& def, CellRun& cell) {
  if (options_.extout) {
    auto text = write_res_ext(cell.input, cell.nets, *editor_.extract.current,
                              compat::CompatSettings{request_.magic_compat});
    if (!text.ok()) {
      return CommandResult::failure(text.error());
    }
    if (CommandResult r = commit(def, ".res.ext", text.value(), editor_.extract.options.path); !r.ok()) {
      return r;
    }
  }
  if (options_.lumped) {
    if (CommandResult r = commit(def, ".res.lump", cell.lump, std::string{"."}); !r.ok()) {
      return r;
    }
  }
  statistics(cell);
  return CommandResult::success();
}

void Runner::statistics(const CellRun& cell) const {
  if (cell.total == 0) {
    console_.report("Total Nodes: 0");
    return;
  }
  const float total = static_cast<float>(cell.total);
  console_.report("Total Nets: " + std::to_string(cell.total) + "\nNets extracted: " +
                  std::to_string(cell.extracted) + " (" +
                  fixed6(static_cast<double>(static_cast<float>(cell.extracted) / total)) + ")\nNets output: " +
                  std::to_string(cell.output) + " (" +
                  fixed6(static_cast<double>(static_cast<float>(cell.output) / total)) + ")");
}

// The run's target: the box cell (and, with magic-compat OFF, the selected
// cell of `extresist cell`).
db::CellDef* target(editor::EditorState& editor, const ResistRun& request) {
  if (request.cell && !request.magic_compat) {
    return editor.cells.find(*request.cell);
  }
  return editor.box ? editor.box_root : nullptr;
}

// geometry and fasthenry: for this run only, all on and the files and
// simplification off (CmdExtResis's saveFlags).
class FlagGuard {
 public:
  FlagGuard(ResistOptions& options, bool active) : options_(options), saved_(options), active_(active) {
    if (active_) {
      options_.all = true;
      options_.extout = false;
      options_.lumped = false;
      options_.simplify = false;
    }
  }
  FlagGuard(const FlagGuard&) = delete;
  FlagGuard& operator=(const FlagGuard&) = delete;
  ~FlagGuard() {
    if (active_) {
      options_ = saved_;
    }
  }

 private:
  ResistOptions& options_;
  ResistOptions saved_;
  bool active_;
};

}  // namespace

CommandResult run(editor::EditorState& editor, const ResistRun& request, const mask::Progress* progress) {
  const Console console{request.emit};
  const FlagGuard guard(editor.extresist.options, request.geometry || request.fasthenry);
  db::CellDef* root = target(editor, request);
  if (root == nullptr) {
    console.message("Couldn't find def corresponding to box");
    return CommandResult::success();
  }
  if (request.geometry || request.fasthenry) {
    return cmd::m7_not_implemented(request.geometry ? "extresist geometry" : "extresist fasthenry");
  }
  if (!editor.technology || !editor.extract.current) {
    console.message("No extraction style: extresist cannot run.");
    return CommandResult::success();
  }
  platform::LocalFileSystem local;
  platform::FileSystem& fs = request.files != nullptr ? *request.files : local;
  Runner runner(editor, request, progress, fs);
  return runner.run(*root);
}

}  // namespace hefesto::extresist
