// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract_cell (extract/hier/extract_cell.hpp; capability extract-hierarchy
// "Bottom-up extraction order", "Hierarchical section order in the file";
// openspec/changes/m7-extraction tasks.md 8.1): extCellFile and extBasic,
// in Magic's order, over the R3 passes of EXTNODE, EXTCAP and EXTDEV:
//   1. the temporary unique pass and the isolated substrate plane (unless
//      the driver's prepare phase did both, ExtractContext::prepared);
//   2. the header and the `use` records (extHeader, extOutputUsesFunc);
//   3. the device regions, then the node pass seeded with the substrate
//      tiles (the slots kept for the later passes), the labels and the
//      duplicate-label feedback;
//   4. substrate capacitance, coupling (with option `coupling`) and its
//      relocation to substrate capacitance (a style with a substrate plane);
//   5. the `device` cell property, `parameters`, `abstract`, the node
//      records with their final R and C, the coupling `cap` records (no
//      `device` property) and the device records;
//   6. EXTHIER's subtree and array passes (extract_subtree);
//   7. the slots reset (ExtResetTiles), then path length for the root.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtCell.c (extCellFile,
// extHeader, extOutputUsesFunc) and extract/ExtBasic.c (extBasic),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTCMD (tasks.md 8.1, the single-cell and bottom-up part).

#include "hefesto/extract/hier/extract_cell.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/plane.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/extract/device/devices.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/hier/subtree.hpp"
#include "hefesto/extract/length/length.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/node/unique.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract {

namespace {

constexpr std::string_view kCodeInputs = "EXTRACT-CMD-001";

// extDevTable, in DeviceClass order.
constexpr std::array<std::string_view, 17> kDeviceClasses = {
    "fet",     "mosfet",  "asymmetric", "bjt",     "devres",  "devcap",  "devcaprev", "vsource",  "diode",
    "pdiode",  "ndiode",  "subckt",     "rsubckt", "msubckt", "csubckt", "dsubckt",   "veriloga"};

bool cancelled(const ExtractContext& ctx) {
  return ctx.progress != nullptr && ctx.progress->cancelled();
}

void report(const ExtractContext& ctx, const diag::Diagnostic& d) {
  if (ctx.sink != nullptr) {
    ctx.sink->emit(d);
  }
}

bool has_property(const db::CellDef& cell, std::string_view name) {
  return cell.properties().find(name) != nullptr;
}

// extHeader's records before the uses.
ExtHeader header_of(const db::CellDef& cell, const tech::Technology& technology, const ExtStyle& style) {
  ExtHeader h;
  h.timestamp = cell.timestamp();
  h.tech = technology.name();
  h.style = style.name;
  h.rscale = static_cast<std::int32_t>(style.rscale);
  h.cscale = static_cast<std::int32_t>(style.cscale);
  h.units_per_lambda = style.units_per_lambda;
  h.resist_classes = resistance_class_header(style);
  h.cell_name = cell.name();
  if (const db::PropertyValue* value = cell.properties().find("parameter")) {
    h.cell_parameter = db::property_text(*value);
  }
  return h;
}

// extOutputUsesFunc for every use in DBCellEnum order, except the uses of
// "don't use" cells.
void write_uses(ExtWriter& writer, const db::CellDef& cell, const ExtractContext& ctx) {
  for (const db::CellUse* use : db::uses_in_search_order(cell, ctx.compat)) {
    if (use->def == nullptr || ctx.dont_use.contains(use->def)) {
      continue;
    }
    std::string id = use->id;
    if (use->array.is_array()) {
      id += "[" + std::to_string(use->array.xlo) + ":" + std::to_string(use->array.xhi) + ":" +
            std::to_string(use->array.xsep) + "][" + std::to_string(use->array.ylo) + ":" +
            std::to_string(use->array.yhi) + ":" + std::to_string(use->array.ysep) + "]";
    }
    const db::Transform& t = use->transform;
    const std::array<std::int32_t, 6> values{t.a, t.b, static_cast<std::int32_t>(t.c),
                                             t.d, t.e, static_cast<std::int32_t>(t.f)};
    writer.use(use->def->name(), id, values);
  }
}

NodePassOptions node_pass_options(const ExtractContext& ctx, const db::CellDef& cell) {
  NodePassOptions o;
  o.resistance = ctx.options.lumped;
  o.magic_compat = ctx.compat.magic_compat;
  const SubstrateSeeds seeds = substrate_seeds(cell, *ctx.technology, *ctx.style, ctx.options.extresist);
  for (const auto& [plane, tile] : seeds.tiles) {
    o.substrate_seeds.push_back(RegionSeed{plane, db::Point{tile.area.x0, tile.area.y0}, tile.split && tile.right_side});
  }
  if (seeds.tileless) {
    o.substrate_without_tiles = ctx.style->substrate.plane;
  }
  return o;
}

// The cell-scoped changes extCellFile makes before the passes and undoes
// after the run: the temporary unique labels and the isolated substrate
// plane (when the driver has not made them).
class CellPrep {
 public:
  CellPrep(const ExtractContext& ctx, db::CellDef& cell, bool is_top) : cell_(cell) {
    if (ctx.prepared) {
      return;
    }
    if (ctx.options.unique) {
      UniqueContext uc;
      uc.technology = ctx.technology;
      uc.style = ctx.style.get();
      uc.temporary = true;
      uc.compat = ctx.compat;
      uc.sink = ctx.sink;
      uc.substrate = node_pass_options(ctx, cell);
      const UniqueMode mode = ctx.options.unique_notopports && is_top ? UniqueMode::noports : UniqueMode::all;
      (void)renamer_.apply(cell, mode, uc);
      renamed_ = true;
    }
    if (std::optional<db::Plane> plane = isolated_substrate_plane(cell, *ctx.technology, *ctx.style)) {
      plane_ = *ctx.style->substrate.plane;
      saved_ = std::move(cell.plane(*plane_));
      cell.plane(*plane_) = std::move(*plane);
    }
  }
  ~CellPrep() {
    if (plane_ && saved_) {
      cell_.plane(*plane_) = std::move(*saved_);
    }
    if (renamed_) {
      renamer_.revert();
    }
  }
  CellPrep(const CellPrep&) = delete;
  CellPrep& operator=(const CellPrep&) = delete;

 private:
  db::CellDef& cell_;
  UniqueRenamer renamer_;
  bool renamed_ = false;
  std::optional<tech::PlaneId> plane_;
  std::optional<db::Plane> saved_;
};

// The state of extBasic between its passes.
struct Basic {
  RegionFinder finder;
  std::vector<DeviceRegionTiles> regions;
  NodeRegionList nodes;
  CouplingTable coupling;
  std::optional<std::size_t> substrate;  // glob_subsnode (none for an abstract view)
  SubstrateName substrate_name;
  std::vector<std::string> names;  // extNodeName per list index
  bool abstract = false;
  DeviceProperty property;
};

// extSubsName: the declared name of a node at -infinity, else none.
std::optional<std::string> declared_name(const ExtractContext& ctx, const NodeRegion& node) {
  const std::string& declared = ctx.style->substrate.node_name;
  if (declared.empty() || node.reference.x > kTilelessSubstrateCoord) {
    return std::nullopt;
  }
  return substrate_name(declared, ctx.variable);
}

// The substrate node's name (extMakeNodeNumPrint with extSubsName). An
// abstract view has no substrate node designation (glob_subsnode is NULL),
// but extMakeNodeNumPrint still names its node at -infinity with
// extSubsName: that node is named without the `substrate` record.
SubstrateName substrate_naming(const ExtractContext& ctx, const Basic& b, std::optional<std::size_t> abstract_node) {
  SubstrateName out;
  if (b.substrate) {
    const NodeRegion& node = b.nodes[*b.substrate];
    out.node = b.substrate;
    out.name = declared_name(ctx, node).value_or(generated_name(node, *ctx.technology));
  } else if (abstract_node) {
    if (std::optional<std::string> name = declared_name(ctx, b.nodes[*abstract_node])) {
      out.node = abstract_node;
      out.name = std::move(*name);
      out.record = false;
    }
  }
  return out;
}

// extFindNodes, the label pass and the duplicate-label check.
diag::Status find_nodes(ExtractContext& ctx, const db::CellDef& cell, Basic& b, CellResult& result) {
  b.finder = RegionFinder(*ctx.technology);
  b.finder.keep_clients = true;
  auto found = b.finder.find_nodes(cell, *ctx.style, node_pass_options(ctx, cell));
  if (!found.ok()) {
    return found.error();
  }
  b.nodes = std::move(found).value();
  b.abstract = has_property(cell, "LEFview");
  attach_labels(cell, b.finder, b.nodes, *ctx.style, *ctx.technology, LabelPassOptions{}, ctx.compat);
  const std::optional<std::size_t> d = b.finder.substrate_region();
  std::optional<std::size_t> abstract_node;
  if (d) {
    (b.abstract ? abstract_node : b.substrate) = node_index(b.nodes.size(), *d - b.finder.first_region());
  }
  b.substrate_name = substrate_naming(ctx, b, abstract_node);
  if (ctx.options.warn_dup && !b.abstract) {
    for (const NodeFeedback& f : duplicate_label_feedback(cell, b.nodes, *ctx.technology, b.substrate_name)) {
      result.feedback.push_back(CellFeedback{f.area, f.text, f.medium});
      ++result.warnings;
    }
  }
  return diag::Done{};
}

CapInputs cap_inputs(const ExtractContext& ctx, const db::CellDef& cell, const Basic& b) {
  CapInputs in;
  in.cell = &cell;
  in.style = ctx.style.get();
  in.compat = &ctx.compat;
  in.technology = ctx.technology;
  in.options = &ctx.options;
  in.node_of = region_node_lookup(b.finder, cell, b.nodes.size());
  return in;
}

// Substrate capacitance, then extFindCoupling and the relocation.
diag::Status find_caps(const ExtractContext& ctx, const db::CellDef& cell, Basic& b) {
  const CapInputs in = cap_inputs(ctx, cell, b);
  if (diag::Status s = substrate_caps(b.nodes, in); !s.ok()) {
    return s;
  }
  if (!ctx.options.coupling) {
    return diag::Done{};
  }
  if (diag::Status s = find_coupling(b.nodes, in, b.coupling); !s.ok()) {
    return s;
  }
  if (ctx.style->substrate.plane) {
    relocate_substrate_coupling(b.nodes, b.substrate, b.coupling);
  }
  return diag::Done{};
}

// `parameters <model> ...` lines of extOutputParameters.
void write_parameters(ExtWriter& writer, const std::vector<std::string>& lines) {
  for (const std::string& line : lines) {
    std::string_view rest{line};
    rest.remove_prefix(std::min(rest.size(), std::string_view{"parameters "}.size()));
    const std::size_t space = rest.find(' ');
    std::vector<std::string> params;
    std::string_view tail = space == std::string_view::npos ? std::string_view{} : rest.substr(space + 1);
    while (!tail.empty()) {
      const std::size_t next = tail.find(' ');
      params.emplace_back(tail.substr(0, next));
      tail = next == std::string_view::npos ? std::string_view{} : tail.substr(next + 1);
    }
    writer.parameters(rest.substr(0, space), params);
  }
}

// The property's `parameters :<cell>` record and the parameter records.
void write_cell_parameters(ExtWriter& writer, const db::CellDef& cell, const ExtStyle& style, const Basic& b) {
  if (b.property.primitive && !b.property.parameters_line.empty()) {
    const std::string prefix = "parameters :" + cell.name() + " ";
    writer.cell_parameters(cell.name(), std::string_view{b.property.parameters_line}.substr(prefix.size()));
  }
  write_parameters(writer, parameter_records(style, b.regions, cell));
}

// The node name extNodeName gives list entry i.
std::string record_name(const ExtractContext& ctx, const Basic& b, std::size_t i) {
  if (const LabelRegion* label = name_label(b.nodes[i])) {
    return label->text;
  }
  return number_name(b.nodes, i, *ctx.technology, b.substrate_name);
}

// extOutputNodes with the final R and C of every node.
void write_nodes(ExtractContext& ctx, ExtWriter& writer, const db::CellDef& cell, Basic& b) {
  NodeRecordOptions o;
  o.cscale = ctx.style->cscale;
  o.substrate = b.substrate_name;
  o.aliases = ctx.options.aliases;
  o.resistance_on = ctx.options.lumped;
  o.extresist = ctx.options.extresist;
  for (const NodeRegion& node : b.nodes) {
    const bool parasitics = !b.property.found;
    const std::int64_t r = ctx.options.lumped && parasitics
                               ? resistance_output(lumped_resistance(node, *ctx.style), ctx.style->rscale)
                               : 0;
    o.resistance.push_back(r);
    o.capacitance.push_back(parasitics ? node_cap_output(node.substrate_cap, ctx.style->cscale) : 0.0);
  }
  diag::NullSink quiet;
  diag::DiagnosticSink& sink = ctx.sink != nullptr ? *ctx.sink : quiet;
  (void)write_node_records(writer, b.nodes, cell, *ctx.technology, o, ctx.compat, sink);
  for (std::size_t i = 0; i < b.nodes.size(); ++i) {
    b.names.push_back(record_name(ctx, b, i));
  }
}

std::string node_text(const Basic& b, const std::optional<std::size_t>& node) {
  return node && *node < b.names.size() ? b.names[*node] : std::string{"(none)"};
}

// One terminal of extOutputDevices: `"<node>" <length> <attrs|0>[,a,p]`.
std::string terminal_text(const Basic& b, const DeviceTerminal& t, bool gate) {
  std::string s = " \"" + node_text(b, t.node) + "\" " + std::to_string(t.length);
  char sep = ' ';
  for (const std::string& a : t.attributes) {
    s += sep;
    s += "\"" + a + "\"";
    sep = ',';
  }
  if (!gate && t.area != 0 && t.perimeter != 0) {
    s += sep + std::to_string(t.area) + "," + std::to_string(t.perimeter);
  } else if (sep == ' ') {
    s += " 0";
  }
  return s;
}

// A DeviceRecord as Magic's `device` (or `fet`) line.
std::string device_text(const ExtStyle& style, const Basic& b, const DeviceRecord& d) {
  const DevicePattern& p = style.devices.at(d.pattern);
  const bool fet = p.device_class == DeviceClass::fet;
  std::string s = fet ? "fet " : "device " + std::string{kDeviceClasses.at(static_cast<std::size_t>(p.device_class))} + " ";
  s += p.model + " " + std::to_string(d.area.x0) + " " + std::to_string(d.area.y0) + " " + std::to_string(d.area.x1) +
       " " + std::to_string(d.area.y1);
  for (const std::string& f : d.fields) {
    s += " " + f;
  }
  for (const std::string& f : d.params) {
    s += " " + f;
  }
  if (d.substrate_field == SubstrateField::node) {
    s += " \"" + node_text(b, d.substrate) + "\"";
  } else if (d.substrate_field == SubstrateField::name) {
    s += " \"" + d.substrate_name + "\"";
  }
  s += terminal_text(b, d.gate, true);
  for (const DeviceTerminal& t : d.terminals) {
    s += terminal_text(b, t, false);
  }
  return s;
}

// extOutputDevices.
void write_devices(ExtractContext& ctx, ExtWriter& writer, const db::CellDef& cell, Basic& b, CellResult& result) {
  DeviceInputs in;
  in.cell = &cell;
  in.technology = ctx.technology;
  in.style = ctx.style.get();
  in.nodes = &b.nodes;
  in.substrate_node = b.substrate;
  in.compat = ctx.compat;
  in.warn_fets = ctx.options.warn_fets;
  in.warn_any = ctx.options.warn_dup || ctx.options.warn_fets || ctx.options.warn_labels;
  in.node_of = region_slot_lookup(b.finder, cell, b.nodes.size());
  in.variable = ctx.variable;
  DeviceResult found = find_devices(in, b.regions);
  for (const diag::Diagnostic& m : found.messages) {
    report(ctx, m);
  }
  for (const DeviceFeedback& f : found.feedback) {
    result.feedback.push_back(CellFeedback{f.area, f.text, false});
  }
  result.warnings += found.warnings;
  for (const DeviceRecord& d : found.devices) {
    writer.device_line(device_text(*ctx.style, b, d));
  }
}

// The device section: the `device` property's line, else the devices.
void write_device_section(ExtractContext& ctx, ExtWriter& writer, const db::CellDef& cell, Basic& b,
                          CellResult& result) {
  if (b.property.primitive) {
    writer.device_line("primitive");
    return;
  }
  if (b.property.found && !b.property.device_line.empty()) {
    writer.device_line(b.property.device_line);
    if (ctx.compat.magic_compat) {
      result.unterminated = b.property.device_line;  // Magic prints no newline after it
    }
    return;
  }
  write_devices(ctx, writer, cell, b, result);
}

void write_coupling(const ExtractContext& ctx, ExtWriter& writer, const Basic& b) {
  if (!ctx.options.coupling || b.property.found) {
    return;
  }
  for (const CouplingRecord& r : coupling_records(b.coupling, b.names, ctx.style->cscale)) {
    writer.cap(r.first, r.second, r.value);
  }
}

// The `device` property: messages, and no parasitics on the local nodes.
void read_device_property(ExtractContext& ctx, const db::CellDef& cell, Basic& b) {
  b.property = device_property(cell, *ctx.style);
  for (const diag::Diagnostic& m : b.property.messages) {
    report(ctx, m);
  }
}

// extBasic after the node pass.
void write_basic(ExtractContext& ctx, ExtWriter& writer, const db::CellDef& cell, Basic& b, CellResult& result) {
  read_device_property(ctx, cell, b);
  write_cell_parameters(writer, cell, *ctx.style, b);
  if (b.abstract) {
    writer.abstract();
  }
  write_nodes(ctx, writer, cell, b);
  write_coupling(ctx, writer, b);
  write_device_section(ctx, writer, cell, b, result);
}

// extSubtree and extArray (EXTHIER: pass 1 and pass 2, hierarchical
// coupling, merges), while the node slots of the flat pass are still kept.
diag::Status write_subtree(ExtractContext& ctx, ExtWriter& writer, db::CellDef& cell, const Basic& b,
                           CellResult& result) {
  SubtreeInputs in;
  in.technology = ctx.technology;
  in.parent = ParentNodes{&b.finder, b.nodes.size(), b.names, b.substrate};
  if (!ctx.style->substrate.node_name.empty()) {
    in.substrate_name = substrate_name(ctx.style->substrate.node_name, ctx.variable);
  }
  in.dont_use = [&ctx](const db::CellDef& def) { return ctx.dont_use.contains(&def); };
  auto hier = extract_subtree(ctx, cell, writer, in);
  if (!hier.ok()) {
    return hier.error();
  }
  result.errors += hier.value().errors;
  result.warnings += hier.value().warnings;
  result.cancelled = result.cancelled || hier.value().cancelled;
  for (const NodeFeedback& f : hier.value().feedback) {
    result.feedback.push_back(CellFeedback{f.area, f.text, f.medium});
  }
  return diag::Done{};
}

// extBasic: false when the pass was cancelled.
diag::Outcome<bool> extract_basic(ExtractContext& ctx, db::CellDef& cell, ExtWriter& writer, CellResult& result) {
  Basic b;
  b.regions = find_device_regions(cell, *ctx.technology, *ctx.style);
  disable_subcell_devices(cell, b.regions, *ctx.technology, *ctx.style);
  if (cancelled(ctx)) {
    return false;
  }
  if (diag::Status s = find_nodes(ctx, cell, b, result); !s.ok()) {
    return s.error();
  }
  if (cancelled(ctx)) {
    return false;
  }
  if (diag::Status s = find_caps(ctx, cell, b); !s.ok()) {
    return s.error();
  }
  if (cancelled(ctx)) {
    return false;
  }
  write_basic(ctx, writer, cell, b, result);
  if (diag::Status s = write_subtree(ctx, writer, cell, b, result); !s.ok()) {
    return s.error();
  }
  return true;
}

}  // namespace

void unterminate(std::string& text, std::string_view line) {
  const std::string whole = std::string{line} + "\n";
  for (std::size_t at = text.find(whole); at != std::string::npos; at = text.find(whole, at + 1)) {
    if (at == 0 || text[at - 1] == '\n') {
      text.erase(at + line.size(), 1);
      return;
    }
  }
}

diag::Outcome<CellResult> extract_cell(ExtractContext& ctx, db::CellDef& cell, ExtWriter& writer, bool is_top) {
  if (ctx.style == nullptr || ctx.technology == nullptr) {
    return diag::make(kCodeInputs, "Cannot extract " + cell.name() + ": no extraction style or technology.");
  }
  CellResult result;
  const CellPrep prep(ctx, cell, is_top);
  writer.header(header_of(cell, *ctx.technology, *ctx.style));
  write_uses(writer, cell, ctx);
  auto basic = extract_basic(ctx, cell, writer, result);
  reset(cell);
  if (!basic.ok()) {
    return basic.error();
  }
  result.cancelled = !basic.value() || cancelled(ctx);
  if (!result.cancelled && is_top && ctx.options.length) {
    if (diag::Status s = path_lengths(ctx, cell, ctx.length, writer); !s.ok()) {
      return s.error();
    }
  }
  return result;
}

}  // namespace hefesto::extract
