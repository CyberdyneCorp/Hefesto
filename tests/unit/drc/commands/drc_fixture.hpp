// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A scripted session for the `drc` command tests: every command through
// the dispatcher on a one-plane technology (`m1` on plane `metal1`), with
// helpers to build cells, paint paint and error tiles, show a cell in a
// window and place the box. The error checks themselves are stood in for
// by drc::ScopedRecheck, so these tests do not depend on RULES, CHECK or
// ENGINE results.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/drc/commands/reporting.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::drc::test {

struct DrcFixture {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  tech::TileType m1 = 0;
  tech::PlaneId metal1 = 0;

  /// `in_memory` false: the local disk instead of a memory file system.
  explicit DrcFixture(bool in_memory = true) : services{platform::PortableOptions{{}, {}, in_memory, true}} {
    tech::TechnologyBuilder b;
    b.set_name("drct");
    metal1 = b.add_plane("metal1").value();
    m1 = b.add_type("metal1", "m1").value();
    editor.technology = b.finalize().value();
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
  }

  void compat(bool on) { project.compat.magic_compat = on; }

  db::CellDef* cell(const std::string& name) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->flags().available = true;
    return def;
  }

  /// m1 over `r` (so the bounding box covers it).
  void paint(db::CellDef& def, const db::Rect& r) {
    db::PaintRow row{};
    row.fill(m1);
    REQUIRE(def.plane(metal1).paint(r, row).ok());
    db::recompute_bbox(def);
  }

  /// An error tile of `type` over `r` in the error plane.
  static void error(db::CellDef& def, const db::Rect& r, tech::TileType type = tech::kErrorP) {
    REQUIRE(def.plane(tech::kPlaneDrcError).paint(r, identity_paint_row(type)).ok());
  }

  /// `child` placed in `parent` at offset (dx, dy).
  static db::CellUse* place(db::CellDef& parent, db::CellDef& child, db::Coord dx, db::Coord dy) {
    db::PlaceOptions o;
    o.transform = db::translation(dx, dy);
    db::CellUse* use = db::place(parent, child, o).value();
    db::recompute_bbox(parent);
    return use;
  }

  /// Opens a window on `root`, which becomes the edit cell.
  void show(db::CellDef& root) {
    editor.edit_cell = &root;
    REQUIRE(editor.windows.open_frame(&root).ok());
  }

  void box(db::CellDef& root, const db::Rect& r) {
    editor.box = r;
    editor.box_root = &root;
  }

  /// Splits on single spaces; `{...}` keeps its spaces (one word).
  static cmd::Invocation parse(const std::string& line) {
    cmd::Invocation inv;
    std::string word;
    int depth = 0;
    const auto flush = [&] {
      if (!word.empty()) {
        (inv.name.empty() ? inv.name : inv.args.emplace_back()) = word;
        word.clear();
      }
    };
    for (const char c : line) {
      if (c == '{' && depth++ == 0) {
        continue;
      }
      if (c == '}' && --depth == 0) {
        continue;
      }
      if (c == ' ' && depth == 0) {
        flush();
        continue;
      }
      word += c;
    }
    flush();
    return inv;
  }

  cmd::CommandResult run(const std::string& line) { return dispatcher->execute(parse(line)); }

  /// Every diagnostic text of running `line`, one per line.
  std::string out(const std::string& line) { return texts(run(line)); }
  /// The result of `line` in Tcl list syntax.
  std::string value(const std::string& line) {
    const auto r = run(line);
    REQUIRE(r.ok());
    return r.value.to_string();
  }

  static std::string texts(const cmd::CommandResult& r) {
    std::string text;
    for (const auto& d : r.diagnostics) {
      text += d.text + "\n";
    }
    return text;
  }

  /// Installs a rule set whose why table holds `whys` (indices 6, 7, ...).
  void whys(const std::vector<std::string>& texts, const ScaleFactor& scale = {}) {
    RuleSetBuilder builder;
    for (const std::string& t : texts) {
      (void)builder.why(t);
    }
    builder.scale(scale);
    editor.drc.rules = builder.build();
  }

  /// A rule reporting why index `why` with distances `dist`/`cdist`.
  static Rule rule(WhyIndex why, std::int32_t dist = 0, std::int32_t cdist = 0) {
    Rule r;
    r.why = why;
    r.dist = dist;
    r.cdist = cdist;
    return r;
  }
};

/// One error a stand-in recheck reports: in cell `cell` (nullptr: any).
struct FakeError {
  const db::CellDef* cell = nullptr;
  db::Rect area;
  Rule rule;
};

/// A recheck that reports the listed errors of the checked cell lying in
/// (touching) the checked area, clipped to it, in list order; it records
/// every (cell, area) it was asked to check.
struct FakeRecheck {
  std::vector<FakeError> errors;
  std::vector<std::pair<const db::CellDef*, db::Rect>> calls;

  RecheckFn fn() {
    return [this](editor::EditorState&, const db::CellDef& cell, const db::Rect& area, ErrorSink& sink) {
      calls.emplace_back(&cell, area);
      for (const FakeError& e : errors) {
        if ((e.cell == nullptr || e.cell == &cell) && db::touch(e.area, area)) {
          sink.report(e.area, e.rule);
        }
      }
    };
  }
};

}  // namespace hefesto::drc::test
