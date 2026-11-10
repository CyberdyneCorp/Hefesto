// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the select and connect unit tests: the test technology
// `mt` of tests/golden/MODEL/CASES.md (built through the tech contract), a
// command session with the db, cell and select commands, and geometry
// helpers. The connect tests include it as "../select/select_fixture.hpp".

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "hefesto/cell/commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/select/commands.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/technology.hpp"

namespace Catch {
template <>
struct StringMaker<hefesto::db::Rect> {
  static std::string convert(const hefesto::db::Rect& r) {
    return "(" + std::to_string(r.x0) + "," + std::to_string(r.y0) + ")-(" + std::to_string(r.x1) + "," +
           std::to_string(r.y1) + ")";
  }
};
}  // namespace Catch

namespace hefesto::select::test {

/// The `mt` technology: planes active, metal1..3; contacts pdc
/// (pdiff, metal1), via1 (metal1, metal2), via2 (metal2, metal3), stackable
/// via1 + via2.
struct MtTech {
  std::shared_ptr<const tech::Technology> tech;
  tech::PlaneId active = 0, metal1 = 0, metal2 = 0, metal3 = 0;
  tech::TileType ndiff = 0, pdiff = 0, poly = 0, nfet = 0, m1 = 0, m2 = 0, m3 = 0, pdc = 0, via1 = 0, via2 = 0;
};

inline std::optional<MtTech> build_mt() {
  tech::TechnologyBuilder b;
  b.set_name("mt");
  MtTech mt;
  const auto plane = [&](std::string_view n, tech::PlaneId& id) {
    auto r = b.add_plane(n);
    if (r.ok()) {
      id = r.value();
    }
    return r.ok();
  };
  const auto type = [&](std::string_view p, std::string_view n, tech::TileType& id) {
    auto r = b.add_type(p, n);
    if (r.ok()) {
      id = r.value();
    }
    return r.ok();
  };
  bool ok = plane("active", mt.active) && plane("metal1", mt.metal1) && plane("metal2", mt.metal2) &&
            plane("metal3", mt.metal3);
  ok = ok && type("active", "ndiff", mt.ndiff) && type("active", "pdiff", mt.pdiff) &&
       type("active", "poly", mt.poly) && type("active", "nfet", mt.nfet) && type("metal1", "metal1", mt.m1) &&
       type("metal2", "metal2", mt.m2) && type("metal3", "metal3", mt.m3) && type("active", "pdc", mt.pdc) &&
       type("metal1", "via1", mt.via1) && type("metal2", "via2", mt.via2);
  const std::vector<std::string_view> pdc{"pdiff", "metal1"};
  const std::vector<std::string_view> v1{"metal1", "metal2"};
  const std::vector<std::string_view> v2{"metal2", "metal3"};
  const std::vector<std::string_view> stack{"via1", "via2"};
  ok = ok && b.add_contact("pdc", pdc).ok() && b.add_contact("via1", v1).ok() && b.add_contact("via2", v2).ok() &&
       b.add_stackable(stack).ok();
  ok = ok && b.add_connect("metal1", "via1,pdc").ok() && b.add_connect("metal2", "via1,via2").ok() &&
       b.add_connect("metal3", "via2").ok() && b.add_connect("pdiff", "pdc").ok();
  if (!ok) {
    return std::nullopt;
  }
  auto t = b.finalize();
  if (!t.ok()) {
    return std::nullopt;
  }
  mt.tech = t.value();
  return mt;
}

#define HEFESTO_SELECT_MT(var)                                    \
  const auto var##_opt = ::hefesto::select::test::build_mt();     \
  if (!var##_opt) {                                               \
    SKIP("tech contract cannot build the mt technology yet");     \
  }                                                               \
  const ::hefesto::select::test::MtTech& var = *var##_opt

/// Paints `type` over `r` in `def` (the user-level paint).
inline void paint(db::CellDef& def, const MtTech& mt, const db::Rect& r, tech::TileType type) {
  REQUIRE(db::paint(def, *mt.tech, r, type).ok());
  db::recompute_bbox(def);
}

/// The union of the non-space tiles of one type on one plane, as the
/// rectangles of its canonical decomposition (tiles of `type` only).
inline std::vector<db::Rect> rects_of(const db::CellDef& def, tech::PlaneId plane, tech::TileType type) {
  std::vector<db::Rect> out;
  def.plane(plane).enumerate(db::whole_plane(), tech::TypeMask::of(type), [&](const db::TileView& t) {
    out.push_back(t.area);
    return true;
  });
  std::sort(out.begin(), out.end(), [](const db::Rect& a, const db::Rect& b) {
    return std::tie(a.y0, a.x0, a.y1, a.x1) < std::tie(b.y0, b.x0, b.y1, b.x1);
  });
  return out;
}

/// The area (in square units) covered by `type` on `plane`.
inline std::int64_t area_of(const db::CellDef& def, tech::PlaneId plane, tech::TileType type) {
  std::int64_t a = 0;
  for (const db::Rect& r : rects_of(def, plane, type)) {
    a += r.width() * r.height();
  }
  return a;
}

/// A dispatcher session with the db, cell and select commands, on `mt`,
/// with an edit cell `top` (units: internal, snap internal).
struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  const MtTech* mt = nullptr;

  explicit Session(const MtTech& tech) : mt(&tech) {
    REQUIRE(db::register_db_commands(registry));
    REQUIRE(cell::register_cell_commands(registry));
    REQUIRE(register_select_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = tech.tech;
    editor.edit_cell = cell("top");
    editor.snap = editor::SnapMode::internal;
    ok("snap internal");
  }

  db::CellDef* cell(const std::string& name) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->flags().available = true;
    return def;
  }

  db::CellDef& top() { return *editor.edit_cell; }

  cmd::CommandResult run(const std::string& line) {
    cmd::Invocation inv;
    std::size_t pos = 0;
    while (pos < line.size()) {
      const std::size_t end = line.find(' ', pos);
      const std::string tok = line.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
      if (!tok.empty()) {
        (inv.name.empty() ? inv.name : inv.args.emplace_back()) = tok;
      }
      pos = end == std::string::npos ? line.size() : end + 1;
    }
    return dispatcher->execute(inv);
  }

  void ok(const std::string& line) {
    const auto r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }

  /// Paints in the edit cell through the db primitive.
  void paint(const db::Rect& r, tech::TileType type) { test::paint(top(), *mt, r, type); }

  /// Places `child` in the edit cell at (dx, dy).
  db::CellUse* place(db::CellDef& child, const std::string& id, db::Coord dx = 0, db::Coord dy = 0) {
    db::PlaceOptions o;
    o.id = id;
    o.transform = db::translation(dx, dy);
    auto u = db::place(top(), child, o);
    REQUIRE(u.ok());
    db::recompute_bbox(top());
    return u.value();
  }

  const db::CellDef& selection() { return *editor.selection.def(); }
};

/// Every diagnostic text of a result, joined with line feeds.
inline std::string all_text(const cmd::CommandResult& r) {
  std::string out;
  for (const auto& d : r.diagnostics) {
    out += d.text + "\n";
  }
  return out;
}

}  // namespace hefesto::select::test
