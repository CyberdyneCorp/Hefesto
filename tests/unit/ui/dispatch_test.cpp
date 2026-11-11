// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Window clients, command tables, name lookup, dispatch, help and command
// line splitting (capability ui-command-dispatch; ui-wizard-and-debug
// "Wizard command visibility").

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/ui/client.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/ui/state.hpp"
#include "hefesto/ui/magic_help.hpp"
#include "ui_fixture.hpp"

namespace cmd = hefesto::cmd;
namespace ui = hefesto::ui;
using hefesto::test::UiSession;

namespace {

cmd::CommandSpec named(std::string name) {
  return cmd::CommandSpec{name, name, "test command", [name](cmd::CommandContext&, const cmd::Invocation& inv) {
                            std::string out = name;
                            for (const auto& a : inv.args) {
                              out += " " + a;
                            }
                            return cmd::CommandResult::success(out);
                          }};
}

}  // namespace

// --- Window client model ------------------------------------------------------

TEST_CASE("built-in clients and user-typed client names", "[cap:ui-command-dispatch][req:Window client model]") {
  const auto clients = ui::builtin_clients();
  REQUIRE(clients.size() == 5);
  CHECK(clients[0].name == "*window");
  CHECK(clients[0].hidden);
  CHECK(clients[3].optional_module);
  CHECK_FALSE(ui::client_available(clients[4]));
  CHECK(ui::client_available(clients[2]));

  // "Client prefix": `col` names the color client (case-sensitive prefix).
  const auto color = ui::find_client("col");
  REQUIRE(color.ok());
  CHECK(color.value()->name == "color");
  CHECK_FALSE(ui::find_client("Col").ok());
  // "Hidden client": `*window` is never matched; the window types are listed.
  const auto hidden = ui::find_client("*window");
  REQUIRE_FALSE(hidden.ok());
  CHECK(hidden.error().code == ui::codes::kClient);
  CHECK(hidden.error().text.find("\tlayout\n\tcolor\n\tnetlist\n\twind3d") != std::string::npos);
  CHECK(ui::window_types_listing() == "\tlayout\n\tcolor\n\tnetlist\n\twind3d");
  CHECK_FALSE(ui::find_client("").ok());  // ambiguous
  CHECK(ui::find_client_exact("*window") != nullptr);
}

// --- Command registration and help text -----------------------------------------

TEST_CASE("client tables keep strcmp order of the help strings",
          "[cap:ui-command-dispatch][req:Command registration and help text]") {
  cmd::CommandRegistry r;
  for (const char* name : {"zoom", "center", "view"}) {
    REQUIRE_FALSE(r.add(named(name), "*window").has_value());
  }
  const auto table = r.table("*window");
  REQUIRE(table.size() == 3);
  CHECK(table[0]->name == "center");
  CHECK(table[1]->name == "view");
  CHECK(table[2]->name == "zoom");
  // The same name may live in two tables; a duplicate in one table is refused.
  CHECK_FALSE(r.add(named("load"), "layout").has_value());
  CHECK_FALSE(r.add(named("load"), "color").has_value());
  CHECK(r.add(named("load"), "color").has_value());
  CHECK(r.clients() == std::vector<std::string>{"*window", "color", "layout"});
  // Magic's global window commands land in `*window` by default.
  CHECK(cmd::CommandRegistry::default_client("closewindow") == "*window");
  CHECK(cmd::CommandRegistry::default_client("*bypass") == "*window");
  CHECK(cmd::CommandRegistry::default_client("paint") == "layout");
  CHECK(cmd::CommandRegistry::help_string(cmd::CommandSpec{"paint", "layers", "", {}}) == "paint layers");
}

// --- Command name lookup ----------------------------------------------------------

TEST_CASE("command words match by unique prefix, exact name and any case",
          "[cap:ui-command-dispatch][req:Command name lookup][golden:UI-G-001][golden:UI-G-002][golden:UI-G-004]") {
  cmd::CommandRegistry r;
  for (const char* name : {"getcell", "ext", "extract", "edit", "expand", "zoom"}) {
    REQUIRE_FALSE(r.add(named(name), "layout").has_value());
  }
  // Unique abbreviation.
  auto found = r.lookup("getc", "layout");
  REQUIRE(found.status == cmd::LookupStatus::found);
  CHECK(found.spec->name == "getcell");
  // Exact name beats abbreviation.
  found = r.lookup("ext", "layout");
  REQUIRE(found.status == cmd::LookupStatus::found);
  CHECK(found.spec->name == "ext");
  CHECK(r.lookup("extr", "layout").spec->name == "extract");
  CHECK(r.lookup("e", "layout").status == cmd::LookupStatus::ambiguous);
  // Case-insensitive, `magic::` / `::magic::` ignored.
  CHECK(r.lookup("ZoOm", "layout").spec->name == "zoom");
  CHECK(r.lookup("::magic::zoom", "layout").spec->name == "zoom");
  CHECK(r.lookup("magic::getc", "layout").spec->name == "getcell");
  CHECK(r.lookup("nothing", "layout").status == cmd::LookupStatus::not_found);
  // An empty word is ambiguous in a table of two or more entries.
  CHECK(r.lookup("", "layout").status == cmd::LookupStatus::ambiguous);
  CHECK(cmd::match_name("", {"only"}).status == cmd::LookupStatus::found);
}

// --- Command dispatch across global and client tables -------------------------------

TEST_CASE("dispatch looks in *window and the client table",
          "[cap:ui-command-dispatch][req:Command dispatch across global and client tables][golden:UI-G-003]") {
  cmd::CommandRegistry r;
  REQUIRE_FALSE(r.add(named("zoom"), "*window").has_value());
  REQUIRE_FALSE(r.add(named("zoom"), "wind3d").has_value());
  REQUIRE_FALSE(r.add(named("zebra"), "layout").has_value());
  REQUIRE_FALSE(r.add(named("view"), "*window").has_value());
  REQUIRE_FALSE(r.add(named("viewall"), "layout").has_value());
  // "Client overrides global".
  auto found = r.resolve("zoom", "wind3d");
  REQUIRE(found.status == cmd::LookupStatus::found);
  CHECK(found.client == "wind3d");
  // "No window": layout and *window are searched.
  CHECK(r.resolve("zoom", "layout").client == "*window");
  CHECK(r.resolve("zeb", "layout").client == "layout");
  // An exact name in one table beats an abbreviation in the other.
  CHECK(r.resolve("view", "layout").spec->name == "view");
  CHECK(r.resolve("viewa", "layout").spec->name == "viewall");
  // Ambiguous in one table: the whole lookup is ambiguous.
  CHECK(r.resolve("z", "layout").status == cmd::LookupStatus::ambiguous);
  // Both tables match by abbreviation only: the pair is ambiguous.
  CHECK(r.resolve("vie", "layout").status == cmd::LookupStatus::ambiguous);
  // Window commands refused by the window: only the client table.
  CHECK(r.resolve("view", "layout", false).spec->name == "viewall");
}

TEST_CASE("the dispatcher reports unknown and ambiguous words and resolves the rest",
          "[cap:ui-command-dispatch][req:Command dispatch across global and client tables]") {
  UiSession s;
  const auto unknown = s.run("foo");
  REQUIRE_FALSE(unknown.ok());
  CHECK(unknown.diagnostics.size() == 1);  // no "Did you point to the correct window?"
  CHECK(unknown.diagnostics[0].code == hefesto::diag::codes::kCmdDispatch);
  // A line typed in the command bar may abbreviate (Q-UI-41).
  const auto ambiguous = s.run("p", cmd::Origin::gui);
  REQUIRE_FALSE(ambiguous.ok());
  CHECK(UiSession::first_error(ambiguous) == "UI-DISPATCH-001: That command abbreviation is ambiguous.");
  // Abbreviation and case: the handler runs with the resolved name.
  s.run("GETC foo", cmd::Origin::gui);
  CHECK(s.calls_of("getcell") == std::vector<std::string>{"getcell foo"});
  // An empty command is a no-op.
  CHECK(s.dispatcher->execute(cmd::Invocation{}).ok());
}

// Regression (golden UI-G-001..004, Q-UI-41): Magic's Tcl build resolves a
// script line by its exact Tcl command name.
TEST_CASE("script lines need exact command names; typed lines may abbreviate",
          "[cap:ui-command-dispatch][req:Command name lookup][golden:UI-G-001][golden:UI-G-004]") {
  UiSession s;
  for (const cmd::Origin origin : {cmd::Origin::script, cmd::Origin::cli, cmd::Origin::key}) {
    const auto r = s.run("getc a", origin);
    REQUIRE_FALSE(r.ok());
    CHECK(UiSession::first_error(r) == "CMD-DISPATCH-001: invalid command name \"getc\"");
    CHECK(UiSession::first_error(s.run("GETCELL a", origin)) == "CMD-DISPATCH-001: invalid command name \"GETCELL\"");
  }
  CHECK(s.calls_of("getcell").empty());
  // Exact names, with or without the magic:: namespace, run.
  CHECK(s.run("getcell a").ok());
  CHECK(s.run("::magic::getcell b").ok());
  CHECK(s.calls_of("getcell").size() == 2);
  // The command bar abbreviates unless the front end turns it off (the CLI does).
  CHECK(s.run("getc c", cmd::Origin::gui).ok());
  s.dispatcher->set_typed_abbreviations(false);
  CHECK_FALSE(s.run("getc d", cmd::Origin::gui).ok());
  CHECK(s.dispatcher->name_lookup(cmd::Origin::gui) == cmd::NameLookup::exact);
}

// Regression (golden UI-G-060): a command of another client only is a Tcl
// command in Magic, dispatched quietly: an empty error result.
TEST_CASE("a command of another client fails with an empty result",
          "[cap:ui-command-dispatch][req:Command dispatch across global and client tables][golden:UI-G-060]") {
  UiSession s;
  REQUIRE_FALSE(s.registry.add(named("hue"), "color").has_value());
  const auto r = s.run("hue");
  REQUIRE_FALSE(r.ok());
  CHECK(r.value.to_string().empty());
  CHECK_FALSE(r.value.is_none());
  CHECK(UiSession::first_error(r) == "CMD-DISPATCH-001: \"hue\" is not a command of the layout window");
}

TEST_CASE("a script stops at an unknown word unless each line is caught",
          "[cap:ui-command-dispatch][req:Command name lookup][golden:UI-G-001]") {
  UiSession s;
  hefesto::script::Interpreter& interp = hefesto::script::open_project_interpreter(s.editor, {});
  REQUIRE(hefesto::script::bind_registry(interp, s.registry, *s.dispatcher).ok());
  // Scripts are Tcl (m4-scripting tasks.md 13.2): an inexact name is not a command.
  const cmd::CommandResult stopped = interp.evaluate("getcell a\ngetc x\ngetcell b");
  REQUIRE_FALSE(stopped.ok());
  CHECK(stopped.diagnostics.front().text == "invalid command name \"getc\"");
  CHECK(s.calls_of("getcell").size() == 1);
  // The golden reference scripts catch every line, so the script goes on.
  CHECK(interp.evaluate("catch {getcell a}\ncatch {getc x}\ncatch {getcell b}").ok());
  CHECK(s.calls_of("getcell").size() == 3);
  hefesto::script::close_project_interpreter(s.editor);
}

// --- help command -------------------------------------------------------------------

TEST_CASE("help lists the global and the client table, by pattern or wizard",
          "[cap:ui-command-dispatch][req:help command][golden:UI-G-005]") {
  UiSession s;
  for (const char* name : {"closewindow", "openwindow", "windowborder"}) {
    REQUIRE_FALSE(s.registry.add(named(name)).has_value());
  }
  REQUIRE_FALSE(s.registry.add(named("*watch"), "layout").has_value());
  const std::string all = UiSession::printed(s.run("help"));
  CHECK(all.starts_with("\nGlobal Commands\n---------------\n"));
  CHECK(all.find("\nLayout Commands\n---------------\n") != std::string::npos);
  // Magic's help strings (golden UI-G-005).
  CHECK(all.find("\nzoom amount\t\tzoom window by amount") != std::string::npos);
  // "Pattern help": the pattern matches the whole help string.
  const std::string window = UiSession::printed(s.run("help window"));
  CHECK(window.find("closewindow") != std::string::npos);
  CHECK(window.find("openwindow") != std::string::npos);
  CHECK(window.find("windowborder") != std::string::npos);
  CHECK(window.find("zoom amount") != std::string::npos);  // "zoom window by amount"
  CHECK(window.find("redraw") == std::string::npos);
  const auto usage = s.run("help a b");
  REQUIRE_FALSE(usage.ok());
  CHECK(usage.diagnostics.size() == 2);  // once per table, as windHelp prints it
  CHECK(usage.diagnostics[0].text == "Usage:  help [pattern]");
}

TEST_CASE("wizard commands are dispatched but listed only by help wizard",
          "[cap:ui-wizard-and-debug][req:Wizard command visibility]") {
  UiSession s;
  REQUIRE_FALSE(s.registry.add(named("*watch"), "layout").has_value());
  const std::string plain = UiSession::printed(s.run("help"));
  CHECK(plain.find("*watch") == std::string::npos);
  CHECK(plain.find("*bypass") == std::string::npos);
  const std::string wizard = UiSession::printed(s.run("help wizard"));
  CHECK(wizard.find("\nWizard Global Commands\n----------------------\n") != std::string::npos);
  CHECK(wizard.find("\nWizard Layout Commands\n----------------------\n*watch") != std::string::npos);
  CHECK(wizard.find("*bypass") != std::string::npos);
  CHECK(s.run("*watch x").value.to_string() == "*watch x");
}

// --- Command line splitting and quoting ------------------------------------------------

TEST_CASE("command lines split on ; and blanks with Magic's quoting",
          "[cap:ui-command-dispatch][req:Command line splitting and quoting]") {
  using Words = std::vector<std::vector<std::string>>;
  // "Two commands with quoting".
  CHECK(ui::split_command_line(R"(label "a;b" n ; box 0 0 1 1)").commands ==
        Words{{"label", "a;b", "n"}, {"box", "0", "0", "1", "1"}});
  // "Empty quoted word".
  CHECK(ui::split_command_line(R"(macro x "")").commands == Words{{"macro", "x", ""}});
  // "Backslash inside quotes"; outside quotes a backslash is literal.
  CHECK(ui::split_command_line(R"(echo "say \"hi\"")").commands == Words{{"echo", R"(say "hi")"}});
  CHECK(ui::split_command_line(R"(echo a\b)").commands == Words{{"echo", R"(a\b)"}});
  // Quoted spans join adjacent text; single quotes work the same.
  CHECK(ui::split_command_line(R"(a"b c"d 'x;y')").commands == Words{{R"(ab cd)", "x;y"}});
  CHECK(ui::split_command_line("  ;; ").commands.empty());
  const auto unmatched = ui::split_command_line(R"(echo "abc)");
  CHECK(unmatched.commands == Words{{"echo", "abc"}});
  REQUIRE(unmatched.unmatched_quote.has_value());
  CHECK(ui::unmatched_quote_warning('"').text == R"(Unmatched " in string, I'll pretend that there is one at the end)");
}

TEST_CASE("long command lines are not limited", "[cap:ui-command-dispatch][req:Command line splitting and quoting]") {
  std::string line = "echo";
  for (int i = 0; i < 299; ++i) {
    line += " word" + std::to_string(i) + std::string(10, 'x');
  }
  REQUIRE(line.size() > 5000);
  const auto split = ui::split_command_line(line);
  REQUIRE(split.commands.size() == 1);
  CHECK(split.commands[0].size() == 300);
  UiSession s;
  const auto r = s.run(line);
  CHECK(r.ok());
  CHECK(r.diagnostics.empty());
  CHECK(r.value.to_string().size() == line.size() - 5);
}

// --- Host command interface mechanics ----------------------------------------------------

TEST_CASE("commands dispatch without a terminal or a scripting interpreter",
          "[cap:ui-command-dispatch][req:Host command interface mechanics]") {
  UiSession s;
  const auto results = ui::run_command_line(*s.dispatcher, "zoom 2", cmd::Origin::gui);
  REQUIRE(results.size() == 1);
  CHECK(results[0].ok());
  CHECK(s.calls_of("zoom") == std::vector<std::string>{"zoom 2"});
}

TEST_CASE("help lists Hefesto's own commands only with magic-compat off",
          "[cap:ui-command-dispatch][req:help command][golden:UI-G-005]") {
  UiSession s;
  REQUIRE_FALSE(s.registry.add(named("hefesto::example"), "*window").has_value());
  CHECK(UiSession::printed(s.run("help")).find("hefesto::example") == std::string::npos);
  s.project.compat.magic_compat = false;
  CHECK(UiSession::printed(s.run("help")).find("\nhefesto::example") != std::string::npos);
}

// Regression (golden UI-G-002): Magic's Lookup() ends a table entry's name at a space only, so
// `id\tOR` and `get\tOR` are no exact names and `id`/`get` are ambiguous with `identify`/`getcell`;
// the quiet Tcl dispatch makes them fail with an empty result. Replicated with magic-compat on.
TEST_CASE("id and get fail quietly with magic-compat on, as Magic's lookup makes them ambiguous",
          "[cap:ui-command-dispatch][req:Command name lookup][golden:UI-G-002]") {
  CHECK(hefesto::ui::magic_lookup_ambiguous("layout", "id"));
  CHECK(hefesto::ui::magic_lookup_ambiguous("layout", "get"));
  CHECK_FALSE(hefesto::ui::magic_lookup_ambiguous("layout", "identify"));
  CHECK_FALSE(hefesto::ui::magic_lookup_ambiguous("layout", "ext"));
  CHECK_FALSE(hefesto::ui::magic_lookup_ambiguous("*window", "grow"));
  // No other Magic command name is ambiguous in its own table or the *window table.
  for (const auto& e : hefesto::ui::magic_help_entries()) {
    if (e.name != "id" && e.name != "get") {
      INFO(e.client << " " << e.name);
      CHECK_FALSE(hefesto::ui::magic_lookup_ambiguous(e.client, e.name));
      CHECK_FALSE(hefesto::ui::magic_lookup_ambiguous("*window", e.name));
    }
  }
  UiSession s;
  REQUIRE_FALSE(s.registry.add(named("id"), "layout").has_value());
  const auto quiet = s.run("id x");
  REQUIRE_FALSE(quiet.ok());
  CHECK(quiet.value.to_string().empty());
  CHECK_FALSE(quiet.value.is_none());
  s.project.compat.magic_compat = false;
  CHECK(s.run("id x").ok());
}
