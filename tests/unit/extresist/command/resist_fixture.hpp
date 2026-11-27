// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the extresist command and output tests (capabilities
// extresist-command, extresist-output): a small technology (poly, nfet,
// ndiff, a poly contact and metal1) with an extraction style (rscale and
// cscale given by the test, `device mosfet nfet`), a command session that
// loads it, a cell `top` with a box, and `.ext` files in the session's
// memory file system. Owner: RESCMD.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extresist/command/command.hpp"
#include "hefesto/extresist/command/cutoff.hpp"
#include "hefesto/extresist/output/output.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace resist_test {

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace extract = hefesto::extract;
namespace extresist = hefesto::extresist;
namespace platform = hefesto::platform;
namespace tech = hefesto::tech;
namespace view = hefesto::view;

inline std::string tech_text(int rscale = 1, int cscale = 1) {
  return "tech\n format 35\n restest\nend\n"
         "planes\n active,a\n metal1,m1\nend\n"
         "types\n active ndiff,nd\n active poly,p\n active nfet\n active pc\n metal1 metal1,m1\nend\n"
         "contact\n pc poly metal1\nend\n"
         "styles\n styletype mos\nend\n"
         "compose\nend\n"
         "connect\n poly nfet,pc\n metal1 pc\nend\n"
         "cifoutput\nstyle lambda\n scalefactor 10\n layer CM1 metal1\nend\n"
         "cifinput\nend\ndrc\nend\n"
         "extract\n style restest\n cscale " +
         std::to_string(cscale) + "\n rscale " + std::to_string(rscale) +
         "\n lambda 1\n step 100\n planeorder active 0\n planeorder metal1 1\n"
         " device mosfet nfet nfet ndiff None GND\nend\n";
}

/// A command session with the technology loaded, a window on `top` and,
/// unless `box` is false, a box in it.
struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  db::CellDef* top = nullptr;

  explicit Session(bool magic_compat = true, bool box = true, const std::string& technology = tech_text()) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
    REQUIRE(files().write_atomic("/t/restest.tech", technology).ok());
    REQUIRE(run("tech load /t/restest.tech -noprompt").ok());
    REQUIRE(editor.extract.current != nullptr);
    auto made = editor.cells.create("top", editor.technology->plane_count());
    REQUIRE(made.ok());
    top = made.value();
    top->flags().available = true;
    REQUIRE(editor.windows.open(top, view::ScreenRect{0, 0, 999, 999}).ok());
    if (box) {
      editor.box = db::Rect{0, 0, 1, 1};
      editor.box_root = top;
    }
    (void)printed.take();
  }

  platform::FileSystem& files() { return services.view().files; }

  tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  std::vector<std::string> take() {
    std::vector<std::string> out;
    for (const auto& d : printed.take()) {
      out.push_back(d.text);
    }
    return out;
  }

  std::string file(const std::string& path) {
    auto text = files().read(platform::Path{path});
    return text.ok() ? text.value() : std::string{"<none>"};
  }
};

inline bool has(const std::vector<std::string>& lines, std::string_view text) {
  return std::find(lines.begin(), lines.end(), text) != lines.end();
}

/// Whether a printed message starts with `text` (the statistics are one
/// multi-line message).
inline bool has_prefix(const std::vector<std::string>& lines, std::string_view text) {
  return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.starts_with(text); });
}

/// The read of `text` as `<cell>.ext` (and `parents` by name) with the
/// session's technology and style.
inline diag::Outcome<extresist::InputModel> read_model(const Session& s, const std::string& text,
                                                       bool magic_compat = true,
                                                       std::vector<std::pair<std::string, std::string>> parents = {},
                                                       std::vector<std::string>* messages = nullptr) {
  extresist::InputOptions options;
  options.tech = s.editor.technology.get();
  options.style = s.editor.extract.current.get();
  options.magic_compat = magic_compat;
  options.load = [text, parents](std::string_view cell) -> std::optional<std::string> {
    if (cell == "top") {
      return text;
    }
    for (const auto& [name, body] : parents) {
      if (name == cell) {
        return body;
      }
    }
    return std::nullopt;
  };
  for (const auto& p : parents) {
    options.parents.push_back(p.first);
  }
  if (messages != nullptr) {
    options.emit = [messages](diag::Diagnostic d) { messages->push_back(d.text); };
  }
  return extresist::read_input("top", options);
}

}  // namespace resist_test
