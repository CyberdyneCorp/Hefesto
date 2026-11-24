// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2spice hierarchy on` run (capability netlist-spice-hierarchy
// "Definition traversal order", "Port promotion", "Omitted, stubbed and
// absorbed definitions"): the read with subcircuit marking, esMakePorts
// over every definition, children first, then esHierVisit's choice per
// definition (primitive: nothing; device-less and use-less: absorbed with
// its ports cleared unless `subcircuits top` keeps it; DEF_NODEVICES after
// the one-level flattening: nothing unless a black-box stub), the
// one-level netlist of each written definition and its calls. The writing
// is hier_writer.cpp's.
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2hier.c:71-105
// (ESGenerateHierarchy) and 2300-2400 (esHierVisit), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "netlist/spice/run_impl.hpp"

#include "hefesto/app/settings_layers.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/netlist/spice/input.hpp"
#include "hefesto/netlist/spice/writer.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::netlist::spice::detail {

namespace {

bool infinite_cap(double cthresh) {
  return !std::isfinite(cthresh) || static_cast<float>(cthresh) == 1.0E38F;
}

std::optional<std::string> setting(const editor::EditorState& editor, std::string_view key) {
  if (const auto value = editor.settings.effective(key); value && !value->value.empty()) {
    return value->value;
  }
  return std::nullopt;
}

// The path component of one array element (ext-flattening-names "Instance
// and array element names"): `[y,x]` over both axes, else the one index.
std::string element_name(const extflat::HierUse& use, std::int32_t x, std::int32_t y) {
  const bool xs = use.x_range[0] != use.x_range[1];
  const bool ys = use.y_range[0] != use.y_range[1];
  if (xs && ys) {
    return use.id + "[" + std::to_string(y) + "," + std::to_string(x) + "]";
  }
  if (xs) {
    return use.id + "[" + std::to_string(x) + "]";
  }
  if (ys) {
    return use.id + "[" + std::to_string(y) + "]";
  }
  return use.id;
}

std::vector<std::int32_t> indices(const std::array<std::int32_t, 3>& range) {
  std::vector<std::int32_t> out;
  const std::int32_t lo = std::min(range[0], range[1]);
  const std::int32_t hi = std::max(range[0], range[1]);
  for (std::int32_t i = lo; i <= hi; ++i) {
    out.push_back(i);
  }
  return out;
}

class HierRun {
 public:
  HierRun(editor::EditorState& editor, const RunRequest& request, diag::DiagnosticSink& sink,
          extflat::Hierarchy& hierarchy)
      : editor_(editor), request_(request), sink_(sink), h_(hierarchy) {
    for (const extflat::HierDef& def : h_.defs) {
      defs_.emplace(def.name, def);
    }
  }

  diag::Outcome<std::vector<HierSection>> sections() {
    std::vector<HierSection> out;
    for (const extflat::HierDef& def : h_.defs) {
      auto section = visit(def);
      if (!section.ok()) {
        return section.error();
      }
      if (section.value()) {
        out.push_back(std::move(*section.value()));
      }
    }
    return out;
  }

  [[nodiscard]] const SpiceInput& head() const { return head_; }

 private:
  [[nodiscard]] bool is_top(const extflat::HierDef& def) const { return def.name == h_.root; }
  [[nodiscard]] const SpiceSettings& settings() const { return request_.settings; }

  // esHierVisit's test for a cell without devices or uses: kept as a
  // subcircuit (top on, or auto with a port), else absorbed into its
  // parent with its ports cleared.
  bool absorbed(const extflat::HierDef& def) {
    if (is_top(def) || def.devices != 0 || def.uses != 0) {
      return false;
    }
    const TopSubckt top = settings().subckt_top;
    const bool keep = top == TopSubckt::on || (top == TopSubckt::automatic && extflat::def_ports(h_, def.name).any_port);
    if (!keep) {
      extflat::clear_ports(h_, def.name);
    }
    return !keep;
  }

  // `.subckt` line or `* Top level circuit`: every cell but a root that is
  // no subcircuit, under top off or under auto without a port.
  bool wrapped(const extflat::HierDef& def) {
    if (!is_top(def) || def.subcircuit || settings().subckt_top == TopSubckt::on) {
      return true;
    }
    return settings().subckt_top == TopSubckt::automatic && extflat::def_ports(h_, def.name).any_port;
  }

  diag::Outcome<std::optional<HierSection>> visit(const extflat::HierDef& def) {
    if (def.primitive || absorbed(def)) {
      return std::optional<HierSection>{};
    }
    bool nodevices = false;
    auto netlist = extflat::flatten_one_level(h_, def.name, flat_flags(settings()),
                                              flatten_options(editor_, settings(), request_.symbols), sink_,
                                              &nodevices);
    if (!netlist.ok()) {
      return netlist.error();
    }
    const bool stub = def.abstract && settings().blackbox;
    if (!is_top(def) && nodevices && !stub) {
      return std::optional<HierSection>{};
    }
    HierSection section;
    section.input = from_netlist(netlist.value(), def.name);
    apply_style(section.input, editor_);
    section.input.root_abstract = def.abstract;
    section.stub = stub;
    section.wrapped = wrapped(def);
    add_calls(def, section);
    if (is_top(def)) {
      head_ = section.input;
    }
    return std::optional<HierSection>{std::move(section)};
  }

  // EFHierVisitSubcircuits: each use in use-table order, each array
  // element with x outer and y inner.
  void add_calls(const extflat::HierDef& def, HierSection& section) {
    h_.visit_uses(def.name, [&](const extflat::HierUse& use) {
      const extflat::HierDef& child = defs_.at(use.def);
      HierCall call;
      call.cell = use.def;
      call.ports = extflat::def_ports(h_, use.def).entries;
      call.devices_absent = extflat::devices_absent(h_, use.def);
      call.stub = (child.abstract || child.primitive) && settings().blackbox;
      for (const std::int32_t x : indices(use.x_range)) {
        for (const std::int32_t y : indices(use.y_range)) {
          call.path = {element_name(use, x, y)};
          section.calls.push_back(call);
        }
      }
    });
  }

  editor::EditorState& editor_;
  const RunRequest& request_;
  diag::DiagnosticSink& sink_;
  extflat::Hierarchy& h_;
  std::map<std::string, extflat::HierDef, std::less<>> defs_;
  SpiceInput head_;
};

}  // namespace

diag::Outcome<WriteResult> write_hierarchy(editor::EditorState& editor, std::string_view cell,
                                           const RunRequest& request, platform::FileSystem& fs,
                                           diag::DiagnosticSink& sink, extflat::NameFlags& names) {
  cell::LookupContext lookup{fs, editor.search_paths, editor.path_variables, sink};
  auto hierarchy = extflat::read(lookup, cell, read_options(editor, request.settings, request.compat));
  if (!hierarchy.ok()) {
    return hierarchy.error();
  }
  extflat::make_ports(hierarchy.value(), infinite_cap(request.settings.cthresh), request.settings.cthresh);
  HierRun run{editor, request, sink, hierarchy.value()};
  auto sections = run.sections();
  if (!sections.ok()) {
    return sections.error();
  }
  HierHead head{run.head(), {}, {}};
  head.root.cell = std::string{cell};
  if (const auto& style = editor.extract.current) {
    const auto table = style->device_info();
    head.table.assign(table.begin(), table.end());
  }
  head.power = PowerNames{setting(editor, app::kSub), setting(editor, app::kVdd), setting(editor, app::kGnd)};
  return write_hier(head, sections.value(), request.settings, request.flags, request.compat, names);
}

}  // namespace hefesto::netlist::spice::detail
