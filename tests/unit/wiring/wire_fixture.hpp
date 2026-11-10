// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the wiring-tool unit tests: a session with the
// tests/data/wiring/wire.tech technology, an edit cell `top` shown in one
// window, the `wire`/`trackwire` commands and the box commands, a cursor
// setter and paint inspection.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/wiring/commands.hpp"

namespace hefesto::wiring::test {

inline std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// Loads a technology from text (as `/t/<name>`); fails the test on errors.
inline std::shared_ptr<const tech::Technology> load_tech(const std::string& text, const std::string& name = "x.tech",
                                                         std::vector<diag::Diagnostic>* diagnostics = nullptr) {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/" + name, fs, tech::LoadOptions{}, sink);
  if (diagnostics != nullptr) {
    *diagnostics = sink.take();
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

inline std::shared_ptr<const tech::Technology> wire_tech() {
  return load_tech(read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "wire.tech"), "wire.tech");
}

inline tech::TileType type_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

/// Non-space rectangles of `type` on its home plane, in enumeration order.
inline std::vector<db::Rect> rects_of(const db::CellDef& def, const tech::Technology& t, tech::TileType type) {
  std::vector<db::Rect> out;
  def.plane(*t.home_plane(type)).enumerate(db::whole_plane(), tech::TypeMask::of(type), [&](const db::TileView& v) {
    out.push_back(v.area);
    return true;
  });
  return out;
}

/// `rects` in (y0, x0) order, for comparisons that do not depend on the
/// enumeration order.
inline std::vector<db::Rect> sorted(std::vector<db::Rect> rects) {
  std::sort(rects.begin(), rects.end(), [](const db::Rect& a, const db::Rect& b) {
    return std::tie(a.y0, a.x0, a.y1, a.x1) < std::tie(b.y0, b.x0, b.y1, b.x1);
  });
  return rects;
}

struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  db::CellDef* top = nullptr;
  view::WindowId window = 0;

  explicit Session(std::shared_ptr<const tech::Technology> technology = wire_tech()) {
    REQUIRE(register_wiring_commands(registry));
    REQUIRE(db::register_db_commands(registry));  // box, paint, snap
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = std::move(technology);
    diag::NullSink quiet;
    drc::reload_rules(editor, quiet);  // as `tech load`: the wire widths come from the drc style
    editor.snap = editor::SnapMode::internal;
    top = editor.cells.create("top", editor.technology->plane_count()).value();
    top->flags().available = true;
    editor.edit_cell = top;
    auto w = editor.windows.open(top, view::ScreenRect{0, 0, 499, 499});
    REQUIRE(w.ok());
    window = w.value()->id();
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const { return type_of(*editor.technology, name); }

  /// Puts the pointer at `(x, y)` in the window (root coordinates).
  void point_at(db::Coord x, db::Coord y) {
    editor.command_point = cmd::PointerState{window, db::Point{x, y}, db::Rect{x, y, x, y}};
  }

  cmd::CommandResult run(const std::string& line) {
    cmd::Invocation inv;
    std::istringstream words(line);
    std::string word;
    while (words >> word) {
      (inv.name.empty() ? inv.name : inv.args.emplace_back()) = word;
    }
    return dispatcher->execute(inv);
  }

  void ok(const std::string& line) {
    const auto r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }

  /// Paints `layer` over the rectangle (through `box` and `paint`).
  void paint(const std::string& layer, db::Coord x0, db::Coord y0, db::Coord x1, db::Coord y1) {
    ok("box " + std::to_string(x0) + " " + std::to_string(y0) + " " + std::to_string(x1) + " " + std::to_string(y1));
    ok("paint " + layer);
  }

  [[nodiscard]] std::vector<db::Rect> rects(std::string_view layer) const {
    return rects_of(*top, *editor.technology, type(layer));
  }
};

/// The text of the last diagnostic, or "".
inline std::string last_text(const cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text;
}

/// Every diagnostic text joined by newlines.
inline std::string all_text(const cmd::CommandResult& r) {
  std::string out;
  for (const auto& d : r.diagnostics) {
    out += (out.empty() ? "" : "\n") + d.text;
  }
  return out;
}

}  // namespace hefesto::wiring::test
