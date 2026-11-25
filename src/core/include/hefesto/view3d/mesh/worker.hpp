// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MeshWorker: background mesh builds of one 3D window (hefesto-3d-
// visualization "Mesh generation from layout": "Generating the model SHALL
// be incremental and run in the background"; openspec/changes/m10-3d-view/
// design.md D4 "Threading", "Change notification", D6 "Chunks").
//
// One worker thread runs one build at a time:
// - build(inputs): a full build. It cancels the build in progress, if any,
//   and replaces a queued one; dirty areas recorded so far are dropped (the
//   full build reads the whole region).
// - changed(dirty): database change events for the rendered cell, in root
//   coordinates. The areas accumulate; once a model exists the worker
//   rebuilds the affected chunks of the current model (rebuild_chunks) with
//   every area recorded since. A change never cancels a running build: the
//   areas wait for it, so edits during a long build cannot starve it. A
//   build() that cancels a rebuild covers its areas.
// - cancel(): cancels the running build and drops the queued full build;
//   the previous model stays current. The areas recorded so far, and those
//   of a rebuild it cancels, are kept aside: the next changed() rebuilds
//   them with its own, so the model never misses an edit; the next build()
//   covers them.
// A finished build becomes current() and is handed to on_done; a cancelled
// one leaves current() as it was ("cancellation drops unfinished chunks and
// leaves the previous model visible"). on_chunk sees each chunk as it is
// built, nearest the view centre first; on_progress the fraction done.
// Callbacks run on the worker thread; the host posts them to its UI loop.
//
// With BuildInputs::lock set, the database is read under the shared lock
// (and the read starts again when a command wants the lock), so the cell
// may be edited meanwhile. The host keeps the cell, the stack and the
// technology alive until the worker is idle or destroyed, and calls build()
// again when the stack, the clip area, the rendered cell or the J1 option
// change.
//
// Mask-layer mode (P2, design D15): with BuildInputs::mask set, a full
// build generates the mask layers (build_from_mask) and a change
// regenerates only the chunks within the halo of the edit
// (rebuild_mask_chunks). A change of output style is a new stack and
// source: the host calls build() (a full rebuild).
//
// Owner: MESH (design D3).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d {

/// What a background build reads.
struct BuildInputs {
  std::shared_ptr<const LayerStack> stack;
  const db::CellDef* cell = nullptr;            // the rendered cell
  ClipArea clip;
  bool span_connected_layers = false;           // J1 view option
  const tech::Technology* technology = nullptr;  // residues for the J1 option
  std::optional<db::Point> centre;               // progressive order
  db::DatabaseLock* lock = nullptr;              // the session's database lock, or null
  std::size_t max_chunk_tiles = 65536;           // chunk size of the first build (tests use small ones)
  std::optional<MaskSource> mask;                // mask-layer mode: generated layers; `clip` is then mask_clip(*mask)
  /// The host's tag of this build: every model built or rebuilt from these
  /// inputs carries it (MeshModel::build_id), so that a model of an earlier
  /// build delivered after a new build() can be told apart and ignored.
  std::uint64_t build_id = 0;
};

/// One build of `inputs` on the calling thread (what the worker runs for a
/// full build): ctx's progress, on_chunk and cancellation, the inputs' J1
/// option, technology, centre and lock.
[[nodiscard]] std::optional<MeshModel> build_once(const BuildInputs& inputs, BuildContext& ctx);

/// Worker-thread callbacks; each may be empty.
struct WorkerCallbacks {
  std::function<void(const Chunk&)> on_chunk;
  std::function<void(double)> on_progress;
  std::function<void(const MeshHandle&)> on_done;  // after current() became this model
};

class MeshWorker {
 public:
  explicit MeshWorker(WorkerCallbacks callbacks = {});
  ~MeshWorker();  // cancels and joins
  MeshWorker(const MeshWorker&) = delete;
  MeshWorker& operator=(const MeshWorker&) = delete;

  /// A full build of `inputs`, cancelling the running build.
  void build(BuildInputs inputs);
  /// Records changed areas (root coordinates) for an incremental rebuild.
  void changed(std::span<const db::Rect> dirty);
  /// Cancels the running build and drops the queued work.
  void cancel();

  /// The last finished model (null before the first).
  [[nodiscard]] MeshHandle current() const;
  /// True when no build runs or waits.
  [[nodiscard]] bool idle() const;
  /// Waits until idle or `timeout_ms` passed; true when idle. A finished
  /// build's on_done has returned by then.
  bool wait_idle(int timeout_ms);

 private:
  struct State;
  std::unique_ptr<State> state_;
};

}  // namespace hefesto::view3d
