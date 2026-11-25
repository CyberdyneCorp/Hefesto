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
// The network contract (extresist/network/network.hpp) does not yet carry
// the driver node, the Elmore delay or the device terminals of each node;
// until it does, maxres is the largest shortest-path resistance from the
// network's first node, no delay is computed, and terminal nodes are not
// renamed (tasks.md 14.3). `geometry` and `fasthenry` need the network's
// widths and heights and answer CORE-NOTIMPL-008.
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
#include <limits>
#include <queue>
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

namespace hefesto::extresist {

namespace {

using cmd::CommandResult;
using detail::Console;

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-008";
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

// The largest shortest-path resistance from the network's first node.
double max_resistance(const Network& network) {
  const std::size_t n = network.nodes.size();
  if (n == 0) {
    return 0.0;
  }
  std::vector<std::vector<std::pair<std::size_t, double>>> edges(n);
  for (const NetResistor& r : network.resistors) {
    if (r.a < n && r.b < n) {
      edges[r.a].emplace_back(r.b, r.resistance);
      edges[r.b].emplace_back(r.a, r.resistance);
    }
  }
  std::vector<double> dist(n, std::numeric_limits<double>::infinity());
  using Item = std::pair<double, std::size_t>;
  std::priority_queue<Item, std::vector<Item>, std::greater<>> queue;
  dist[0] = 0.0;
  queue.emplace(0.0, 0);
  while (!queue.empty()) {
    const auto [d, u] = queue.top();
    queue.pop();
    if (d > dist[u]) {
      continue;
    }
    for (const auto& [v, w] : edges[u]) {
      if (d + w < dist[v]) {
        dist[v] = d + w;
        queue.emplace(dist[v], v);
      }
    }
  }
  double best = 0.0;
  for (const double d : dist) {
    best = d != std::numeric_limits<double>::infinity() && d > best ? d : best;
  }
  return best;
}

// One cell's outputs and counters.
struct CellRun {
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
  CommandResult process_net(db::CellDef& def, CellRun& cell, std::size_t net);
  CommandResult extract_net(db::CellDef& def, CellRun& cell, std::size_t net, const Driver& driver);
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
  cell.input = std::move(input).value();
  for (const std::size_t net : cell.input.net_order()) {
    if (progress_ != nullptr && progress_->cancelled()) {
      // Q-EXTRACT-27: nothing is written; the previous files stay.
      return CommandResult::failure(diag::make(kCancelled, "extresist interrupted: " + def.name() + " not written"));
    }
    if (CommandResult r = process_net(def, cell, net); !r.ok()) {
      return r;
    }
  }
  return write_outputs(def, cell);
}

// ResProcessNode.
CommandResult Runner::process_net(db::CellDef& def, CellRun& cell, std::size_t net) {
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
  return extract_net(def, cell, net, *driver);
}

CommandResult Runner::extract_net(db::CellDef& def, CellRun& cell, std::size_t net, const Driver& driver) {
  const InputNode& node = cell.input.nodes[net];
  const tech::TileType type = driver.type.value_or(node.tile_type.value_or(tech::kSpace));
  auto geometry = yank_net(def, driver.location, type, options_.skip);
  diag::Outcome<Network> network = geometry.ok() ? build_network(geometry.value(), *editor_.extract.current,
                                                                 cell.input.annotations(net))
                                                 : diag::Outcome<Network>{geometry.error()};
  if (network.ok()) {
    const Tolerances tol{options_.min_resist, min_delay_zs(options_), options_.simplify};
    if (diag::Status st = reduce(network.value(), tol, editor_.extresist.heap, compat::CompatSettings{request_.magic_compat});
        !st.ok()) {
      network = st.error();
    }
  }
  if (!network.ok()) {
    if (network.error().code == kNotImplemented) {
      return CommandResult::failure(network.error());  // RESNET's network is not there yet
    }
    if (node.tile_type == tech::kSpace) {
      console_.report("Note:  Substrate node " + node.name + " not extracted as network.");
    } else {
      console_.message("Error in extracting node " + node.name);
    }
    return CommandResult::success();
  }
  const Extracted measured{max_resistance(network.value()), std::nullopt};
  if (options_.lumped) {
    cell.lump += lump_line(node.name, lumped_ohms(node, measured, options_));
  }
  if (withheld(node, measured, options_)) {
    if (options_.debug) {
      console_.report("Not adding " + node.name + " (maxres = " + fixed2(measured.maxres * 0.001) + "ohm)");
    }
    return CommandResult::success();
  }
  ++cell.output;
  cell.nets.push_back(OutputNet{net, std::move(network).value(), {}, lumped_ohms(node, measured, options_)});
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
