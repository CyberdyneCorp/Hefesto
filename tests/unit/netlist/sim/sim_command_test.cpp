// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tests of the `ext2sim` / `exttosim` command (netlist/sim/sim.hpp;
// capability netlist-sim; openspec/changes/m7-extraction/design.md D15):
// the option handlers, the first-word rule, help, and the run with a
// hand-built source or EXTFLAT's read and flattening of `.ext` files in
// an in-memory file system. The run parses its flags with
// extflat::parse_common_args.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/names.hpp"
#include "hefesto/netlist/first_word.hpp"
#include "hefesto/netlist/sim/sim.hpp"
#include "hefesto/netlist/sim/writer.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace extflat = hefesto::extflat;
namespace netlist = hefesto::netlist;
namespace platform = hefesto::platform;
namespace sim = hefesto::netlist::sim;

namespace {

struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState state;
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

// The run of one `ext2sim <args>` against a hand-built circuit, writing to
// an in-memory file system.
struct Runner {
  hefesto::editor::EditorState& editor;
  platform::MemoryFileSystem files;
  std::vector<sim::ReadRequest> reads;
  std::vector<std::string> messages;
  sim::SimInput circuit;

  explicit Runner(hefesto::editor::EditorState& e) : editor(e) {
    circuit.scale = 1.0F;
    circuit.tech = "nltest";
    sim::SimNode a;
    a.names = {sim::NodeName{{"a"}, false}, sim::NodeName{{"u1", "x"}, false}};
    a.layer = "metal1";
    sim::SimNode b;
    b.names = {sim::NodeName{{"vdd!"}, true}};
    b.layer = "space";
    circuit.nodes = {a, b};
    circuit.caps = {sim::SimCap{{"a"}, {"vdd!"}, 5000}};
  }

  cmd::CommandResult operator()(std::vector<std::string> args, bool compat = true) {
    sim::SimRun request;
    request.args = std::move(args);
    request.magic_compat = compat;
    request.files = &files;
    request.emit = [this](diag::Diagnostic d) { messages.push_back(d.text); };
    return sim::run(editor, request, [this](const sim::ReadRequest& r) -> diag::Outcome<sim::SimInput> {
      reads.push_back(r);
      return circuit;
    });
  }

  std::string file(const std::string& name) {
    auto text = files.read(platform::Path{name});
    return text.ok() ? text.value() : std::string{"<none>"};
  }
};

// The run of one `ext2sim <args>` through the default source: EXTFLAT's
// read and flattening of the `.ext` files under /w of `files`.
struct ExtRunner {
  hefesto::editor::EditorState editor;
  platform::MemoryFileSystem files;
  std::vector<std::string> messages;

  void ext(const std::string& cell, std::string_view text) {
    REQUIRE(files.write_atomic(platform::Path{"/w/" + cell + ".ext"}, text).ok());
  }

  cmd::CommandResult operator()(const std::vector<std::string>& args) {
    sim::SimRun request;
    request.args = {"-p", "/w"};
    request.args.insert(request.args.end(), args.begin(), args.end());
    request.files = &files;
    request.emit = [this](diag::Diagnostic d) { messages.push_back(d.text); };
    return sim::run(editor, request);
  }

  [[nodiscard]] bool printed(std::string_view text) const {
    return std::find(messages.begin(), messages.end(), text) != messages.end();
  }
};

}  // namespace

TEST_CASE("ext2sim options query and set the session settings",
          "[cap:netlist-sim][req:Command options and defaults]") {
  Session s;
  CHECK(s.run("ext2sim alias").value.to_string() == "off");
  REQUIRE(s.run("ext2sim alias on").ok());
  CHECK(s.state.netlist.sim.alias);
  REQUIRE(s.run("exttosim labels yes").ok());
  CHECK(s.state.netlist.sim.labels);
  REQUIRE(s.run("ext2sim extresist true").ok());
  CHECK(s.run("ext2sim extresist").value.to_string() == "on");
  REQUIRE(s.run("ext2sim format su").ok());
  CHECK(s.run("ext2sim format").value.to_string() == "SU");
  REQUIRE(s.run("ext2sim cthresh infinite").ok());
  CHECK(s.run("ext2sim cthresh").value.to_string() == "infinite");
  REQUIRE(s.run("ext2sim rthresh 25").ok());
  CHECK(s.run("ext2sim rthresh").value.to_string() == "25");
  REQUIRE(s.run("ext2sim merge aggressive").ok());
  CHECK(s.run("ext2sim merge").value.to_string() == "aggressive");
  s.state.netlist.names.trim_global = true;
  REQUIRE(s.run("ext2sim default").ok());
  CHECK(s.state.netlist.sim.cthresh == 2.0);
  CHECK(s.state.netlist.sim.rthresh == 10);
  CHECK(s.state.netlist.names == extflat::NameFlags{false, false, false, false, false});
  CHECK(s.state.netlist.sim.format == sim::SimFormat::su);  // `default` keeps the format
}

TEST_CASE("ext2sim option values Magic rejects", "[cap:netlist-sim][req:Command options and defaults]") {
  Session s;
  (void)s.take_printed();
  REQUIRE(s.run("ext2sim format spice").ok());
  CHECK(s.take_printed() == "exttosim: output formats are MIT, LBL, or SU\n");
  REQUIRE(s.run("ext2sim rthresh 1.5").ok());
  CHECK(s.take_printed() == "exttosim: integer value or \"infinite\" expected.\n");
  // Q-NETLIST-14: the merge-type table is terminated, so an unknown value
  // lists the three types instead of reading past the table.
  REQUIRE(s.run("ext2sim merge sometimes").ok());
  CHECK(s.take_printed() ==
        "Merge types are:\n"
        "    none\t\t\tdon't merge parallel devices\n"
        "    conservative\t\tmerge devices with same L, W\n"
        "    aggressive\t\tmerge devices with same L\n");
  CHECK(s.state.netlist.sim.merge == sim::SimMerge::none);
}

// Q-NETLIST-49 (compat): ON, `run` and an ambiguous prefix are run arguments.
TEST_CASE("ext2sim first word run with magic-compat ON", "[cap:netlist-sim][req:Command options and defaults]") {
  const std::vector<std::string> run{"run"};
  auto on = netlist::classify_first_word(run, sim::sim_options(), true);
  CHECK(on.kind == netlist::FirstWord::run);
  CHECK(on.run_args == run);  // the cell named `run`
  // `[run] [options]` matches no word, so `r` is a unique prefix of rthresh.
  const std::vector<std::string> r{"r"};
  const auto rthresh = netlist::classify_first_word(r, sim::sim_options(), true);
  CHECK(rthresh.kind == netlist::FirstWord::option);
  CHECK(rthresh.index == 5);
  const std::vector<std::string> inv{"inv"};
  CHECK(netlist::classify_first_word(inv, sim::sim_options(), true).run_args == inv);
}

TEST_CASE("ext2sim first word run with magic-compat OFF", "[cap:netlist-sim][req:Command options and defaults]") {
  const std::vector<std::string> run_inv{"run", "inv"};
  auto off = netlist::classify_first_word(run_inv, sim::sim_options(), false);
  CHECK(off.kind == netlist::FirstWord::run);
  CHECK(off.run_args == std::vector<std::string>{"inv"});
  const std::vector<std::string> inv{"inv"};
  CHECK(netlist::classify_first_word(inv, sim::sim_options(), false).run_args == inv);
  Session s;
  s.project.compat.magic_compat = false;
  (void)s.take_printed();
  REQUIRE(s.run("ext2sim r").ok());  // run, rthresh
  CHECK(s.take_printed() == "Ambiguous option: r\n");
}

// Q-NETLIST-48: sim2simp is not provided; help names `ext2sim merge`.
TEST_CASE("ext2sim help with magic-compat ON is Magic's table", "[cap:netlist-sim][req:Deprecated sim2simp filter]") {
  const std::string help = sim::sim_help(true);
  CHECK(help.starts_with("    [run] [options]\trun exttosim on current cell\n"));
  CHECK(help.ends_with("    help\t\t\tprint help information\n"));
  CHECK(help.find("sim2simp") == std::string::npos);
}

TEST_CASE("ext2sim help with magic-compat OFF says sim2simp is not provided",
          "[cap:netlist-sim][req:Deprecated sim2simp filter]") {
  Session s;
  s.project.compat.magic_compat = false;
  (void)s.take_printed();
  REQUIRE(s.run("ext2sim help").ok());
  const std::string printed = s.take_printed();
  CHECK(printed.find("sim2simp is not provided") != std::string::npos);
  CHECK(printed.find("ext2sim merge") != std::string::npos);
}

TEST_CASE("ext2sim run writes the sim file and reports", "[cap:netlist-sim][req:Command options and defaults]") {
  hefesto::editor::EditorState editor;
  Runner run{editor};
  REQUIRE(run({"inv"}).ok());
  CHECK(run.file("inv.sim") == "| units: 1 tech: nltest format: MIT\nC a vdd! 5.0\n= a u1/x\n");
  CHECK(run.file("inv.al") == "<none>");
  CHECK(run.file("inv.nodes") == "<none>");
  CHECK(run.messages.back() == "exttosim finished.");
  // -o names the output of this run only.
  REQUIRE(run({"-o", "other.sim", "inv"}).ok());
  CHECK(run.file("other.sim") != "<none>");
  REQUIRE(run.files.write_atomic(platform::Path{"inv.sim"}, "old").ok());
  REQUIRE(run({"inv"}).ok());
  CHECK(run.file("inv.sim").starts_with("| units:"));
}

TEST_CASE("ext2sim extresist file naming", "[cap:netlist-sim][req:Command options and defaults]") {
  hefesto::editor::EditorState editor;
  editor.netlist.sim.extresist = true;
  editor.netlist.sim.alias = true;
  Runner run{editor};
  REQUIRE(run({"inv"}).ok());
  CHECK(run.file("inv.ext.sim") != "<none>");
  CHECK(run.file("inv.ext.al") == "= a u1/x\n");
}

// Q-NETLIST-52 (fix, both modes): a run after `alias off` / `labels off`
// never writes or closes the previous run's `.al` / `.nodes`.
TEST_CASE("ext2sim alias and labels turned off after a run", "[cap:netlist-sim][req:Command options and defaults]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    hefesto::editor::EditorState editor;
    Runner run{editor};
    editor.netlist.sim.alias = true;
    editor.netlist.sim.labels = true;
    REQUIRE(run({"inv"}, compat).ok());
    CHECK(run.file("inv.al") == "= a u1/x\n");
    CHECK(run.file("inv.nodes") == "a 0 0 metal1\nvdd! 0 0 space\n");
    REQUIRE(run.files.write_atomic(platform::Path{"inv.al"}, "first run\n").ok());
    REQUIRE(run.files.write_atomic(platform::Path{"inv.nodes"}, "first run\n").ok());
    editor.netlist.sim.alias = false;
    editor.netlist.sim.labels = false;
    REQUIRE(run({"inv"}, compat).ok());
    CHECK(run.file("inv.sim").find("= a u1/x\n") != std::string::npos);
    CHECK(run.file("inv.al") == "first run\n");
    CHECK(run.file("inv.nodes") == "first run\n");
  }
}

TEST_CASE("ext2sim without a cell or window", "[cap:netlist-sim][req:Command options and defaults]") {
  hefesto::editor::EditorState editor;
  Runner run{editor};
  REQUIRE(run({}).ok());
  CHECK(run.reads.empty());
  CHECK(run.messages == std::vector<std::string>{"Point to a window or specify a cell name."});
}

// Q-NETLIST-18 (compat): which threshold enables the coupling lines.
TEST_CASE("ext2sim run-level coupling threshold with magic-compat ON",
          "[cap:netlist-sim][req:File structure and order]") {
  hefesto::editor::EditorState editor;
  editor.netlist.sim.cthresh = std::numeric_limits<double>::infinity();
  Runner run{editor};
  REQUIRE(run({"-c", "1", "inv"}, true).ok());
  REQUIRE(run.reads.size() == 1);
  CHECK_FALSE(extflat::has(run.reads.front().flags, extflat::FlatFlags::flat_caps));
  CHECK(run.file("inv.sim").find("C a vdd! 5.0") == std::string::npos);
}

TEST_CASE("ext2sim run-level coupling threshold with magic-compat OFF",
          "[cap:netlist-sim][req:File structure and order]") {
  hefesto::editor::EditorState editor;
  editor.netlist.sim.cthresh = std::numeric_limits<double>::infinity();
  Runner run{editor};
  REQUIRE(run({"-c", "1", "inv"}, false).ok());
  REQUIRE(run.reads.size() == 1);
  CHECK(extflat::has(run.reads.front().flags, extflat::FlatFlags::flat_caps));
  CHECK(run.file("inv.sim").find("C a vdd! 5.0\n") != std::string::npos);
}

// simParseArgs' switches are globals in Magic: sticky with magic-compat ON,
// this run only with it OFF.
TEST_CASE("ext2sim run flags stay set with magic-compat ON only", "[cap:netlist-sim][req:Command options and defaults]") {
  hefesto::editor::EditorState editor;
  Runner run{editor};
  REQUIRE(run({"-B", "-f", "su", "inv"}, false).ok());
  CHECK(run.file("inv.sim").starts_with("| units: 1 tech: nltest format: SU\n"));
  CHECK(editor.netlist.sim == sim::SimSettings{});
  REQUIRE(run({"-B", "-f", "su", "inv"}, true).ok());
  CHECK(editor.netlist.sim.no_attrs);
  CHECK(editor.netlist.sim.format == sim::SimFormat::su);
  REQUIRE_FALSE(run({"-q", "inv"}, true).ok());
}

// Q-NETLIST-16 (compat): the name flags an earlier ext2spice left set.
TEST_CASE("ext2sim name conversion after ext2spice with magic-compat ON",
          "[cap:netlist-sim][req:Name conversion state]") {
  hefesto::editor::EditorState editor;
  editor.netlist.names = extflat::NameFlags{true, true, false, true, false};  // as ext2spice leaves them
  Runner run{editor};
  REQUIRE(run({"inv"}, true).ok());
  CHECK(run.file("inv.sim").find("C a vdd 5.0\n") != std::string::npos);
  REQUIRE(run({"-t", "#", "inv"}, true).ok());
  CHECK(editor.netlist.names.trim_local);  // -t adds to the session's flags
}

TEST_CASE("ext2sim name conversion after ext2spice with magic-compat OFF",
          "[cap:netlist-sim][req:Name conversion state]") {
  hefesto::editor::EditorState editor;
  const extflat::NameFlags session{true, true, false, true, false};
  editor.netlist.names = session;
  Runner run{editor};
  REQUIRE(run({"-t", "#", "inv"}, false).ok());
  CHECK(run.file("inv.sim").find("C a vdd! 5.0\n") != std::string::npos);
  CHECK(editor.netlist.names == session);  // the run leaves the session's flags alone
}

// ext2sim.c:624-662: with EFStyle NULL the style's device table is not used,
// and the SU format warns that area and perimeter are zero.
TEST_CASE("ext2sim when the extraction style does not match", "[cap:netlist-sim][req:Transistor attributes]") {
  hefesto::editor::EditorState editor;
  Runner run{editor};
  run.circuit.style_matched = false;
  REQUIRE(run({"-f", "su", "inv"}, false).ok());
  REQUIRE(run.messages.size() >= 2);
  CHECK(run.messages[0] == "Warning:  Current extraction style does not match .ext file!");
  CHECK(run.messages[1] == "Area/Perimeter values will be zero.");
  run.messages.clear();
  REQUIRE(run({"inv"}, false).ok());  // MIT: no warning
  CHECK(run.messages == std::vector<std::string>{"exttosim finished."});
}

// Regression: the default source flattened through the overload that drops
// every message and turns a failure into an empty netlist, so a use cycle
// wrote a header-only `.sim` and reported "exttosim finished.".
TEST_CASE("ext2sim fails on a flattening error and writes no file",
          "[cap:netlist-sim][req:Command options and defaults]") {
  ExtRunner run;
  run.ext("top", "use a a0 1 0 0 0 1 0\n");
  run.ext("a", "use top t0 1 0 0 0 1 0\n");
  const cmd::CommandResult r = run({"top"});
  REQUIRE_FALSE(r.ok());
  REQUIRE_FALSE(r.diagnostics.empty());
  CHECK(r.diagnostics.back().code == "EXTFLAT-READ-010");
  CHECK_FALSE(run.files.exists(platform::Path{"top.sim"}));
  CHECK_FALSE(run.printed("exttosim finished."));
}

TEST_CASE("ext2sim prints the flattening messages", "[cap:netlist-sim][req:Command options and defaults]") {
  ExtRunner run;
  run.ext("top", "node a 0 0 0 0 m1\ncap a nosuch 5000\n");
  REQUIRE(run({"top"}).ok());
  CHECK(run.printed("cap(2): no such node nosuch"));
  CHECK(run.printed("exttosim finished."));
}

// Regression: -s/-S were parsed and dropped, and the flattening used the
// default 2 fF threshold instead of the run's.
TEST_CASE("ext2sim passes the run's threshold and symbols to the flattening",
          "[cap:netlist-sim][req:Command options and defaults]") {
  hefesto::editor::EditorState editor;
  Runner run{editor};
  REQUIRE(run.files.write_atomic(platform::Path{"syms"}, "W=7\nbad\n").ok());
  REQUIRE(run({"-c", "0.5", "-s", "L=4", "-S", "syms", "inv"}).ok());
  REQUIRE(run.reads.size() == 1);
  const extflat::FlattenOptions& flatten = run.reads.front().flatten;
  CHECK(flatten.cap_threshold == 0.5);
  CHECK(flatten.symbols == extflat::SymbolTable{{"L", 4}, {"W", 7}});
  REQUIRE(run.messages.size() >= 2);
  CHECK(run.messages[0] == "Missing '=' in symbol assignment");
  CHECK(run.messages[1] == "Error at line 2 of syms");
}

// EFArgs runs simParseArgs for -h before its own usage text, so ext2sim
// prints "Unrecognized flag: -h" and its usage; -T is a flag of the
// standalone EFArgs only.
TEST_CASE("ext2sim -h and -T go to simParseArgs", "[cap:netlist-sim][req:Command options and defaults]") {
  hefesto::editor::EditorState editor;
  Runner run{editor};
  const cmd::CommandResult help = run({"-h"});
  REQUIRE_FALSE(help.ok());
  REQUIRE(run.messages.size() == 2);
  CHECK(run.messages[0] == "Unrecognized flag: -h");
  CHECK(run.messages[1].starts_with("Usage: ext2sim"));
  CHECK(help.diagnostics.back().text == extflat::common_args_usage());
  run.messages.clear();
  REQUIRE_FALSE(run({"-T", "scmos", "inv"}).ok());
  CHECK(run.messages.front() == "Unrecognized flag: -T");
  CHECK(run.reads.empty());
}
