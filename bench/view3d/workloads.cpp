// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The workloads of hefesto-bench-view3d; see workloads.hpp.

#include "workloads.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::bench_view3d {

namespace {

// --- synthetic ---------------------------------------------------------------

constexpr db::Coord kSlotPitch = 40;   // 10 x 10 slots per unit cell
constexpr int kSlots = 10;
constexpr db::Coord kUnitPitch = kSlots * kSlotPitch;  // slots never reach the unit's edge: gaps between units

struct SyntheticLayer {
  const char* plane;
  const char* type;
  float height;     // internal units (1 unit = 10 nm), about sky130's
  float thickness;
};

constexpr SyntheticLayer kSyntheticLayers[] = {
    {"active", "ndiff", 0.0F, 12.0F},
    {"active", "poly", 33.0F, 18.0F},
    {"metal1", "metal1", 137.61F, 36.0F},
    {"metal2", "metal2", 276.31F, 36.0F},
};

/// A small deterministic hash of a slot index (sizes vary without a RNG).
constexpr db::Coord slot_size(int index, int salt) {
  return 10 + static_cast<db::Coord>((static_cast<unsigned>(index) * 2654435761U >> (8 + salt)) % 21U);
}

class SyntheticWorkload final : public Workload {
 public:
  explicit SyntheticWorkload(std::uint64_t min_rects) {
    build_tech();
    paint_unit();
    arrange(min_rects);
  }

  [[nodiscard]] const view3d::LayerStack& stack() const override { return stack_; }
  [[nodiscard]] const db::CellDef& cell() const override { return *top_; }
  [[nodiscard]] std::string description() const override {
    return "synthetic: " + std::to_string(nx_) + " x " + std::to_string(ny_) + " array of a unit cell with " +
           std::to_string(unit_tiles_) + " rectangles on 4 layers";
  }

 private:
  void build_tech() {
    tech::TechnologyBuilder b;
    b.set_name("bench3d");
    for (const char* plane : {"active", "metal1", "metal2"}) {
      (void)b.add_plane(plane);
    }
    for (const SyntheticLayer& l : kSyntheticLayers) {
      types_.push_back(b.add_type(l.plane, l.type).value());
    }
    tech_ = b.finalize().value();
    std::vector<view3d::StackLayer> layers;
    std::vector<view3d::DrawHeight> heights(tech::kMaxTileTypes);
    for (std::size_t i = 0; i < types_.size(); ++i) {
      view3d::StackLayer layer;
      layer.name = kSyntheticLayers[i].type;
      layer.type = types_[i];
      layer.style = i;
      layer.height = kSyntheticLayers[i].height;
      layer.thickness = kSyntheticLayers[i].thickness;
      layer.source = view3d::ValueSource::extract;
      heights[types_[i]] = view3d::DrawHeight{layer.height, layer.thickness};
      layers.push_back(std::move(layer));
    }
    stack_ = view3d::LayerStack{std::move(layers), std::move(heights), true, 0.01};
  }

  // Each slot holds a rectangle; every third slot an L (a foot strip and a
  // bar above it: two tiles with no side face between them).
  void paint_unit() {
    unit_ = std::make_unique<db::CellDef>("unit", tech_->plane_count());
    for (int i = 0; i < kSlots * kSlots; ++i) {
      const db::Coord x = (i % kSlots) * kSlotPitch + 5;
      const db::Coord y = (i / kSlots) * kSlotPitch + 5;
      const db::Coord w = slot_size(i, 0);
      const db::Coord h = slot_size(i, 5);
      const tech::TileType type = types_[static_cast<std::size_t>(i) % types_.size()];
      (void)db::paint(*unit_, *tech_, db::Rect{x, y, x + w, y + 4}, type);
      ++unit_tiles_;
      if (i % 3 == 0) {
        (void)db::paint(*unit_, *tech_, db::Rect{x, y + 4, x + 4, y + h}, type);
        ++unit_tiles_;
      }
    }
    db::recompute_bbox(*unit_);
  }

  void arrange(std::uint64_t min_rects) {
    const std::uint64_t copies = (min_rects + unit_tiles_ - 1) / unit_tiles_;
    nx_ = static_cast<std::int32_t>(std::ceil(std::sqrt(static_cast<double>(copies))));
    ny_ = static_cast<std::int32_t>((copies + static_cast<std::uint64_t>(nx_) - 1) / static_cast<std::uint64_t>(nx_));
    top_ = std::make_unique<db::CellDef>("top", tech_->plane_count());
    db::PlaceOptions array;
    array.id = "u";
    array.array = db::ArrayInfo{0, nx_ - 1, 0, ny_ - 1, kUnitPitch, kUnitPitch};
    (void)db::place(*top_, *unit_, array);
    db::recompute_bbox(*top_);
  }

  std::shared_ptr<const tech::Technology> tech_;
  std::vector<tech::TileType> types_;
  view3d::LayerStack stack_;
  std::unique_ptr<db::CellDef> unit_;
  std::unique_ptr<db::CellDef> top_;
  std::uint64_t unit_tiles_ = 0;
  std::int32_t nx_ = 0;
  std::int32_t ny_ = 0;
};

// --- sky130 reference layouts -----------------------------------------------

constexpr const char* kLibrary = "sky130_fd_sc_hd";
constexpr const char* kArrayCell = "sky130_fd_sc_hd__dfxtp_1";
constexpr db::Coord kRowWidth = 40000;  // internal units per row of the block (bench/drc/drc_bench.py)

/// The session a reference workload runs in: technology, cells, styles.
class Sky130Workload final : public Workload {
 public:
  explicit Sky130Workload(const WorkloadOptions& options)
      : services_{platform::PortableOptions{{}, {}, false, true}},
        script_{project_},
        dispatcher_{registry_, services_.view(), project_, script_},
        options_{options} {}

  bool load(std::string& error) {
    if (!start_session(error) || !run("load top\n", error)) {
      return false;
    }
    top_ = editor_.cells.find("top");
    const bool placed = options_.name == "array" ? place_array(error) : place_block(error);
    if (!placed) {
      return false;
    }
    db::recompute_bbox(*top_);
    build_stack();
    if (stack_.size() == 0) {
      error = "no display styles loaded (pass Magic's scmos/ and graphics/ directories with --style-dir)";
      return false;
    }
    return true;
  }

  [[nodiscard]] const view3d::LayerStack& stack() const override { return stack_; }
  [[nodiscard]] const db::CellDef& cell() const override { return *top_; }
  [[nodiscard]] std::string description() const override { return description_; }

 private:
  bool start_session(std::string& error) {
    if (!cmd::register_all_commands(registry_)) {
      error = "cannot register the commands";
      return false;
    }
    dispatcher_.set_editor(&editor_);
    editor_.style_dirs = {options_.style_dirs.begin(), options_.style_dirs.end()};
    interp_ = &script::open_project_interpreter(editor_, {});
    if (!script::bind_registry(*interp_, registry_, dispatcher_).ok()) {
      error = "cannot bind the commands";
      return false;
    }
    const std::filesystem::path tech = options_.pdk_root / "libs.tech/magic/sky130A.tech";
    return run("tech load " + tech.string() + " -noprompt\nsnap internal\nscalegrid 1 2\ndrc off\n", error);
  }

  bool run(const std::string& text, std::string& error) {
    const cmd::CommandResult r = interp_->evaluate(text, cmd::Origin::cli);
    if (!r.ok()) {
      error = "command failed: " + text;
    }
    return r.ok();
  }

  [[nodiscard]] std::filesystem::path mag_dir() const { return options_.pdk_root / "libs.ref" / kLibrary / "mag"; }

  db::CellDef* load_cell(const std::filesystem::path& mag, std::string& error) {
    if (!run("load " + mag.string() + "\n", error)) {
      return nullptr;
    }
    db::CellDef* def = editor_.cells.find(mag.stem().string());
    if (def == nullptr) {
      error = "cell not loaded: " + mag.stem().string();
    }
    return def;
  }

  bool place_array(std::string& error) {
    db::CellDef* leaf = load_cell(mag_dir() / (std::string{kArrayCell} + ".mag"), error);
    if (leaf == nullptr) {
      return false;
    }
    const db::Rect b = leaf->bbox();
    db::PlaceOptions array;
    array.id = "x";
    array.transform = db::translation(-b.x0, -b.y0);
    array.array = db::ArrayInfo{0, 99, 0, 99, b.x1 - b.x0, b.y1 - b.y0};
    description_ = "array: 100 x 100 " + std::string{kArrayCell} + " under sky130A";
    return db::place(*top_, *leaf, array).ok();
  }

  bool place_block(std::string& error) {
    std::vector<std::filesystem::path> mags;
    for (const auto& entry : std::filesystem::directory_iterator(mag_dir())) {
      if (entry.path().extension() == ".mag") {
        mags.push_back(entry.path());
      }
    }
    std::sort(mags.begin(), mags.end());
    Row row;
    for (const auto& mag : mags) {
      db::CellDef* def = load_cell(mag, error);
      if (def == nullptr || !place_in_row(*def, row)) {
        return false;
      }
    }
    description_ = "block: " + std::to_string(row.cells) + " " + kLibrary + " cells in rows, under sky130A";
    return true;
  }

  struct Row {
    db::Coord x = 0;
    db::Coord y = 0;
    db::Coord height = 0;
    int cells = 0;
  };

  bool place_in_row(db::CellDef& def, Row& row) {
    const db::Rect b = def.bbox();
    if (b.is_null()) {
      return true;  // nothing to draw
    }
    if (row.x > 0 && row.x + (b.x1 - b.x0) > kRowWidth) {
      row = Row{0, row.y + row.height, 0, row.cells};
    }
    db::PlaceOptions use;
    use.id = "c" + std::to_string(row.cells);
    use.transform = db::translation(row.x - b.x0, row.y - b.y0);
    row.x += b.x1 - b.x0;
    row.height = std::max(row.height, b.y1 - b.y0);
    ++row.cells;
    return db::place(*top_, def, use).ok();
  }

  void build_stack() {
    diag::NullSink quiet;
    const auto heights = extract::HeightView::from_technology(*editor_.technology, quiet);
    display::TypeStyleMap none;
    const display::TypeStyleMap& styles = editor_.styles ? editor_.styles->types : none;
    stack_ = view3d::build_layout_stack(*editor_.technology, heights, styles, true, 0.005);
    description_ += ", " + std::to_string(stack_.size()) + " (type, style) layers";
  }

  platform::PortableServices services_;
  settings::ProjectSettings project_;
  cmd::ScriptSession script_;
  cmd::CommandRegistry registry_;
  editor::EditorState editor_;
  cmd::Dispatcher dispatcher_;
  script::Interpreter* interp_ = nullptr;
  WorkloadOptions options_;
  db::CellDef* top_ = nullptr;
  view3d::LayerStack stack_;
  std::string description_;
};

}  // namespace

std::unique_ptr<Workload> load_workload(const WorkloadOptions& options, std::string& error) {
  if (options.name == "synthetic") {
    return std::make_unique<SyntheticWorkload>(options.rects);
  }
  if (options.name != "array" && options.name != "block") {
    error = "unknown workload " + options.name + " (synthetic, array or block)";
    return nullptr;
  }
  if (!std::filesystem::exists(options.pdk_root / "libs.tech/magic/sky130A.tech")) {
    error = "sky130A not found under --pdk-root " + options.pdk_root.string();
    return nullptr;
  }
  auto workload = std::make_unique<Sky130Workload>(options);
  if (!workload->load(error)) {
    return nullptr;
  }
  return workload;
}

}  // namespace hefesto::bench_view3d
