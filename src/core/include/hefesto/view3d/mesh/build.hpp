// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mesh builders (view-3d "Rendered geometry"; hefesto-3d-visualization
// "Mesh generation from layout", "Region model"; openspec/changes/
// m10-3d-view/design.md D6, D15; Q-MISC-70 fix).
//
// - build_from_layout: the layout-layer source. Flattens the rendered
//   cell's subtree through instance transforms and clips in root
//   coordinates (Q-MISC-70, both settings) to the cut box, or to the window
//   area grown by 1; halo 0.
// - build_from_planes (P2): the mask-layer source, from GEN's LayerPlanes
//   (already flat in root coordinates, in mask units), clipped to the box:
//   one layer per mask layer of the stack (build_mask_stack), in mask-layer
//   order, with the same faces, chunks and versions as the layout builder.
//   W3DCIFredisplay (W3Dmain.c:1726-1784) draws the planes with no clip
//   rectangle: they are already clipped, so a split tile's cut follows the
//   triangle in both settings (Q-MISC-83 concerns the layout-layer clip).
// - build_from_mask (P2, design D15): mask::generate_flat of the rendered
//   cell over MaskSource::area (the flatten reads it grown by the style's
//   radius, W3Dmain.c:1745), then build_from_planes. The mesh is in mask
//   units (internal units times the scale factor), as Magic draws the CIF
//   planes.
// - rebuild_chunks: rebuilds only the chunks whose area meets a dirty
//   rectangle grown by 1 (database change events, root coordinates),
//   adds chunks where new paint falls outside every chunk and drops the
//   chunks left empty; the others are reused unchanged.
//
// Builds run on a worker (MeshWorker, worker.hpp): with
// BuildContext::lock set, the flatten step reads the database under the
// shared lock and restarts when a writer waits (as the DRC worker does);
// the faces are then emitted from that snapshot without the lock, so the
// UI thread never waits (D4). A cancelled build returns nullopt and the
// caller keeps the previous model. `on_chunk` sees each chunk as it
// finishes, nearest `centre` first (progressive display); the model keeps
// its chunks in Morton order.
//
// J1 "Span connected layers" (task 6.5): with span_connected_layers and a
// technology, a contact type is drawn from the top of its lowest residue
// to the bottom of its highest (residues from tech-contacts-and-stacking,
// heights from the stack); MeshModel::layer_heights holds the h and t each
// layer is drawn with. Changing the option, the stack or the clip area
// needs a full build; rebuild_chunks assumes they are unchanged.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); MESH fills in
// src/core/src/view3d/mesh/ (design D3, section 6; build_from_planes,
// build_from_mask, rebuild_mask_chunks: MASKSRC, P2).

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <span>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

/// The area a build covers, in root coordinates.
struct ClipArea {
  db::Rect area;         // the cut box, or the window area grown by 1
  bool cut_box = false;  // true when `area` is the cut box (faces on it are closed)
  db::Coord halo = 0;    // 0 for the layout-layer source; the style's radius for the mask source
};

/// Progress, cancellation and progressive chunks of one build.
struct BuildContext {
  mask::Progress progress;                          // fraction done; cancel flag
  std::function<void(const Chunk&)> on_chunk;       // may be empty
  bool span_connected_layers = false;               // J1 view option "Span connected layers" (task 6.5)
  const tech::Technology* technology = nullptr;     // the contacts' residues, for span_connected_layers
  std::optional<db::Point> centre;                  // progressive order; the clip area's centre when unset
  db::DatabaseLock* lock = nullptr;                 // held shared while the database is read; may be null
  /// Mask-layer builds with magic-compat ON (MaskSource::generation's
  /// compat): the generation errors, reported once per build (Q-MISC-89,
  /// approved 2026-11-26): a full build reports those of the whole source
  /// area, an incremental rebuild (rebuild_mask_chunks) those of the areas
  /// it regenerates; never as feedback. Null or OFF: nothing reported.
  /// Called on the build's thread: a worker's host passes a thread-safe
  /// sink.
  diag::DiagnosticSink* sink = nullptr;
};

/// The layout-layer mesh of `cell` (the rendered cell) over `clip`.
[[nodiscard]] std::optional<MeshModel> build_from_layout(const LayerStack& stack, const db::CellDef& cell,
                                                         const ClipArea& clip, BuildContext& ctx);

/// The mask-layer mesh from generated planes (P2, design D15): stack layer
/// i draws planes[stack.layers()[i].mask_layer] (a layer with no plane
/// draws nothing). `clip.area` is in mask units (mask_clip).
[[nodiscard]] std::optional<MeshModel> build_from_planes(const LayerStack& stack, const mask::LayerPlanes& planes,
                                                         const ClipArea& clip, BuildContext& ctx);

/// The mask-layer source of a build (design D15).
struct MaskSource {
  std::shared_ptr<const mask::OutputStyle> style;  // the current output style (rescaled to the grid)
  mask::GenerationSource generation;               // technology, compat settings, `cif unfracture`
  db::Rect area;                                   // what is generated, internal units (the cut box, or the cell's)
};

/// The clip area of a mask-layer build: MaskSource::area in mask units
/// (times the scale factor, cifGenClip), with the halo the generation
/// reads around it (mask::generation_radius, internal units).
[[nodiscard]] ClipArea mask_clip(const MaskSource& source, bool cut_box);

/// The mask-layer mesh of `cell` (the rendered cell): generate_flat over
/// `source.area` with the build's progress and cancellation (with
/// BuildContext::lock, under the shared database lock), then the faces of
/// build_from_planes over mask_clip. Nullopt when cancelled.
[[nodiscard]] std::optional<MeshModel> build_from_mask(const LayerStack& stack, const db::CellDef& cell,
                                                       const MaskSource& source, const ClipArea& clip,
                                                       BuildContext& ctx);

/// rebuild_chunks for the mask-layer source: `dirty` (root coordinates,
/// internal units) grown by the halo and scaled to mask units marks the
/// chunks to regenerate; their areas (grown by 1) are generated again from
/// `cell` and their faces rebuilt; the other chunks are kept.
[[nodiscard]] std::optional<MeshModel> rebuild_mask_chunks(const MeshModel& previous, const LayerStack& stack,
                                                           const db::CellDef& cell, const MaskSource& source,
                                                           std::span<const db::Rect> dirty, BuildContext& ctx);

/// `previous` with the chunks that meet `dirty` (grown by 1) rebuilt from
/// `cell`, new chunks where paint appeared outside every chunk, and the
/// chunks left empty dropped. Rebuilt and new chunks get one fresh version
/// from the process-wide counter, as does the model (greater than every
/// version given before); the others keep theirs.
/// `previous` itself when no dirty rectangle meets its region.
[[nodiscard]] std::optional<MeshModel> rebuild_chunks(const MeshModel& previous, const LayerStack& stack,
                                                      const db::CellDef& cell, std::span<const db::Rect> dirty,
                                                      BuildContext& ctx);

}  // namespace hefesto::view3d
