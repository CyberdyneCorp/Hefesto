// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the antenna unit tests (capability antenna-check): a small
// technology with poly, a gate type `nfet`, a poly contact `pc`, and two
// metal-to-metal contacts on metal1 (`capc`, declared first, and `via1`)
// and `via2` on metal2; a cifoutput style with `squares 10 20 20` cuts for
// via1 (scalefactor 10); and an extraction style with a plane order, the
// antenna rules given by the test and a `device mosfet nfet` line. The
// session loads it through `tech load` (which installs the extraction and
// cifoutput styles), builds cells in memory, and opens a window on `top`.
//
// Owner: ANTENNA.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/antenna/antenna.hpp"
#include "hefesto/antenna/measure.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace antenna_test {

namespace antenna = hefesto::antenna;
namespace cmd = hefesto::cmd;
namespace connect = hefesto::connect;
namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace extract = hefesto::extract;
namespace platform = hefesto::platform;
namespace tech = hefesto::tech;
namespace view = hefesto::view;

inline const std::string kOrder =
    " planeorder well 0\n planeorder active 1\n planeorder metal1 2\n planeorder metal2 3\n planeorder metal3 4\n";

inline const std::string kDevice = " device mosfet nfet nfet ndiff pwell,space gnd!\n";

/// The technology with `extract` as the body of its extract section (after
/// `style anttest`), `cif` as its cifoutput style body.
inline std::string tech_text(const std::string& extract,
                             const std::string& cif = " layer CV1 via1\n  squares 10 20 20\n") {
  return "tech\n format 35\n anttest\nend\n"
         "planes\n well,w\n active,a\n metal1,m1\n metal2,m2\n metal3,m3\nend\n"
         "types\n well pwell,pw\n active ndiff,nd\n active poly,p\n active nfet\n active pc\n"
         " metal1 metal1,m1\n metal1 capc\n metal1 via1,v1\n metal2 metal2,m2\n metal2 via2,v2\n"
         " metal3 metal3,m3\nend\n"
         "contact\n pc poly metal1\n capc metal1 metal2\n via1 metal1 metal2\n via2 metal2 metal3\nend\n"
         "styles\n styletype mos\nend\n"
         "compose\nend\n"
         "connect\n poly nfet,pc\n metal1 pc,capc,via1\n metal2 capc,via1,via2\n metal3 via2\n pwell ndiff\nend\n"
         "cifoutput\nstyle lambda\n scalefactor 10\n" +
         cif + "end\ncifinput\nend\ndrc\nend\nextract\n style anttest\n" + extract + "end\n";
}

/// A partial surface model with metal1 and metal2 rules, ndiff tie-downs.
inline std::string partial_rules(const std::string& extra = "") {
  return kOrder + " model partial surface\n antenna metal1 surface 400 none\n antenna metal2 surface 400 none\n"
                  " tiedown ndiff\n" +
         kDevice + extra;
}

/// A command session with the technology loaded and, unless `window` is
/// false, a window on the cell `top`.
struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  db::CellDef* top = nullptr;

  explicit Session(const std::string& technology = tech_text(partial_rules()), bool magic_compat = true,
                   bool window = true) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
    REQUIRE(files().write_atomic("/t/anttest.tech", technology).ok());
    REQUIRE(run("tech load /t/anttest.tech -noprompt").ok());
    REQUIRE(editor.extract.current != nullptr);
    auto made = editor.cells.create("top", editor.technology->plane_count());
    REQUIRE(made.ok());
    top = made.value();
    top->flags().available = true;
    if (window) {
      REQUIRE(editor.windows.open(top, view::ScreenRect{0, 0, 999, 999}).ok());
    }
    (void)printed.take();
  }

  platform::FileSystem& files() { return services.view().files; }
  const tech::Technology& technology() const { return *editor.technology; }

  tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  tech::PlaneId plane(std::string_view name) const {
    const auto found = editor.technology->find_plane(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  void paint(std::string_view type_name, const db::Rect& r) {
    REQUIRE(db::paint_type(*top, *editor.technology, r, type(type_name)).ok());
    db::recompute_bbox(*top);
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  /// The texts printed since the last call.
  std::vector<std::string> take() {
    std::vector<std::string> out;
    for (const auto& d : printed.take()) {
      out.push_back(d.text);
    }
    return out;
  }

  antenna::RuleView rules() const { return antenna::RuleView::from_style(*editor.extract.current); }

  /// The measurement inputs over the session's styles.
  antenna::MeasureSetup setup(const antenna::RuleView& view, bool magic_compat = true) const {
    const auto style = editor.mask.output_styles.current_shared();
    antenna::CutLookup cuts = [style](tech::TileType t) -> std::optional<antenna::CutRule> {
      const auto cut = style ? style->contact_cut(t) : std::nullopt;
      if (!cut) {
        return std::nullopt;
      }
      return antenna::CutRule{cut->edge, cut->spacing, cut->border, style->scale.scale_factor};
    };
    return antenna::MeasureSetup{editor.technology.get(), &view, cuts, magic_compat};
  }

  /// The net through `start` on `gate`'s connectivity, traced for `check`.
  antenna::TracedNet trace(const db::Rect& start, std::string_view gate, const antenna::RuleView& view,
                           const antenna::CheckPlane& check, bool magic_compat = true) const {
    antenna::TracedNet net(*editor.technology);
    struct Collect : connect::NetVisitor {
      antenna::TracedNet* net = nullptr;
      bool on_tile(const connect::NetTile& t) override { return net->add(t).ok(); }
    } collect;
    collect.net = &net;
    connect::SearchOptions options;
    options.magic_compat = magic_compat;
    options.connect = antenna::restricted_connect(view, *editor.technology, check, magic_compat);
    const connect::SearchStart s{start, editor.technology->connects_to(type(gate))};
    REQUIRE(connect::search_hierarchical(*top, *editor.technology, s, options, collect).ok());
    return net;
  }

  /// Writes `<cell>.ext` where the run reads it.
  void ext(const std::string& cell, const std::string& text) {
    REQUIRE(files().write_atomic(cell + ".ext", text).ok());
  }

  /// antennacheck's run through check(), its console lines collected.
  cmd::CommandResult check(std::vector<std::string> args, std::vector<std::string>& lines, bool debug = false,
                           bool magic_compat = true, bool with_root = true) {
    antenna::AntennaOptions options;
    options.args = std::move(args);
    options.debug = debug;
    options.magic_compat = magic_compat;
    options.files = &files();
    options.emit = [&lines](diag::Diagnostic d) { lines.push_back(d.text); };
    return antenna::check(editor, with_root ? top : nullptr, options);
  }
};

/// The `.ext` header of the hand-written inputs.
inline std::string ext_header() {
  return "timestamp 0\nversion 8.3\ntech anttest\nstyle anttest\n";
}

inline bool has(const std::vector<std::string>& lines, std::string_view text) {
  return std::find(lines.begin(), lines.end(), text) != lines.end();
}

}  // namespace antenna_test
