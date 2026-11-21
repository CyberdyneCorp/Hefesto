// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Background builds of the mesh (M10 task 6.4; openspec/changes/
// m10-3d-view/design.md D4 "Threading", D6 "Chunks"): MeshWorker builds
// off the calling thread, keeps the previous model when a build is
// cancelled, rebuilds from change events and waits for the database lock.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "hefesto/view3d/mesh/worker.hpp"
#include "mesh_fixture.hpp"
#include "view3d/mesh/internal.hpp"

namespace db = hefesto::db;
namespace view3d = hefesto::view3d;
using db::Rect;

namespace {

constexpr int kWaitMs = 10000;

struct Grid {
  view3d::test::MeshTech tech = view3d::test::build_tech();
  db::CellDef root{"root", tech.tech->plane_count()};
  std::shared_ptr<const view3d::LayerStack> stack;

  Grid() {
    view3d::test::paint_grid(root, tech, false);
    stack = std::make_shared<const view3d::LayerStack>(view3d::test::grid_stack(tech));
  }

  [[nodiscard]] view3d::BuildInputs inputs() const {
    view3d::BuildInputs in;
    in.stack = stack;
    in.cell = &root;
    in.clip = view3d::ClipArea{Rect{-1, -1, 161, 161}, false, 0};
    in.max_chunk_tiles = 16;
    return in;
  }
};

// A gate the worker thread waits on inside a callback.
class Gate {
 public:
  void arrive_and_wait() {
    std::unique_lock lock(mutex_);
    arrived_ = true;
    cv_.notify_all();
    cv_.wait(lock, [this] { return open_; });
  }
  bool wait_arrived() {
    std::unique_lock lock(mutex_);
    return cv_.wait_for(lock, std::chrono::milliseconds(kWaitMs), [this] { return arrived_; });
  }
  void open() {
    const std::lock_guard lock(mutex_);
    open_ = true;
    cv_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool arrived_ = false;
  bool open_ = false;
};

}  // namespace

TEST_CASE("a background build becomes current and reports its chunks and progress",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const Grid g;
  std::atomic<std::size_t> chunks{0};
  std::atomic<int> done{0};
  std::atomic<double> fraction{0.0};
  view3d::WorkerCallbacks cb;
  cb.on_chunk = [&](const view3d::Chunk&) { ++chunks; };
  cb.on_progress = [&](double f) { fraction = f; };
  cb.on_done = [&](const view3d::MeshHandle&) { ++done; };
  view3d::MeshWorker worker(cb);
  CHECK(worker.current() == nullptr);
  worker.build(g.inputs());
  REQUIRE(worker.wait_idle(kWaitMs));
  const view3d::MeshHandle model = worker.current();
  REQUIRE(model != nullptr);
  CHECK(done == 1);
  CHECK(chunks == model->chunks.size());
  CHECK(fraction == 1.0);
  view3d::BuildContext ctx;
  const auto direct = hefesto::view3d::detail::build_layout(*g.stack, g.root, g.inputs().clip, ctx, 16);
  REQUIRE(direct.has_value());
  CHECK(view3d::test::models_byte_equal(*model, *direct));
}

TEST_CASE("cancelling a build mid-way keeps the previous model",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const Grid g;
  Gate gate;
  std::atomic<bool> hold{false};
  std::atomic<int> done{0};
  view3d::WorkerCallbacks cb;
  cb.on_chunk = [&](const view3d::Chunk&) {
    if (hold.exchange(false)) {
      gate.arrive_and_wait();
    }
  };
  cb.on_done = [&](const view3d::MeshHandle&) { ++done; };
  view3d::MeshWorker worker(cb);
  worker.build(g.inputs());
  REQUIRE(worker.wait_idle(kWaitMs));
  const view3d::MeshHandle previous = worker.current();
  REQUIRE(previous != nullptr);

  hold = true;
  worker.build(g.inputs());
  REQUIRE(gate.wait_arrived());  // the second build is half done
  CHECK_FALSE(worker.idle());
  worker.cancel();
  gate.open();
  REQUIRE(worker.wait_idle(kWaitMs));
  CHECK(worker.current() == previous);
  CHECK(done == 1);
}

TEST_CASE("a new build cancels the running one and only the latest becomes current",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  Grid g;
  Gate gate;
  std::atomic<bool> hold{true};
  std::vector<std::size_t> finished;
  std::mutex finished_mutex;
  view3d::WorkerCallbacks cb;
  cb.on_chunk = [&](const view3d::Chunk&) {
    if (hold.exchange(false)) {
      gate.arrive_and_wait();
    }
  };
  cb.on_done = [&](const view3d::MeshHandle& m) {
    const std::lock_guard lock(finished_mutex);
    finished.push_back(m->chunks.size());
  };
  view3d::MeshWorker worker(cb);
  worker.build(g.inputs());
  REQUIRE(gate.wait_arrived());
  view3d::BuildInputs smaller = g.inputs();
  smaller.clip.area = Rect{-1, -1, 41, 41};
  worker.build(smaller);
  gate.open();
  REQUIRE(worker.wait_idle(kWaitMs));
  const view3d::MeshHandle model = worker.current();
  REQUIRE(model != nullptr);
  CHECK(model->region == smaller.clip.area);
  const std::lock_guard lock(finished_mutex);
  CHECK(finished.size() == 1);
}

TEST_CASE("a change event rebuilds the affected chunks of the current model in the background",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  Grid g;
  view3d::MeshWorker worker;
  worker.build(g.inputs());
  REQUIRE(worker.wait_idle(kWaitMs));
  const view3d::MeshHandle before = worker.current();
  REQUIRE(before != nullptr);
  const Rect dirty{16, 11, 18, 13};
  view3d::test::paint(g.root, g.tech, dirty, g.tech.metal1);
  const std::vector<Rect> areas{dirty};
  worker.changed(areas);
  REQUIRE(worker.wait_idle(kWaitMs));
  const view3d::MeshHandle after = worker.current();
  REQUIRE(after != nullptr);
  CHECK(after->version == before->version + 1);
  std::size_t kept = 0;
  for (const view3d::Chunk& c : after->chunks) {
    kept += c.version == before->version ? 1U : 0U;
  }
  CHECK(kept > 0);
  CHECK(kept < after->chunks.size());
  CHECK(view3d::test::face_area(*after, 0) == view3d::test::face_area(*before, 0) + 4.0);
}

TEST_CASE("a background build reads the database only when no command holds the lock",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const Grid g;
  db::DatabaseLock lock;
  view3d::MeshWorker worker;
  view3d::BuildInputs in = g.inputs();
  in.lock = &lock;
  {
    const db::DatabaseLock::Exclusive command(lock);
    worker.build(in);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(worker.current() == nullptr);
    CHECK_FALSE(worker.idle());
  }
  REQUIRE(worker.wait_idle(kWaitMs));
  CHECK(worker.current() != nullptr);
}
