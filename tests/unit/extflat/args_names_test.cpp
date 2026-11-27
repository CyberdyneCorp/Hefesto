// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 round 2: EFArgs (parse_common_args, add_symbol) and the SPICE and sim
// output name conversions with their per-run flags (Q-NETLIST-16,
// Q-NETLIST-30, Q-NETLIST-42). Owner: EXTFLAT.

#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ext_fixture.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/names.hpp"
#include "hier_name.hpp"

namespace extflat = hefesto::extflat;
using extflat::test::GoldenCase;
using extflat::test::nltest_options;
using Strings = std::vector<std::string>;

namespace {

hefesto::diag::Outcome<extflat::CommonArgs> parse(const Strings& args, const extflat::ExtraFlags& extra = {},
                                                  const extflat::ArgOptions& options = {}) {
  return extflat::parse_common_args(std::span<const std::string>{args}, extra, options);
}

// A tool that takes -o and -T, reading -S files from `files`.
extflat::ArgOptions all_flags(const std::map<std::string, std::string>& files = {}) {
  extflat::ArgOptions options{true, true, {}};
  options.read_file = [files](const std::string& path) -> hefesto::diag::Outcome<std::string> {
    const auto it = files.find(path);
    if (it == files.end()) {
      return hefesto::diag::make(hefesto::diag::codes::kPlatformNotFound, "no such file '" + path + "'");
    }
    return it->second;
  };
  return options;
}

hefesto::compat::CompatSettings compat_mode(bool on) {
  hefesto::compat::CompatSettings c;
  c.magic_compat = on;
  return c;
}

std::string spice(const Strings& components, extflat::NameFlags flags, bool compat = true,
                  extflat::SpiceDialect dialect = extflat::SpiceDialect::spice3) {
  return extflat::name_to_spice(std::span<const std::string>{components}, dialect, flags, compat_mode(compat));
}

std::string sim(const Strings& components, extflat::NameFlags flags) {
  return extflat::name_to_sim(std::span<const std::string>{components}, flags, compat_mode(true));
}

extflat::NameFlags spice_base() {
  extflat::NameFlags f = extflat::no_name_flags();
  f.trim_global = true;
  f.convert_comma = true;
  f.convert_equal = true;
  return f;
}

Strings texts(const extflat::CommonArgs& a) {
  Strings out;
  for (const auto& d : a.messages) {
    out.push_back(d.text);
  }
  return out;
}

}  // namespace

// ---- Common netlister arguments --------------------------------------------

TEST_CASE("EFArgs -c sets this run's capacitance threshold and a word is the root cell",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  const auto a = parse({"-c", "0.5", "top"});
  REQUIRE(a.ok());
  REQUIRE(a.value().cap_threshold.has_value());
  CHECK(*a.value().cap_threshold == 0.5);
  CHECK(a.value().cell == std::optional<std::string>{"top"});
  CHECK(a.value().messages.empty());
}

TEST_CASE("a flag's value may be glued to it; -C and -R are infinite",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  const auto a = parse({"-r50", "-C", "-R", "-v", "-pa:b:c", "-t!", "-t", "#,", "-Tnltest", "-S", "syms"}, {},
                       all_flags({{"syms", "A=1\n"}}));
  REQUIRE(a.ok());
  const auto& v = a.value();
  CHECK(v.cap_infinite);
  CHECK(*v.cap_threshold == static_cast<double>(1.0E38F));
  CHECK(v.resist_infinite);
  CHECK(*v.resist_threshold == INT_MAX);  // -R after -r50 wins
  CHECK(v.verbose);
  CHECK(v.search_path == Strings{"a", "b", "c"});
  CHECK(v.trim_chars == "!#,");  // every -t concatenated
  CHECK(v.tech == std::optional<std::string>{"nltest"});
  CHECK(v.symbol_file == std::optional<std::string>{"syms"});
  CHECK(v.symbols == extflat::SymbolTable{{"A", 1}});
  CHECK_FALSE(v.cell.has_value());
}

TEST_CASE("a later root cell replaces an earlier one with a warning; .ext is removed",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  const auto a = parse({"first", "second.ext"});
  REQUIRE(a.ok());
  CHECK(a.value().cell == std::optional<std::string>{"second"});
  CHECK(texts(a.value()) == Strings{"Warning: multiple input files specified; ignoring \"first\""});
}

TEST_CASE("EFArgs -s accepts numeric symbols once; rejected ones are reported and skipped",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  const auto a = parse({"-s", "L=4", "-s", "W=x", "-s", "L=5", "-s", "noeq"});
  REQUIRE(a.ok());
  CHECK(a.value().symbols == extflat::SymbolTable{{"L", 4}});
  CHECK(texts(a.value()) == Strings{"Symbol value must be numeric; ignoring \"W=x\"", "Symbol \"L\" already defined",
                                    "Missing '=' in symbol assignment"});
  extflat::SymbolTable table;
  CHECK_FALSE(extflat::add_symbol(table, "N=-3").has_value());
  CHECK(table.at("N") == -3);
}

TEST_CASE("an unknown flag prints the usage text and fails; a tool flag is accepted",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  const auto bad = parse({"-q", "top"});
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().text == extflat::common_args_usage());
  const auto missing = parse({"-c"});
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().text == "-c requires a following cap threshold\n" + std::string{extflat::common_args_usage()});
  // ext2spice's -d: the tool's callback consumes it.
  const extflat::ExtraFlags extra = [](std::span<const std::string> args, std::size_t i) -> std::optional<std::size_t> {
    return args[i] == "-d" ? std::optional<std::size_t>{i + 1} : std::nullopt;
  };
  const auto ok = parse({"-d", "top"}, extra);
  REQUIRE(ok.ok());
  CHECK(ok.value().cell == std::optional<std::string>{"top"});
}

// Regression (Q-NETLIST-77, compat-neutral: the same text in both modes;
// the parse takes no compat setting): EFArgs writes an empty line before
// the usage text when the tool's own parser rejects a flag (ext2spice -T;
// EFargs.c:288-291), and none for a tool without a parser.
TEST_CASE("Q-NETLIST-77: a flag the tool's parser rejects is followed by an empty line, then the usage text",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  const extflat::ExtraFlags rejects_all = [](std::span<const std::string>, std::size_t) -> std::optional<std::size_t> {
    return std::nullopt;
  };
  const auto rejected = parse({"-T", "top"}, rejects_all);
  REQUIRE_FALSE(rejected.ok());
  CHECK(rejected.error().text == "\n" + std::string{extflat::common_args_usage()});
  const auto no_parser = parse({"-T", "top"});
  REQUIRE_FALSE(no_parser.ok());
  CHECK(no_parser.error().text == extflat::common_args_usage());
}

// Regression (Q-NETLIST-42, fix in both modes): `-o` names the output of
// one run; a later run without it has no output name.
TEST_CASE("EFArgs -o applies to one run only (Q-NETLIST-42)", "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  const auto first = parse({"-o", "out.spice", "top"}, {}, all_flags());
  REQUIRE(first.ok());
  CHECK(first.value().output == std::optional<std::string>{"out.spice"});
  const auto second = parse({"top"}, {}, all_flags());
  REQUIRE(second.ok());
  CHECK_FALSE(second.value().output.has_value());
}

// efSymAddFile: each line of a -S file is a -s argument, in argument order
// with the -s flags; a rejected line is followed by its line number.
TEST_CASE("EFArgs -S reads a symbol file in argument order",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  const auto a = parse({"-s", "L=4", "-S", "syms", "-s", "W=9", "top"}, {},
                       all_flags({{"syms", "W=7\nL=5\n\nX=y"}}));
  REQUIRE(a.ok());
  CHECK(a.value().symbols == extflat::SymbolTable{{"L", 4}, {"W", 7}});
  CHECK(texts(a.value()) == Strings{"Symbol \"L\" already defined", "Error at line 2 of syms",
                                    "Missing '=' in symbol assignment", "Error at line 3 of syms",
                                    "Symbol value must be numeric; ignoring \"X=y\"", "Error at line 4 of syms",
                                    "Symbol \"W\" already defined"});
  const auto missing = parse({"-S", "nope", "top"}, {}, all_flags());
  REQUIRE(missing.ok());
  CHECK(texts(missing.value()) == Strings{"nope: No such file or directory"});
  CHECK(missing.value().cell == std::optional<std::string>{"top"});
}

// -o and -T are opt-in: a tool without them sees the flag as unknown.
TEST_CASE("EFArgs -o and -T only for the tools that take them",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  for (const char* flag : {"-o", "-T"}) {
    INFO(flag);
    const auto bad = parse({flag, "x", "top"});
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().text == extflat::common_args_usage());
  }
  std::vector<std::string> seen;
  const extflat::ExtraFlags extra = [&seen](std::span<const std::string> args, std::size_t i) -> std::optional<std::size_t> {
    seen.push_back(args[i]);
    return std::nullopt;
  };
  (void)parse({"-T", "x"}, extra);
  CHECK(seen == Strings{"-T"});
}

// EFargs.c case 'h': argsProc runs first, then the usage text.
TEST_CASE("EFArgs -h calls the tool's flag parser before failing",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  std::vector<std::string> seen;
  const extflat::ExtraFlags extra = [&seen](std::span<const std::string> args, std::size_t i) -> std::optional<std::size_t> {
    seen.push_back(args[i]);
    return i + 1;
  };
  const auto help = parse({"-h", "top"}, extra);
  REQUIRE_FALSE(help.ok());
  CHECK(help.error().text == extflat::common_args_usage());
  CHECK(seen == Strings{"-h"});
}

// ---- SPICE output name conversion ------------------------------------------

TEST_CASE("default SPICE conversion of an array element path", "[cap:ext-flattening-names][req:SPICE output name conversion]") {
  CHECK(spice({"u1[0,1]", "vdd!"}, spice_base()) == "u1[0|1]/vdd");
  CHECK(spice({"x", "a.b"}, spice_base(), true, extflat::SpiceDialect::hspice) == "x/a@b");
  CHECK(spice({"x.y", "a.b"}, spice_base()) == "x.y/a.b");  // '.' is kept outside HSPICE
}

TEST_CASE("prefix and leaf characters convert or vanish with their flags",
          "[cap:ext-flattening-names][req:SPICE output name conversion]") {
  extflat::NameFlags f = extflat::no_name_flags();
  f.convert_brackets = true;
  // Prefix: '=' kept, ',' deleted without convert-comma; leaf: both deleted.
  CHECK(spice({"a=b,c[1]", "d=e,f[2]"}, f) == "a=bc_1_/def_2_");
  // With every flag off only the leaf is printed verbatim.
  CHECK(spice({"x", "a,b!"}, extflat::no_name_flags()) == "x/a,b!");
}

TEST_CASE("every ! in a SPICE leaf is deleted with magic-compat ON (Q-NETLIST-30)",
          "[cap:ext-flattening-names][req:SPICE output name conversion]") {
  CHECK(spice({"a!b"}, spice_base(), true) == "ab");
  extflat::NameFlags f = spice_base();
  f.trim_local = true;
  CHECK(spice({"n#1#"}, f, true) == "n1");
}

TEST_CASE("only a trailing ! or # is deleted with magic-compat OFF (Q-NETLIST-30)",
          "[cap:ext-flattening-names][req:SPICE output name conversion]") {
  CHECK(spice({"a!b"}, spice_base(), false) == "a!b");
  CHECK(spice({"vdd!"}, spice_base(), false) == "vdd");
  extflat::NameFlags f = spice_base();
  f.trim_local = true;
  CHECK(spice({"n#1#"}, f, false) == "n#1");
}

// Regression (Q-NETLIST-30 tier 1, both modes): Magic deletes a ',' of a
// prefix component by stepping its output back, before the buffer for a
// leading ','. The comma is dropped and nothing else changes.
TEST_CASE("a leading comma of the root prefix component is dropped in both modes",
          "[cap:ext-flattening-names][req:SPICE output name conversion]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    extflat::NameFlags f = spice_base();
    f.convert_comma = false;
    CHECK(spice({",u1", "a"}, f, compat) == "u1/a");
  }
}

// ---- Sim output name conversion --------------------------------------------

TEST_CASE("sim names trim only the last leaf character", "[cap:ext-flattening-names][req:Sim output name conversion]") {
  extflat::NameFlags f = extflat::no_name_flags();
  f.trim_global = true;
  CHECK(sim({"a!b!"}, f) == "a!b");
  CHECK(sim({"x,y", "a,b"}, f) == "x,y/ab");  // ',' deleted in the leaf only
  f.convert_comma = true;
  f.convert_brackets = true;
  CHECK(sim({"p[0]", "q[1]"}, f) == "p[0]/q_1]");  // the last character is kept verbatim
  CHECK(sim({"x", "a!"}, extflat::no_name_flags()) == "x/a!");
}

TEST_CASE("an ext2spice run leaves its flags for a later ext2sim with magic-compat ON (Q-NETLIST-16)",
          "[cap:ext-flattening-names][req:Sim output name conversion]") {
  const auto c = compat_mode(true);
  extflat::NameFlags session = extflat::no_name_flags();
  const auto run = extflat::spice_run_flags(session, extflat::SpiceDialect::hspice, "[", c);
  CHECK(run.trim_global);
  CHECK(run.trim_local);
  CHECK(run.convert_brackets);
  const auto later = extflat::sim_run_flags(session, "", c);
  CHECK(later == run);  // one session-wide set, only ever added to
  CHECK(sim({"vdd!"}, later) == "vdd");
}

TEST_CASE("each run starts from its own base flags with magic-compat OFF (Q-NETLIST-16)",
          "[cap:ext-flattening-names][req:Sim output name conversion]") {
  const auto c = compat_mode(false);
  extflat::NameFlags session = extflat::no_name_flags();
  const auto run = extflat::spice_run_flags(session, extflat::SpiceDialect::cdl, "#", c);
  CHECK_FALSE(run.trim_global);  // CDL keeps '!'
  CHECK(run.trim_local);
  CHECK(run.convert_comma);
  CHECK(run.convert_equal);
  const auto later = extflat::sim_run_flags(session, "", c);
  CHECK(later == extflat::no_name_flags());
  CHECK(session == extflat::no_name_flags());
  CHECK(sim({"vdd!"}, later) == "vdd!");
}

// ---- NL-FLAT-18: the session's conversions over the flat device names ------

namespace {

// Each device's terminal names (gate, then the others) converted by `convert`.
template <typename F>
std::vector<Strings> converted(const extflat::Netlist& n, F&& convert) {
  std::vector<Strings> out;
  for (const auto& d : n.devices) {
    Strings row;
    for (const auto& t : d.terminals) {
      row.push_back(convert(hefesto::extflat::detail::name_components(hefesto::extflat::detail::name_from_string(t.node))));
    }
    out.push_back(row);
  }
  return out;
}

}  // namespace

// The session of NL-FLAT-18's commands.tcl with magic-compat ON: ext2spice
// spice3, hspice and cdl, then `ext2sim -t !#,=[`; the flags accumulate
// (Q-NETLIST-16). Expected: expected/collected/top.{spice3.spice,cdl,sim}
// (HSPICE prefixes become x<n> in the writer, so only top-level names are
// compared for it).
TEST_CASE("NL-FLAT-18: names through spice3, hspice, cdl and sim in one session",
          "[cap:ext-flattening-names][req:SPICE output name conversion][req:Sim output name conversion]") {
  GoldenCase g{"NL-FLAT-18"};
  const auto n = g.flat("top", extflat::test::ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  const auto c = compat_mode(true);
  extflat::NameFlags session = extflat::no_name_flags();
  const auto as_spice = [&](extflat::SpiceDialect dialect) {
    extflat::NameFlags run = extflat::spice_run_flags(session, dialect, "", c);
    return converted(n.value(), [&](const Strings& parts) {
      return extflat::name_to_spice(std::span<const std::string>{parts}, dialect, run, c);
    });
  };
  CHECK(as_spice(extflat::SpiceDialect::spice3) ==
        std::vector<Strings>{{"u:1|a.b/a|b", "u:1|a.b/c:d", "u:1|a.b/e[1]"},
                             {"u:1|a.b/f.g", "u:1|a.b/h#", "u:1|a.b/i"},
                             {"t.o|p", "t.o|p", "t.o|p"}});
  CHECK(as_spice(extflat::SpiceDialect::hspice).back() == Strings{"t@o|p", "t@o|p", "t@o|p"});
  CHECK(as_spice(extflat::SpiceDialect::cdl) ==
        std::vector<Strings>{{"u:1|a.b/a|b", "u:1|a.b/c:d", "u:1|a.b/e[1]"},
                             {"u:1|a.b/f.g", "u:1|a.b/h", "u:1|a.b/i!"},
                             {"t.o|p", "t.o|p", "t.o|p"}});
  extflat::NameFlags sim_run = extflat::sim_run_flags(session, "!#,=[", c);
  CHECK(converted(n.value(), [&](const Strings& parts) {
          return extflat::name_to_sim(std::span<const std::string>{parts}, sim_run, c);
        }) == std::vector<Strings>{{"u=1,a.b/a|b", "u=1,a.b/c=d", "u=1,a.b/e_1]"},
                                   {"u=1,a.b/f.g", "u=1,a.b/h", "u=1,a.b/i"},
                                   {"t.o|p", "t.o|p", "t.o|p"}});
}
