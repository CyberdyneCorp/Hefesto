// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 scaffold (openspec/changes/m7-extraction tasks.md 3.2, 3.3, 3.5): the
// netlist, extresist and antenna code areas exist; `extract`/`ext`,
// `ext2spice`/`exttospice`, `ext2sim`/`exttosim`, `extresist` and
// `antennacheck` are registered with Magic's full option tables, option
// lookup and invalid-option messages; CORE-NOTIMPL-008 is retired (kept in
// the table, never reused; tasks.md 19.5); the area-contract stubs and the EditorState
// members answer their documented values. Owners replace these
// [m7-scaffold] tests with [cap:][req:] ones as the stubs go away.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "hefesto/antenna/antenna.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/editor/extract_hook.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/extflat/names.hpp"
#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/extract/command/commands.hpp"
#include "hefesto/extract/command/run.hpp"
#include "hefesto/extract/tech/testing/style_builder.hpp"
#include "hefesto/extresist/command/command.hpp"
#include "hefesto/netlist/first_word.hpp"
#include "hefesto/netlist/sim/sim.hpp"
#include "hefesto/netlist/spice/spice.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace editor = hefesto::editor;
namespace extflat = hefesto::extflat;
namespace extract = hefesto::extract;
namespace netlist = hefesto::netlist;

namespace {

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-008";
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

// The run reached EXTFLAT's read of `<cell>.ext` (no such file here); the
// failure is the last diagnostic, after EFArgs' messages.
void check_reads(const cmd::CommandResult& result, const std::string& cell) {
  REQUIRE_FALSE(result.ok());
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.back().text == "Cannot read extract file " + cell + ".ext");
}

// With magic-compat ON `ext2spice` leaves EFReadFile's message as the
// command's result and succeeds, as Magic's Tcl result (M7 round 3).
void check_read_result(const cmd::CommandResult& result, const std::string& cell) {
  REQUIRE(result.ok());
  CHECK(result.value.to_string() == "Cannot read extract file " + cell + ".ext\n");
}

// No cell argument and no layout window (NETLIST-SPICE-001).
void check_no_cell(const cmd::CommandResult& result) {
  REQUIRE_FALSE(result.ok());
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.front().code == "NETLIST-SPICE-001");
}

}  // namespace

// --- tasks.md 3.2/3.3: code areas and CORE-NOTIMPL-008 ----------------------

// Task 19.5: the stub code is retired once no M7 option answers it (kept, never reused).
TEST_CASE("CORE-NOTIMPL-008 is retired and the M7 code areas are declared", "[m7-scaffold]") {
  const auto code = diag::find_code(kNotImplemented);
  REQUIRE(code.has_value());
  CHECK(code->severity == diag::Severity::error);
  CHECK(code->summary.starts_with("Retired in M7, never reused."));
  const std::array<std::pair<std::string_view, std::string_view>, 5> areas{{
      {"extract", "EXTRACT"},
      {"extflat", "EXTFLAT"},
      {"netlist", "NETLIST"},
      {"extresist", "EXTRESIST"},
      {"antenna", "ANTENNA"},
  }};
  for (const auto& [name, prefix] : areas) {
    INFO(name);
    const auto found = diag::find_code_area(name);
    REQUIRE(found.has_value());
    CHECK(found->prefix == prefix);
    CHECK(found->doc == "docs/diagnostics/codes/" + std::string{name} + ".md");
  }
}

// --- tasks.md 3.3: extract and ext (design D4, D12) -------------------------

TEST_CASE("extract and ext have Magic's option tables", "[m7-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("extract") != nullptr);
  REQUIRE(s.registry.find("ext") != nullptr);
  CHECK(option_names(extract::extract_options()) ==
        std::vector<std::string>{"all", "cell", "do", "halo", "help", "length", "no", "parents", "path",
                                 "showparents", "stepsize", "style", "unique", "warn"});
  CHECK(option_names(extract::extract_do_options()) ==
        std::vector<std::string>{"adjust", "all", "capacitance", "corners", "coupling", "extresist", "length",
                                 "local", "lumped", "labelcheck", "aliases", "unique", "resistance", "fringehalo"});
  CHECK(option_names(extract::extract_warn_options()) == std::vector<std::string>{"all", "dup", "fets", "labels"});
  CHECK(option_names(extract::extract_length_options()) ==
        std::vector<std::string>{"clear", "driver", "receiver"});
  CHECK(option_names(extract::extract_unique_options()) ==
        std::vector<std::string>{"all", "#", "noports", "notopports"});
}

TEST_CASE("extract help prints Magic's help text", "[m7-scaffold]") {
  Session s;
  for (const auto* line : {"extract help", "ext help", "extract listall help"}) {
    INFO(line);
    REQUIRE(s.run(line).ok());
    const std::string text = s.take_printed();
    CHECK(text.starts_with("Extract commands have the form \"extract option\",\nwhere option is one of:\n"
                           "  all\t\t\textract root cell and all its children\n"));
    CHECK(text.find("  unique [option]\tgenerate unique names when different nodes\n\t\t\thave the same name\n") !=
          std::string::npos);
    CHECK(text.ends_with("If no option is given, the root cell and all its\nchildren are extracted incrementally.\n"));
  }
  REQUIRE(s.run("extract help me").ok());
  CHECK(s.take_printed() ==
        "Wrong number of arguments in \"extract\" command.  Try \"extract help\" for help.\n");
}

TEST_CASE("an invalid or ambiguous extract option prints Magic's message", "[m7-scaffold]") {
  Session s;
  // `s` matches showparents, stepsize and style (extract-commands "Ambiguous
  // prefix"); `bogus` matches nothing. Neither needs a window.
  for (const auto* word : {"s", "bogus", "list"}) {
    const std::string line = std::string{"extract list "} + word;
    INFO(line);
    REQUIRE(s.run(line).ok());
    CHECK(s.take_printed() == "\"" + std::string{word} +
                                  "\" isn't a valid extract option.  Type \"extract help\" for a list of valid "
                                  "options.\n");
  }
  REQUIRE(s.run("ext bogus").ok());
  CHECK(s.take_printed().starts_with("\"bogus\" isn't a valid extract option."));
}

// Q-EXTRACT-28 (fix, both modes): Magic looks up argv[1] after removing
// `list`/`listall`, a NULL pointer when nothing follows, and crashes
// (CmdE.c:1151-1162). Hefesto reports the wrong argument count.
TEST_CASE("extract list and listall with no option print the argument-count message",
          "[cap:extract-commands][req:Command syntax and option matching]") {
  Session s;
  for (const bool compat : {true, false}) {
    s.project.compat.magic_compat = compat;
    for (const auto* line : {"extract list", "extract listall", "ext listall"}) {
      INFO(line << " magic-compat " << compat);
      REQUIRE(s.run(line).ok());
      CHECK(s.take_printed() ==
            "Wrong number of arguments in \"extract\" command.  Try \"extract help\" for help.\n");
    }
  }
}

TEST_CASE("extract options other than style and help need a window", "[m7-scaffold]") {
  Session s;
  for (const auto* line : {"extract", "extract all", "extract do capacitance"}) {
    INFO(line);
    const auto result = s.run(line);
    REQUIRE_FALSE(result.ok());
    CHECK(result.diagnostics.front().code == kNoWindow);
    CHECK(result.diagnostics.front().text == "Point to a window first");
  }
}

// --- tasks.md 3.3: ext2spice and exttospice (design D4, D14) ----------------

TEST_CASE("ext2spice has Magic's option table and help", "[m7-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("ext2spice") != nullptr);
  REQUIRE(s.registry.find("exttospice") != nullptr);
  CHECK(option_names(netlist::spice::spice_options()) ==
        std::vector<std::string>{"[run]", "default", "format", "rthresh", "cthresh", "merge", "extresist",
                                 "resistor", "scale", "short", "subcircuits", "hierarchy", "blackbox", "renumber",
                                 "global", "lvs", "help"});
  for (const auto* line : {"ext2spice help", "exttospice help"}) {
    INFO(line);
    REQUIRE(s.run(line).ok());
    std::string expected;
    for (const std::string_view entry : netlist::spice::spice_options()) {
      expected += "    " + std::string{entry} + "\n";
    }
    CHECK(s.take_printed() == expected);
  }
  // SPICE landed the options (tasks.md 11.1; tests/unit/netlist/spice/).
  CHECK(s.run("ext2spice format").value.to_string() == "spice3");
  REQUIRE(s.run("exttospice lvs").ok());
  REQUIRE(s.run("ext2spice scale off").ok());
  REQUIRE(s.run("exttospice scale on").ok());
}

TEST_CASE("ext2spice's first word follows Q-NETLIST-13 in each mode", "[m7-scaffold]") {
  Session s;
  // magic-compat ON: `run`, an ambiguous word (`re`: resistor, renumber) and
  // an unknown word are all run arguments: the word is the cell whose
  // `.ext` file the run reads.
  check_no_cell(s.run("ext2spice"));
  const cmd::CommandResult run_foo_result = s.run("ext2spice run foo");
  check_read_result(run_foo_result, "foo");
  // EFArgs takes `run` as the first root cell and warns when `foo` replaces it.
  CHECK(run_foo_result.diagnostics.front().text == "Warning: multiple input files specified; ignoring \"run\"");
  check_read_result(s.run("ext2spice re"), "re");
  check_read_result(s.run("ext2spice foo"), "foo");
  const std::array<std::string_view, 3> table{"[run] [options]\trun", "resistor tee", "renumber"};
  const std::vector<std::string> run_foo{"run", "foo"};
  auto on = netlist::classify_first_word(run_foo, table, true);
  CHECK(on.kind == netlist::FirstWord::run);
  CHECK(on.run_args == run_foo);
  const std::vector<std::string> re{"re"};
  CHECK(netlist::classify_first_word(re, table, true).run_args == re);

  // magic-compat OFF: `run` is a keyword, an ambiguous prefix prints
  // `Ambiguous option: <w>` and does nothing, an unknown word is a run
  // argument.
  auto off = netlist::classify_first_word(run_foo, table, false);
  CHECK(off.kind == netlist::FirstWord::run);
  CHECK(off.run_args == std::vector<std::string>{"foo"});
  CHECK(netlist::classify_first_word(re, table, false).kind == netlist::FirstWord::ambiguous);
  s.project.compat.magic_compat = false;
  (void)s.take_printed();
  REQUIRE(s.run("ext2spice re").ok());
  CHECK(s.take_printed() == "Ambiguous option: re\n");
  check_reads(s.run("ext2spice run foo"), "foo");
  check_reads(s.run("ext2spice foo"), "foo");
  CHECK(s.run("ext2spice renumber").value.to_string() == "off");
}

// --- tasks.md 3.3: ext2sim and exttosim (design D4, D15) --------------------

// SIM landed the options and the run (tasks.md 12.1); their tests are in
// tests/unit/netlist/sim/.
TEST_CASE("ext2sim has Magic's option table and help", "[m7-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("ext2sim") != nullptr);
  REQUIRE(s.registry.find("exttosim") != nullptr);
  CHECK(option_names(netlist::sim::sim_options()) ==
        std::vector<std::string>{"[run]", "alias", "labels", "default", "format", "rthresh", "cthresh", "merge",
                                 "extresist", "help"});
  REQUIRE(s.run("exttosim help").ok());
  CHECK(s.take_printed().starts_with("    [run] [options]\trun exttosim on current cell\n"));
  for (const std::string_view entry : netlist::sim::sim_options().subspan(1)) {
    const std::string name = option_name(entry);
    INFO(name);
    CHECK(s.run("ext2sim " + name).ok());
  }
}

// --- tasks.md 3.3: extresist (design D4, D16) -------------------------------

TEST_CASE("extresist has Magic's option table and messages", "[m7-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("extresist") != nullptr);
  CHECK(option_names(hefesto::extresist::extresist_options()) ==
        std::vector<std::string>{"all", "threshold", "minresist", "mindelay", "tolerance", "simplify", "extout",
                                 "lumped", "silent", "debug", "skip", "force", "ignore", "include", "box", "cell",
                                 "blackbox", "fasthenry", "geometry", "stats", "help"});
  REQUIRE(s.run("extresist s").ok());  // simplify, silent, skip, stats
  CHECK(s.take_printed() == "Ambiguous option: s\n");
  REQUIRE(s.run("extresist bogus").ok());
  CHECK(s.take_printed() == "Unknown option: bogus\n");
  REQUIRE(s.run("extresist help").ok());
  std::string expected;
  for (const std::string_view entry : hefesto::extresist::extresist_options()) {
    expected += std::string{entry} + "\n";
  }
  CHECK(s.take_printed() == expected);
  // RESCMD landed the options and the run (tasks.md 14.1): without a box
  // cell every option answers without CORE-NOTIMPL-008.
  REQUIRE(s.run("extresist").ok());
  for (const std::string_view entry : hefesto::extresist::extresist_options()) {
    const std::string name = option_name(entry);
    INFO(name);
    CHECK(s.run("extresist " + name).ok());
  }
}

// --- tasks.md 3.3: antennacheck (design D4, D17) ----------------------------

TEST_CASE("antennacheck has Magic's option table and help", "[m7-scaffold]") {
  Session s;
  REQUIRE(s.registry.find("antennacheck") != nullptr);
  CHECK(option_names(hefesto::antenna::antennacheck_options()) == std::vector<std::string>{"[run]", "debug", "help"});
  REQUIRE(s.run("antennacheck help").ok());
  CHECK(s.take_printed() ==
        "    [run] [options]\trun antennacheck on current cell\n\t\t\tuse \"run -help\" to get standard options\n"
        "    debug\t\t\tprint detailed information about each error\n"
        "    help\t\t\tprint help information\n");
  REQUIRE(s.run("antennacheck debug").ok());  // ANTENNA landed debug (tasks.md 15.2)
}

// Q-MISC-91 (compat, as Q-NETLIST-13): ON, the word `run` is a run argument
// (the cell named `run`); `debug` and `help` are the only options.
TEST_CASE("antennacheck first word run with magic-compat ON", "[cap:antenna-check][req:Antenna check command]") {
  Session s;
  const std::vector<std::string> run{"run"};
  const auto on = netlist::classify_first_word(run, hefesto::antenna::antennacheck_options(), true);
  CHECK(on.kind == netlist::FirstWord::run);
  CHECK(on.run_args == run);
  const std::vector<std::string> d{"d"};
  CHECK(netlist::classify_first_word(d, hefesto::antenna::antennacheck_options(), true).kind ==
        netlist::FirstWord::option);
  // ANTENNA landed the run (tasks.md 15.2): no extraction style, no plane order.
  REQUIRE(s.run("antennacheck").ok());
  REQUIRE(s.run("antennacheck run -x").ok());
}

// OFF: `run` is a keyword whose following words are the run arguments; an
// unknown word is still the cell name.
TEST_CASE("antennacheck first word run with magic-compat OFF", "[cap:antenna-check][req:Antenna check command]") {
  Session s;
  s.project.compat.magic_compat = false;
  const std::vector<std::string> run_inv{"run", "inv"};
  const auto off = netlist::classify_first_word(run_inv, hefesto::antenna::antennacheck_options(), false);
  CHECK(off.kind == netlist::FirstWord::run);
  CHECK(off.run_args == std::vector<std::string>{"inv"});
  const std::vector<std::string> inv{"inv"};
  CHECK(netlist::classify_first_word(inv, hefesto::antenna::antennacheck_options(), false).run_args == inv);
  REQUIRE(s.run("antennacheck run inv").ok());
  REQUIRE(s.run("antennacheck inv").ok());
}

// --- tasks.md 3.3/3.5: the area-contract stubs ------------------------------

TEST_CASE("StyleBuilder builds ExtStyle fixtures", "[m7-scaffold]") {
  hefesto::tech::TypeMask gate;
  gate.set(7);
  extract::DevicePattern nfet;
  nfet.model = "nfet";
  nfet.types = gate;
  const auto style = extract::testing::StyleBuilder("lvs")
                         .step(100)
                         .side_halo(4)
                         .device(nfet)
                         .device_info({extract::DeviceInfo{"nfet", 7, 0, 0, -1, "VSUBS"}})
                         .shared();
  REQUIRE(style != nullptr);
  CHECK(style->name == "lvs");
  CHECK(style->step_size == 100);
  CHECK(style->gate_types() == gate);
  REQUIRE(style->device_info().size() == 1);
  CHECK(style->device_info().front().substrate_node == "VSUBS");
  // ExtTechScale(1, 2): distances scaled by 2 (tasks.md 4.6).
  CHECK(style->rescale(1, 2, {}).side_halo == 8);
  CHECK(style->rescale(1, 2, {}).step_size == 200);
  CHECK(extract::StyleTable{}.select("lvs") == nullptr);
  const auto rules = hefesto::antenna::RuleView::from_style(*style);
  CHECK_FALSE(rules.rules(7).has_value());
}

TEST_CASE("the M7 entry points: landed runs and the remaining stubs", "[m7-scaffold]") {
  Session s;
  CHECK(extract::run(s.state, extract::RunRequest{}, nullptr).ok());  // EXTCMD landed the run driver
  CHECK_FALSE(extract::ensure_extracted(s.state, "top", nullptr).ok());
  check_no_cell(netlist::spice::run(s.state, {}));  // SPICE landed the run (tasks.md 11.1)
  const auto rendered = netlist::spice::render(s.state, "top", netlist::spice::SpiceSettings{});
  REQUIRE_FALSE(rendered.ok());
  CHECK(rendered.error().code == "EXTFLAT-READ-001");
  CHECK(hefesto::extresist::run(s.state, {}, nullptr).ok());  // RESCMD landed the run: no box cell
  // EXTCAP landed fringe_fraction (tasks.md 6.4): tests/unit/extract/cap/cr_atan_test.cpp.
  const std::vector<std::string> parts{"x1", "n1"};
  extflat::NameFlags flags;
  CHECK(extflat::name_to_spice(parts, extflat::SpiceDialect::spice3, flags, {}) == "x1/n1");
  CHECK(extflat::flatten(extflat::Hierarchy{}, extflat::FlatFlags::flat_nodes).nodes().empty());
}

TEST_CASE("EditorState has the M7 members with Magic's defaults", "[m7-scaffold]") {
  editor::EditorState state;
  CHECK(state.extract.current == nullptr);
  CHECK(state.extract.styles.names().empty());
  const extract::ExtractOptions& options = state.extract.options;
  CHECK(options.adjust);
  CHECK(options.capacitance);
  CHECK(options.coupling);
  CHECK(options.lumped);
  CHECK(options.length);
  CHECK(options.unique);
  CHECK(options.fringehalo);
  CHECK(options.labelcheck);
  CHECK(options.aliases);
  CHECK_FALSE(options.corners);
  CHECK(options.corner_factor == 1.0);
  CHECK_FALSE(options.extresist);
  CHECK_FALSE(options.unique_notopports);
  CHECK(options.warn_dup);
  CHECK(options.warn_fets);
  CHECK_FALSE(options.warn_labels);
  CHECK_FALSE(options.path.has_value());

  const netlist::spice::SpiceSettings& spice = state.netlist.spice;
  CHECK(spice.format == netlist::spice::Dialect::spice3);
  CHECK(spice.cthresh == 2.0);
  CHECK(spice.scale);
  CHECK(spice.subckt_top == netlist::spice::TopSubckt::automatic);
  CHECK(spice.global);
  const auto lvs = netlist::spice::SpiceSettings::lvs(spice);
  CHECK(lvs.hierarchy);
  CHECK(lvs.format == netlist::spice::Dialect::ngspice);
  CHECK(std::isinf(lvs.cthresh));
  CHECK_FALSE(lvs.scale);
  CHECK(lvs.blackbox);
  CHECK_FALSE(lvs.global);
  CHECK(state.netlist.sim.format == netlist::sim::SimFormat::mit);
  CHECK(state.netlist.sim.rthresh == 10);

  CHECK(state.extresist.options.threshold == 10000.0);
  CHECK(state.extresist.options.min_resist == 1000.0);
  CHECK(state.extresist.options.min_delay_ps == 1.0);
  CHECK(state.extresist.options.simplify);
  CHECK(state.extresist.options.extout);
  CHECK_FALSE(state.extresist.options.fasthenry_frequency.has_value());
}

TEST_CASE("the tech-load and grid hooks install and rescale the current style", "[m7-scaffold]") {
  Session s;
  diag::NullSink quiet;
  editor::install_extract_styles(s.state, quiet);
  REQUIRE(s.state.extract.current != nullptr);
  // A style installed by a test is rescaled by the grid hook
  // (ExtTechScale(1, 2): distances doubled).
  s.state.extract.current = extract::testing::StyleBuilder("x").side_halo(4).shared();
  editor::rescale_extract_style(s.state, 2, 1);
  REQUIRE(s.state.extract.current != nullptr);
  CHECK(s.state.extract.current->side_halo == 8);
  // `scalegrid` still runs through tech::scale_layout with the hook in it.
  REQUIRE(s.run("scalegrid 1 2").ok());
  // A technology load rebuilds the table and selects the first style.
  editor::install_extract_styles(s.state, quiet);
  CHECK(s.state.extract.current != nullptr);
}
