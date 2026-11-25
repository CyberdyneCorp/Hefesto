// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A small technology with an extraction style and helpers to paint, label
// and place cells, for the node-naming and probe tests of sim/probe
// (task 14.2 naming portion). Planes have no short alias, so generated node
// names read `metal1_<x>_<y>#`.
//
// Owner: SIMPROBE.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/probe/probe.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::test::simprobe {

namespace db = hefesto::db;
namespace sim = hefesto::sim;
namespace tech = hefesto::tech;

inline constexpr std::string_view kTech = R"(tech
    format 35
    probetest
end

planes
    active
    metal1
    metal2
end

types
    active  ndiff,nd
    active  poly,p
    active  nfet
    metal1  metal1,m1
    metal1  via
    metal2  metal2,m2
end

contact
    via  metal1 metal2
end

styles
    styletype mos
end

compose
end

connect
    metal1  via
    metal2  via
    poly    nfet
end

cifoutput
end

cifinput
end

drc
end

extract
style probetest
    areacap metal1 1
    areacap metal2 1
    areacap poly 1
    areacap ndiff 1
end
)";

struct Layout {
  std::shared_ptr<const tech::Technology> technology;
  std::shared_ptr<const extract::ExtStyle> style;
  db::CellLibrary library;

  Layout() {
    platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    auto loaded = tech::load_technology_text(kTech, "/t/probetest.tech", fs, tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    technology = loaded.value();
    const tech::RawSection* section = nullptr;
    for (const tech::RawSection& raw : technology->raw_sections()) {
      if (raw.name == "extract") {
        section = &raw;
      }
    }
    REQUIRE(section != nullptr);
    const extract::StyleTable table = extract::load_styles(*section, *technology, sink, compat::CompatSettings{});
    REQUIRE_FALSE(table.names().empty());
    style = table.select(table.names().front());
    REQUIRE(style != nullptr);
  }

  db::CellDef& cell(std::string_view name) {
    auto made = library.create(name, technology->plane_count());
    REQUIRE(made.ok());
    return *made.value();
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  void paint(db::CellDef& cell, std::string_view type_name, db::Rect r) const {
    REQUIRE(db::paint(cell, *technology, r, type(type_name)).ok());
  }

  void label(db::CellDef& cell, std::string text, std::string_view type_name, db::Rect r) const {
    db::Label l;
    l.text = std::move(text);
    l.type = type(type_name);
    l.rect = r;
    cell.labels().push_back(std::move(l));
  }

  static db::CellUse& place(db::CellDef& parent, db::CellDef& child, std::string id, db::Transform t,
                            db::ArrayInfo array = {}) {
    db::recompute_bbox(child);  // the use's bbox comes from the child's
    db::PlaceOptions o;
    o.id = std::move(id);
    o.transform = t;
    o.array = array;
    auto use = db::place(parent, child, o);
    REQUIRE(use.ok());
    db::recompute_bbox(parent);
    return *use.value();
  }

  [[nodiscard]] sim::LayoutSelection select(const db::CellDef& root, std::vector<std::string> path, db::Point p,
                                            bool globals = true) const {
    sim::LayoutSelection s;
    s.root = &root;
    s.point = sim::LayoutPoint{std::move(path), p};
    s.naming.technology = technology.get();
    s.naming.style = style.get();
    s.naming.globals = globals;
    return s;
  }

  [[nodiscard]] std::string name(const sim::LayoutSelection& s) const {
    const auto n = sim::node_name(s);
    INFO((n.ok() ? std::string{} : n.error().code + ": " + n.error().text));
    REQUIRE(n.ok());
    return n.value();
  }
};

}  // namespace hefesto::test::simprobe
