// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// read(), flatten(), flatten_one_level() and read_res_overlay() (see
// hefesto/extflat/hierarchy.hpp): the read keeps its circuit model in the
// Hierarchy, and each flattening works on a copy-free const view of it.
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13).

#include "hefesto/extflat/hierarchy.hpp"

#include <memory>
#include <unordered_set>
#include <utility>

#include "flatten.hpp"
#include "ports.hpp"
#include "reader.hpp"
#include "state.hpp"
#include "text.hpp"

namespace hefesto::extflat {

namespace detail {

namespace {

// Forwards to the read's sink while the read runs, and drops what comes
// after (the model outlives the caller's sink).
class ForwardingSink final : public diag::DiagnosticSink {
 public:
  void emit(const diag::Diagnostic& diagnostic) override {
    if (target_ != nullptr) {
      target_->emit(diagnostic);
    }
  }
  void attach(diag::DiagnosticSink* target) noexcept { target_ = target; }

 private:
  diag::DiagnosticSink* target_ = nullptr;
};

}  // namespace

struct HierModel {
  explicit HierModel(const ReadOptions& o) : options(o), st(std::make_unique<ReadState>(sink, options)) {}

  ForwardingSink sink;
  ReadOptions options;
  std::unique_ptr<ReadState> st;
  std::uint32_t root = 0;
  std::vector<char> nodevices;  // DEF_NODEVICES, kept across one-level flattenings
};

namespace {

HierDef hier_def(const Def& def) {
  std::size_t uses = 0;  // HashGetNumEntries(&def->def_uses)
  def.use_table.for_each([&uses](const auto& entry) {
    if (entry.value) {
      ++uses;
    }
  });
  return HierDef{def.name, def.subcircuit, def.abstract, def.primitive, def.devices.size(), uses};
}

HierUse hier_use(const DefTable& defs, const Def& parent, const Use& use) {
  HierUse out{parent.name, defs.at(use.def).name, use.id};
  out.transform = {use.trans.a, use.trans.b, use.trans.c, use.trans.d, use.trans.e, use.trans.f};
  out.x_range = {use.xlo, use.xhi, use.xsep};
  out.y_range = {use.ylo, use.yhi, use.ysep};
  return out;
}

// EFHierSrDefs from the root: each def once, after the defs it uses.
class DefWalk {
 public:
  DefWalk(const DefTable& defs, std::size_t max_depth, Hierarchy& out)
      : defs_(defs), max_depth_(max_depth), out_(out), seen_(defs.size(), 0) {}

  void walk(std::uint32_t id, std::size_t depth) {
    seen_[id] = 1;
    const Def& def = defs_.at(id);
    def.use_table.for_each([&](const auto& entry) {
      if (!entry.value) {
        return;
      }
      const Use& use = def.uses[*entry.value];
      out_.uses.push_back(hier_use(defs_, def, use));
      if (seen_[use.def] == 0 && depth < max_depth_) {
        walk(use.def, depth + 1);
      }
    });
    out_.defs.push_back(hier_def(def));
  }

 private:
  const DefTable& defs_;
  std::size_t max_depth_;
  Hierarchy& out_;
  std::vector<char> seen_;
};

void copy_read_globals(const ReadState& st, Netlist& n) {
  for (const auto& [type, list] : st.params) {
    auto& out = n.device_params[type];
    for (const DevParam& p : list) {
      out.push_back(DeviceParam{p.type, p.name, p.scale, p.offset});
    }
  }
  n.scale = st.ef_scale == 0.0F ? 1.0F : st.ef_scale;
  n.scale_changed = st.scale_changed;
  n.tech = st.tech;
  n.style = st.style;
  n.substrate_compat = st.compat;
  n.resist_classes = st.resists;
  n.resist_changed = st.resist_changed;
  n.selected_style = st.selected_style;
}

FlatPlan plan_for(const ReadState& st, FlatFlags flags, const FlattenOptions& options) {
  FlatPlan plan;
  plan.flags = flags;
  plan.options = options;
  plan.substrate_compat = st.compat;
  plan.resists = st.resists;
  plan.scale_changed = st.scale_changed;
  return plan;
}

}  // namespace

}  // namespace detail

diag::Outcome<Hierarchy> read(cell::LookupContext& ctx, std::string_view root, const ReadOptions& options) {
  auto model = std::make_shared<detail::HierModel>(options);
  model->sink.attach(&ctx.sink);
  const auto root_def = detail::read_tree(*model->st, ctx, root);
  model->sink.attach(nullptr);
  if (!root_def.ok()) {
    return root_def.error();
  }
  model->root = root_def.value();
  Hierarchy h;
  h.root = std::string{root};
  detail::DefWalk{model->st->defs, options.limits.max_depth, h}.walk(model->root, 0);
  h.model = std::move(model);
  return h;
}

diag::Outcome<Netlist> flatten(const Hierarchy& hierarchy, FlatFlags flags, const FlattenOptions& options,
                               diag::DiagnosticSink& sink) {
  if (!hierarchy.model) {
    return Netlist{};
  }
  const detail::ReadState& st = *hierarchy.model->st;
  detail::FlatContext ctx{sink, hierarchy.model->options, st.globals, st.defs, st.layers, std::nullopt};
  Netlist netlist = detail::flatten_full(ctx, hierarchy.model->root, detail::plan_for(st, flags, options));
  if (ctx.fatal) {
    return *ctx.fatal;
  }
  detail::copy_read_globals(st, netlist);
  netlist.root_ports = detail::port_entries(st.defs.at(hierarchy.model->root));
  return netlist;
}

Netlist flatten(const Hierarchy& hierarchy, FlatFlags flags) {
  diag::NullSink sink;
  auto netlist = flatten(hierarchy, flags, FlattenOptions{}, sink);
  return netlist.ok() ? std::move(netlist).value() : Netlist{};
}

diag::Outcome<Netlist> flatten_one_level(const Hierarchy& hierarchy, std::string_view def, FlatFlags flags,
                                         const FlattenOptions& options, diag::DiagnosticSink& sink,
                                         bool* devices_absent) {
  if (!hierarchy.model) {
    return Netlist{};
  }
  const detail::ReadState& st = *hierarchy.model->st;
  const auto id = st.defs.lookup(def);
  if (!id) {
    return diag::make(detail::codes::kCannotRead, "Cannot read extract file " + std::string{def} + ".ext");
  }
  detail::FlatContext ctx{sink, hierarchy.model->options, st.globals, st.defs, st.layers, std::nullopt};
  std::vector<char>& marks = hierarchy.model->nodevices;
  marks.resize(st.defs.size(), 0);
  Netlist netlist = detail::flatten_level(ctx, *id, detail::plan_for(st, flags, options), marks);
  if (ctx.fatal) {
    return *ctx.fatal;
  }
  if (devices_absent != nullptr) {
    *devices_absent = marks[*id] != 0;
  }
  detail::copy_read_globals(st, netlist);
  netlist.root_ports = detail::port_entries(st.defs.at(*id));
  return netlist;
}

void make_ports(Hierarchy& hierarchy, bool cap_infinite, double cap_threshold) {
  if (!hierarchy.model) {
    return;
  }
  detail::ReadState& st = *hierarchy.model->st;
  for (const HierDef& def : hierarchy.defs) {
    if (const auto id = st.defs.lookup(def.name)) {
      detail::make_ports(st, *id, cap_infinite, cap_threshold);
    }
  }
}

DefPorts def_ports(const Hierarchy& hierarchy, std::string_view def) {
  if (!hierarchy.model) {
    return {};
  }
  const detail::ReadState& st = *hierarchy.model->st;
  const auto id = st.defs.lookup(def);
  return id ? detail::def_ports(st.defs.at(*id)) : DefPorts{};
}

void clear_ports(Hierarchy& hierarchy, std::string_view def) {
  if (!hierarchy.model) {
    return;
  }
  detail::ReadState& st = *hierarchy.model->st;
  if (const auto id = st.defs.lookup(def)) {
    detail::clear_ports(st.defs.at(*id));
  }
}

bool devices_absent(const Hierarchy& hierarchy, std::string_view def) {
  if (!hierarchy.model) {
    return false;
  }
  const auto id = hierarchy.model->st->defs.lookup(def);
  const std::vector<char>& marks = hierarchy.model->nodevices;
  return id && *id < marks.size() && marks[*id] != 0;
}

diag::Outcome<std::vector<OverlayRecord>> read_res_overlay(cell::LookupContext& ctx, std::string_view cell,
                                                           const ReadOptions& options) {
  const auto path = detail::find_ext_file(options, ctx, cell, ".res.ext");
  const diag::Diagnostic missing =
      diag::make(detail::codes::kCannotRead, "Cannot read extract file " + std::string{cell} + ".res.ext");
  if (!path) {
    return missing;
  }
  const auto size = ctx.fs.size(*path);
  if (size.ok() && size.value() > options.limits.max_file_bytes) {
    return diag::make_native(detail::codes::kLimit, *path + ": file larger than " +
                                                        std::to_string(options.limits.max_file_bytes) + " bytes");
  }
  const auto text = ctx.fs.read(*path);
  if (!text.ok()) {
    return missing;
  }
  std::vector<OverlayRecord> out;
  detail::LineReader reader{text.value(), options.limits.max_line_bytes};
  while (const auto line = reader.next()) {
    if (line->too_long) {
      return diag::make_native(detail::codes::kLimit, *path + ", line " + std::to_string(line->number) +
                                                          ": line longer than " +
                                                          std::to_string(options.limits.max_line_bytes) + " bytes");
    }
    out.push_back(OverlayRecord{line->tokens, line->number});
  }
  return out;
}

}  // namespace hefesto::extflat
