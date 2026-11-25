// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// glTF and STL export (P2; hefesto-3d-visualization "3D export";
// openspec/changes/m10-3d-view/design.md D14, J2 decided (a)).
//
// `export3d <file> [-format gltf|glb|stl] [-merged] [-zscale k]
// [-visible|-all]` writes the current mesh model in physical µm, without
// the view's z exaggeration unless `-zscale` is given (k recorded in
// `asset.extras.hefesto.zscale`). glTF: one mesh node per exported layer in
// layer order, one material per layer named by the display style's long
// name; written by cgltf under the C locale. STL: binary, fixed 80-byte
// header, triangles in mesh order. The same model writes the same bytes on
// every platform. Mask layers without a physical height are left out with
// VIEW3D-STACK-001.
//
// Exported layers (export_layers): the stack entries that are not excluded
// (ExportOptions::excluded), visible (`-visible`, the default: not flagged
// in ExportOptions::hidden; `-all`: every entry), drawn (a solid or
// stippled display style, as the renderer draws; every entry without a
// style set), that have a physical height and
// geometry in the mesh. A type in two display styles (Q-MISC-77) is the
// same geometry twice: it is exported once, as its last drawn entry (the
// style that wins on coplanar faces in the view).
//
// Geometry: x and y relative to the model origin (the region's lower-left
// corner), computed from the integer coordinates in double and rounded once
// to float; a layer's slab runs from z = h to z = h + t (higher layers up,
// as the displayed scene). Faces: a rectangle gives its top and bottom as
// two triangles each with the diagonal from its lower-left corner; a side
// face is a quad of two triangles; a split-tile triangle its top and
// bottom. Every face has its own vertices and its outward normal; triangles
// are counter-clockwise seen from outside. glTF is +Y up, so a point
// (x, y, z) is written as (x, z, -y) and the root node's translation holds
// the origin; STL keeps (x, y, z), relative to the origin.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); EXPORT fills in
// src/core/src/view3d/export/ (design D3, tasks 11.3, 11.4).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

enum class ExportFormat : std::uint8_t { gltf, glb, stl };

struct ExportOptions {
  ExportFormat format = ExportFormat::gltf;
  bool merged = false;               // STL: one solid for every layer (the command's file split)
  std::optional<double> zscale;      // `-zscale k`; nullopt = physical heights (k = 1)
  bool visible_only = true;          // `-visible` (default) or `-all`
  const display::StyleSet* styles = nullptr;  // material names and colors; nullptr = layer names, grey
  /// Per stack layer: hidden in the window (Scene::hidden); `-visible`
  /// leaves these out. Missing entries are visible.
  std::vector<bool> hidden;
  /// Per stack layer: never exported, whatever `visible_only` (`-all`
  /// included): with magic-compat OFF the temporary mask layers, which are
  /// no masks (excluded_layers). Missing entries are exported.
  std::vector<bool> excluded;
  /// Mask-layer mode: the output style the stack was built from, so that
  /// VIEW3D-STACK-001 names only its non-temporary layers (as cross-sections
  /// do); nullptr names every mask layer without a `render` height.
  const mask::OutputStyle* output_style = nullptr;
  /// µm per mesh x/y unit; 0 = LayerStack::microns_per_unit().
  double xy_unit_um = 0.0;
  /// `.gltf`: the sink of the `.bin` buffer and the URI the JSON names it
  /// by; with no sink the buffer is embedded as a base64 data URI.
  gds::ByteSink* bin_out = nullptr;
  std::string bin_uri;
  /// STL: write only this stack layer (one file per layer); nullopt writes
  /// every exported layer into one solid.
  std::optional<LayerId> stl_layer;
};

/// One exported layer, in layer order.
struct ExportLayer {
  LayerId layer = 0;
  std::string name;                  // the stack entry's name (tile type or mask layer)
  std::string material;              // the display style's long name, else `name`
  std::array<float, 3> srgb{0.5F, 0.5F, 0.5F};  // the style's color (sRGB); grey without styles
  double z_bottom_um = 0.0;          // h * k
  double z_top_um = 0.0;             // (h + t) * k
};

/// The layers `write_gltf` and `write_stl` export for these options, in
/// layer order. Emits VIEW3D-STACK-001 (mask-layer mode) or VIEW3D-EXPORT-003
/// (layout-layer layers with geometry but no physical height) to `sink`.
[[nodiscard]] std::vector<ExportLayer> export_layers(const MeshModel& mesh, const LayerStack& stack,
                                                     const ExportOptions& options, diag::DiagnosticSink& sink);

/// Writes `mesh` as glTF (`options.format` gltf: the JSON to `out`) or GLB
/// (`glb`) to `out`; warnings go to `sink`.
[[nodiscard]] diag::Status write_gltf(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
                                      gds::ByteSink& out, diag::DiagnosticSink& sink);

/// Writes `mesh` as binary STL to `out`; warnings go to `sink`.
[[nodiscard]] diag::Status write_stl(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
                                     gds::ByteSink& out, diag::DiagnosticSink& sink);

}  // namespace hefesto::view3d
