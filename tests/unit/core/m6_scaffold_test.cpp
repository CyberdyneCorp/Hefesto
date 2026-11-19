// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M6 scaffold (openspec/changes/m6-lef-def-oasis tasks.md 2.2, 2.3): the
// lef, def, oasis, extflat and extract code areas exist; `lef` and `def`
// share one dispatcher with Magic's option lookup and help, and `oasis` is
// registered with its full option table; every option not implemented yet
// fails with CORE-NOTIMPL-006; the area-contract stubs answer their
// documented neutral values. Owners replace these [m6-scaffold] tests with
// [cap:][req:] ones as the stubs go away.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/lef/command/command.hpp"
#include "hefesto/oasis/codec/cblock.hpp"
#include "hefesto/oasis/codec/validate.hpp"
#include "hefesto/oasis/command/command.hpp"
#include "hefesto/oasis/command/format.hpp"
#include "hefesto/oasis/command/settings.hpp"
#include "hefesto/oasis/read/stamps.hpp"
#include "hefesto/oasis/write/repetitions.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace editor = hefesto::editor;
namespace gds = hefesto::gds;
namespace lef = hefesto::lef;
namespace oasis = hefesto::oasis;

namespace {

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-006";
constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";

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

std::vector<std::string> option_names(std::span<const std::string_view> table) {
  std::vector<std::string> names;
  for (const std::string_view entry : table) {
    names.push_back(option_name(entry));
  }
  return names;
}

void check_not_implemented(const cmd::CommandResult& result, const std::string& what) {
  INFO(what);
  REQUIRE_FALSE(result.ok());
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.front().code == kNotImplemented);
  CHECK(result.diagnostics.front().text == "not implemented yet: " + what);
}

template <typename T>
void check_not_implemented(const diag::Outcome<T>& outcome) {
  REQUIRE_FALSE(outcome.ok());
  CHECK(outcome.error().code == kNotImplemented);
}

}  // namespace

// --- tasks.md 2.2/2.3: code areas and CORE-NOTIMPL-006 ----------------------

TEST_CASE("CORE-NOTIMPL-006 and the M6 code areas are declared", "[m6-scaffold]") {
  const auto code = diag::find_code(kNotImplemented);
  REQUIRE(code.has_value());
  CHECK(code->severity == diag::Severity::error);
  CHECK(diag::codes::kCoreNotImplemented == kNotImplemented);
  const std::array<std::pair<std::string_view, std::string_view>, 5> areas{{
      {"lef", "LEF"},
      {"def", "DEF"},
      {"oasis", "OASIS"},
      {"extflat", "EXTFLAT"},
      {"extract", "EXTRACT"},
  }};
  for (const auto& [name, prefix] : areas) {
    INFO(name);
    const auto found = diag::find_code_area(name);
    REQUIRE(found.has_value());
    CHECK(found->prefix == prefix);
    CHECK(found->doc == "docs/diagnostics/codes/" + std::string{name} + ".md");
  }
}

// --- tasks.md 2.3: the lef and def commands (design D14) ----------------------

TEST_CASE("lef and def share Magic's lef option table and print their own help", "[m6-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("lef") != nullptr);
  REQUIRE(s.registry.find("def") != nullptr);
  CHECK(option_names(lef::lef_options()) ==
        std::vector<std::string>{"read", "write", "writeall", "nocheck", "datestamp", "help"});
  CHECK(option_names(lef::def_options()) == std::vector<std::string>{"read", "", "write", "writeall", "help"});

  // A bare command and `help` print the command's own table (LEF_HELP).
  for (const auto* line : {"lef", "lef help"}) {
    INFO(line);
    REQUIRE(s.run(line).ok());
    const std::string text = s.take_printed();
    CHECK(text.starts_with("The \"lef\" options are:\n"));
    for (const std::string_view entry : lef::lef_options()) {
      CHECK(text.find("    lef " + std::string{entry} + "\n") != std::string::npos);
    }
  }
  REQUIRE(s.run("def help").ok());
  const std::string text = s.take_printed();
  CHECK(text ==
        "The \"def\" options are:\n"
        "    def read [filename]\t\tread a DEF file filename[.def]\n"
        "    read [filename] -labels\tread a DEF file with net labeling\n"
        "    read [filename] -annotate\tread a DEF file for net annotation only\n"
        "\n"
        "    def     read [filename] -noblockage\tread a DEF file (mask layers only).\n"
        "    def write [cell] [-allspecial]\twrite DEF for current or indicated cell\n"
        "    def writeall\t\t\t(use \"flatten -nosubckt\" + \"def write\" instead)\n"
        "    def help                   \tprint this help information\n");
}

TEST_CASE("an invalid lef or def option prints Magic's message and the help", "[m6-scaffold]") {
  Session s;
  REQUIRE(s.run("lef bogus").ok());
  CHECK(s.take_printed().starts_with("\"bogus\" isn't a valid lef option.\nThe \"lef\" options are:\n"));
  REQUIRE(s.run("def bogus").ok());
  CHECK(s.take_printed().starts_with("\"bogus\" isn't a valid def option.\nThe \"def\" options are:\n"));
  // `wr` is ambiguous (write, writeall): Lookup() < 0.
  REQUIRE(s.run("lef wr").ok());
  CHECK(s.take_printed().starts_with("\"wr\" isn't a valid lef option."));
}

TEST_CASE("lef and def options need a window, then answer CORE-NOTIMPL-006", "[m6-scaffold]") {
  Session s;
  const auto no_window = s.run("lef read cells");
  REQUIRE_FALSE(no_window.ok());
  CHECK(no_window.diagnostics.front().code == kNoWindow);
  CHECK(no_window.diagnostics.front().text == "Point to a window first");
  s.state.command_point = cmd::PointerState{};

  // lef read/datestamp (LEFR) and lef write/writeall/nocheck (LEFW) are
  // implemented: tests/unit/lef/read/ and tests/unit/lef/write/.
  check_not_implemented(s.run("def read top.def -labels"), "def read");
  check_not_implemented(s.run("def write"), "def write");
}

TEST_CASE("def writeall, datestamp and nocheck answer as in Magic", "[m6-scaffold]") {
  Session s;
  s.state.command_point = cmd::PointerState{};
  REQUIRE(s.run("def writeall").ok());
  CHECK(s.take_printed() == "Sorry, can't write hierarchical DEF at this time.\nTry \"def write\"\n");
  REQUIRE(s.run("def datestamp 5").ok());
  CHECK(s.take_printed() == "The \"datestamp\" option is only for LEF reads.\n");
  REQUIRE(s.run("def nocheck vdd").ok());
  CHECK(s.take_printed() == "The \"nocheck\" option is only for LEF writes.\n");
}

// --- tasks.md 2.3: the oasis command (design D15) -----------------------------

TEST_CASE("oasis has the full option table of oasis-command", "[m6-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("oasis") != nullptr);
  const std::vector<std::string> expected = {
      "help",        "abstract",  "addendum",    "arrays",     "compress",     "contacts",  "datestamp",
      "drccheck",    "flatten",   "flatglob",    "labels",     "library",      "lower",     "magscale",
      "maskhints",   "merge",     "nodatestamp", "noduplicates", "ordering",   "read",      "readonly",
      "rescale",     "savepaths", "warning",     "write",      "polygon",      "path",      "undefined",
      "unique",      "strict",    "validation",  "repetitions", "layernames", "circlepoints"};
  CHECK(option_names(oasis::oasis_options()) == expected);

  REQUIRE(s.run("oasis help").ok());
  const std::string text = s.take_printed();
  CHECK(text.starts_with("OASIS commands have the form \":oasis option\", where option is one of:\n"));
  for (const std::string_view entry : oasis::oasis_options()) {
    CHECK(text.find("    " + std::string{entry} + "\n") != std::string::npos);
  }
}

TEST_CASE("oasis options answer CORE-NOTIMPL-006; an unknown one is an argument error", "[m6-scaffold]") {
  Session s;
  // `read`, `write` and a bare `oasis` check for a window first, as for gds.
  const auto no_window = s.run("oasis");
  REQUIRE_FALSE(no_window.ok());
  CHECK(no_window.diagnostics.front().code == kNoWindow);
  check_not_implemented(s.run("oasis compress"), "oasis compress");  // no window needed
  s.state.command_point = cmd::PointerState{};

  check_not_implemented(s.run("oasis"), "oasis write");
  for (const std::string_view entry : oasis::oasis_options()) {
    const std::string name = option_name(entry);
    if (name != "help") {
      check_not_implemented(s.run("oasis " + name), "oasis " + name);
    }
  }
  check_not_implemented(s.run("oasis nodup t"), "oasis noduplicates");  // unique prefix
  check_not_implemented(s.run("oasis rep no"), "oasis repetitions");

  for (const auto* line : {"oasis bogus", "oasis la yes"}) {  // unknown; ambiguous (labels, layernames)
    INFO(line);
    const auto bad = s.run(line);
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.diagnostics.front().code == diag::codes::kCmdUsage);
  }
}

TEST_CASE("OasisSettings has the defaults of oasis-command", "[m6-scaffold]") {
  const oasis::OasisSettings settings;
  CHECK(settings.compress == 6);
  CHECK(settings.labels);
  CHECK(settings.lower);
  CHECK_FALSE(settings.merge);
  CHECK_FALSE(settings.allow_abstract);
  CHECK_FALSE(settings.allow_undefined);
  CHECK_FALSE(settings.datestamp.has_value());
  CHECK(settings.magscale == 1.0);
  CHECK(settings.strict);
  CHECK(settings.validation == oasis::ValidationScheme::crc32);
  CHECK(settings.repetitions);
  CHECK(settings.layernames);
  CHECK(settings.circlepoints == 64);
}

// --- tasks.md 2.3: the area-contract stubs ------------------------------------

TEST_CASE("the M6 area-contract stubs answer their documented values", "[m6-scaffold]") {
  const std::vector<std::byte> bytes(16, std::byte{0});
  gds::MemorySource source{bytes};
  CHECK(oasis::sniff_format(source) == oasis::FileFormat::unknown);
  CHECK(oasis::kOasisMagic.size() == 13);
  CHECK(oasis::content_stamps(source).empty());
  // validate, inflate_cblock and deflate_cblock are implemented (OASFMT,
  // tests/unit/oasis/codec/).
  CHECK(oasis::ValidationResult{}.ok());

  const std::array<db::Point, 2> points{db::Point{0, 0}, db::Point{10, 0}};
  const auto groups = oasis::detect_repetitions(points);
  REQUIRE(groups.size() == 2);
  CHECK(groups[1] == oasis::RepetitionGroup{db::Point{10, 0}, {}});

  // lef::format_distance and lef::MacroBuilder are implemented (LEFW):
  // tests/unit/lef/write/.
}
