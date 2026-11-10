// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the DRC engine unit tests: a session with the
// tests/data/drc/engine/engine.tech technology, cell and use builders,
// rule sets built with drc::RuleSetBuilder, plane inspection and a
// recording fake kernel (drc::ScopedKernel) whose errors are a simple,
// deterministic function of the checked cell: every tile of the
// `report` types overlapping the clip area (why 6), and every error_s tile
// the DRC paint table left in a flattened cell (why 2, as the real
// kernel's subcell-error report).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/drc/engine/kernel_hook.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::drc::test {

inline std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

inline std::shared_ptr<const tech::Technology> engine_tech() {
  static const std::shared_ptr<const tech::Technology> tech = [] {
    platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    auto loaded = tech::load_technology_text(
        read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "engine.tech"), "/t/engine.tech", fs,
        tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    return loaded.value();
  }();
  return tech;
}

// --- the recording kernel --------------------------------------------------

struct KernelCall {
  const db::CellDef* cell = nullptr;
  bool flattened = false;  // the engine's internal yank cell
  db::Rect check;          // K
  db::Rect clip;           // L
};

struct KernelLog {
  std::mutex mutex;
  std::vector<KernelCall> calls;
  tech::TypeMask report;  // tile types reported as errors (why 6)

  static KernelLog& get() {
    static KernelLog log;
    return log;
  }
};

inline const Rule& fake_rule(WhyIndex why) {
  static const auto rules = [] {
    std::vector<Rule> out(8);
    for (std::size_t i = 0; i < out.size(); ++i) {
      out[i].why = static_cast<WhyIndex>(i);
    }
    return out;
  }();
  return rules.at(why);
}

inline CheckOutcome recording_kernel(const CheckInput& in, const db::Rect& check, const db::Rect& clip,
                                     ErrorSink& sink) {
  KernelLog& log = KernelLog::get();
  tech::TypeMask report;
  {
    const std::lock_guard lock(log.mutex);
    log.calls.push_back(KernelCall{in.cell, in.cell->flags().internal, check, clip});
    report = log.report;
  }
  const tech::TypeMask subcell_error = tech::TypeMask::of(tech::kErrorS);
  for (std::size_t p = tech::kFirstUserPlane; p < in.cell->plane_count(); ++p) {
    in.cell->plane(static_cast<tech::PlaneId>(p)).enumerate(check, report | subcell_error, [&](const db::TileView& t) {
      const db::Rect r{std::max(t.area.x0, clip.x0), std::max(t.area.y0, clip.y0), std::min(t.area.x1, clip.x1),
                       std::min(t.area.y1, clip.y1)};
      if (!r.is_null()) {
        sink.report(r, fake_rule(t.type == tech::kErrorS ? why::kOverlap : why::kFirstUser));
      }
      return !in.cancel.requested();
    });
  }
  return in.cancel.requested() ? CheckOutcome::cancelled : CheckOutcome::completed;
}

/// Installs the recording kernel with an empty log for the test's lifetime.
class Recorder {
 public:
  explicit Recorder(tech::TypeMask report = {}) : scoped_(&recording_kernel) {
    KernelLog& log = KernelLog::get();
    const std::lock_guard lock(log.mutex);
    log.calls.clear();
    log.report = report;
  }
  [[nodiscard]] std::vector<KernelCall> calls() const {
    KernelLog& log = KernelLog::get();
    const std::lock_guard lock(log.mutex);
    return log.calls;
  }
  void clear() const {
    KernelLog& log = KernelLog::get();
    const std::lock_guard lock(log.mutex);
    log.calls.clear();
  }

 private:
  ScopedKernel scoped_;
};

// --- the session -------------------------------------------------------------

struct Session {
  editor::EditorState ed;
  std::shared_ptr<const tech::Technology> tech = engine_tech();

  Session() {
    ed.technology = tech;
    set_rules(rules(0, 1000).build());
  }

  /// A builder with the given halo and step size.
  [[nodiscard]] RuleSetBuilder rules(std::int32_t halo, std::int32_t step) const {
    RuleSetBuilder b(tech->type_count());
    b.halo(Halo{halo, halo, 0});
    b.step_size(step);
    return b;
  }
  void set_rules(std::shared_ptr<const RuleSet> r) { ed.drc.rules = std::move(r); }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = tech->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  db::CellDef& cell(std::string_view name) {
    auto made = ed.cells.create(name, tech->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    return *made.value();
  }

  void paint(db::CellDef& def, std::string_view type_name, const db::Rect& r) {
    REQUIRE(db::paint_type(def, *tech, r, type(type_name)).ok());
    db::recompute_bbox(def);
  }

  db::CellUse& place(db::CellDef& parent, db::CellDef& child, const db::Transform& t, db::ArrayInfo array = {}) {
    db::PlaceOptions o;
    o.transform = t;
    o.array = array;
    auto u = db::place(parent, child, o);
    REQUIRE(u.ok());
    db::recompute_bbox(parent);
    return *u.value();
  }
};

/// The tiles of `types` on `plane`, sorted by (y0, x0).
inline std::vector<ErrorTile> tiles(const db::CellDef& def, tech::PlaneId plane, const tech::TypeMask& types) {
  std::vector<ErrorTile> out;
  def.plane(plane).enumerate(db::whole_plane(), types, [&](const db::TileView& t) {
    out.push_back(ErrorTile{t.area, t.type});
    return true;
  });
  std::sort(out.begin(), out.end(), [](const ErrorTile& a, const ErrorTile& b) {
    return std::tie(a.area.y0, a.area.x0, a.type) < std::tie(b.area.y0, b.area.x0, b.type);
  });
  return out;
}

inline std::vector<ErrorTile> errors(const db::CellDef& def) {
  return tiles(def, tech::kPlaneDrcError, ~tech::TypeMask::of(tech::kSpace));
}

inline std::vector<ErrorTile> checks(const db::CellDef& def) {
  return tiles(def, tech::kPlaneDrcCheck, ~tech::TypeMask::of(tech::kSpace));
}

/// Paints `type` over `r` of the check plane (as a `.mag` read would).
inline void paint_check(db::CellDef& def, const tech::Technology& tech, const db::Rect& r,
                        tech::TileType type = tech::kCheckPaint) {
  REQUIRE(def.plane(tech::kPlaneDrcCheck).paint(r, db::paint_row(tech, tech::kPlaneDrcCheck, type)).ok());
}

}  // namespace hefesto::drc::test
