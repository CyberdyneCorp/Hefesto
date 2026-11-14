// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M5 scaffold (openspec/changes/m5-mask-gds-cif tasks.md 2.2-2.4): the
// mask, gds and cif code areas exist; `gds`, `calma` and `cif` are
// registered with Magic's option tables, and every option not yet
// implemented fails with CORE-NOTIMPL-005 while the M4 `cif` queries keep
// working; the shared interfaces of design D4 exist with their stubs; the
// `random` generator lives in EditorState::mask and keeps the M4 sequence.
// Owners replace these [m5-scaffold] tests with [cap:][req:] ones as the
// stubs go away.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/cif/read/read.hpp"
#include "hefesto/cif/write/write.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/command/command.hpp"
#include "hefesto/gds/read/read.hpp"
#include "hefesto/gds/read/stamps.hpp"
#include "hefesto/gds/stream/encode.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/mask/ops/operators.hpp"
#include "hefesto/mask/style/core.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace editor = hefesto::editor;
namespace mask = hefesto::mask;
namespace gds = hefesto::gds;
namespace cif = hefesto::cif;

namespace {

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-005";

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};
  diag::CollectingSink printed;

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(&state);
    dispatcher.add_sink(printed);
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  std::string take_printed() {
    std::string text;
    for (const auto& d : printed.take()) {
      text += d.text + "\n";
    }
    return text;
  }
};

std::string option_name(std::string_view entry) {
  return std::string{entry.substr(0, entry.find_first_of(" \t\n"))};
}

void check_not_implemented(const cmd::CommandResult& result, const std::string& what) {
  INFO(what);
  REQUIRE_FALSE(result.ok());
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.front().code == kNotImplemented);
  CHECK(result.diagnostics.front().text == "not implemented yet: " + what);
}

}  // namespace

// --- tasks.md 2.2/2.3: code areas and CORE-NOTIMPL-005 ----------------------

TEST_CASE("CORE-NOTIMPL-005 and the M5 code areas are declared", "[m5-scaffold]") {
  const auto code = diag::find_code(kNotImplemented);
  REQUIRE(code.has_value());
  CHECK(code->severity == diag::Severity::error);
  for (const auto* area : {"mask", "gds", "cif"}) {
    INFO(area);
    const auto found = diag::find_code_area(area);
    REQUIRE(found.has_value());
    CHECK(found->doc == "docs/diagnostics/codes/" + std::string{area} + ".md");
  }
  CHECK(diag::find_code_area("mask")->prefix == "MASK");
  CHECK(diag::find_code_area("gds")->prefix == "GDS");
  CHECK(diag::find_code_area("cif")->prefix == "CIF");
}

// --- tasks.md 2.3: the gds, calma and cif commands --------------------------

TEST_CASE("gds help lists every gds option of Magic's table", "[m5-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("gds") != nullptr);
  REQUIRE(s.registry.find("calma") != nullptr);
  const std::vector<std::string> expected = {
      "help",     "abstract", "addendum",     "arrays",   "compress", "contacts", "datestamp", "drccheck",
      "flatten",  "flatglob", "labels",       "library",  "lower",    "magscale", "maskhints", "merge",
      "nodatestamp", "noduplicates", "ordering", "read",   "readonly", "rescale",  "savepaths", "warning",
      "write",    "polygon",  "path",         "undefined", "unique"};
  std::vector<std::string> names;
  for (const std::string_view entry : gds::gds_options()) {
    names.push_back(option_name(entry));
  }
  CHECK(names == expected);

  for (const auto* command : {"gds help", "calma help"}) {
    INFO(command);
    REQUIRE(s.run(command).ok());
    const std::string text = s.take_printed();
    CHECK(text.starts_with("GDS commands have the form \":gds option\", where option is one of:\n"));
    for (const std::string_view entry : gds::gds_options()) {
      CHECK(text.find("    " + std::string{entry} + "\n") != std::string::npos);
    }
    CHECK(text.ends_with("    to select the mask layers read by :gds read.\n"));
  }
}

TEST_CASE("cif help lists every cif option of Magic's table", "[m5-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("cif") != nullptr);
  REQUIRE(s.run("cif help wizard").ok());
  const std::string text = s.take_printed();
  for (const auto* name : {"*array", "*hier", "arealabels", "coverage", "drccheck", "help", "idcell", "istyle",
                           "lambda", "limit", "ostyle", "paint", "prefix", "read", "rescale", "scale", "see",
                           "statistics", "warning", "write", "flat", "polygon", "unfracture"}) {
    INFO(name);
    CHECK(text.find("    " + std::string{name}) != std::string::npos);
  }
}

TEST_CASE("unimplemented gds and cif options fail with CORE-NOTIMPL-005", "[m5-scaffold]") {
  Session s;
  // `read` and `write` (and a bare `gds`) check for a window first.
  const auto no_window = s.run("gds write out");
  REQUIRE_FALSE(no_window.ok());
  CHECK(no_window.diagnostics.front().code == "VIEW-WINDOW-003");
  s.state.command_point = cmd::PointerState{};

  check_not_implemented(s.run("calma read in.gds"), "gds read");
  // GDSW landed `write` and the write options (tests/unit/gds/command/).
  const std::array<std::string_view, 13> landed{"help",  "write",  "abstract", "addendum",    "arrays",
                                                "compress", "contacts", "datestamp", "labels",  "library",
                                                "lower", "merge",  "nodatestamp"};
  for (const std::string_view entry : gds::gds_options()) {
    const std::string name = option_name(entry);
    if (std::find(landed.begin(), landed.end(), name) == landed.end() && name != "undefined") {
      check_not_implemented(s.run("gds " + name), "gds " + name);
    }
  }
  check_not_implemented(s.run("gds nodup yes"), "gds noduplicates");  // unique prefix
  // CIF landed every `cif` option (tests/unit/cif/command/).
}

TEST_CASE("an unknown gds option prints the error and the help", "[m5-scaffold]") {
  Session s;
  REQUIRE(s.run("gds bogus").ok());
  const std::string text = s.take_printed();
  CHECK(text.starts_with("\"bogus\" isn't a valid gds option.\nGDS commands have the form"));
}

TEST_CASE("the M4 cif queries keep working after the move", "[m5-scaffold]") {
  Session s;
  s.state.command_point = cmd::PointerState{};
  const auto scale = s.run("cif scale out");
  REQUIRE(scale.ok());  // 1.0 without an output style
  REQUIRE(s.run("cif list ostyle").ok());
  REQUIRE(s.run("cif listall istyle").ok());
}

// --- tasks.md 2.4: shared interfaces ----------------------------------------

TEST_CASE("random seed 1 gives the M4 sequence from EditorState", "[m5-scaffold]") {
  Session s;
  REQUIRE(s.run("random seed 1").ok());
  // glibc random() after srandom(1).
  CHECK(s.run("random").value.to_string() == "1804289383");
  CHECK(s.run("random").value.to_string() == "846930886");
  CHECK(s.run("random").value.to_string() == "1681692777");
  // The generator is the session's, shared with the full-dump prefix.
  hefesto::util::LibcRandom expected;
  expected.seed(1);
  static_cast<void>(expected.next());
  static_cast<void>(expected.next());
  static_cast<void>(expected.next());
  CHECK(s.state.mask.random.next() == expected.next());
  // A fresh session starts seeded with 1, as a C program does.
  Session fresh;
  CHECK(fresh.run("random").value.to_string() == "1804289383");
}

TEST_CASE("the mask session has its defaults", "[m5-scaffold]") {
  editor::EditorState state;
  CHECK(state.mask.output_styles.current_style() == nullptr);
  CHECK(state.mask.input_styles.current_style() == nullptr);
  CHECK(state.mask.gds == gds::GdsSettings{});
  CHECK(state.mask.gds.labels);
  CHECK(state.mask.gds.flatten_limit == 10);
  CHECK(state.mask.cif == cif::CifSettings{});
  CHECK(state.mask.warnings.level == mask::WarningLevel::warn_default);
}

TEST_CASE("the M5 entry points exist and fail with CORE-NOTIMPL-005", "[m5-scaffold]") {
  editor::EditorState state;
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  db::CellDef top{"top", 1};
  db::CellDef* root = &top;
  // GDSW and CIF landed the writers: without an output style they fail before any file is opened.
  const auto prepared = gds::prepare_write(state, *root, gds::GdsWriteOptions{});
  REQUIRE_FALSE(prepared.ok());
  CHECK(prepared.error().code == "GDS-WRITE-002");
  const auto cif_prepared = cif::prepare_write(state, *root, cif::CifWriteOptions{});
  REQUIRE_FALSE(cif_prepared.ok());
  CHECK(cif_prepared.error().code == "CIF-WRITE-001");
  CHECK_FALSE(gds::open_source(services.view().files, "in.gds").ok());
  CHECK(gds::content_stamps({}).empty());

  // The memory streams work already (tests and fuzzing use them).
  gds::MemorySink sink;
  const std::array bytes{std::byte{1}, std::byte{2}, std::byte{3}};
  REQUIRE(sink.write(bytes).ok());
  gds::MemorySource source{sink.bytes()};
  std::array<std::byte, 2> out{};
  CHECK(source.read(out).value() == 2);
  CHECK(source.read(out).value() == 1);
  CHECK(source.position() == 3);
  gds::RecordReader reader{source};
  auto end = reader.next();  // GDSR landed the reader: at the end of the stream there are no more records
  REQUIRE(end.ok());
  CHECK_FALSE(end.value().has_value());

  // Mask planes, operators and generation.
  mask::PlanePool pool;
  mask::MaskPlane& plane = pool.acquire();
  REQUIRE(plane.paint(db::Rect{0, 0, 10, 10}).ok());
  CHECK_FALSE(plane.empty());
  pool.release(plane);
  CHECK(pool.acquire().empty());  // reused and cleared
  CHECK(pool.size() == 1);
  diag::CollectingSink sink_diag;
  mask::GenerationContext ctx;
  ctx.sink = &sink_diag;
  const mask::OutputStyle style;
  // GEN landed flat generation: a style without layers generates nothing.
  CHECK(mask::generate_flat(state, top, db::Rect{0, 0, 1, 1}, style, ctx).empty());
  CHECK(sink_diag.take().empty());
  std::atomic<bool> stop{true};
  mask::Progress progress{{}, &stop};
  CHECK(progress.cancelled());
  ctx.progress = &progress;
  CHECK(ctx.cancelled());

  // Styles and the shared core: implemented by STYLES (tests/unit/mask/style/);
  // input styles and the read engine by INPUT (tests/unit/mask/input/).
}
