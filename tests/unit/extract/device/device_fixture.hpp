// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixtures of the extract/device tests (openspec/changes/m7-extraction
// tasks.md 7.1-7.6): the captured EX cases' technology and layout
// (tests/golden/EXTRACT/cases/EX-*/input), read without the cell reader,
// and a stand-in for EXTNODE's RegionFinder that floods the nodes (the
// substrate node first, from substrate_seeds()) and leaves each tile's node
// index in its plane client slot, as EXTNODE documents. device_text()
// renders a DeviceRecord as Magic's `device` line with the nodes named by
// their first label, or `@x_y` (the reference point) without one, so the
// tests compare against the captured `.ext` lines.
//
// Owner: EXTDEV.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/device/devices.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::test::dev {

namespace extract = hefesto::extract;
namespace tech = hefesto::tech;
namespace db = hefesto::db;
namespace diag = hefesto::diag;

inline std::string read_text(const std::string& path) {
  std::ifstream in(path);
  REQUIRE(in.good());
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

inline std::vector<std::string> one_line(const std::string& line) {
  return {line};
}

inline std::string case_dir(const std::string& id) {
  return std::string(HEFESTO_SOURCE_DIR) + "/tests/golden/EXTRACT/cases/" + id + "/input/";
}

inline std::shared_ptr<const tech::Technology> load_tech_text(const std::string& text) {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/exttest.tech", fs, tech::LoadOptions{}, sink);
  for (const auto& d : sink.snapshot()) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

// The tech-load style, micron-converted with the first cifoutput style (the
// hook's order at a 1:1 grid).
inline extract::ExtStyle load_style(const tech::Technology& t, bool magic_compat) {
  const tech::RawSection* section = nullptr;
  for (const tech::RawSection& raw : t.raw_sections()) {
    if (raw.name == "extract") {
      section = &raw;
    }
  }
  REQUIRE(section != nullptr);
  diag::CollectingSink sink;
  compat::CompatSettings c;
  c.magic_compat = magic_compat;
  const extract::StyleTable table = extract::load_styles(*section, t, sink, c);
  REQUIRE_FALSE(table.names().empty());
  const auto style = table.select(table.names().front());
  REQUIRE(style != nullptr);
  const std::optional<mask::OutputStyle> cif = mask::read_first_output_style(t);
  return style->micron_convert(cif ? &*cif : nullptr);
}

inline std::vector<std::string> words(const std::string& line) {
  std::istringstream is(line);
  std::vector<std::string> out;
  for (std::string w; is >> w;) {
    out.push_back(w);
  }
  return out;
}

inline tech::TileType type_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

// The paint, labels and string properties of a `.mag` text.
inline void read_mag(db::CellDef& cell, const tech::Technology& t, const std::string& text) {
  std::istringstream is(text);
  std::string section;
  for (std::string line; std::getline(is, line);) {
    const std::vector<std::string> w = words(line);
    if (w.size() >= 3 && w[0] == "<<") {
      section = w[1];
    } else if (!w.empty() && w[0] == "rect" && w.size() == 5) {
      const db::Rect r{std::stoi(w[1]), std::stoi(w[2]), std::stoi(w[3]), std::stoi(w[4])};
      REQUIRE(db::paint(cell, t, r, type_of(t, section)).ok());
    } else if (!w.empty() && w[0] == "rlabel" && w.size() >= 8) {
      db::Label l;
      l.type = type_of(t, w[1]);
      l.rect = {std::stoi(w[2]), std::stoi(w[3]), std::stoi(w[4]), std::stoi(w[5])};
      l.text = w[7];
      cell.labels().push_back(l);
    } else if (!w.empty() && w[0] == "port" && !cell.labels().empty()) {
      cell.labels().back().port_directions = 0x0F;
    } else if (!w.empty() && w[0] == "string" && w.size() >= 3) {
      cell.properties().put(w[1], line.substr(line.find(w[1]) + w[1].size() + 1));
    }
  }
}

/// The nodes of a cell and the client slots that point at them.
struct Nodes {
  extract::NodeRegionList list;
  std::optional<std::size_t> substrate;  // the substrate node, when the style has one
};

class NodeFlood {
 public:
  NodeFlood(const db::CellDef& cell, const tech::Technology& t, const extract::ExtStyle& s)
      : cell_(cell), tech_(t), style_(s) {}

  Nodes run() {
    const extract::SubstrateSeeds seeds = extract::substrate_seeds(cell_, tech_, style_);
    if (!seeds.tiles.empty()) {
      out_.substrate = flood(seeds.tiles);
    } else if (seeds.tileless) {
      extract::NodeRegion n;
      n.reference = {extract::kTilelessSubstrateCoord, extract::kTilelessSubstrateCoord};
      n.plane = *style_.substrate.plane;
      n.type = tech::kSpace;
      out_.substrate = out_.list.size();
      out_.list.push_back(n);
    }
    for (std::size_t p = tech::kFirstUserPlane; p < cell_.plane_count(); ++p) {
      const auto pnum = static_cast<tech::PlaneId>(p);
      tech::TypeMask active = ~style_.non_electrical;
      active.clear_type(tech::kSpace);
      (void)cell_.plane(pnum).enumerate(db::whole_plane(), active, [&](const db::TileView& t) {
        if (cell_.plane(pnum).client(t) == db::kClientDefault) {
          (void)flood({{pnum, t}});
        }
        return true;
      });
    }
    attach_labels();
    return out_;
  }

 private:
  using PT = std::pair<tech::PlaneId, db::TileView>;

  [[nodiscard]] const tech::TypeMask& conn(tech::TileType t) const {
    static const tech::TypeMask none;
    return t < style_.node_conn.size() ? style_.node_conn[t] : none;
  }

  void push_if(std::vector<PT>& stack, tech::PlaneId p, const db::TileView& v, const tech::TypeMask& mask) const {
    if (v.type != tech::kSpace && mask.test(v.type) && cell_.plane(p).client(v) == db::kClientDefault) {
      stack.emplace_back(p, v);
    }
  }

  void neighbours(std::vector<PT>& stack, const PT& at) const {
    const tech::TypeMask& mask = conn(at.second.type);
    std::vector<db::TileView> out;
    const db::Rect& a = at.second.area;
    for (const db::Side s : {db::Side::top, db::Side::left, db::Side::bottom, db::Side::right}) {
      const bool vertical = s == db::Side::left || s == db::Side::right;
      cell_.plane(at.first).neighbors(at.second, s, vertical ? a.y0 : a.x0, vertical ? a.y1 : a.x1, out);
      for (const db::TileView& v : out) {
        push_if(stack, at.first, v, mask);
      }
    }
    const tech::PlaneMask planes = tech_.contact_planes(at.second.type) | tech_.cross_planes(at.second.type);
    for (std::size_t p = tech::kFirstUserPlane; p < cell_.plane_count(); ++p) {
      const auto pnum = static_cast<tech::PlaneId>(p);
      if (pnum == at.first || (planes & tech::plane_bit(pnum)) == 0) {
        continue;
      }
      (void)cell_.plane(pnum).enumerate(db::expand(a, 1), mask, [&](const db::TileView& v) {
        const bool corner = !db::overlap(v.area, a) && (v.area.x1 == a.x0 || v.area.x0 == a.x1) &&
                            (v.area.y1 == a.y0 || v.area.y0 == a.y1);
        if (!corner) {
          push_if(stack, pnum, v, mask);
        }
        return true;
      });
    }
  }

  std::size_t flood(const std::vector<PT>& seeds) {
    const std::size_t index = out_.list.size();
    out_.list.emplace_back();
    extract::NodeRegion& n = out_.list.back();
    bool first = true;
    std::vector<PT> stack = seeds;
    while (!stack.empty()) {
      const PT at = stack.back();
      stack.pop_back();
      if (cell_.plane(at.first).client(at.second) != db::kClientDefault) {
        continue;
      }
      cell_.plane(at.first).set_client(at.second, index);
      const db::Point ll{at.second.area.x0, at.second.area.y0};
      if (first || at.first < n.plane || (at.first == n.plane && (ll.x < n.reference.x ||
                                                                  (ll.x == n.reference.x && ll.y < n.reference.y)))) {
        n.plane = at.first;
        n.reference = ll;
        n.type = at.second.type;
        first = false;
      }
      neighbours(stack, at);
    }
    return index;
  }

  void attach_labels() {
    for (const db::Label& l : cell_.labels()) {
      const auto home = tech_.home_plane(l.type);
      if (!home) {
        continue;
      }
      const db::TileView t = cell_.plane(*home).tile_at({l.rect.x0, l.rect.y0});
      const std::uintptr_t v = cell_.plane(*home).client(t);
      if (v != db::kClientDefault && v < out_.list.size()) {
        out_.list[v].labels.push_back(extract::LabelRegion{l.text, {l.rect.x0, l.rect.y0}, l.type, l.is_port()});
      }
    }
  }

  const db::CellDef& cell_;
  const tech::Technology& tech_;
  const extract::ExtStyle& style_;
  Nodes out_;
};

/// One EX case loaded and extracted at the device level.
struct Case {
  std::shared_ptr<const tech::Technology> tech;
  extract::ExtStyle style;
  std::unique_ptr<db::CellDef> cell;
  Nodes nodes;
  std::vector<extract::DeviceRegionTiles> regions;
  extract::DeviceResult result;

  ~Case() {
    if (cell) {
      for (std::size_t p = 0; p < cell->plane_count(); ++p) {
        cell->plane(static_cast<tech::PlaneId>(p)).reset_clients(db::whole_plane());
      }
    }
  }

  /// A node's first label text, or `@x_y` of its reference point.
  [[nodiscard]] std::string name(std::size_t i) const {
    REQUIRE(i < nodes.list.size());
    const extract::NodeRegion& n = nodes.list[i];
    for (const auto& l : n.labels) {
      const char last = l.text.back();
      if (last != '$' && last != '^' && last != '@') {
        return l.text;
      }
    }
    return "@" + std::to_string(n.reference.x) + "_" + std::to_string(n.reference.y);
  }

  [[nodiscard]] std::string name(const std::optional<std::size_t>& i) const {
    return i ? name(*i) : std::string("(none)");
  }

  [[nodiscard]] std::string terminal(const extract::DeviceTerminal& t, bool gate) const {
    std::string s = " \"" + name(t.node) + "\" " + std::to_string(t.length);
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

  /// Magic's `device` (or `fet`) line of a record.
  [[nodiscard]] std::string device_text(const extract::DeviceRecord& d) const {
    static const char* const kClasses[] = {"fet",     "mosfet",  "asymmetric", "bjt",     "devres",  "devcap",
                                           "devcaprev", "vsource", "diode",     "pdiode",  "ndiode",  "subckt",
                                           "rsubckt", "msubckt", "csubckt",    "dsubckt", "veriloga"};
    const extract::DevicePattern& p = style.devices.at(d.pattern);
    const bool fet = p.device_class == extract::DeviceClass::fet;
    std::string s = fet ? "fet " : std::string("device ") + kClasses[static_cast<int>(p.device_class)] + " ";
    s += p.model + " " + std::to_string(d.area.x0) + " " + std::to_string(d.area.y0) + " " +
         std::to_string(d.area.x1) + " " + std::to_string(d.area.y1);
    for (const std::string& f : d.fields) {
      s += " " + f;
    }
    for (const std::string& f : d.params) {
      s += " " + f;
    }
    if (d.substrate_field == extract::SubstrateField::node) {
      s += " \"" + name(*d.substrate) + "\"";
    } else if (d.substrate_field == extract::SubstrateField::name) {
      s += " \"" + d.substrate_name + "\"";
    }
    s += terminal(d.gate, true);
    for (const auto& t : d.terminals) {
      s += terminal(t, false);
    }
    return s;
  }

  [[nodiscard]] std::vector<std::string> lines() const {
    std::vector<std::string> out;
    for (const auto& d : result.devices) {
      out.push_back(device_text(d));
    }
    return out;
  }

  [[nodiscard]] std::vector<std::string> codes() const {
    std::vector<std::string> out;
    for (const auto& m : result.messages) {
      out.push_back(m.code);
    }
    return out;
  }
};

struct Options {
  bool magic_compat = true;
  bool warn_fets = true;
  bool warn_any = true;
  std::string cell = "ex";
  std::function<std::optional<std::string>(std::string_view)> variable;
};

/// Loads EX case `id`'s technology and cell (or the given texts) and runs
/// the device pass.
inline std::unique_ptr<Case> extract_texts(const std::string& tech_text, const std::string& mag_text,
                                           const Options& o = {}) {
  auto c = std::make_unique<Case>();
  c->tech = load_tech_text(tech_text);
  c->style = load_style(*c->tech, o.magic_compat);
  c->cell = std::make_unique<db::CellDef>(o.cell, c->tech->plane_count());
  read_mag(*c->cell, *c->tech, mag_text);
  db::recompute_bbox(*c->cell);
  c->regions = extract::find_device_regions(*c->cell, *c->tech, c->style);
  c->nodes = NodeFlood(*c->cell, *c->tech, c->style).run();
  extract::DeviceInputs in;
  in.cell = c->cell.get();
  in.technology = c->tech.get();
  in.style = &c->style;
  in.nodes = &c->nodes.list;
  in.substrate_node = c->nodes.substrate;
  in.compat.magic_compat = o.magic_compat;
  in.warn_fets = o.warn_fets;
  in.warn_any = o.warn_any;
  in.variable = o.variable;
  c->result = extract::find_devices(in, c->regions);
  for (const auto& m : c->result.messages) {
    UNSCOPED_INFO(m.code << ": " << m.text);
  }
  return c;
}

inline std::unique_ptr<Case> extract_case(const std::string& id, const Options& o = {}) {
  return extract_texts(read_text(case_dir(id) + "exttest.tech"), read_text(case_dir(id) + o.cell + ".mag"), o);
}

/// The base technology of the EX cases with another `extract` section body.
inline std::string tech_with_extract(const std::string& extract_lines) {
  const std::string base = read_text(case_dir("EX-026") + "exttest.tech");
  const std::size_t at = base.find("\nextract\n");
  REQUIRE(at != std::string::npos);
  const std::size_t body = base.find("    device ", at);
  REQUIRE(body != std::string::npos);
  return base.substr(0, body) + extract_lines + "end\n";
}

}  // namespace hefesto::test::dev
