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
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
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

// The synthetic region: a unit cell of kSlots x kSlots slots arrayed with
// gaps until it holds at least `min_rects` rectangles (shared by the
// synthetic and mask workloads).
class SyntheticCells {
 public:
  SyntheticCells(const tech::Technology& tech, const std::vector<tech::TileType>& types, std::uint64_t min_rects) {
    paint_unit(tech, types);
    arrange(tech, min_rects);
  }

  [[nodiscard]] const db::CellDef& top() const { return *top_; }
  [[nodiscard]] std::string description() const {
    return std::to_string(nx_) + " x " + std::to_string(ny_) + " array of a unit cell with " +
           std::to_string(unit_tiles_) + " rectangles on 4 layers";
  }

 private:
  // Each slot holds a rectangle; every third slot an L (a foot strip and a
  // bar above it: two tiles with no side face between them).
  void paint_unit(const tech::Technology& tech, const std::vector<tech::TileType>& types) {
    unit_ = std::make_unique<db::CellDef>("unit", tech.plane_count());
    for (int i = 0; i < kSlots * kSlots; ++i) {
      const db::Coord x = (i % kSlots) * kSlotPitch + 5;
      const db::Coord y = (i / kSlots) * kSlotPitch + 5;
      const db::Coord w = slot_size(i, 0);
      const db::Coord h = slot_size(i, 5);
      const tech::TileType type = types[static_cast<std::size_t>(i) % types.size()];
      (void)db::paint(*unit_, tech, db::Rect{x, y, x + w, y + 4}, type);
      ++unit_tiles_;
      if (i % 3 == 0) {
        (void)db::paint(*unit_, tech, db::Rect{x, y + 4, x + 4, y + h}, type);
        ++unit_tiles_;
      }
    }
    db::recompute_bbox(*unit_);
  }

  void arrange(const tech::Technology& tech, std::uint64_t min_rects) {
    const std::uint64_t copies = (min_rects + unit_tiles_ - 1) / unit_tiles_;
    nx_ = static_cast<std::int32_t>(std::ceil(std::sqrt(static_cast<double>(copies))));
    ny_ = static_cast<std::int32_t>((copies + static_cast<std::uint64_t>(nx_) - 1) / static_cast<std::uint64_t>(nx_));
    top_ = std::make_unique<db::CellDef>("top", tech.plane_count());
    db::PlaceOptions array;
    array.id = "u";
    array.array = db::ArrayInfo{0, nx_ - 1, 0, ny_ - 1, kUnitPitch, kUnitPitch};
    (void)db::place(*top_, *unit_, array);
    db::recompute_bbox(*top_);
  }

  std::unique_ptr<db::CellDef> unit_;
  std::unique_ptr<db::CellDef> top_;
  std::uint64_t unit_tiles_ = 0;
  std::int32_t nx_ = 0;
  std::int32_t ny_ = 0;
};

class SyntheticWorkload final : public Workload {
 public:
  explicit SyntheticWorkload(std::uint64_t min_rects) : tech_(build_tech()), cells_(*tech_, types_, min_rects) {}

  [[nodiscard]] const view3d::LayerStack& stack() const override { return stack_; }
  [[nodiscard]] const db::CellDef& cell() const override { return cells_.top(); }
  [[nodiscard]] std::string description() const override { return "synthetic: " + cells_.description(); }

 private:
  std::shared_ptr<const tech::Technology> build_tech() {
    tech::TechnologyBuilder b;
    b.set_name("bench3d");
    for (const char* plane : {"active", "metal1", "metal2"}) {
      (void)b.add_plane(plane);
    }
    for (const SyntheticLayer& l : kSyntheticLayers) {
      types_.push_back(b.add_type(l.plane, l.type).value());
    }
    std::shared_ptr<const tech::Technology> tech = b.finalize().value();
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
    return tech;
  }

  std::vector<tech::TileType> types_;
  view3d::LayerStack stack_;
  std::shared_ptr<const tech::Technology> tech_;
  SyntheticCells cells_;
};

// --- mask: the synthetic region in mask-layer mode (task 13.3) -------------

// The synthetic layers with an output style: one mask layer per type with
// sky130's `render` values (µm), and NWELL, ndiff grown by 6 mask units
// through a temporary layer (no `render`: flat), so generation has a halo.
// scalefactor 1: mask units are internal units.
constexpr const char* kMaskTech = R"(tech
    format 35
    bench3dm
end
planes
    active,a
    metal1,m1
    metal2,m2
end
types
    active  ndiff
    active  poly
    metal1  metal1
    metal2  metal2
end
styles
    styletype mos
    ndiff 2
    poly 1
    metal1 20
    metal2 21
end
contact
end
compose
end
connect
end
cifoutput
style gdsii
    scalefactor 1
    layer DIFF ndiff
        calma 65 20
    layer POLY poly
        calma 66 20
    layer MET1 metal1
        calma 68 20
    layer MET2 metal2
        calma 69 20
    templayer WELLGROW ndiff
        grow 6
    layer NWELL WELLGROW
        calma 64 20
    render DIFF 1 0.0 0.12
    render POLY 1 0.3262 0.18
    render MET1 1 1.3761 0.36
    render MET2 1 2.7861 0.36
end
cifinput
end
drc
end
extract
end
)";

/// kMaskTech, or null with `error` set.
std::shared_ptr<const tech::Technology> load_mask_tech(std::string& error) {
  platform::LocalFileSystem files;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(kMaskTech, "bench3dm.tech", files, tech::LoadOptions{}, sink);
  if (loaded.ok()) {
    return loaded.value();
  }
  error = "the mask workload's technology does not load: " + loaded.error().text;
  for (const diag::Diagnostic& d : sink.take()) {
    error += "\n  " + d.text;
  }
  return nullptr;
}

class MaskWorkload final : public Workload {
 public:
  MaskWorkload(std::shared_ptr<const tech::Technology> tech, std::uint64_t min_rects)
      : tech_(std::move(tech)), cells_(*tech_, types(*tech_), min_rects) {
    diag::NullSink quiet;
    styles_ = mask::read_output_styles(*tech_, quiet);
    stack_ = view3d::build_mask_stack(*styles_.current_style(), nullptr, true, quiet);
    view3d::MaskSource source;
    source.style = styles_.current_shared();
    source.generation.technology = tech_.get();
    source.area = cells_.top().bbox();
    source_ = std::move(source);
  }

  [[nodiscard]] const view3d::LayerStack& stack() const override { return stack_; }
  [[nodiscard]] const db::CellDef& cell() const override { return cells_.top(); }
  [[nodiscard]] std::string description() const override {
    return "mask: the synthetic " + cells_.description() + ", generated in mask-layer mode (" +
           std::to_string(stack_.size()) + " mask layers)";
  }
  [[nodiscard]] const view3d::MaskSource* mask_source() const override { return &*source_; }

 private:
  static std::vector<tech::TileType> types(const tech::Technology& tech) {
    std::vector<tech::TileType> out;
    for (const SyntheticLayer& l : kSyntheticLayers) {
      out.push_back(*tech.find_type(l.type).id);
    }
    return out;
  }

  std::shared_ptr<const tech::Technology> tech_;
  SyntheticCells cells_;
  mask::OutputStyleSet styles_;
  view3d::LayerStack stack_;
  std::optional<view3d::MaskSource> source_;
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
  if (options.name == "mask") {
    std::shared_ptr<const tech::Technology> tech = load_mask_tech(error);
    return tech ? std::make_unique<MaskWorkload>(std::move(tech), options.rects) : nullptr;
  }
  if (options.name != "array" && options.name != "block") {
    error = "unknown workload " + options.name + " (synthetic, mask, array or block)";
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
