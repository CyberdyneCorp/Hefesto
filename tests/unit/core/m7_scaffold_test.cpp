// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 scaffold (openspec/changes/m7-extraction tasks.md 3.2, 3.3, 3.5): the
// netlist, extresist and antenna code areas exist; `extract`/`ext`,
// `ext2spice`/`exttospice`, `ext2sim`/`exttosim`, `extresist` and
// `antennacheck` are registered with Magic's full option tables, option
// lookup and invalid-option messages; every option not implemented yet
// fails with CORE-NOTIMPL-008; the area-contract stubs and the EditorState
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
#include "hefesto/cmd/m7_stubs.hpp"
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

// The option was reached and is a CORE-NOTIMPL-008 stub naming `what`.
void check_not_implemented(const cmd::CommandResult& result, const std::string& what) {
  INFO(what);
  REQUIRE_FALSE(result.ok());
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.front().code == kNotImplemented);
  CHECK(result.diagnostics.front().text == "not implemented yet: " + what);
}

}  // namespace

// --- tasks.md 3.2/3.3: code areas and CORE-NOTIMPL-008 ----------------------

TEST_CASE("CORE-NOTIMPL-008 and the M7 code areas are declared", "[m7-scaffold]") {
  const auto code = diag::find_code(kNotImplemented);
  REQUIRE(code.has_value());
  CHECK(code->severity == diag::Severity::error);
  CHECK(diag::codes::kExtractNotImplemented == kNotImplemented);
  CHECK(cmd::m7_not_implemented_diagnostic("x").code == kNotImplemented);
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

TEST_CASE("extract options need a window except style and help, then fail with CORE-NOTIMPL-008",
          "[m7-scaffold]") {
  Session s;
  for (const auto* line : {"extract", "extract all", "extract do capacitance"}) {
    INFO(line);
    const auto result = s.run(line);
    REQUIRE_FALSE(result.ok());
    CHECK(result.diagnostics.front().code == kNoWindow);
    CHECK(result.diagnostics.front().text == "Point to a window first");
  }
  check_not_implemented(s.run("extract style"), "extract style");
  check_not_implemented(s.run("extract listall style"), "extract style");
  s.state.command_point = cmd::PointerState{};
  check_not_implemented(s.run("extract"), "extract");
  for (const std::string_view entry : extract::extract_options()) {
    const std::string name = option_name(entry);
    if (name == "help") {
      continue;
    }
    check_not_implemented(s.run("extract " + name), "extract " + name);
    check_not_implemented(s.run("ext " + name), "extract " + name);
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
  // an unknown word are all run arguments. The run stops at EXTFLAT's
  // argument parser until it lands (tasks.md 10.2).
  for (const auto* line : {"ext2spice", "ext2spice run foo", "ext2spice re", "ext2spice foo"}) {
    INFO(line);
    check_not_implemented(s.run(line), "extflat arguments");
  }
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
  check_not_implemented(s.run("ext2spice run foo"), "extflat arguments");
  check_not_implemented(s.run("ext2spice foo"), "extflat arguments");
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
  check_not_implemented(s.run("extresist"), "extresist run");
  for (const std::string_view entry : hefesto::extresist::extresist_options()) {
    const std::string name = option_name(entry);
    if (name != "help") {
      check_not_implemented(s.run("extresist " + name), "extresist " + name);
    }
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
  check_not_implemented(s.run("antennacheck debug"), "antennacheck debug");
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
  check_not_implemented(s.run("antennacheck"), "antennacheck run");
  check_not_implemented(s.run("antennacheck run -x"), "antennacheck run");
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
  check_not_implemented(s.run("antennacheck run inv"), "antennacheck run");
  check_not_implemented(s.run("antennacheck inv"), "antennacheck run");
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
  // The builders are stubs until EXTTECH lands them (tasks.md 4.1-4.6).
  CHECK(style->rescale(1, 2, {}) == *style);
  CHECK(extract::StyleTable{}.select("lvs") == nullptr);
  const auto rules = hefesto::antenna::RuleView::from_style(*style);
  CHECK_FALSE(rules.rules(7).has_value());
}

TEST_CASE("the M7 entry points are CORE-NOTIMPL-008 stubs", "[m7-scaffold]") {
  Session s;
  check_not_implemented(extract::run(s.state, extract::RunRequest{}, nullptr), "extract run");
  const auto ensured = extract::ensure_extracted(s.state, "top", nullptr);
  REQUIRE_FALSE(ensured.ok());
  CHECK(ensured.error().code == kNotImplemented);
  check_not_implemented(netlist::spice::run(s.state, {}), "extflat arguments");
  const auto rendered = netlist::spice::render(s.state, "top", netlist::spice::SpiceSettings{});
  REQUIRE_FALSE(rendered.ok());
  CHECK(rendered.error().code == kNotImplemented);
  check_not_implemented(hefesto::extresist::run(s.state, {}, nullptr), "extresist run");
  CHECK(extract::fringe_fraction(1.0, 1.0) == 0.0);
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

TEST_CASE("the tech-load and grid hooks leave no style until EXTTECH lands the builder", "[m7-scaffold]") {
  Session s;
  diag::NullSink quiet;
  editor::install_extract_styles(s.state, quiet);
  CHECK(s.state.extract.current == nullptr);
  editor::rescale_extract_style(s.state, 2, 1);
  CHECK(s.state.extract.current == nullptr);
  // A style installed by a test survives a rescale (the stub copies it).
  s.state.extract.current = extract::testing::StyleBuilder("x").side_halo(4).shared();
  editor::rescale_extract_style(s.state, 2, 1);
  REQUIRE(s.state.extract.current != nullptr);
  CHECK(s.state.extract.current->side_halo == 4);
  // `scalegrid` still runs through tech::scale_layout with the hook in it.
  REQUIRE(s.run("scalegrid 1 2").ok());
  // A technology load rebuilds the (empty) table.
  editor::install_extract_styles(s.state, quiet);
  CHECK(s.state.extract.current == nullptr);
}
