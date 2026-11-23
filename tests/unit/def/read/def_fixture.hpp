// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the def/read unit tests: tests/data/def/read/def.tech with a
// test's own cifoutput section, DRC widths of 140 for metal1 and metal2,
// and the lef section
//   routing metal1 met1 / routing metal2 met2 / cut m2c via1 /
//   obstruction obsm1 met1;
// a command session loaded with it (`tech load`, so the LEF table, the DRC
// rules and the styles are installed as in a real session), a `defr`
// command calling def::read_command (the shared `lef`/`def` dispatcher
// calls it once INTEGRATION wires it), and DEF reads from memory.
//
// Without an output style the DEF scale s0 is 1, so DEF numbers are
// internal units when the file has no UNITS statement (oscale = 1).
//
// Owner: DEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/def/read/command.hpp"
#include "hefesto/def/read/read.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"

namespace Catch {
template <>
struct StringMaker<hefesto::db::Rect> {
  static std::string convert(const hefesto::db::Rect& r) {
    return "(" + std::to_string(r.x0) + "," + std::to_string(r.y0) + ")-(" + std::to_string(r.x1) + "," +
           std::to_string(r.y1) + ")";
  }
};
}  // namespace Catch

namespace def_read_test {

namespace db = hefesto::db;
namespace def = hefesto::def;
namespace diag = hefesto::diag;
namespace tech = hefesto::tech;
namespace cmd = hefesto::cmd;
namespace lef = hefesto::lef;

inline constexpr std::string_view kDrc = R"(
drc
    width metal1 140 "metal1 width < 140"
    width metal2 140 "metal2 width < 140"
end
)";

inline constexpr std::string_view kLef = R"(
lef
    routing metal1 met1
    routing metal2 met2
    cut m2c via1
    obstruction obsm1 met1
end
)";

inline std::string tech_text(std::string_view cifoutput, std::string_view lef_section = kLef) {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "def.tech", std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  std::string text = out.str();
  text += "\ncifoutput\n";
  text += cifoutput;
  text += "\nend\n\ncifinput\nend\n";
  text += kDrc;
  text += lef_section;
  text += "\nextract\nend\n";
  return text;
}

/// `def read` through def::read_command.
inline cmd::CommandResult defr_handler(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  return def::read_command(ctx, invocation);
}

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  std::vector<diag::Diagnostic> messages;  // of the last read
  def::DefReadOptions options;

  explicit Session(bool magic_compat = true, std::string_view cifoutput = {}, std::string_view lef_section = kLef) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    REQUIRE(!registry
                 .add(cmd::CommandSpec{"defr", "defr read <file>", "def read (DEFR tests)", defr_handler},
                      cmd::kLayoutClient)
                 .has_value());
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
    file("/t/def.tech", tech_text(cifoutput, lef_section));
    const auto loaded = run("tech load /t/def.tech -noprompt");
    for (const auto& d : printed.snapshot()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
    REQUIRE(loaded.ok());
    editor.drc.compat = &project.compat;
    (void)printed.take();
  }

  void file(const std::string& path, const std::string& text) {
    REQUIRE(services.view().files.write_atomic(path, text).ok());
  }

  /// Runs a command line; returns the first failure, else the last result.
  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      if (!r.ok()) {
        return std::move(r);
      }
      last = std::move(r);
    }
    return last;
  }

  /// def::read of `text` (the typed name "test.def" unless set).
  cmd::CommandResult read(std::string_view text) {
    std::vector<std::byte> bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
    hefesto::gds::MemorySource source{std::move(bytes)};
    def::DefReadOptions o = options;
    if (o.typed_name.empty()) {
      o.typed_name = "test.def";
    }
    auto result = def::read(editor, source, "test.def", o);
    messages = result.diagnostics;
    return result;
  }

  [[nodiscard]] std::vector<std::string> texts() const {
    std::vector<std::string> out;
    for (const auto& d : messages) {
      out.push_back(d.text);
    }
    return out;
  }

  /// True when some message of the last read ends with `text`.
  [[nodiscard]] bool said(std::string_view text) const {
    return std::any_of(messages.begin(), messages.end(),
                       [&](const diag::Diagnostic& d) { return std::string_view{d.text}.ends_with(text); });
  }

  [[nodiscard]] std::size_t count(std::string_view text) const {
    return static_cast<std::size_t>(std::count_if(messages.begin(), messages.end(), [&](const diag::Diagnostic& d) {
      return std::string_view{d.text}.ends_with(text);
    }));
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  [[nodiscard]] db::CellDef& cell(std::string_view name) {
    db::CellDef* def = editor.cells.find(name);
    INFO("cell " << name);
    REQUIRE(def != nullptr);
    return *def;
  }

  /// The tiles of `type_name` in `cell_name` on its home plane, in
  /// enumeration order.
  [[nodiscard]] std::vector<db::Rect> painted(std::string_view cell_name, std::string_view type_name) {
    const db::CellDef& def = cell(cell_name);
    const tech::TileType t = type(type_name);
    std::vector<db::Rect> out;
    def.plane(*editor.technology->home_plane(t))
        .enumerate(db::whole_plane(), tech::TypeMask::of(t), [&](const db::TileView& v) {
          out.push_back(v.area);
          return true;
        });
    return out;
  }

  /// The type painted at the unit square at (x, y) on `type_name`'s plane.
  [[nodiscard]] bool covers(std::string_view cell_name, std::string_view type_name, const db::Rect& area) {
    const db::CellDef& def = cell(cell_name);
    const tech::TileType t = type(type_name);
    bool all = true;
    bool any = false;
    def.plane(*editor.technology->home_plane(t))
        .enumerate(area, tech::TypeMask::all(), [&](const db::TileView& v) {
          any = true;
          all = all && v.type == t;
          return true;
        });
    return any && all;
  }

  /// The bounding box of all `type_name` paint of `cell_name` (null when none).
  [[nodiscard]] db::Rect extent(std::string_view cell_name, std::string_view type_name) {
    std::optional<db::Rect> box;
    for (const db::Rect& r : painted(cell_name, type_name)) {
      box = box ? db::Rect{std::min(box->x0, r.x0), std::min(box->y0, r.y0), std::max(box->x1, r.x1),
                           std::max(box->y1, r.y1)}
                : r;
    }
    return box.value_or(db::Rect{});
  }

  [[nodiscard]] const db::Label* label(std::string_view cell_name, std::string_view text) {
    for (const db::Label& l : cell(cell_name).labels()) {
      if (l.text == text) {
        return &l;
      }
    }
    return nullptr;
  }

  /// Loads `count` copies of a one-glyph test font (rendering fonts exist).
  void add_fonts(int count) {
    constexpr std::string_view kFont = R"(%!PS-AdobeFont-1.0: Test
/FontBBox {0 0 1000 1000 }readonly def
/CharProcs 128 dict def
CharProcs begin
  /.notdef { 500 0 0 0 400 700 setcachedevice
	} bind def
currentdict end
/Test.Regular exch definefont
)";
    for (int i = 0; i < count; ++i) {
      auto font = hefesto::display::parse_outline_font(kFont, "Test.pt3");
      REQUIRE(font.ok());
      font.value().name = "Test" + std::to_string(i);
      editor.fonts.fonts.push_back(
          std::make_shared<const hefesto::display::OutlineFont>(std::move(font).value()));
    }
  }
};

}  // namespace def_read_test
