// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// glTF and STL export (P2; hefesto-3d-visualization "3D export";
// openspec/changes/m10-3d-view/design.md D14, J2 decided (a)).
//
// `export3d <file> [-format gltf|glb|stl] [-merged] [-zscale k]
// [-visible|-all]` writes the current mesh model in physical µm, without
// the view's z exaggeration unless `-zscale` is given (k recorded in
// `asset.extras.hefesto.zscale`). glTF: one mesh node per visible layer in
// layer order, one material per layer named by the display style's long
// name; written by cgltf under the C locale. STL: binary, fixed 80-byte
// header, triangles in mesh order. The same model writes the same bytes on
// every platform. Mask layers without a physical height are left out with
// VIEW3D-STACK-001.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); EXPORT fills in
// src/core/src/view3d/export/ (design D3, tasks 11.3, 11.4).

#pragma once

#include <cstdint>
#include <optional>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

enum class ExportFormat : std::uint8_t { gltf, glb, stl };

struct ExportOptions {
  ExportFormat format = ExportFormat::gltf;
  bool merged = false;               // STL: one solid for every layer
  std::optional<double> zscale;      // `-zscale k`; nullopt = physical heights
  bool visible_only = true;          // `-visible` (default) or `-all`
  const display::StyleTable* styles = nullptr;  // material names and colors; nullptr = layer names, grey
};

/// Writes `mesh` as glTF or GLB to `out`; warnings go to `sink`.
[[nodiscard]] diag::Status write_gltf(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
                                      gds::ByteSink& out, diag::DiagnosticSink& sink);

/// Writes `mesh` as binary STL to `out`; warnings go to `sink`.
[[nodiscard]] diag::Status write_stl(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
                                     gds::ByteSink& out, diag::DiagnosticSink& sink);

}  // namespace hefesto::view3d
