// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lef read` and `lef datestamp` commands (capability lef-read "lef
// read side effects", "lef read arguments", "lef datestamp", "MACRO and SITE
// cell creation"; layout-undo "Undoable LEF and DEF reads"; openspec/
// changes/m6-lef-def-oasis tasks.md 5.6): through the dispatcher, on files
// in a scratch directory.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/read/find_file.hpp"
#include "hefesto/lef/read/read.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/line.hpp"
#include "macro_fixture.hpp"

using namespace lefread_test;

namespace {

/// A scratch directory of LEF files, removed at the end of the test.
struct Files {
  explicit Files(std::string_view name)
      : dir(std::filesystem::temp_directory_path() / ("hefesto-lef-read-" + std::string{name})) {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
  }
  ~Files() {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }
  Files(const Files&) = delete;
  Files& operator=(const Files&) = delete;

  std::string write(const std::string& name, std::string_view text) const {
    const auto path = dir / name;
    std::ofstream out{path, std::ios::binary};
    out << text;
    return path.generic_string();
  }
  [[nodiscard]] std::string path(const std::string& name) const { return (dir / name).generic_string(); }

  std::filesystem::path dir;
};

struct Session {
  hefesto::platform::PortableServices services;
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};
  diag::CollectingSink printed;

  /// On the local disk (the scratch directories) unless `in_memory`.
  explicit Session(bool in_memory = false)
      : services{hefesto::platform::PortableOptions{{}, {}, in_memory, true}} {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(&state);
    dispatcher.add_sink(printed);
    state.technology = technology();
    state.command_point = cmd::PointerState{};
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  std::vector<std::string> take_printed() {
    std::vector<std::string> texts;
    for (const auto& d : printed.take()) {
      texts.push_back(d.text);
    }
    return texts;
  }
};

std::vector<std::string> texts_of(const cmd::CommandResult& result) {
  std::vector<std::string> out;
  for (const auto& d : result.diagnostics) {
    out.push_back(d.text);
  }
  return out;
}

bool contains(const std::vector<std::string>& texts, std::string_view text) {
  return std::find(texts.begin(), texts.end(), text) != texts.end();
}

constexpr std::string_view kInv =
    "VERSION 5.7 ;\nMACRO INV\n  CLASS CORE ;\n  SIZE 2 BY 4 ;\n  PIN A\n    PORT\n      LAYER metal1 ;\n"
    "      RECT 0 0 1 1 ;\n    END\n  END A\nEND INV\n";
const std::string kCells = std::string{kInv} + "END LIBRARY\n";

}  // namespace

// --- lef read side effects -----------------------------------------------------------

TEST_CASE("Extension supplied", "[cap:lef-read][req:lef read side effects]") {
  Files files{"extension"};
  const std::string lef = files.write("cells.lef", kCells);
  Session s;
  const auto result = s.run("lef read " + files.path("cells"));
  REQUIRE(result.ok());
  CHECK(texts_of(result) == Strings{"Reading LEF data from file " + lef + ".", "LEF read: Processed 12 lines."});
  REQUIRE(s.state.cells.find("INV") != nullptr);
  CHECK(fixed_bbox(*s.state.cells.find("INV")) == db::Rect{0, 0, 2, 4});
}

TEST_CASE("Unusual extension", "[cap:lef-read][req:lef read side effects]") {
  Files files{"unusual"};
  const std::string tlef = files.write("tech.tlef", "MACRO T\n  SIZE 1 BY 1 ;\nEND T\n");
  files.write("tech.lef", "MACRO L\n  SIZE 1 BY 1 ;\nEND L\n");
  Session s;
  const auto result = s.run("lef read " + tlef);
  REQUIRE(result.ok());
  CHECK(texts_of(result).front() == "Reading LEF data from file " + tlef + ".");
  CHECK(s.state.cells.find("T") != nullptr);
  CHECK(s.state.cells.find("L") == nullptr);
}

TEST_CASE("an unusual extension that does not open falls back to .lef", "[cap:lef-read][req:lef read side effects]") {
  Files files{"fallback"};
  const std::string lef = files.write("tech.lef", "MACRO L\n  SIZE 1 BY 1 ;\nEND L\n");
  Session s;
  const auto result = s.run("lef read " + files.path("tech.tlef"));
  REQUIRE(result.ok());
  CHECK(texts_of(result).front() == "Reading LEF data from file " + lef + ".");
  CHECK(s.state.cells.find("L") != nullptr);
}

TEST_CASE("a file that cannot be opened is reported", "[cap:lef-read][req:lef read side effects]") {
  Files files{"missing"};
  Session s;
  const auto result = s.run("lef read " + files.path("none"));
  REQUIRE_FALSE(result.ok());
  CHECK(texts_of(result) ==
        Strings{"Cannot open input file " + files.path("none.lef") + " (No such file or directory)."});
}

TEST_CASE("the read prints the totals and the search path finds the file", "[cap:lef-read][req:lef read side effects]") {
  Files files{"totals"};
  files.write("cells.lef", "MACRO A\n  BOGUS ;\n  CLASS ;\nEND A\n");
  Session s;
  s.state.search_paths.path = {".", files.dir.generic_string()};
  const auto result = s.run("lef read cells");
  REQUIRE(result.ok());
  const auto texts = texts_of(result);
  CHECK(contains(texts, "LEF read, Line 2 (Message): Unknown keyword \"BOGUS\" in LEF file; ignoring."));
  CHECK(contains(texts, "LEF read: Processed 4 lines."));
  CHECK(contains(texts, "LEF Read: encountered 1 warning total."));
}

TEST_CASE("a gzip-compressed LEF is read", "[cap:lef-read][req:lef read side effects]") {
  // cells.lef.gz is found for `cells` (cell::search_file) and inflated.
  Files files{"gzip"};
  hefesto::platform::LocalFileSystem local;
  auto sink = gds::open_sink(local, files.path("cells.lef.gz"), 6);
  REQUIRE(sink.ok());
  const auto data = bytes(kCells);
  REQUIRE(sink.value()->write(data).ok());
  REQUIRE(sink.value()->finish().ok());
  Session s;
  REQUIRE(s.run("lef read " + files.path("cells")).ok());
  CHECK(s.state.cells.find("INV") != nullptr);
}

TEST_CASE("regression: lef read reads through the session's file system", "[cap:lef-read][req:lef read side effects]") {
  // `lef read` opened the local disk whatever the session's file system was
  // (the Tcl sandbox's granted roots, the iPadOS document model); it now
  // reads through it, as `gds read`, `def read` and `lef write` do.
  Session s{true};
  REQUIRE(s.services.view().files.write_atomic("/mem/cells.lef", kCells).ok());
  const auto result = s.run("lef read /mem/cells");
  REQUIRE(result.ok());
  CHECK(texts_of(result).front() == "Reading LEF data from file /mem/cells.lef.");
  CHECK(s.state.cells.find("INV") != nullptr);
}

// --- lef read arguments --------------------------------------------------------------

TEST_CASE("Unknown option ignored", "[cap:lef-read][req:lef read arguments]") {
  // -annotate is requested: the duplicate INV with no INV_1 is skipped (to
  // END MACRO, with magic-compat ON), so INV_1 is not created.
  Files files{"options"};
  const std::string text = "MACRO INV\n  SIZE 1 BY 1 ;\nEND INV\nMACRO INV\n  SIZE 1 BY 1 ;\nEND INV\n";
  const std::string lef = files.write("cells.lef", text);
  Session with;
  REQUIRE(with.run("lef read " + lef + " -foo -annotate").ok());
  CHECK(with.state.cells.find("INV") != nullptr);
  CHECK(with.state.cells.find("INV_1") == nullptr);
  Session without;
  REQUIRE(without.run("lef read " + lef + " -foo").ok());
  CHECK(without.state.cells.find("INV_1") != nullptr);
}

TEST_CASE("Option before file name", "[cap:lef-read][req:lef read arguments]") {
  Session s;
  const auto result = s.run("lef read -annotate cells.lef");
  REQUIRE_FALSE(result.ok());
  CHECK(texts_of(result) == Strings{"Cannot open input file -annotate.lef (No such file or directory)."});
}

TEST_CASE("lef read without a file name prints the help", "[cap:lef-read][req:lef read arguments]") {
  Session s;
  REQUIRE(s.run("lef read").ok());
  const auto printed = s.take_printed();
  REQUIRE_FALSE(printed.empty());
  CHECK(printed.front().starts_with("The \"lef\" options are:"));
}

TEST_CASE("the DEF-only options are reported and ignored", "[cap:lef-read][req:lef read arguments]") {
  Files files{"defonly"};
  const std::string lef = files.write("cells.lef", kCells);
  Session s;
  REQUIRE(s.run("lef read " + lef + " -labels -noblockage -import").ok());
  const auto printed = s.take_printed();
  CHECK(contains(printed, "The \"-labels\" option is only for def read"));
  CHECK(contains(printed, "The \"-noblockage\" option is only for def read"));
  CHECK(s.state.cells.find("INV") != nullptr);
}

// --- lef datestamp -------------------------------------------------------------------

TEST_CASE("Fixed stamp applied", "[cap:lef-read][req:lef datestamp]") {
  Files files{"stamp"};
  const std::string lef = files.write("cells.lef", kCells);
  Session s;
  const auto initial = s.run("lef datestamp");
  REQUIRE(initial.ok());
  CHECK(initial.value == cmd::Value{std::string{"default"}});
  REQUIRE(s.run("lef datestamp 1600000000").ok());
  CHECK(s.run("lef datestamp").value == cmd::Value{std::int64_t{1600000000}});
  REQUIRE(s.run("lef read " + lef).ok());
  const db::CellDef* inv = s.state.cells.find("INV");
  REQUIRE(inv != nullptr);
  CHECK(inv->timestamp() == 1600000000);
  CHECK(inv->flags().fixed_timestamp);
  REQUIRE(s.run("lef datestamp default").ok());
  CHECK_FALSE(s.state.lefdef.datestamp.has_value());
}

TEST_CASE("Invalid stamp", "[cap:lef-read][req:lef datestamp]") {
  Session s;
  REQUIRE(s.run("lef datestamp 5").ok());
  s.take_printed();
  REQUIRE(s.run("lef datestamp yesterday").ok());
  auto printed = s.take_printed();
  REQUIRE(printed.size() >= 2);
  CHECK(printed[0] == "Unrecognizable date stamp \"yesterday\".");
  CHECK(printed[1].starts_with("The \"lef\" options are:"));
  CHECK(s.state.lefdef.datestamp == 5);
  REQUIRE(s.run("lef datestamp 1 2").ok());
  printed = s.take_printed();
  REQUIRE_FALSE(printed.empty());
  CHECK(printed[0].starts_with("The \"lef\" options are:"));
  CHECK(s.state.lefdef.datestamp == 5);
  // -1 is Magic's "default".
  REQUIRE(s.run("lef datestamp -1").ok());
  CHECK_FALSE(s.state.lefdef.datestamp.has_value());
}

// --- undo ----------------------------------------------------------------------------

TEST_CASE("undo after a lef read removes every created cell", "[cap:lef-read][req:MACRO and SITE cell creation]") {
  Files files{"undo"};
  const std::string lef =
      files.write("cells.lef", std::string{kInv} + "SITE core\n  SIZE 1 BY 1 ;\nEND core\nEND LIBRARY\n");
  Session s;
  REQUIRE(s.run("lef read " + lef).ok());
  REQUIRE(s.state.cells.find("INV") != nullptr);
  REQUIRE(s.state.cells.find("core") != nullptr);
  REQUIRE(s.run("undo").ok());
  CHECK(s.state.cells.find("INV") == nullptr);
  CHECK(s.state.cells.find("core") == nullptr);
  REQUIRE(s.run("redo").ok());
  const db::CellDef* inv = s.state.cells.find("INV");
  REQUIRE(inv != nullptr);
  CHECK(fixed_bbox(*inv) == db::Rect{0, 0, 2, 4});
  REQUIRE(inv->labels().size() == 1);
  CHECK(inv->labels().front().text == "A");
  CHECK(tiles(*inv, "metal1") == std::vector<db::Rect>{{0, 0, 1, 1}});
  CHECK(s.state.cells.find("core") != nullptr);
}

TEST_CASE("Undo a LEF annotation", "[cap:layout-undo][req:Undoable LEF and DEF reads]") {
  Files files{"undo-annotate"};
  const std::string lef = files.write(
      "inv.lef", "MACRO inv\n  PIN B\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n  END B\n"
                 "  PIN A\n    DIRECTION INPUT ;\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n"
                 "  END A\n  SIZE 3 BY 3 ;\nEND inv\n");
  Session s;
  auto made = s.state.cells.create("inv", technology()->plane_count());
  REQUIRE(made.ok());
  db::CellDef& inv = *made.value();
  inv.flags().available = true;
  REQUIRE(db::paint_type(inv, *technology(), db::Rect{0, 0, 2, 2}, type_of("metal1")).ok());
  db::Label a = label("A", "metal1", db::Rect{0, 0, 2, 2});
  a.port_directions = db::kPortNorth;
  a.port_index = 1;
  REQUIRE(db::put_label(inv, a).ok());
  const std::vector<db::Label> before = inv.labels();
  s.state.undo.clear();  // the set-up is not part of the read's step
  REQUIRE(s.run("lef read " + lef + " -annotate").ok());
  CHECK(inv.labels().size() == 2);
  CHECK(fixed_bbox(inv) == db::Rect{0, 0, 3, 3});
  REQUIRE(s.run("undo").ok());
  CHECK(inv.labels() == before);
  CHECK_FALSE(fixed_bbox(inv).has_value());
  REQUIRE(s.run("redo").ok());
  CHECK(inv.labels().size() == 2);
  CHECK(fixed_bbox(inv) == db::Rect{0, 0, 3, 3});
}

TEST_CASE("undo keeps a created cell that a window shows", "[cap:lef-read][req:MACRO and SITE cell creation]") {
  Files files{"undo-shown"};
  const std::string lef = files.write("cells.lef", kCells);
  Session s;
  REQUIRE(s.run("lef read " + lef).ok());
  db::CellDef* inv = s.state.cells.find("INV");
  REQUIRE(inv != nullptr);
  s.state.edit_cell = inv;
  REQUIRE(s.run("undo").ok());
  REQUIRE(s.state.cells.find("INV") == inv);
  CHECK(inv->labels().empty());
  CHECK_FALSE(fixed_bbox(*inv).has_value());
  s.state.edit_cell = nullptr;
}

TEST_CASE("Q-LEFDEF-22: the read prints no undo warning", "[cap:lef-read][req:lef read side effects]") {
  Files files{"no-warning"};
  const std::string lef = files.write("cells.lef", kCells);
  Session s;
  const auto result = s.run("lef read " + lef);
  for (const auto& text : texts_of(result)) {
    CHECK(text.find("cannot be undone") == std::string::npos);
  }
  CHECK(s.state.undo.step_count() == 1);
}

TEST_CASE("lef_candidate names the last try", "[cap:lef-read][req:lef read side effects]") {
  CHECK(lef::lef_candidate("cells") == "cells.lef");
  CHECK(lef::lef_candidate("cells.lef") == "cells.lef");
  CHECK(lef::lef_candidate("tech.tlef") == "tech.lef");
  CHECK(lef::lef_candidate("dir.v2/cells") == "dir.v2/cells.lef");
}
