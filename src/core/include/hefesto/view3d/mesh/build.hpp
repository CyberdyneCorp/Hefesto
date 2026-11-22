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
//   (already flat in root coordinates), clipped to the box.
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
// src/core/src/view3d/mesh/ (design D3, section 6; build_from_planes:
// MASKSRC, P2).

#pragma once

#include <functional>
#include <optional>
#include <span>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/lock.hpp"
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
};

/// The layout-layer mesh of `cell` (the rendered cell) over `clip`.
[[nodiscard]] std::optional<MeshModel> build_from_layout(const LayerStack& stack, const db::CellDef& cell,
                                                         const ClipArea& clip, BuildContext& ctx);

/// The mask-layer mesh from generated planes (P2, design D15).
[[nodiscard]] std::optional<MeshModel> build_from_planes(const LayerStack& stack, const mask::LayerPlanes& planes,
                                                         const ClipArea& clip, BuildContext& ctx);

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
