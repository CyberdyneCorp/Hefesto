// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol resolution for the schematic canvas off the UI thread
// (schematic_symbols.hpp; M8 task 13.2, round-4 open item): the first scene
// draws placeholders without resolving on the calling thread, the resolved
// symbols arrive when the worker finishes, and a result for a document
// closed or reloaded meanwhile is dropped. Run under the TSan preset too.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/shell/schematic_canvas.hpp"
#include "hefesto/shell/schematic_symbols.hpp"

using namespace hefesto::shell;
namespace schem = hefesto::schem;
namespace platform = hefesto::platform;
namespace diag = hefesto::diag;
using schem::Point;

namespace {

// A symbol with one vertical line, named after its reference.
schem::Symbol line_symbol(const std::string& reference) {
  schem::Document doc;
  doc.kind = schem::FileKind::symbol;
  doc.lines.push_back(schem::LineRecord{4, Point{0, -30}, Point{0, 30}, {}});
  return schem::make_symbol(reference, "/lib/" + reference, std::move(doc));
}

schem::Document document_with(std::vector<std::string> references) {
  schem::Document doc;
  doc.path = "/work/top.sch";
  doc.disk_digest = "d1";
  double x = 0.0;
  int n = 1;
  for (std::string& reference : references) {
    doc.instances.push_back(
        schem::InstanceRecord{std::move(reference), Point{x, 0}, 0, 0, "name=X" + std::to_string(n++)});
    x += 100.0;
  }
  return doc;
}

// A ResolveFn that blocks until open() and records the threads it ran on.
class Gate {
 public:
  void open() {
    {
      const std::lock_guard guard(mutex_);
      open_ = true;
    }
    cv_.notify_all();
  }
  SchematicSymbolResolver::ResolveFn resolve() {
    return [this](const SymbolRequest&, const std::string& reference, std::vector<diag::Diagnostic>&) {
      std::unique_lock lock(mutex_);
      threads_.push_back(std::this_thread::get_id());
      calls_.push_back(reference);
      cv_.wait(lock, [this] { return open_; });
      return line_symbol(reference);
    };
  }
  [[nodiscard]] std::vector<std::thread::id> threads() {
    const std::lock_guard guard(mutex_);
    return threads_;
  }
  [[nodiscard]] std::vector<std::string> calls() {
    const std::lock_guard guard(mutex_);
    return calls_;
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool open_ = false;
  std::vector<std::thread::id> threads_;
  std::vector<std::string> calls_;
};

const schem::SearchPath kLib{std::vector<platform::Path>{"/lib"}};

std::size_t pending_instances(const SchematicScene& scene) {
  std::size_t n = 0;
  for (const SceneInstance& i : scene.instances) n += i.pending ? 1U : 0U;
  return n;
}

}  // namespace

TEST_CASE("the first scene draws placeholders and never resolves on the UI thread",
          "[cap:schematic-symbols-and-libraries][req:Symbol generators]") {
  Gate gate;
  std::atomic<int> notified{0};
  SchematicSymbolResolver resolver({nullptr, gate.resolve(), [&notified] { ++notified; }});
  const schem::Document doc = document_with({"res.tcl(1k)", "r.sym", "res.tcl(1k)"});

  // show() returns at once although the generator is still "running".
  CHECK(resolver.show(1, doc, kLib, true));
  CHECK(resolver.running());
  CHECK(resolver.pending_count() == 2);  // distinct references
  CHECK_FALSE(resolver.show(1, doc, kLib, true));  // one job at a time

  const SchematicScene first = build_scene(doc, resolver.lookup(), {}, &resolver.pending_symbol());
  REQUIRE(first.instances.size() == 3);
  CHECK(pending_instances(first) == 3);
  CHECK(first.instances[0].placeholder);  // drawn with the placeholder meanwhile
  CHECK_FALSE(resolver.take_finished());

  gate.open();
  resolver.wait();
  CHECK(resolver.take_finished());
  CHECK(notified.load() == 1);
  for (const std::thread::id id : gate.threads()) CHECK(id != std::this_thread::get_id());
  CHECK(gate.calls() == std::vector<std::string>{"res.tcl(1k)", "r.sym"});

  const SchematicScene resolved = build_scene(doc, resolver.lookup(), {}, &resolver.pending_symbol());
  CHECK(pending_instances(resolved) == 0);
  CHECK_FALSE(resolved.instances[0].placeholder);
  CHECK(resolved.segments.size() == 3);  // each instance's line
  CHECK(resolver.pending_count() == 0);
  CHECK_FALSE(resolver.show(1, doc, kLib, true));  // nothing left to resolve
}

TEST_CASE("a result for a document closed while resolving is dropped",
          "[cap:schematic-symbols-and-libraries][req:Symbol generators]") {
  Gate gate;
  SchematicSymbolResolver resolver({nullptr, gate.resolve(), {}});
  const schem::Document a = document_with({"a.tcl(1)"});
  schem::Document b = document_with({"b.sym"});
  b.path = "/work/b.sch";
  REQUIRE(resolver.show(1, a, kLib, true));
  // Document 1 is closed: document 2 is shown while the job of 1 runs.
  CHECK_FALSE(resolver.show(2, b, kLib, true));
  gate.open();
  resolver.wait();
  CHECK(resolver.take_finished());  // finished, but stale
  CHECK(resolver.pending_count() == 1);
  const SchematicScene scene = build_scene(b, resolver.lookup(), {}, &resolver.pending_symbol());
  CHECK(scene.instances[0].pending);  // a.tcl's symbol never reaches b

  REQUIRE(resolver.show(2, b, kLib, true));  // the job for b
  resolver.wait();
  CHECK(resolver.take_finished());
  CHECK(resolver.pending_count() == 0);

  resolver.forget();  // nothing shown
  CHECK(resolver.pending_count() == 0);
  CHECK(build_scene(b, resolver.lookup(), {}, &resolver.pending_symbol()).instances[0].pending);
}

TEST_CASE("a result for a document reloaded while resolving is dropped",
          "[cap:schematic-symbols-and-libraries][req:Symbol cache and reload]") {
  SECTION("the file changed on disk") {
    Gate gate;
    SchematicSymbolResolver resolver({nullptr, gate.resolve(), {}});
    const schem::Document before = document_with({"g.tcl(1)"});
    REQUIRE(resolver.show(1, before, kLib, true));
    schem::Document reloaded = before;  // same id and file, new content
    reloaded.disk_digest = "d2";
    CHECK_FALSE(resolver.show(1, reloaded, kLib, true));
    gate.open();
    resolver.wait();
    CHECK(resolver.take_finished());
    CHECK(resolver.pending_count() == 1);  // resolved again, never from the stale job
  }
  SECTION("a reference the reloaded document no longer uses") {
    Gate gate;
    SchematicSymbolResolver resolver({nullptr, gate.resolve(), {}});
    REQUIRE(resolver.show(1, document_with({"g.tcl(1)", "r.sym"}), kLib, true));
    const schem::Document reloaded = document_with({"r.sym", "g.tcl(2)"});
    CHECK_FALSE(resolver.show(1, reloaded, kLib, true));
    gate.open();
    resolver.wait();
    CHECK(resolver.take_finished());
    const SchematicScene scene = build_scene(reloaded, resolver.lookup(), {}, &resolver.pending_symbol());
    CHECK_FALSE(scene.instances[0].pending);  // r.sym is still valid
    CHECK(scene.instances[1].pending);        // g.tcl(2) is still to resolve
    CHECK(resolver.pending_count() == 1);
  }
  SECTION("schematic reload_symbols") {
    Gate gate;
    gate.open();
    SchematicSymbolResolver resolver({nullptr, gate.resolve(), {}});
    const schem::Document doc = document_with({"r.sym"});
    REQUIRE(resolver.show(1, doc, kLib, true));
    resolver.wait();
    REQUIRE(resolver.take_finished());
    resolver.reload_symbols();
    CHECK(resolver.pending_count() == 1);
    CHECK(resolver.show(1, doc, kLib, true));
    resolver.wait();
    CHECK(resolver.take_finished());
    CHECK(gate.calls().size() == 2);
  }
}

TEST_CASE("the worker resolves through a symbol cache of its own",
          "[cap:schematic-symbols-and-libraries][req:Missing symbols]") {
  auto fs = std::make_shared<platform::MemoryFileSystem>();
  REQUIRE(fs->create_directories("/lib").ok());
  REQUIRE(fs->create_directories("/work").ok());
  REQUIRE(fs->write_atomic("/lib/r.sym",
                           "v {xschem version=3.4.5 file_version=1.2}\n"
                           "K {type=resistor}\n"
                           "L 4 0 -30 0 30 {}\n"
                           "B 5 -2.5 -32.5 2.5 -27.5 {name=P dir=inout}\n")
              .ok());
  std::atomic<int> notified{0};
  SchematicSymbolResolver resolver({fs, {}, [&notified] { ++notified; }});
  const schem::Document doc = document_with({"r.sym", "nowhere.sym"});
  REQUIRE(resolver.show(7, doc, kLib, true));
  resolver.wait();
  REQUIRE(resolver.take_finished());
  CHECK(notified.load() == 1);
  const SchematicScene scene = build_scene(doc, resolver.lookup(), {}, &resolver.pending_symbol());
  REQUIRE(scene.instances.size() == 2);
  CHECK_FALSE(scene.instances[0].pending);
  CHECK_FALSE(scene.instances[0].placeholder);
  CHECK_FALSE(scene.instances[1].pending);
  CHECK(scene.instances[1].placeholder);  // missing: the placeholder for good
  CHECK_FALSE(resolver.take_warnings().empty());  // the missing symbol's warning
  CHECK(resolver.take_warnings().empty());
}

TEST_CASE("a Tcl generator runs on the worker, never on the UI thread",
          "[cap:schematic-symbols-and-libraries][req:Symbol generators]") {
  auto fs = std::make_shared<platform::MemoryFileSystem>();
  REQUIRE(fs->create_directories("/lib").ok());
  REQUIRE(fs->create_directories("/work").ok());
  REQUIRE(fs->write_atomic("/lib/bar.tcl",
                           "puts {v {xschem version=3.4.5 file_version=1.2}}\n"
                           "puts \"L 4 0 0 [lindex $argv 0] 0 {}\"\n")
              .ok());
  SchematicSymbolResolver resolver({fs, {}, {}});
  const schem::Document doc = document_with({"bar.tcl(40)"});
  REQUIRE(resolver.show(3, doc, kLib, true));
  CHECK(build_scene(doc, resolver.lookup(), {}, &resolver.pending_symbol()).instances[0].pending);
  resolver.wait();
  REQUIRE(resolver.take_finished());
  const SchematicScene scene = build_scene(doc, resolver.lookup(), {}, &resolver.pending_symbol());
  REQUIRE(scene.instances.size() == 1);
  CHECK_FALSE(scene.instances[0].pending);
  CHECK_FALSE(scene.instances[0].placeholder);
  REQUIRE(scene.segments.size() == 1);
  CHECK(scene.segments[0].b.x == 40.0);
}

TEST_CASE("document references are distinct and in first-use order",
          "[cap:schematic-symbols-and-libraries][req:Symbol references]") {
  CHECK(document_references(document_with({"b.sym", "a.sym", "b.sym"})) == std::vector<std::string>{"b.sym", "a.sym"});
  CHECK(document_references(schem::Document{}).empty());
}
