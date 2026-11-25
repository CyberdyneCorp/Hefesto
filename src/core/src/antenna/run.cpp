// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `antennacheck` run (antenna/antenna.hpp check(); capability
// antenna-check "Antenna check command", "Gate nets visited once",
// "Violation reporting"; design D17): the plane-order requirement, EFArgs
// with antennacheck's (empty) flag set, EFReadFile and EFFlatBuild with
// EF_FLATNODES | EF_WARNABSTRACT, the device-type table of the style, one
// evaluation per gate net and check plane, the feedback and the progress
// lines.
//
// Beyond Magic, in both magic-compat settings (crashes and undefined
// behaviour, the standing rule): a cell named with no layout window fails
// with "Point to a window or specify a cell name." (Magic dereferences the
// missing window); a device whose model names no tile type of the style,
// or that has no terminal, is skipped (Magic indexes with -1 or reads past
// the terminal list).
//
// Ported from Magic 8.3.694 (fc5daa9e) extflat/EFantenna.c:126-765
// (CmdAntennaCheck, antennacheckArgs, antennacheckVisit, areaMarkFunc),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: ANTENNA.

#include <cstdio>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "antenna/console.hpp"
#include "hefesto/antenna/antenna.hpp"
#include "hefesto/antenna/measure.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::antenna {

namespace {

using cmd::CommandResult;

// antennacheckArgs: antennacheck adds no flag of its own.
extflat::ExtraFlags antenna_flags(const Console& console) {
  return [console](std::span<const std::string> args, std::size_t flag) -> std::optional<std::size_t> {
    console.message("Unrecognized flag: " + args[flag]);
    console.message("Usage: antennacheck");
    return std::nullopt;
  };
}

// EFReadFile(inName, FALSE, FALSE, FALSE, FALSE): no subcircuits, no
// `.res.ext` overlays, scaled.
extflat::ReadOptions read_options(const editor::EditorState& editor, const extflat::CommonArgs& args,
                                  bool magic_compat) {
  extflat::ReadOptions options;
  options.subcircuits = false;
  options.resist_overlay = false;
  options.noscale = false;
  options.magic_compat = magic_compat;
  options.search_path = args.search_path;
  if (editor.technology) {
    options.tile_type_count = static_cast<std::int32_t>(editor.technology->type_count());
    options.tech_name = editor.technology->name();
  }
  if (editor.extract.current) {
    options.units_per_lambda = editor.extract.current->units_per_lambda;
    options.device_tile_type = [style = editor.extract.current](std::string_view name) -> std::optional<std::int32_t> {
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
  options.cell_directory = [&editor](std::string_view name) -> std::optional<std::string> {
    const db::CellDef* def = editor.cells.find(name);
    if (def == nullptr || def->file_path().empty()) {
      return std::nullopt;
    }
    return def->file_path().parent_path().string();
  };
  return options;
}

void forward(const Console& console, diag::CollectingSink& sink) {
  for (diag::Diagnostic& d : sink.take()) {
    console.forward(std::move(d));
  }
}

// EFReadFile then EFFlatBuild(EF_FLATNODES | EF_WARNABSTRACT).
diag::Outcome<extflat::Netlist> read_netlist(editor::EditorState& editor, platform::FileSystem& fs,
                                             const std::string& cell, const extflat::ReadOptions& options,
                                             const Console& console) {
  diag::CollectingSink sink;
  cell::LookupContext lookup{fs, editor.search_paths, editor.path_variables, sink};
  console.report("Reading extract file.");
  auto hierarchy = extflat::read(lookup, cell, options);
  forward(console, sink);
  if (!hierarchy.ok()) {
    return hierarchy.error();
  }
  console.report("Building flattened netlist.");
  auto netlist = extflat::flatten(hierarchy.value(), extflat::FlatFlags::flat_nodes | extflat::FlatFlags::warn_abstract,
                                  extflat::FlattenOptions{}, sink);
  forward(console, sink);
  return netlist;
}

// The cifoutput cut rule of a contact (CIFGetContactSize on CIFCurStyle).
CutLookup cut_lookup(const editor::EditorState& editor) {
  std::shared_ptr<const mask::OutputStyle> style = editor.mask.output_styles.current_shared();
  return [style](tech::TileType type) -> std::optional<CutRule> {
    if (!style) {
      return std::nullopt;
    }
    const auto cut = style->contact_cut(type);
    if (!cut) {
      return std::nullopt;
    }
    return CutRule{cut->edge, cut->spacing, cut->border, style->scale.scale_factor};
  };
}

// The MOSFET-like classes whose gate nets are checked.
bool gate_class(std::string_view device_class) {
  return device_class == "fet" || device_class == "mosfet" || device_class == "asymmetric" ||
         device_class == "msubckt";
}

// extGetDevType through ExtGetDevInfo's table.
std::optional<tech::TileType> device_type(const extract::ExtStyle& style, std::string_view model) {
  for (const extract::DeviceInfo& info : style.device_info()) {
    if (info.model == model) {
      return info.type;
    }
  }
  return std::nullopt;
}

// The net as one search adds it.
class Collector : public connect::NetVisitor {
 public:
  explicit Collector(TracedNet& net) : net_(net) {}
  bool on_tile(const connect::NetTile& tile) override {
    status = net_.add(tile);
    return status.ok();
  }
  diag::Status status = diag::Done{};

 private:
  TracedNet& net_;
};

// The %g of C's printf.
std::string printf_g(double value) {
  char buffer[64];
  (void)std::snprintf(buffer, sizeof buffer, "%g", value);
  return buffer;
}

std::string rect_text(const db::Rect& r) {
  return "(" + std::to_string(r.x0) + " " + std::to_string(r.y0) + ") to (" + std::to_string(r.x1) + " " +
         std::to_string(r.y1) + ")";
}

// One device's gate net across its check planes.
struct GateContext {
  const extflat::FlatDevice* device = nullptr;
  tech::TileType type = 0;
  tech::PlaneId gate_plane = 0;
};

// The measurement of one check plane.
struct PlaneResult {
  AreaSum gate;
  AntennaAreas areas;
  Evaluation evaluation;
};

class GateChecker {
 public:
  GateChecker(editor::EditorState& editor, db::CellDef& root, const MeasureSetup& setup, const Console& console,
              bool debug)
      : editor_(editor), root_(root), setup_(setup), console_(console), debug_(debug) {}

  [[nodiscard]] diag::Status visit(const extflat::FlatDevice& device);

 private:
  [[nodiscard]] bool first_visit(const extflat::FlatTerminal& gate);
  [[nodiscard]] diag::Status check_plane(const GateContext& gate, const CheckPlane& check);
  void debug_report(const GateContext& gate, const CheckPlane& check, const PlaneResult& r, bool via) const;
  void mark(const GateContext& gate, const CheckPlane& check, const TracedNet& net);

  editor::EditorState& editor_;
  db::CellDef& root_;
  const MeasureSetup& setup_;
  const Console& console_;
  bool debug_;
  std::set<std::size_t> visited_nodes_;
  std::set<std::string> visited_names_;
  std::size_t gates_ = 0;
};

bool GateChecker::first_visit(const extflat::FlatTerminal& gate) {
  if (gate.node_index != extflat::kNoNode) {
    return visited_nodes_.insert(gate.node_index).second;
  }
  return visited_names_.insert(gate.node).second;
}

diag::Status GateChecker::visit(const extflat::FlatDevice& device) {
  if (!gate_class(device.device_class) || device.terminals.empty() || !editor_.extract.current) {
    return diag::Done{};
  }
  if (!first_visit(device.terminals.front())) {
    return diag::Done{};
  }
  if (++gates_ % 100 == 0) {
    console_.report("   " + std::to_string(gates_) + " gates analyzed.");
  }
  const std::optional<tech::TileType> type = device_type(*editor_.extract.current, device.model);
  const std::optional<tech::PlaneId> plane = type ? setup_.tech->home_plane(*type) : std::nullopt;
  if (!plane) {
    return diag::Done{};  // no tile type for the model (Magic indexes with -1)
  }
  const GateContext gate{&device, *type, *plane};
  for (const CheckPlane& check : check_planes(*setup_.rules, *setup_.tech, *plane)) {
    if (diag::Status st = check_plane(gate, check); !st.ok()) {
      return st;
    }
  }
  return diag::Done{};
}

diag::Status GateChecker::check_plane(const GateContext& gate, const CheckPlane& check) {
  TracedNet net(*setup_.tech);
  Collector collect(net);
  connect::SearchOptions options;
  options.magic_compat = setup_.magic_compat;
  options.connect = restricted_connect(*setup_.rules, *setup_.tech, check, setup_.magic_compat);
  const connect::SearchStart start{gate.device->area, setup_.tech->connects_to(gate.type)};
  if (diag::Status st = connect::search_hierarchical(root_, *setup_.tech, start, options, collect); !st.ok()) {
    return st;
  }
  if (!collect.status.ok()) {
    return collect.status;
  }
  PlaneResult r;
  const AreaSum diffusion = tiedown_area(net, setup_);
  r.gate = gate_area(net, setup_, gate.gate_plane);
  r.areas = antenna_areas(net, setup_, check);
  r.evaluation = evaluate(setup_, r.areas.area, r.gate.area, diffusion.area, check);
  if (!r.evaluation.violation()) {
    return diag::Done{};
  }
  if (debug_) {
    if (r.evaluation.metal_violation) {
      debug_report(gate, check, r, false);
    }
    if (r.evaluation.via_violation) {
      debug_report(gate, check, r, true);
    }
  }
  mark(gate, check, net);
  return diag::Done{};
}

// The cell of the device: its instance id, or the definition's name at the
// root (hc_use->use_id, else use_def->def_name).
std::string device_cell(const extflat::FlatDevice& device) {
  const std::string& path = device.hier_prefix;
  if (path.empty()) {
    return device.def;
  }
  std::string id = path;
  while (!id.empty() && id.back() == '/') {
    id.pop_back();
  }
  const std::size_t slash = id.rfind('/');
  return slash == std::string::npos ? id : id.substr(slash + 1);
}

void GateChecker::debug_report(const GateContext& gate, const CheckPlane& check, const PlaneResult& r,
                               bool via) const {
  const Evaluation& e = r.evaluation;
  const std::string plane{setup_.tech->plane_name(check.plane)};
  console_.message("Cell: " + device_cell(*gate.device));
  console_.message("Antenna violation detected at plane " + plane + (via ? " contact" : ""));
  const float ratio = effective_ratio(e.limit, via ? e.via : e.metal, r.gate.area);
  console_.message("Effective antenna ratio " + printf_g(static_cast<double>(ratio)) + " > limit " + printf_g(static_cast<double>(e.limit)));
  console_.message("Gate rect " + rect_text(r.gate.last));
  console_.message("Antenna rect " + rect_text(r.areas.last));
}

// areaMarkFunc: the gate tiles, then every non-space tile of the planes
// searched, each a feedback area in the root cell.
void GateChecker::mark(const GateContext& gate, const CheckPlane& check, const TracedNet& net) {
  const std::string text = "Antenna error at plane " + std::string{setup_.tech->plane_name(check.plane)} + "\n";
  const auto add = [&](const db::TileView& t) {
    (void)editor_.feedback.add(editor_.windows, root_, t.area, 1, view::producer_style::kAntenna, text, &editor_.highlights);
    return true;
  };
  (void)net.plane(gate.gate_plane).enumerate(db::whole_plane(), setup_.rules->gate_types(), add);
  tech::TypeMask all;
  for (std::size_t t = tech::kFirstUserType; t < setup_.tech->type_count(); ++t) {
    all.set(static_cast<tech::TileType>(t));
  }
  for (std::size_t p = tech::kFirstUserPlane; p < net.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    if (plane_included(*setup_.rules, plane, check)) {
      (void)net.plane(plane).enumerate(db::whole_plane(), all, add);
    }
  }
}

// The run after the plane-order check and the root (a window's cell).
CommandResult run_check(editor::EditorState& editor, db::CellDef& root, const extflat::CommonArgs& args,
                        const AntennaOptions& options, const Console& console) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
  const std::string cell = args.cell.value_or(root.name());
  auto netlist = read_netlist(editor, fs, cell, read_options(editor, args, options.magic_compat), console);
  if (!netlist.ok()) {
    return CommandResult::failure(netlist.error());
  }
  const RuleView rules = RuleView::from_style(*editor.extract.current);
  const MeasureSetup setup{editor.technology.get(), &rules, cut_lookup(editor), options.magic_compat};
  GateChecker checker(editor, root, setup, console, options.debug);
  console.report("Running antenna checks.");
  for (const extflat::FlatDevice& device : netlist.value().devices) {
    if (diag::Status st = checker.visit(device); !st.ok()) {
      return CommandResult::failure(st.error());
    }
  }
  console.report("antennacheck finished.");
  if (options.finished) {
    options.finished();
  }
  return CommandResult::success();
}

}  // namespace

CommandResult check(editor::EditorState& editor, db::CellDef* root, const AntennaOptions& options) {
  const Console console{options.emit};
  if (!editor.technology || !editor.extract.current || !editor.extract.current->plane_ordering) {
    console.message("No planeorder specified for this process:  Cannot run antenna checks!");
    return CommandResult::success();
  }
  platform::LocalFileSystem local;
  platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
  auto args = extflat::parse_common_args(options.args, antenna_flags(console),
                                         extflat::ArgOptions{false, false, extflat::file_reader(fs)});
  if (!args.ok()) {
    return CommandResult::failure(args.error());
  }
  for (const diag::Diagnostic& d : args.value().messages) {
    console.forward(d);
  }
  if (root == nullptr) {
    // Magic dereferences the missing window when a cell is named; both modes stop here.
    console.message("Point to a window or specify a cell name.");
    return CommandResult::success();
  }
  return run_check(editor, *root, args.value(), options, console);
}

}  // namespace hefesto::antenna
