// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol generators (capability schematic-symbols-and-libraries "Symbol
// generators"; Q-SCHEM-11): Tcl generators run in the sandbox and their
// output is the symbol; other generators are missing symbols.
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/generator.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/substitute.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;

namespace {

constexpr const char* kGenerators = "[cap:schematic-symbols-and-libraries][req:Symbol generators]";

// Prints a symbol with argv[0] inout pins named p0, p1, ...
constexpr std::string_view kPinsGenerator = R"tcl(
set n [lindex $argv 0]
puts "v {xschem version=3.4.8RC file_version=1.2}"
puts "K {type=subcircuit format=\"@name @pinlist @symname\" template=\"name=x1\"}"
for {set i 0} {$i < $n} {incr i} {
  puts "B 5 [expr {$i * 20}] 0 [expr {$i * 20 + 5}] 5 {name=p$i dir=inout}"
}
puts -nonewline "T {args=$argc:[join $argv ,]} 0 -20 0 0 0.2 0.2 {}\n"
puts stderr "not part of the symbol"
)tcl";

std::vector<diag::Diagnostic> with_code(const std::vector<diag::Diagnostic>& all, std::string_view code) {
  std::vector<diag::Diagnostic> out;
  std::copy_if(all.begin(), all.end(), std::back_inserter(out), [code](const auto& d) { return d.code == code; });
  return out;
}

struct Library {
  platform::MemoryFileSystem fs;
  schem::SearchPath path{{"/lib"}};
  schem::Document top;

  Library() {
    REQUIRE(fs.write_atomic("/lib/pins.tcl", kPinsGenerator).ok());
    top.path = "/w/top.sch";
  }
};

}  // namespace

TEST_CASE("Generator name in netlist", kGenerators) {
  CHECK(schem::symbol_name("ramdec(4, 8)") == "ramdec_4_8");
  CHECK(schem::symbol_name("res.tcl(1, 2.5)") == "res_tcl_1_2_5");
}

TEST_CASE("generator references are recognised and split", kGenerators) {
  CHECK(schem::is_generator_reference("ramdec(4, 8)"));
  CHECK(schem::is_generator_reference("res.tcl(1,2) \t"));
  CHECK(schem::is_generator_reference("lib/gen()"));
  CHECK_FALSE(schem::is_generator_reference("(4)"));
  CHECK_FALSE(schem::is_generator_reference("a b(4)"));
  CHECK_FALSE(schem::is_generator_reference("gen(4(5))"));
  CHECK_FALSE(schem::is_generator_reference("gen(4) x"));
  CHECK_FALSE(schem::is_generator_reference("inv.sym"));
  CHECK(schem::parse_generator("ramdec(4, 8)") == schem::GeneratorCall{"ramdec", {"4", "8"}});
  CHECK(schem::parse_generator("res.tcl( a ,b c)") == schem::GeneratorCall{"res.tcl", {"a", "b", "c"}});
  CHECK(schem::parse_generator("gen()") == schem::GeneratorCall{"gen", {}});
  CHECK_FALSE(schem::parse_generator("inv.sym").has_value());
}

TEST_CASE("a Tcl generator's output is its symbol", kGenerators) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Library library;
    diag::CollectingSink sink;
    schem::SymbolCache cache;
    const schem::Symbol& symbol = cache.resolve({"pins.tcl(3, 8)"}, library.top, {library.path, library.fs, &sink, compat});
    REQUIRE_FALSE(symbol.placeholder);
    CHECK(symbol.reference == "pins.tcl(3, 8)");
    CHECK(symbol.file == "/lib/pins.tcl");
    REQUIRE(symbol.pins.size() == 3);
    CHECK(symbol.pins[2].name == "p2");
    REQUIRE(symbol.document.texts.size() == 1);
    CHECK(symbol.document.texts[0].text == "args=2:3,8");
    CHECK(sink.take().empty());
    // Generated once per reference; another argument list runs it again.
    CHECK(&cache.resolve({"pins.tcl(3, 8)"}, library.top, {library.path, library.fs, nullptr, compat}) == &symbol);
    CHECK(cache.resolve({"pins.tcl(1)"}, library.top, {library.path, library.fs, nullptr, compat}).pins.size() == 1);
  }
}

TEST_CASE("a generator that runs an external program is a missing symbol", kGenerators) {
  Library library;
  REQUIRE(library.fs.write_atomic("/lib/ramdec", "#!/bin/sh\necho 'v {}'\n").ok());
  REQUIRE(library.fs.write_atomic("/lib/tclgen", "#!/usr/bin/env tclsh\nputs {v {xschem version=3.4.8RC file_version=1.2}}\n").ok());
  CHECK_FALSE(schem::is_tcl_generator("/lib/ramdec", library.fs));
  CHECK(schem::is_tcl_generator("/lib/tclgen", library.fs));
  CHECK(schem::is_tcl_generator("/lib/pins.tcl", library.fs));
  diag::CollectingSink sink;
  schem::SymbolCache cache;
  const schem::ResolveContext context{library.path, library.fs, &sink};
  CHECK(cache.resolve({"ramdec(4, 8)"}, library.top, context).placeholder);
  CHECK(cache.resolve({"ramdec(2)"}, library.top, context).placeholder);
  const auto external = with_code(sink.take(), "SCHEM-LIB-011");
  REQUIRE(external.size() == 1);
  CHECK(external[0].text == "Symbol ramdec(4, 8): symbol generators that run external programs are not supported");
  CHECK_FALSE(cache.resolve({"tclgen(1)"}, library.top, context).placeholder);
  CHECK(cache.resolve({"nope(1)"}, library.top, context).placeholder);
  CHECK(with_code(sink.take(), "SCHEM-LIB-003").size() == 1);
}

TEST_CASE("a generator runs in the sandbox", kGenerators) {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/lib/evil.tcl",
                          "if {[catch {exec ls}]} { puts exec_denied }\n"
                          "if {[catch {open /etc/hosts r}]} { puts read_denied }\n"
                          "if {[catch {open /tmp/hefesto-generator-out w}]} { puts write_denied }\n"
                          "puts $env(HOME)\n")
              .ok());
  const auto output = schem::run_tcl_generator(fs, {"/lib/evil.tcl", {}, "/w/s"});
  REQUIRE(output.text.has_value());
  CHECK(*output.text == "exec_denied\nread_denied\nwrite_denied\n/w/s\n");
}

// Hangs and unbounded output are bounded in both modes: an endless
// generator would have hung the opening of every schematic that places it.
TEST_CASE("an endless or verbose generator is stopped", kGenerators) {
  CHECK(schem::kGeneratorTimeLimit == std::chrono::seconds{10});
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/lib/loop.tcl", "while 1 { catch { while 1 {} } }\n").ok());
  REQUIRE(fs.write_atomic("/lib/chatty.tcl", "while 1 { catch { puts [string repeat x 100] } }\n").ok());
  schem::GeneratorInput loop{"/lib/loop.tcl", {}, "/w"};
  loop.time_limit = std::chrono::milliseconds{200};
  const auto start = std::chrono::steady_clock::now();
  const auto stopped = schem::run_tcl_generator(fs, loop);
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{10});
  CHECK_FALSE(stopped.text.has_value());
  REQUIRE(stopped.diagnostics.size() == 1);
  CHECK(stopped.diagnostics[0].code == "SCHEM-LIB-012");
  CHECK(stopped.diagnostics[0].text == "Symbol generator /lib/loop.tcl failed: stopped after 200 ms");

  schem::GeneratorInput chatty{"/lib/chatty.tcl", {}, "/w"};
  chatty.max_output = 1000;
  chatty.time_limit = std::chrono::milliseconds{2000};
  const auto verbose = schem::run_tcl_generator(fs, chatty);
  CHECK_FALSE(verbose.text.has_value());
  REQUIRE(verbose.diagnostics.size() == 1);
  CHECK(verbose.diagnostics[0].text == "Symbol generator /lib/chatty.tcl failed: it printed more than 1000 bytes");
}

TEST_CASE("a failing generator is a missing symbol, run once", kGenerators) {
  Library library;
  REQUIRE(library.fs.write_atomic("/lib/bad.tcl", "error {no such cell}\n").ok());
  diag::CollectingSink sink;
  schem::SymbolCache cache;
  const schem::ResolveContext context{library.path, library.fs, &sink};
  CHECK(cache.resolve({"bad.tcl(1)"}, library.top, context).placeholder);
  CHECK(cache.resolve({"bad.tcl(1)"}, library.top, context).placeholder);
  const auto failed = with_code(sink.take(), "SCHEM-LIB-012");
  REQUIRE(failed.size() == 1);
  CHECK(failed[0].text.find("no such cell") != std::string::npos);
  cache.forget("/lib/bad.tcl");
  CHECK(cache.resolve({"bad.tcl(1)"}, library.top, context).placeholder);
  CHECK(with_code(sink.take(), "SCHEM-LIB-012").size() == 1);
}

// Regression (review of PR #88), both modes: each distinct generator
// reference ran for up to 10 s with no total, so a schematic with 10,000
// references `gen.tcl(1)`, `gen.tcl(2)`, ... blocked its opening for about a
// day. The runs of one cache now share kGeneratorTotalTimeLimit (until
// clear()); past it no generator runs, with one SCHEM-LIB-012.
TEST_CASE("symbol generators share a total time limit", kGenerators) {
  CHECK(schem::kGeneratorTotalTimeLimit == std::chrono::seconds{60});
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Library library;
    REQUIRE(library.fs.write_atomic("/lib/loop.tcl", "while 1 { catch { while 1 {} } }\n").ok());
    diag::CollectingSink sink;
    schem::SymbolCache cache({}, std::chrono::milliseconds{300});
    const schem::ResolveContext context{library.path, library.fs, &sink, compat};
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
      CHECK(cache.resolve({"loop.tcl(" + std::to_string(i) + ")"}, library.top, context).placeholder);
    }
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{5});
    const auto failed = with_code(sink.take(), "SCHEM-LIB-012");
    REQUIRE(failed.size() == 2);  // the run stopped at 300 ms, then the total
    CHECK(failed[0].text == "Symbol generator /lib/loop.tcl failed: stopped after 300 ms");
    CHECK(failed[1].text.find("symbol generators ran for their total of 300 ms") != std::string::npos);
    cache.clear();  // reloading the symbols gives the generators their time again
    CHECK_FALSE(cache.resolve({"pins.tcl(1)"}, library.top, context).placeholder);
  }
}

// Regression (review of PR #88): telling whether a file without `.tcl` is a
// Tcl generator read the whole file for its first line; a file over
// kMaxGeneratorScriptBytes is now not read (and not a Tcl generator).
TEST_CASE("a generator file over the script size limit is not read", kGenerators) {
  CHECK(schem::kMaxGeneratorScriptBytes == schem::kMaxFlattenedBytes);
  platform::MemoryFileSystem fs;
  const std::string script = "#!/usr/bin/env tclsh\nputs {v {xschem version=3.4.8RC file_version=1.2}}\n";
  REQUIRE(fs.write_atomic("/lib/gen", script).ok());
  REQUIRE(fs.write_atomic("/lib/gen.tcl", script).ok());
  CHECK(schem::is_tcl_generator("/lib/gen", fs));
  CHECK_FALSE(schem::is_tcl_generator("/lib/gen", fs, script.size() - 1));
  schem::GeneratorInput input{"/lib/gen.tcl", {}, "/w"};
  CHECK(schem::run_tcl_generator(fs, input).text.has_value());
  input.max_script = script.size() - 1;
  const auto output = schem::run_tcl_generator(fs, input);
  CHECK_FALSE(output.text.has_value());
  REQUIRE(output.diagnostics.size() == 1);
  CHECK(output.diagnostics[0].text.find("the script is larger than " + std::to_string(script.size() - 1) + " bytes") !=
        std::string::npos);
}

// ---- User decisions M8-D5 to M8-D7 (approved 2026-11-23) ----------------------

namespace {

// A file system that reports chosen sizes and counts the reads of each file,
// so a test can prove a generator file was never read.
class WatchedFileSystem final : public platform::FileSystem {
 public:
  explicit WatchedFileSystem(platform::FileSystem& base) : base_(base) {}

  std::map<platform::Path, std::uint64_t> sizes;  // reported instead of the real size
  std::map<platform::Path, int> reads;

  diag::Outcome<std::string> read(const platform::Path& path) override {
    ++reads[path];
    return base_.read(path);
  }
  diag::Status write_atomic(const platform::Path& path, std::string_view data) override {
    return base_.write_atomic(path, data);
  }
  diag::Status append(const platform::Path& path, std::string_view data) override { return base_.append(path, data); }
  bool exists(const platform::Path& path) override { return base_.exists(path); }
  diag::Outcome<std::uint64_t> size(const platform::Path& path) override {
    const auto it = sizes.find(path);
    return it != sizes.end() ? diag::Outcome<std::uint64_t>{it->second} : base_.size(path);
  }
  diag::Outcome<std::vector<platform::Path>> list(const platform::Path& directory) override {
    return base_.list(directory);
  }
  diag::Status create_directories(const platform::Path& directory) override {
    return base_.create_directories(directory);
  }
  diag::Status rename(const platform::Path& from, const platform::Path& to) override { return base_.rename(from, to); }
  diag::Status remove(const platform::Path& path) override { return base_.remove(path); }

 private:
  platform::FileSystem& base_;
};

std::size_t count_text(const std::vector<diag::Diagnostic>& all, std::string_view text) {
  return static_cast<std::size_t>(
      std::count_if(all.begin(), all.end(), [text](const auto& d) { return d.text.find(text) != std::string::npos; }));
}

}  // namespace

// M8-D5 (approved 2026-11-23; Q-row assigned by DECIDE), both modes,
// identical: the runs of one cache share the total (here 300 ms instead of
// 60 s); once it is spent no generator runs (its file is not even read),
// exactly one SCHEM-LIB-012 reports it, and reloading the symbols (clear(),
// `schematic reload_symbols`) gives the generators their total again.
TEST_CASE("M8-D5: no generator runs once the total time is spent, until reload", kGenerators) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Library library;
    REQUIRE(library.fs.write_atomic("/lib/loop.tcl", "while 1 {}\n").ok());
    WatchedFileSystem fs(library.fs);
    diag::CollectingSink sink;
    schem::SymbolCache cache({}, std::chrono::milliseconds{300});
    const schem::ResolveContext context{library.path, fs, &sink, compat};
    CHECK(cache.resolve({"loop.tcl(1)"}, library.top, context).placeholder);  // spends the 300 ms
    for (int i = 1; i <= 50; ++i) {
      CHECK(cache.resolve({"pins.tcl(" + std::to_string(i) + ")"}, library.top, context).placeholder);
      CHECK(cache.resolve({"loop.tcl(" + std::to_string(i + 1) + ")"}, library.top, context).placeholder);
    }
    CHECK(fs.reads["/lib/pins.tcl"] == 0);
    CHECK(fs.reads["/lib/loop.tcl"] == 1);
    const auto failed = with_code(sink.take(), "SCHEM-LIB-012");
    REQUIRE(failed.size() == 2);
    CHECK(failed[0].text == "Symbol generator /lib/loop.tcl failed: stopped after 300 ms");
    CHECK(count_text(failed, "symbol generators ran for their total of 300 ms") == 1);
    cache.clear();
    CHECK_FALSE(cache.resolve({"pins.tcl(1)"}, library.top, context).placeholder);
    CHECK(fs.reads["/lib/pins.tcl"] == 1);
    CHECK(sink.take().empty());
  }
}

// M8-D6 (approved 2026-11-23; Q-row assigned by DECIDE), both modes: a
// generator file larger than 268,435,456 bytes is not read, not even for its
// `#!` line, and not run: SCHEM-LIB-012 once per file, with or without
// `.tcl` (a file without `.tcl` gave SCHEM-LIB-011 before). A file of
// exactly the limit runs.
TEST_CASE("M8-D6: a generator file over 256 MiB is neither read nor run", kGenerators) {
  CHECK(schem::kMaxGeneratorScriptBytes == std::size_t{268'435'456});
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Library library;
    const std::string script = "#!/usr/bin/env tclsh\nputs {v {xschem version=3.4.8RC file_version=1.2}}\n";
    REQUIRE(library.fs.write_atomic("/lib/big", script).ok());
    REQUIRE(library.fs.write_atomic("/lib/big.tcl", script).ok());
    REQUIRE(library.fs.write_atomic("/lib/edge.tcl", script).ok());
    WatchedFileSystem fs(library.fs);
    fs.sizes = {{"/lib/big", schem::kMaxGeneratorScriptBytes + 1},
                {"/lib/big.tcl", schem::kMaxGeneratorScriptBytes + 1},
                {"/lib/edge.tcl", schem::kMaxGeneratorScriptBytes}};
    diag::CollectingSink sink;
    schem::SymbolCache cache;
    const schem::ResolveContext context{library.path, fs, &sink, compat};
    CHECK(cache.resolve({"big(1)"}, library.top, context).placeholder);
    CHECK(cache.resolve({"big(2)"}, library.top, context).placeholder);
    CHECK(cache.resolve({"big.tcl(1)"}, library.top, context).placeholder);
    CHECK_FALSE(cache.resolve({"edge.tcl(1)"}, library.top, context).placeholder);
    CHECK(fs.reads["/lib/big"] == 0);
    CHECK(fs.reads["/lib/big.tcl"] == 0);
    const auto all = sink.take();
    CHECK(with_code(all, "SCHEM-LIB-011").empty());
    const auto failed = with_code(all, "SCHEM-LIB-012");
    REQUIRE(failed.size() == 2);
    CHECK(failed[0].text ==
          "Symbol big(1): the generator /lib/big is larger than 268435456 bytes; it is neither read nor run");
    CHECK(failed[1].text.starts_with("Symbol big.tcl(1): the generator /lib/big.tcl is larger than"));
    // run_tcl_generator refuses it too, without reading it.
    CHECK_FALSE(schem::run_tcl_generator(fs, {"/lib/big.tcl", {}, "/w"}).text.has_value());
    CHECK(fs.reads["/lib/big.tcl"] == 0);
  }
}

// Regression (review of PR #101): the size was checked, then the whole file
// read, so a script that grew in between was read at any size. The read now
// stops one byte past the limit, and such a script is neither recognised as
// a generator nor run (here the file system reports a stale small size).
TEST_CASE("a generator script that grows after its size check is not run", kGenerators) {
  platform::MemoryFileSystem base;
  const std::string script = "#!/usr/bin/env tclsh\nputs {v {xschem version=3.4.8RC file_version=1.2}}\n";
  REQUIRE(base.write_atomic("/lib/grow", script).ok());
  REQUIRE(base.write_atomic("/lib/grow.tcl", script).ok());
  WatchedFileSystem fs(base);
  fs.sizes = {{"/lib/grow", 10}, {"/lib/grow.tcl", 10}};  // as when the size was checked
  const std::size_t limit = script.size() - 1;
  CHECK_FALSE(schem::is_tcl_generator("/lib/grow", fs, limit));
  schem::GeneratorInput input{"/lib/grow.tcl", {}, "/w"};
  input.max_script = limit;
  const auto output = schem::run_tcl_generator(fs, input);
  CHECK_FALSE(output.text.has_value());
  REQUIRE(output.diagnostics.size() == 1);
  CHECK(output.diagnostics[0].text.ends_with("failed: the script is larger than " + std::to_string(limit) + " bytes"));
  // Within the limit it runs as before.
  input.max_script = script.size();
  CHECK(schem::run_tcl_generator(fs, input).text.has_value());
  CHECK(schem::is_tcl_generator("/lib/grow", fs, script.size()));
}

// M8-D7 (approved 2026-11-23; Q-row assigned by DECIDE), both modes: a
// generator named by an absolute `.tcl` path outside the search path stays
// allowed and runs in the sandbox: it reads its own folder and the document
// folder, and a write (even into those folders) is denied with the sandbox
// diagnostic, which is reported.
TEST_CASE("M8-D7: an absolute-path generator outside the search path runs in the sandbox", kGenerators) {
  namespace stdfs = std::filesystem;
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    const stdfs::path root = stdfs::canonical(stdfs::temp_directory_path()) /
                             ("hefesto-generator-d7-" + std::to_string(::getpid()) + (compat ? "-on" : "-off"));
    stdfs::remove_all(root);
    stdfs::create_directories(root / "gens");
    stdfs::create_directories(root / "doc");
    std::ofstream(root / "doc" / "width.txt") << "4";
    std::ofstream(root / "gens" / "abs.tcl") << "set here [file dirname $argv0]\n"
                                                "set f [open $env(HOME)/width.txt]; set n [read $f]; close $f\n"
                                                "if {[catch {open $here/written w}]} { set a 1 }\n"
                                                "if {[catch {open $env(HOME)/written w}]} { set b 1 }\n"
                                                "puts {v {xschem version=3.4.8RC file_version=1.2}}\n"
                                                "puts \"T {n=$n} 0 0 0 0 0.2 0.2 {}\"\n";
    platform::LocalFileSystem local;
    diag::CollectingSink sink;
    schem::SymbolCache cache;
    schem::Document top;
    top.path = root / "doc" / "top.sch";
    const schem::SearchPath path{{"/no/such/library"}};
    const std::string reference = (root / "gens" / "abs.tcl").generic_string() + "(1)";
    const schem::Symbol& symbol = cache.resolve({reference}, top, {path, local, &sink, compat});
    REQUIRE_FALSE(symbol.placeholder);
    REQUIRE(symbol.document.texts.size() == 1);
    CHECK(symbol.document.texts[0].text == "n=4");
    const auto denied = with_code(sink.take(), "SCRIPT-SANDBOX-004");
    REQUIRE(denied.size() == 2);
    CHECK(denied[0].text.find("caught by the script's catch") != std::string::npos);
    CHECK_FALSE(stdfs::exists(root / "gens" / "written"));
    CHECK_FALSE(stdfs::exists(root / "doc" / "written"));
    stdfs::remove_all(root);
  }
}

// Q-SCHEM-54 (approved 2026-11-27, both modes): no shell processing of the
// argument text; argv is the text split at `(`, `)`, `,`, spaces and tabs,
// each word as written (backslashes kept), `@` tokens not substituted.
TEST_CASE("Generator arguments are not substituted", kGenerators) {
  constexpr std::string_view kTier = R"tcl(
puts {v {xschem version=3.4.8RC file_version=1.2}}
puts "K {type=label}"
puts "T {argc=$argc first=[string map {\\ BS} [lindex $argv 0]] len=[string length [lindex $argv 0]]} 0 0 0 0 0.2 0.2 {}"
)tcl";
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Library library;
    REQUIRE(library.fs.write_atomic("/lib/tier.tcl", kTier).ok());
    library.top.instances.push_back(schem::InstanceRecord{});
    library.top.instances.back().symbol = "tier.tcl(@lab\\)";
    library.top.instances.back().props = "name=l1 lab=A";
    diag::CollectingSink sink;
    schem::SymbolCache cache;
    const schem::Symbol& symbol =
        cache.resolve({"tier.tcl(@lab\\)"}, library.top, {library.path, library.fs, &sink, compat});
    REQUIRE_FALSE(symbol.placeholder);
    REQUIRE(symbol.document.texts.size() == 1);
    CHECK(symbol.document.texts[0].text == "argc=1 first=@labBS len=5");
  }
  CHECK(schem::parse_generator("tier.tcl(@lab\\)") == schem::GeneratorCall{"tier.tcl", {"@lab\\"}});
  CHECK(schem::parse_generator("res.tcl(1\t2,\t3)") == schem::GeneratorCall{"res.tcl", {"1", "2", "3"}});
  CHECK(schem::parse_generator("res.tcl('a b')") == schem::GeneratorCall{"res.tcl", {"'a", "b'"}});
}
