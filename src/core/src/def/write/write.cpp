// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF write (capability def-write "Output file naming", "Preconditions and
// diagnostics", "Output scale and number formatting", "DEF output file
// grammar and section order", "Session state after writing"; design D9;
// Q-LEFDEF-90, 99, 110). The file is assembled in memory in Magic's
// section order, the nets sections in their own buffer (Magic's
// `<name>_part.def`), and written in one piece. The write works on copies
// of the session's layer table (its shared key order), via table and rule
// table and commits them only when the file was written, so a write that
// cannot open its file changes nothing, as in Magic (which opens the file
// first).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defWrite.c DefWriteCell and
// defCountNets, and lef/lefWrite.c lefFileOpen, Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/def/write/write.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "def/write/internal.hpp"
#include "hefesto/app/settings_layers.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/write/number.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::def {

namespace wd = write_detail;

namespace {

constexpr std::string_view kUnnamedCell = "(UNNAMED)";
constexpr std::int32_t kDefaultUnits = 1000;

/// Appends what the via code reports to the write's messages.
class MessageSink final : public diag::DiagnosticSink {
 public:
  explicit MessageSink(std::vector<diag::Diagnostic>& out) : out_(out) {}
  void emit(const diag::Diagnostic& d) override { out_.push_back(d); }

 private:
  std::vector<diag::Diagnostic>& out_;
};

std::optional<std::string> setting(const editor::EditorState& editor, std::string_view name) {
  const auto value = editor.settings.effective(name);
  if (!value || value->value.empty()) {
    return std::nullopt;
  }
  return value->value;
}

wd::GlobalNames global_names(const editor::EditorState& editor) {
  wd::GlobalNames names;
  if (const auto globals = setting(editor, app::kGlobalNets)) {
    std::istringstream in(*globals);
    for (std::string name; in >> name;) {
      names.globals.push_back(name);
    }
  }
  names.vdd = setting(editor, app::kVdd);
  names.gnd = setting(editor, app::kGnd);
  return names;
}

}  // namespace

namespace write_detail {

// lefFileOpen(def, name, ".def", "w"): a name whose last component has a
// foreign extension is tried as given, then with `.def` in its place; a
// name without one gets `.def`. With magic-compat ON a typed name that
// fails is retried as `<cell>.def`, as Magic does (the LEF write rule).
std::vector<std::string> output_names(std::string_view base, std::string_view cell_name, bool magic_compat) {
  std::vector<std::string> names;
  const std::size_t slash = base.rfind('/');
  const std::size_t start = slash == std::string_view::npos ? 0 : slash + 1;
  const std::size_t dot = base.rfind('.');
  if (dot == std::string_view::npos || dot < start) {
    names.push_back(std::string{base} + ".def");
  } else if (base.substr(dot) == ".def") {
    names.emplace_back(base);
  } else {
    names.emplace_back(base);
    names.push_back(std::string{base.substr(0, dot)} + ".def");
  }
  const std::string fallback = std::string{cell_name} + ".def";
  if (magic_compat && base != cell_name && names.back() != fallback) {
    names.push_back(fallback);
  }
  return names;
}

}  // namespace write_detail

namespace {

/// One write: the session state it reads and the messages it prints.
class WriteRun {
 public:
  WriteRun(editor::EditorState& editor, db::CellDef& cell, const DefWriteOptions& options)
      : editor_(editor),
        tech_(*editor.technology),
        cell_(cell),
        options_(options),
        compat_(editor.drc.compat_settings()),
        units_(options.units.value_or(kDefaultUnits)),
        style_(editor.mask.output_styles.current_style()),
        scale_(lef::output_scale(style_) * static_cast<float>(units_)),
        extract_style_(extract::StyleView::from_technology(tech_, quiet_)),
        map_(wd::info_order(editor.lefdef.layers, editor.lefdef.vias), tech_),
        layers_(editor.lefdef.layers),
        vias_(editor.lefdef.vias),
        rules_(compat_.magic_compat ? editor.lefdef.rules : lef::RuleTable{}),
        sink_(messages_),
        c_{tech_, cell_, scale_, compat_.magic_compat, editor.drc.rules.get(), style_, extract_style_, map_,
           messages_, {}} {
    if (options.read_area) {
      c_.read_area = [this](const db::Rect& area) { options_.read_area(cell_, area, sink_); };
    }
  }

  cmd::CommandResult run(const platform::Path& path) {
    std::string text;
    wd::write_header(c_, tech_.name(), units_, text);
    lef::DefViaContext via_context{tech_, c_.rules, [this](tech::TileType t) { return map_.name(t); }, scale_,
                                   compat_, sink_};
    const lef::DefVias vias = lef::register_vias(cell_, vias_, layers_, via_context);
    const std::vector<wd::InfoEntry> order = wd::info_order(layers_, vias_);
    wd::write_vias(c_, order, via_context, vias.created, text);
    wd::write_components(c_, text);
    wd::write_pins(c_, text);
    const std::optional<extflat::Netlist> netlist = read_netlist();
    const wd::NetPlan plan = netlist ? wd::classify_nets(c_, *netlist, global_names(editor_), options_.all_special)
                                     : wd::NetPlan{};
    std::string nets;
    if (netlist) {
      write_nets(*netlist, plan, positions(vias), nets);
    }
    wd::write_rules(c_, rules_, order, layers_, vias_, options_.anal, text);
    text += nets;
    wd::write_blockages(c_, order, netlist ? &*netlist : nullptr, plan, text);
    text += "END DESIGN\n\n";
    return save(path, text);
  }

 private:
  /// The via each contact region names, by position key: its canonical
  /// name (a layer of that name wins, as in Magic's single table).
  wd::ViaPositions positions(const lef::DefVias& vias) const {
    wd::ViaPositions out;
    for (const auto& [key, name] : vias.positions) {
      std::string canonical = name;
      if (const lef::LayerRecord* layer = layers_.find(name)) {
        canonical = layer->canonical_name;
      } else if (const auto* via = vias_.find(name); via != nullptr && via->value) {
        canonical = via->value->canonical_name;
      }
      out.emplace(key, std::move(canonical));
    }
    return out;
  }

  void write_nets(const extflat::Netlist& netlist, const wd::NetPlan& plan, const wd::ViaPositions& vias,
                  std::string& out) {
    wd::RuleBook book(rules_);
    if (plan.special_count > 0) {
      out += "SPECIALNETS " + std::to_string(plan.special_count) + " ;\n";
      wd::write_nets(c_, netlist, plan, options_.all_special ? wd::NetMode::all_special : wd::NetMode::special,
                     vias, book, out);
      out += "END SPECIALNETS\n\n";
    }
    if (plan.regular_count > 0 && !options_.all_special) {
      out += "NETS " + std::to_string(plan.regular_count) + " ;\n";
      wd::write_nets(c_, netlist, plan, wd::NetMode::regular, vias, book, out);
      out += "END NETS\n\n";
    }
  }

  // defCountNets' EFReadFile and EFFlatBuild.
  std::optional<extflat::Netlist> read_netlist() {
    c_.say(wd::kProgress, "Diagnostic:  Finding all nets in cell " + cell_.name());
    c_.say(wd::kProgress, "(This can take a while!)");
    platform::FileSystem& fs = files();
    cell::LookupContext lookup{fs, editor_.search_paths, editor_.path_variables, sink_};
    extflat::ReadOptions options;
    options.timestamps = !compat_.magic_compat;
    options.magic_compat = compat_.magic_compat;
    options.tech_name = tech_.name();
    options.global_nets = global_names(editor_).globals;
    options.cell_directory = [this](std::string_view name) -> std::optional<std::string> {
      const db::CellDef* def = editor_.cells.find(name);
      if (def == nullptr || def->file_path().empty()) {
        return std::nullopt;
      }
      return def->file_path().parent_path().string();
    };
    auto netlist = extflat::read_flat(lookup, cell_.name(), options);
    if (!netlist.ok()) {
      // EFReadFile's "Cannot read extract file" is a Tcl result in Magic,
      // not a printed line.
      diag::Diagnostic why = netlist.error();
      why.native_only = why.native_only || why.code == "EXTFLAT-READ-001";
      messages_.push_back(std::move(why));
      c_.say(wd::kNoExt, "Warning:  Circuit has no .ext file;  no nets written.");
      c_.say(wd::kNoExt, "Run extract on this circuit if you want nets in the output.");
      return std::nullopt;
    }
    if (!compat_.magic_compat) {
      stale_check(netlist.value());
    }
    return std::move(netlist).value();
  }

  // Q-LEFDEF-110 OFF: a `.ext` file whose timestamp differs from its cell's,
  // or whose cell was modified, is reported.
  void stale_check(const extflat::Netlist& netlist) const {
    for (const extflat::FileStamp& stamp : netlist.stamps()) {
      const db::CellDef* def = editor_.cells.find(stamp.cell);
      if (def != nullptr && (def->timestamp() != stamp.timestamp || def->flags().modified)) {
        c_.say(wd::kStale, stamp.cell + ".ext is out of date; nets may be wrong. Run extract.");
      }
    }
  }

  platform::FileSystem& files() {
    return options_.files != nullptr ? *options_.files : local_;
  }

  cmd::CommandResult save(const platform::Path& path, const std::string& text) {
    const std::vector<std::string> names = wd::output_names(path.string(), cell_.name(), compat_.magic_compat);
    std::string reason;
    for (const std::string& name : names) {
      const diag::Status st = files().write_atomic(name, text);
      if (st.ok()) {
        commit();
        return finish(name);
      }
      reason = st.error().text;
    }
    const std::string name = names.back();
    cmd::CommandResult result = cmd::CommandResult::failure(
        diag::make(wd::kCannotOpen, "Cannot open output file " + name + " (" + reason + ")."));
    result.diagnostics.insert(result.diagnostics.begin(), generating(name));
    return result;
  }

  // lefRemoveGeneratedVias: the temporary vias leave the table, their keys
  // stay in the shared key order (an emptied slot can be refilled by a
  // later write, Q-LEFDEF-135); the rules
  // created with magic-compat ON stay for the session (Q-LEFDEF-99).
  void commit() {
    std::vector<std::string> generated;
    vias_.for_each([&](const lef::ViaTable::Entry& e) {
      if (e.value && e.value->ref_count == 0) {
        generated.push_back(e.key);
      }
    });
    for (const std::string& key : generated) {
      (void)vias_.clear_value(key);
    }
    editor_.lefdef.layers = std::move(layers_);
    editor_.lefdef.vias = std::move(vias_);
    if (compat_.magic_compat) {
      editor_.lefdef.rules = std::move(rules_);
    }
  }

  [[nodiscard]] diag::Diagnostic generating(const std::string& name) const {
    return diag::make(wd::kProgress, "Generating DEF output " + name + " for cell " + cell_.name() + ":");
  }

  cmd::CommandResult finish(const std::string& name) {
    cmd::CommandResult result = cmd::CommandResult::success();
    result.diagnostics.push_back(generating(name));
    for (diag::Diagnostic& d : messages_) {
      result.diagnostics.push_back(std::move(d));
    }
    return result;
  }

  editor::EditorState& editor_;
  const tech::Technology& tech_;
  db::CellDef& cell_;
  const DefWriteOptions& options_;
  compat::CompatSettings compat_;
  std::int32_t units_;
  const mask::OutputStyle* style_;
  float scale_;
  diag::NullSink quiet_;
  extract::StyleView extract_style_;
  wd::LayerMap map_;
  lef::LayerTable layers_;
  lef::ViaTable vias_;
  lef::RuleTable rules_;
  std::vector<diag::Diagnostic> messages_;
  MessageSink sink_;
  wd::Context c_;
  platform::LocalFileSystem local_;
};

}  // namespace

// DefWriteCell.
cmd::CommandResult write(editor::EditorState& editor, db::CellDef& cell, const platform::Path& path,
                         const DefWriteOptions& options) {
  if (editor.technology == nullptr) {
    return cmd::CommandResult::failure(
        diag::make_native(wd::kNoCell, "No technology is loaded; nothing is written."));
  }
  if (options.units && *options.units <= 0) {
    return cmd::CommandResult::failure(diag::make(
        wd::kUnits, "The \"-units\" value must be a positive integer; got " + std::to_string(*options.units) + "."));
  }
  if (cell.name() == kUnnamedCell) {
    return cmd::CommandResult::failure(diag::make(wd::kUnnamed, "Please name the cell before generating DEF."));
  }
  WriteRun run(editor, cell, options);
  return run.run(path.empty() ? platform::Path{cell.name()} : path);
}

}  // namespace hefesto::def
