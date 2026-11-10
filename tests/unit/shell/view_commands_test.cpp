// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The command bar's binding to the core command API: lines become
// Invocations with origin `gui`, run through the core Dispatcher, and the
// M0 view commands are ordinary registered commands.

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/context.hpp"
#include "hefesto/shell/command_bar_model.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/screen_models.hpp"
#include "hefesto/shell/view_commands.hpp"
#include "hefesto/shell/view_models.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

platform::PortableOptions test_options() {
  return platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true, .inline_tasks = true};
}

struct Recorder {
  ThemeMode theme = ThemeMode::dark;
  double pan_x = 0.0;
  double pan_y = 0.0;
  std::string paint;
  std::map<PanelId, bool> shown{{PanelId::layers, true}, {PanelId::inspector, true}, {PanelId::console, false}};

  ViewActions actions() {
    return ViewActions{[this](ThemeMode m) { theme = m; },
                       [this](double dx, double dy) {
                         pan_x = dx;
                         pan_y = dy;
                       },
                       [this](PanelId id, std::optional<bool> show) { return shown[id] = show.value_or(!shown[id]); },
                       [this](PanelId id) { return shown[id]; },
                       {},
                       {},
                       [this](std::optional<std::string> name) -> std::optional<std::string> {
                         if (name && *name == "nosuch") return std::nullopt;
                         if (name) paint = *name;
                         return paint;
                       }};
  }
};

class OriginRecorder final : public cmd::CommandObserver {
 public:
  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& /*result*/) override {
    seen.push_back(invocation);
  }
  std::vector<cmd::Invocation> seen;
};

bool has_code(const CommandOutcome& outcome, std::string_view code) {
  for (const std::string& d : outcome.diagnostics) {
    if (d.find(code) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("panel collapses and restores the main window's panels through the command API",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  CoreSession core(test_options());
  Recorder recorder;
  REQUIRE(register_view_commands(core.registry(), recorder.actions()));
  CoreCommandSink sink(core.dispatcher());
  CHECK(sink.submit("panel layers hide").message == "hidden");
  CHECK_FALSE(recorder.shown[PanelId::layers]);
  CHECK(sink.submit("panel layers").message == "hidden");
  CHECK(sink.submit("panel layers toggle").message == "shown");
  CHECK(sink.submit("panel console show").message == "shown");
  CHECK(sink.submit("panel inspector hide").message == "hidden");
  CHECK_FALSE(sink.submit("panel nosuch").ok());
  CHECK_FALSE(sink.submit("panel layers sideways").ok());
  // Without panel actions (another front end) the command is not registered.
  CoreSession other(test_options());
  REQUIRE(register_view_commands(other.registry(), ViewActions{[](ThemeMode) {}, [](double, double) {}, {}, {}, {}, {}, {}}));
  CHECK_FALSE(CoreCommandSink(other.dispatcher()).submit("panel layers").ok());
}

TEST_CASE("palette paint sets the paint layer through the command API",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  CoreSession core(test_options());
  Recorder recorder;
  REQUIRE(register_view_commands(core.registry(), recorder.actions()));
  CoreCommandSink sink(core.dispatcher());
  CHECK(sink.submit("palette paint").message == "none");
  CHECK(sink.submit("palette paint metal2").message == "metal2");
  CHECK(recorder.paint == "metal2");
  CHECK_FALSE(sink.submit("palette paint nosuch").ok());
  CHECK(recorder.paint == "metal2");
  CHECK(sink.submit("palette paint none").message == "none");
  CHECK_FALSE(sink.submit("palette color").ok());
  // Logged and replayable like every UI action.
  const auto log = core.command_log().history();
  CHECK(std::find(log.begin(), log.end(), "palette paint metal2") != log.end());
}

TEST_CASE("view commands are core commands dispatched from the command bar",
          "[cap:hefesto-architecture][req:Single command API]") {
  CoreSession core(test_options());
  // design.md D5: `view` and `zoom` are the core's (register_all_commands);
  // the shell only adds `theme` and `pan`.
  REQUIRE(core.registry().find("zoom") != nullptr);
  REQUIRE(core.registry().find("view") != nullptr);
  Recorder rec;
  REQUIRE(register_view_commands(core.registry(), rec.actions()));
  CoreCommandSink sink(core.dispatcher());

  CHECK(sink.submit("theme light").ok());
  CHECK(rec.theme == ThemeMode::light);
  CHECK(sink.submit("pan -12.5 40").ok());
  CHECK(rec.pan_x == -12.5);
  CHECK(rec.pan_y == 40.0);

  for (const char* bad : {"theme blue", "pan 1", "pan a b"}) {
    const CommandOutcome out = sink.submit(bad);
    CHECK_FALSE(out.ok());
    CHECK(has_code(out, "CMD-USAGE-001"));
  }

  // `help` prints Magic's per-client listing (ui-command-dispatch "help
  // command") as a report, which the console shows: Magic's own help text
  // with magic-compat on, the shell's commands too with it off.
  const CommandOutcome help = sink.submit("help");
  REQUIRE(help.ok());
  const std::string listing = console_result(help, help.message);
  CHECK(listing.find("zoom") != std::string::npos);
  CHECK(listing.find("version") != std::string::npos);  // core built-ins are there too
  core.project().compat.magic_compat = false;
  const CommandOutcome native = sink.submit("help");
  REQUIRE(native.ok());
  CHECK(console_result(native, native.message).find("theme dark|light") != std::string::npos);
  CHECK_FALSE(register_view_commands(core.registry(), rec.actions()));  // names already taken
}

TEST_CASE("the core zoom moves the current window, so the canvas follows it",
          "[cap:ui-windows-and-views][req:View mapping]") {
  CoreSession core(test_options());
  auto& windows = core.editor().windows;
  auto opened = windows.open(nullptr, view::ScreenRect{0, 0, 799, 599});
  REQUIRE(opened.ok());
  windows.set_current(opened.value()->id());
  const db::Rect before = windows.current()->mapping().surface;
  CoreCommandSink sink(core.dispatcher());
  REQUIRE(sink.submit("zoom 0.5").ok());
  CHECK(windows.current()->mapping().surface.width() < before.width());
  CHECK_FALSE(sink.submit("zoom -1").ok());
}

TEST_CASE("command bar lines run through the core Dispatcher with origin gui",
          "[cap:hefesto-architecture][req:Single command API]") {
  CoreSession core(test_options());
  OriginRecorder observer;
  core.dispatcher().add_observer(observer);
  CoreCommandSink sink(core.dispatcher());
  CommandBarModel bar(sink);

  const CommandOutcome echo = bar.submit("  echo a {b c}; echo last ");
  REQUIRE(echo.ok());
  CHECK(echo.message == "last");
  REQUIRE(observer.seen.size() == 2);
  CHECK(observer.seen[0].name == "echo");
  CHECK(observer.seen[0].args == std::vector<std::string>{"a", "b c"});
  CHECK(observer.seen[0].origin == cmd::Origin::gui);
}

// Regression (UI review): `help; paint nosuchlayer` showed only the paint
// error in the Console; the help text was overwritten by the failure.
TEST_CASE("the console keeps the results of a line's commands before a failure",
          "[cap:hefesto-diagnostics][req:Diagnostic presentation and logging]") {
  CoreSession core(test_options());
  CoreCommandSink sink(core.dispatcher());
  const CommandOutcome kept = sink.submit("echo kept text");
  REQUIRE(kept.ok());
  CHECK(kept.output == "kept text");
  CHECK(console_result(kept, kept.message) == kept.output);  // printed once

  const CommandOutcome line = sink.submit("echo kept text; paint nosuchlayer");
  CHECK_FALSE(line.ok());
  CHECK(line.output == kept.output);
  const std::string console = console_result(line, line.message);
  CHECK(console.find("kept text") != std::string::npos);
  REQUIRE_FALSE(line.message.empty());
  CHECK(console.find(line.message) != std::string::npos);  // the failure follows the result
  CHECK(console.find("kept text") < console.find(line.message));

  const CommandOutcome echoes = sink.submit("echo a; echo b");
  CHECK(echoes.output == "a\nb");
  CHECK(console_result(echoes, echoes.message) == "a\nb");
}

TEST_CASE("command bar failures carry core diagnostics",
          "[cap:hefesto-diagnostics][req:Command results and errors]") {
  CoreSession core(test_options());
  CoreCommandSink sink(core.dispatcher());

  // `paint` is a registered M1 command now; use a name no area registers.
  const CommandOutcome unknown = sink.submit("nosuchcommand m1; echo never");
  CHECK_FALSE(unknown.ok());
  CHECK(has_code(unknown, "CMD-DISPATCH-001"));
  CHECK(unknown.message.find("nosuchcommand") != std::string::npos);

  const CommandOutcome parse = sink.submit("echo \"unterminated");
  CHECK_FALSE(parse.ok());
  CHECK(has_code(parse, "CMD-PARSE-001"));

  CHECK_FALSE(core.quit_requested());
  CHECK(sink.submit("quit").ok());
  CHECK(core.quit_requested());
}

TEST_CASE("the desktop session keeps a rotating log and a command log",
          "[cap:hefesto-diagnostics][req:Diagnostic presentation and logging]") {
  CoreSession core(test_options(), platform::Path{"/app/logs"});
  CHECK_FALSE(core.log_warning());
  CoreCommandSink sink(core.dispatcher());
  CHECK(sink.submit("echo logged").ok());
  CHECK_FALSE(sink.submit("nosuch").ok());
  CHECK(core.command_log().history() == std::vector<std::string>{"echo logged", "nosuch"});
  const std::string log = core.files().read("/app/logs/hefesto.log").value();
  CHECK(log.find("info session start hefesto ") != std::string::npos);
  CHECK(log.find("info cmd[gui] echo logged -> ok") != std::string::npos);
  CHECK(log.find("CMD-DISPATCH-001") != std::string::npos);
}

TEST_CASE("a hierarchy row expands one instance one level through core commands",
          "[cap:ui-windows-and-views][req:Per-window expansion state]") {
  // Regression: the row ran `box values <bbox>; expand surround`, which
  // expands every instance inside the box at any depth (the whole subtree).
  // It now runs `expand instance <path>` on its own instance.
  CoreSession core(test_options());
  editor::EditorState& ed = core.editor();
  db::CellDef top("top", 4);
  db::CellDef mid("mid", 4);
  db::CellDef leaf("leaf", 4);
  const auto add = [](db::CellDef& parent, db::CellDef& child, const char* id, db::Rect bbox, db::Transform t) {
    auto use = std::make_unique<db::CellUse>();
    use->id = id;
    use->def = &child;
    use->parent = &parent;
    use->bbox = bbox;
    use->transform = t;
    parent.uses().push_back(std::move(use));
    return parent.uses().back().get();
  };
  db::CellUse* inner = add(mid, leaf, "L1", db::Rect{0, 0, 10, 20}, db::kIdentity);
  db::CellUse* x1 = add(top, mid, "X1", db::Rect{100, 200, 150, 260}, db::Transform{1, 0, 100, 0, 1, 200});
  db::CellUse* x2 = add(top, mid, "X2", db::Rect{300, 200, 350, 260}, db::Transform{1, 0, 300, 0, 1, 200});
  ed.edit_cell = &top;
  ed.box = db::Rect{0, 0, 4, 4};
  auto opened = ed.windows.open(&top, view::ScreenRect{0, 0, 999, 999});
  REQUIRE(opened);
  view::LayoutWindow& win = *opened.value();
  ed.windows.set_current(win.id());

  const auto rows = hierarchy_rows(top, [&win](const db::CellUse& u) { return win.is_expanded(u); });
  REQUIRE(rows.size() == 3);
  CoreCommandSink sink(core.dispatcher());
  CHECK(rows[1].toggle_command() == "expand instance X1");
  const CommandOutcome out = sink.submit(rows[1].toggle_command());
  INFO(out.message);
  REQUIRE(out.status == OutcomeStatus::ok);
  CHECK(win.is_expanded(*x1));
  CHECK_FALSE(win.is_expanded(*inner));  // one level only
  CHECK_FALSE(win.is_expanded(*x2));     // other instances untouched
  CHECK(ed.box == db::Rect{0, 0, 4, 4});  // the box is never touched

  const auto open = hierarchy_rows(top, [&win](const db::CellUse& u) { return win.is_expanded(u); });
  REQUIRE(open.size() == 4);
  CHECK(open[2].toggle_command() == "expand instance X1/L1");
  REQUIRE(sink.submit(open[1].toggle_command()).status == OutcomeStatus::ok);
  CHECK_FALSE(win.is_expanded(*x1));
}

TEST_CASE("a hierarchy row whose child covers it expands one level and keeps the box",
          "[cap:ui-windows-and-views][req:Instance-addressed expansion]") {
  // Regression: with box-based `expand surround`, a wrapper whose only child
  // spans its whole bbox either collapsed again or stayed expanded two
  // levels, and with no visible box the box was left on the last child.
  CoreSession core(test_options());
  editor::EditorState& ed = core.editor();
  db::CellDef top("top", 4);
  db::CellDef wrap("wrap", 4);
  db::CellDef leaf("leaf", 4);
  const auto add = [](db::CellDef& parent, db::CellDef& child, const char* id, db::Rect bbox, db::Transform t) {
    auto use = std::make_unique<db::CellUse>();
    use->id = id;
    use->def = &child;
    use->parent = &parent;
    use->bbox = bbox;
    use->transform = t;
    parent.uses().push_back(std::move(use));
    return parent.uses().back().get();
  };
  db::CellUse* l1 = add(wrap, leaf, "L1", db::Rect{0, 0, 50, 60}, db::kIdentity);
  db::CellUse* w1 = add(top, wrap, "W1", db::Rect{100, 200, 150, 260}, db::Transform{1, 0, 100, 0, 1, 200});
  ed.edit_cell = &top;
  ed.box.reset();  // no visible box
  auto opened = ed.windows.open(&top, view::ScreenRect{0, 0, 999, 999});
  REQUIRE(opened);
  view::LayoutWindow& win = *opened.value();
  ed.windows.set_current(win.id());

  const auto rows = hierarchy_rows(top, [&win](const db::CellUse& u) { return win.is_expanded(u); });
  REQUIRE(rows.size() == 2);
  CoreCommandSink sink(core.dispatcher());
  REQUIRE(sink.submit(rows[1].toggle_command()).status == OutcomeStatus::ok);
  CHECK(win.is_expanded(*w1));
  CHECK_FALSE(win.is_expanded(*l1));  // one level, although L1 covers W1
  CHECK_FALSE(ed.box.has_value());    // no box was created
}

// Regression (M2 review): `specialopen color` succeeded silently in the
// desktop app; the color window screen (task 10.6) now shows the window it
// opens, so the command no longer warns.
TEST_CASE("opening a color window shows it on the color window screen",
          "[cap:ui-color-window][req:Color window creation and layout]") {
  CoreSession core(test_options());
  core.editor().styles.emplace();
  core.editor().styles->colors.entries.resize(8);
  CoreCommandSink sink(core.dispatcher());
  const CommandOutcome opened = sink.submit("specialopen color 3");
  REQUIRE(opened.ok());
  CHECK(opened.printed.find("not available in the desktop app") == std::string::npos);
  const auto shown = color_window_view(core.editor());
  REQUIRE(shown.has_value());
  CHECK(shown->caption == "COLOR = 003");
}

// D10 M1 gap: toggling a hierarchy row must not expand an overlapping
// sibling (the M1 box-based `expand` expanded every instance under the box).
TEST_CASE("a hierarchy row toggles its instance only, not an overlapping sibling",
          "[cap:ui-windows-and-views][req:Instance-addressed expansion]") {
  CoreSession core(test_options());
  editor::EditorState& ed = core.editor();
  db::CellDef top("top", 4);
  db::CellDef a("a", 4);
  db::CellDef b("b", 4);
  const auto add = [&top](db::CellDef& child, const char* id, db::Rect bbox) {
    auto use = std::make_unique<db::CellUse>();
    use->id = id;
    use->def = &child;
    use->parent = &top;
    use->bbox = bbox;
    use->transform = db::Transform{1, 0, bbox.x0, 0, 1, bbox.y0};
    top.uses().push_back(std::move(use));
    return top.uses().back().get();
  };
  db::CellUse* x1 = add(a, "X1", db::Rect{100, 100, 200, 200});
  db::CellUse* x2 = add(b, "X2", db::Rect{150, 150, 250, 250});  // overlaps X1
  ed.edit_cell = &top;
  auto opened = ed.windows.open(&top, view::ScreenRect{0, 0, 999, 999});
  REQUIRE(opened);
  view::LayoutWindow& win = *opened.value();
  ed.windows.set_current(win.id());
  const auto rows = hierarchy_rows(top, [&win](const db::CellUse& u) { return win.is_expanded(u); });
  REQUIRE(rows.size() == 3);
  const auto row_of = [&rows](const db::CellUse* use) {
    return *std::find_if(rows.begin(), rows.end(), [use](const HierarchyRow& r) { return r.use == use; });
  };
  CoreCommandSink sink(core.dispatcher());
  REQUIRE(sink.submit(row_of(x1).toggle_command()).status == OutcomeStatus::ok);
  CHECK(win.is_expanded(*x1));
  CHECK_FALSE(win.is_expanded(*x2));
  REQUIRE(sink.submit(row_of(x2).toggle_command()).status == OutcomeStatus::ok);
  REQUIRE(sink.submit(row_of(x1).toggle_command().replace(0, 6, "unexpand")).status == OutcomeStatus::ok);
  CHECK_FALSE(win.is_expanded(*x1));
  CHECK(win.is_expanded(*x2));  // collapsing X1 leaves its overlapping sibling open
}
