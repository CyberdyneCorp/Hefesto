// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the def/write unit tests: a command session over an in-memory
// file system loaded with tests/data/def/write/lefdef.tech (the LEFDEF
// golden family's tech: `scalefactor 10 nanometers`, so s = 10 at the
// default units; metal1..metal3 route widths 3, 4 and 5 from the DRC
// rules; via1/via2 cut squares 5 20 20; obsm1/obsm2/obsv1 obstructions),
// a `defw` command calling def::write_command (the shared `lef`/`def`
// dispatcher calls it once INTEGRATION wires it), cells built in memory
// and `.ext` files under /t.
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/def/write/command.hpp"
#include "hefesto/def/write/write.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"

namespace def_write_test {

namespace db = hefesto::db;
namespace def = hefesto::def;
namespace diag = hefesto::diag;
namespace tech = hefesto::tech;
namespace cmd = hefesto::cmd;
namespace platform = hefesto::platform;

inline std::string data_file(const std::string& relative) {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / relative, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// `def write` through def::write_command.
inline cmd::CommandResult defw_handler(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  return def::write_command(ctx, invocation);
}

/// A label to add to a cell.
struct Pin {
  std::string text;
  std::string type;
  db::Rect rect;
  bool port = true;
  db::PortClass port_class = db::PortClass::default_class;
  db::PortUse use = db::PortUse::default_use;
};

struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  std::vector<diag::Diagnostic> messages;

  explicit Session(bool magic_compat = true, const std::string& tech_text = data_file("lefdef.tech")) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    REQUIRE(!registry
                 .add(cmd::CommandSpec{"defw", "defw write ...", "def write (DEFW tests)", defw_handler},
                      cmd::kLayoutClient)
                 .has_value());
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
    file("/t/lefdef.tech", tech_text);
    const auto loaded = run("tech load /t/lefdef.tech -noprompt");
    for (const auto& d : messages) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
    REQUIRE(loaded.ok());
    editor.drc.compat = &project.compat;
    editor.search_paths.path = {"/t"};
    editor.command_point = cmd::PointerState{};
    (void)printed.take();
    messages.clear();
  }

  /// The route widths of a filled table (metal1..metal3 = 3, 4, 5). With
  /// magic-compat ON and a 1:1 grid Magic leaves them -1 (LefTechSetDefaults
  /// runs only when the grid is scaled), so every regular wire would take a
  /// TAPERRULE; the spec's scenarios assume filled widths.
  void set_widths() {
    for (const auto& [name, width] : {std::pair{"metal1", 3}, std::pair{"metal2", 4}, std::pair{"metal3", 5}}) {
      hefesto::lef::LayerRecord* rec = editor.lefdef.layers.find_mutable(name);
      REQUIRE(rec != nullptr);
      rec->width = width;
    }
  }

  void file(const std::string& path, const std::string& text) {
    REQUIRE(services.view().files.write_atomic(path, text).ok());
  }

  [[nodiscard]] std::string read(const std::string& path) {
    auto text = services.view().files.read(path);
    REQUIRE(text.ok());
    return text.value();
  }

  [[nodiscard]] bool exists(const std::string& path) { return services.view().files.exists(path); }

  /// Runs a `;`-separated line; returns the first failure, else the last
  /// result. Every diagnostic is kept in `messages`.
  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      for (const auto& d : r.diagnostics) {
        messages.push_back(d);
      }
      if (!r.ok()) {
        return std::move(r);
      }
      last = std::move(r);
    }
    return last;
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  /// A new, available cell, made the edit root.
  db::CellDef& cell(std::string_view name, bool root = true) {
    auto made = editor.cells.create(name, editor.technology->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    if (root) {
      editor.edit_root = made.value();
      editor.edit_cell = made.value();
    }
    return *made.value();
  }

  void paint(db::CellDef& def, std::string_view type, const db::Rect& r) {
    REQUIRE(db::paint_type(def, *editor.technology, r, this->type(type)).ok());
    db::recompute_bbox(def);
  }

  void label(db::CellDef& def, const Pin& p) {
    db::Label l;
    l.text = p.text;
    l.type = p.type.empty() ? tech::kSpace : type(p.type);
    l.rect = p.rect;
    if (p.port) {
      l.port_directions = db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest;
      l.port_class = p.port_class;
      l.port_use = p.use;
    }
    def.labels().push_back(l);
    db::recompute_bbox(def);
  }

  /// Places `child` in `parent` at `transform` with instance id `id`.
  db::CellUse& place(db::CellDef& parent, db::CellDef& child, const std::string& id,
                     const db::Transform& transform = {}) {
    db::PlaceOptions o;
    o.transform = transform;
    o.id = id;
    auto use = db::place(parent, child, o);
    REQUIRE(use.ok());
    db::recompute_bbox(parent);
    return *use.value();
  }

  /// def::write of `def` to `name` (the typed name; empty: the cell's).
  cmd::CommandResult write(db::CellDef& def, const std::string& name, def::DefWriteOptions options = {}) {
    options.files = &services.view().files;
    options.typed_name = name;
    messages.clear();
    auto result = def::write(editor, def, name.empty() ? def.name() : name, options);
    messages = result.diagnostics;
    return result;
  }

  [[nodiscard]] bool said(std::string_view text) const {
    return std::any_of(messages.begin(), messages.end(),
                       [&](const diag::Diagnostic& d) { return d.text == text; });
  }

  [[nodiscard]] const diag::Diagnostic* message(std::string_view code) const {
    for (const auto& d : messages) {
      if (d.code == code) {
        return &d;
      }
    }
    return nullptr;
  }
};

/// The part of a DEF file from its line `first` (e.g. "VIAS") up to and
/// including the line after `END <first>` (a blank line).
inline std::string section(const std::string& text, std::string_view first) {
  const std::size_t start = text.find("\n" + std::string{first} + " ");
  if (start == std::string::npos) {
    return {};
  }
  const std::string end = "END " + std::string{first} + "\n\n";
  const std::size_t stop = text.find(end, start);
  return stop == std::string::npos ? text.substr(start + 1) : text.substr(start + 1, stop + end.size() - start - 1);
}

}  // namespace def_write_test
