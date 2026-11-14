// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The flat views and the statistics (capability mask-hierarchical-generation
// "Flat region generation", "Hierarchical layer preview", "Generation
// statistics"; cif-format-io "cif see, paint, coverage and statistics
// options"; design D8, D13): the back ends of `cif see`, `cif paint`,
// `cif coverage`, `cif *hier`, `cif *array` and `cif statistics`. The `cif`
// command (CIF) parses the arguments, shows the feedback and prints.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFsee.c (CIFSeeLayer,
// CIFPaintLayer, cifPaintDBFunc, CIFSeeHierLayer, CIFCoverageLayer,
// cifCoverageFunc) and cif/CIFmain.c (CIFNameToMask, CIFPrintStats),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: GEN.

#include <cstdio>
#include <string>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "mask/gen/internal.hpp"

namespace hefesto::mask {

namespace {

using gen_detail::kNoStyle;
using gen_detail::kUnknownLayer;

std::vector<std::string_view> split_names(std::string_view names) {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  while (start <= names.size()) {
    const std::size_t comma = names.find(',', start);
    const std::size_t end = comma == std::string_view::npos ? names.size() : comma;
    out.push_back(names.substr(start, end - start));
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return out;
}

diag::Diagnostic unknown_layer(const OutputStyle& style, std::string_view name) {
  std::string text = "CIF name \"" + std::string{name} + "\" doesn't exist in style \"" + style.name +
                     "\".\nThe valid CIF layer names are: ";
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    text += (i == 0 ? "" : ", ") + style.layers[i].name;
  }
  text += ".";
  return diag::make(kUnknownLayer, std::move(text));
}

// The mask layers a bloat-all on mask layers reads.
void add_bloat_all_layers(const LayerOp& op, std::vector<bool>& depend) {
  if (op.kind != OpKind::bloat_all || !op.bloat || op.bloat->plane >= 0) {
    return;
  }
  for (std::size_t t = 0; t < op.bloat->distance.size() && t < depend.size(); ++t) {
    if (op.bloat->distance[t] > 0) {
      depend[t] = true;
    }
  }
}

std::vector<bool> dependencies(const OutputStyle& style, const std::vector<bool>& layers) {
  std::vector<bool> depend = layers;
  for (std::size_t j = style.layers.size(); j-- > 0;) {
    if (!depend[j]) {
      continue;
    }
    for (const LayerOp& op : style.layers[j].ops) {
      for (const std::size_t l : op.cif_layers) {
        if (l < depend.size()) {
          depend[l] = true;
        }
      }
      add_bloat_all_layers(op, depend);
    }
  }
  return depend;
}

const OutputStyle* current_style(const editor::EditorState& editor) {
  return editor.mask.output_styles.current_style();
}

diag::Diagnostic no_style() {
  return diag::make(kNoStyle, "No CIF output style set!");
}

std::vector<db::TileView> layer_tiles(const LayerPlanes& planes, const std::vector<bool>& layers) {
  std::vector<db::TileView> out;
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (layers[i] && planes[i] != nullptr) {
      const std::vector<db::TileView> tiles = planes[i]->solid_tiles();
      out.insert(out.end(), tiles.begin(), tiles.end());
    }
  }
  return out;
}

}  // namespace

diag::Outcome<LayerSelection> select_layers(const OutputStyle& style, std::string_view names) {
  LayerSelection sel;
  sel.layers.assign(style.layers.size(), false);
  for (const std::string_view name : split_names(names)) {
    bool found = false;
    for (std::size_t i = 0; i < style.layers.size(); ++i) {
      if (style.layers[i].name == name) {
        sel.layers[i] = true;
        found = true;
      }
    }
    if (!found) {
      return unknown_layer(style, name);
    }
  }
  sel.depend = dependencies(style, sel.layers);
  return sel;
}

diag::Outcome<std::vector<db::TileView>> see_tiles(const GenerationSource& source, const OutputStyle& style,
                                                   const db::CellDef& cell, std::string_view layer,
                                                   const db::Rect& area, GenerationContext& ctx) {
  diag::Outcome<LayerSelection> sel = select_layers(style, layer);
  if (!sel.ok()) {
    return sel.error();
  }
  GenerateOptions options;
  options.layers = sel.value().depend;
  const LayerPlanes planes = generate_flat(source, cell, area, style, ctx, options);
  return layer_tiles(planes, sel.value().layers);
}

diag::Outcome<std::vector<db::Rect>> see(editor::EditorState& editor, const db::CellDef& cell, std::string_view layer,
                                         const db::Rect& area, GenerationContext& ctx) {
  const OutputStyle* style = current_style(editor);
  if (style == nullptr) {
    return no_style();
  }
  diag::Outcome<std::vector<db::TileView>> tiles = see_tiles(generation_source(editor), *style, cell, layer, area, ctx);
  if (!tiles.ok()) {
    return tiles.error();
  }
  std::vector<db::Rect> out;
  out.reserve(tiles.value().size());
  for (const db::TileView& t : tiles.value()) {
    out.push_back(t.area);
  }
  return out;
}

diag::Outcome<std::vector<db::TileView>> see_hierarchical(const GenerationSource& source, const OutputStyle& style,
                                                          const db::CellDef& cell, std::string_view layer,
                                                          const db::Rect& area, bool arrays, bool subcells,
                                                          GenerationContext& ctx) {
  diag::Outcome<LayerSelection> sel = select_layers(style, layer);
  if (!sel.ok()) {
    return sel.error();
  }
  LayerPlanes planes(ctx.pool, style.layers.size());
  for (MaskPlane*& p : planes.planes) {
    p = &ctx.pool.acquire();  // CIFClearPlanes
  }
  if (subcells) {
    generate_subcells(source, cell, area, style, ctx, planes);
  }
  if (arrays) {
    generate_arrays(source, cell, area, style, ctx, planes);
  }
  return layer_tiles(planes, sel.value().layers);
}

// --- cif paint -------------------------------------------------------------------

diag::Status paint(editor::EditorState& editor, const db::CellDef& cell, std::string_view layer,
                   const db::Rect& area, tech::TileType type, db::CellDef& target, GenerationContext& ctx) {
  const OutputStyle* style = current_style(editor);
  if (style == nullptr) {
    return no_style();
  }
  diag::Outcome<std::vector<db::TileView>> tiles = see_tiles(generation_source(editor), *style, cell, layer, area, ctx);
  if (!tiles.ok()) {
    return tiles.error();
  }
  const std::int32_t s = gen_detail::scale_of(*style);
  for (const db::TileView& t : tiles.value()) {
    // cifPaintDBFunc: C division truncates toward zero.
    const db::Rect r{t.area.x0 / s, t.area.y0 / s, t.area.x1 / s, t.area.y1 / s};
    if (r.x0 == r.x1 || r.y0 == r.y1) {
      continue;  // degenerate after the rescale
    }
    const std::optional<db::TriangleSpec> tri =
        t.split ? std::optional{db::TriangleSpec{t.diagonal, t.right_side}} : std::nullopt;
    (void)db::paint_type(target, *editor.technology, r, type, tri);
  }
  db::recompute_bbox(target, editor.drc.compat_settings().magic_compat);
  return diag::Done{};
}

// --- cif coverage ----------------------------------------------------------------

float Coverage::fraction() const noexcept {
  return total > 0 ? static_cast<float>(covered) / static_cast<float>(total) : 0.0F;
}

diag::Outcome<Coverage> coverage(const GenerationSource& source, const OutputStyle& style, const db::CellDef& cell,
                                 std::string_view layer, const db::Rect& area, GenerationContext& ctx) {
  diag::Outcome<std::vector<db::TileView>> tiles = see_tiles(source, style, cell, layer, area, ctx);
  if (!tiles.ok()) {
    return tiles.error();
  }
  Coverage out;
  std::int64_t twice = 0;  // Q-MASK-325 OFF: twice the area, halved at the end
  for (const db::TileView& t : tiles.value()) {
    const std::int64_t a = t.area.width() * t.area.height();
    out.covered += a;
    twice += t.split ? a : 2 * a;
    out.bounds = db::include(out.bounds, t.area);
  }
  if (!source.compat.magic_compat) {
    out.covered = twice / 2;
  }
  const std::int64_t s = gen_detail::scale_of(style);
  out.total = area.width() * area.height() * (s * s);
  return out;
}

diag::Outcome<Coverage> coverage(editor::EditorState& editor, const db::CellDef& cell, std::string_view layer,
                                 const db::Rect& area, GenerationContext& ctx) {
  const OutputStyle* style = current_style(editor);
  if (style == nullptr) {
    return no_style();
  }
  return coverage(generation_source(editor), *style, cell, layer, area, ctx);
}

std::string coverage_report(const Coverage& coverage, bool box) {
  char percent[64];
  std::snprintf(percent, sizeof percent, "%1.1f", 100.0 * static_cast<double>(coverage.fraction()));
  return std::string{box ? "Cursor Box" : "Cell"} + " Area = " + std::to_string(coverage.total) +
         " CIF units^2\nLayer Bounding Area = " + std::to_string(coverage.bounds_area()) +
         " CIF units^2\nLayer Total Area = " + std::to_string(coverage.covered) + " CIF units^2\nCoverage in " +
         (box ? "box" : "cell") + " = " + percent + "%";
}

// --- cif statistics ----------------------------------------------------------------

void StatisticsCounters::add(const GenerationStatistics& stats) noexcept {
  recent.flat_tiles += stats.flat_tiles;
  recent.hier_tiles += stats.hier_tiles;
  recent.flat_rects += stats.flat_rects;
  recent.hier_rects += stats.hier_rects;
  recent.interaction_areas += stats.interaction_areas;
}

namespace {

std::string stats_text(const GenerationStatistics& recent, const GenerationStatistics& total) {
  // Magic prints its int counters with %d.
  const auto line = [](const char* label, std::uint64_t r, std::uint64_t t) {
    return std::string{"    "} + label + ": " + std::to_string(static_cast<std::int32_t>(r)) + "/" +
           std::to_string(static_cast<std::int32_t>(t));
  };
  return "CIF statistics (recent/total):\n" + line("Geometrical tile operations", recent.flat_tiles, total.flat_tiles) +
         "\n" + line("Tile operations for hierarchy", recent.hier_tiles, total.hier_tiles) + "\n" +
         line("CIF rectangles output", recent.flat_rects, total.flat_rects) + "\n" +
         line("CIF rectangles due to hierarchical interactions", recent.hier_rects, total.hier_rects);
}

}  // namespace

std::string statistics(StatisticsCounters& counters) {
  counters.total.flat_tiles += counters.recent.flat_tiles;
  counters.total.hier_tiles += counters.recent.hier_tiles;
  counters.total.flat_rects += counters.recent.flat_rects;
  counters.total.hier_rects += counters.recent.hier_rects;
  counters.total.interaction_areas += counters.recent.interaction_areas;
  std::string text = stats_text(counters.recent, counters.total);
  counters.recent = GenerationStatistics{};
  return text;
}

std::string statistics(const GenerationStatistics& stats) {
  return stats_text(stats, stats);
}

}  // namespace hefesto::mask
