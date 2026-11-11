// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Design D16 "Background start latency" (hefesto-performance "Edit
// latency": checking starts within 100 ms): a `paint` command on the
// 100 x 100 array of sky130_fd_sc_hd__dfxtp_1 (the M2 benchmark array)
// under sky130A's DRC rules, with the worker hosted as an interactive host
// hosts it (drc::BackgroundHost). Measured from the end of the command
// (the dispatcher wakes the worker) to the first kernel call of the
// worker's square. The real kernel runs; the square is committed on this
// thread before the next paint.
//
// Hidden: run `hefesto_drc_engine_tests "[benchmark]"`. Skips without the
// pinned sky130A data set; the 100 ms check runs only in optimized builds.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/drc/engine/host.hpp"
#include "hefesto/drc/engine/kernel_hook.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "reference_data.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace drc = hefesto::drc;

namespace {

std::atomic<std::int64_t> g_first_kernel_ns{0};

std::int64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

drc::CheckOutcome timed_kernel(const drc::CheckInput& in, const db::Rect& check, const db::Rect& clip,
                               drc::ErrorSink& sink) {
  std::int64_t unset = 0;
  (void)g_first_kernel_ns.compare_exchange_strong(unset, now_ns());
  return drc::basic_check(in, check, clip, sink);
}

struct Mailbox {
  std::mutex mutex;
  std::condition_variable cv;
  bool posted = false;
  void post() {
    const std::lock_guard lock(mutex);
    posted = true;
    cv.notify_all();
  }
  void wait() {
    std::unique_lock lock(mutex);
    (void)cv.wait_for(lock, std::chrono::seconds(1), [this] { return posted; });
    posted = false;
  }
};

}  // namespace

TEST_CASE("Background start latency after a paint on the 100 x 100 dfxtp_1 array",
          "[.][benchmark][cap:hefesto-performance][req:Edit latency]") {
  const auto root = hefesto::test::pinned_sky130a();
  if (!root) {
    SKIP("sky130A with sky130_fd_sc_hd not in .cache/pdks");
  }
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, false, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  REQUIRE(cmd::register_all_commands(registry));
  cmd::Dispatcher dispatcher(registry, services.view(), project, script);
  dispatcher.set_editor(&editor);
  auto run = [&](const std::string& text) {
    const cmd::ScriptRun r = cmd::run_script(dispatcher, text, cmd::Origin::cli);
    REQUIRE(r.ok());
  };
  const std::filesystem::path mag = *root / "libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__dfxtp_1";
  run("tech load " + (*root / "libs.tech/magic/sky130A.tech").string() +
      " -noprompt\nsnap internal\nscalegrid 1 2\ndrc euclidean on\nsnap internal\nload " + mag.string() +
      "\nload top\n");
  db::CellDef* leaf = editor.cells.find("sky130_fd_sc_hd__dfxtp_1");
  db::CellDef* top = editor.cells.find("top");
  REQUIRE(leaf != nullptr);
  REQUIRE(top != nullptr);
  {
    // The array, placed directly: an already checked layout (no check areas).
    db::PlaceOptions array;
    array.id = "x";
    array.array = db::ArrayInfo{0, 99, 0, 99, 1472, 544};  // FIXED_BBOX pitch
    REQUIRE(db::place(*top, *leaf, array).ok());
    db::recompute_bbox(*top);
  }
  REQUIRE(drc::catchup(editor).ok());  // whatever loading left pending (the leaf)
  REQUIRE(editor.drc.pending.empty());

  const drc::ScopedKernel kernel(&timed_kernel);
  Mailbox mail;
  drc::BackgroundHost host(editor, [&mail] { mail.post(); });
  std::vector<double> latency_ms;
  std::vector<double> square_ms;
  for (int i = 0; i < 20; ++i) {
    g_first_kernel_ns = 0;
    const int x = 7000 * i + 301;
    const int y = 27000 + 544 * (i % 7);
    run("box " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(x + 60) + " " +
        std::to_string(y + 40) + "\npaint m1\n");
    const std::int64_t command_end = now_ns();
    while (host.busy()) {
      mail.wait();
      (void)host.service();
    }
    const std::int64_t first = g_first_kernel_ns.load();
    REQUIRE(first != 0);
    latency_ms.push_back(static_cast<double>(std::max<std::int64_t>(0, first - command_end)) / 1e6);
    square_ms.push_back(static_cast<double>(now_ns() - command_end) / 1e6);
  }
  std::sort(latency_ms.begin(), latency_ms.end());
  std::sort(square_ms.begin(), square_ms.end());
  WARN("background start latency (ms): median " << latency_ms[latency_ms.size() / 2] << ", max "
                                                << latency_ms.back() << "; until idle (ms): median "
                                                << square_ms[square_ms.size() / 2] << ", max " << square_ms.back());
#ifdef NDEBUG
  CHECK(latency_ms.back() <= 100.0);
#endif
}
