// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol references, the library search path, missing symbols and the
// symbol cache (capability schematic-symbols-and-libraries "Symbol
// references", "Library search path", "Missing symbols", "Symbol cache and
// reload"; Q-SCHEM-9, Q-SCHEM-30). The search path of golden case
// SCHEM-PR-05 is rebuilt in memory.
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;

namespace {

constexpr const char* kReferences = "[cap:schematic-symbols-and-libraries][req:Symbol references]";
constexpr const char* kSearchPath = "[cap:schematic-symbols-and-libraries][req:Library search path]";
constexpr const char* kMissing = "[cap:schematic-symbols-and-libraries][req:Missing symbols]";
constexpr const char* kCache = "[cap:schematic-symbols-and-libraries][req:Symbol cache and reload]";

const std::map<std::string, std::string> kNoEnv;

void touch(platform::MemoryFileSystem& fs, const std::string& path, std::string_view text = "v {}\n") {
  REQUIRE(fs.write_atomic(path, text).ok());
}

std::vector<diag::Diagnostic> with_code(const std::vector<diag::Diagnostic>& all, std::string_view code) {
  std::vector<diag::Diagnostic> out;
  std::copy_if(all.begin(), all.end(), std::back_inserter(out), [code](const auto& d) { return d.code == code; });
  return out;
}

// SCHEM-PR-05: /work/input with libA, libB (alias lib), sub/top.sch and the
// xschem system library at /opt/ref/share/xschem/xschem_library.
struct Pr05 {
  platform::MemoryFileSystem fs;
  schem::SearchPath path;

  Pr05() {
    touch(fs, "/work/input/x.sym");
    touch(fs, "/work/input/libA/a.sym");
    touch(fs, "/work/input/libB/x.sym");
    touch(fs, "/work/input/sub/x.sym");
    touch(fs, "/work/input/sub/top.sch");
    touch(fs, "/opt/ref/share/xschem/xschem_library/devices/res.sym");
    path = schem::SearchPath::parse(
        "/work/input/libA:/work/input/missing:/work/input/libB | lib:.:/opt/ref/share/xschem/xschem_library", kNoEnv,
        fs);
  }

  [[nodiscard]] schem::ResolvedReference resolve(std::string_view reference) {
    return schem::resolve_reference(reference, "/work/input/sub", path, fs);
  }
};

}  // namespace

TEST_CASE("references are normalized", kReferences) {
  CHECK(schem::normalize_reference("a//b") == "a/b");
  CHECK(schem::normalize_reference("a/./b") == "a/b");
  CHECK(schem::normalize_reference("a/x/../b") == "a/b");
  CHECK(schem::normalize_reference("a/b/") == "a/b");
  CHECK(schem::normalize_reference("a/b/.") == "a/b");
  CHECK(schem::normalize_reference("./x.sym") == "./x.sym");
  CHECK(schem::normalize_reference("../x.sym") == "../x.sym");
  CHECK(schem::normalize_reference("../../a/../x.sym") == "../../x.sym");
  CHECK(schem::normalize_reference(".//./x.sym") == "./x.sym");
}

TEST_CASE("Library-relative reference", kReferences) {
  platform::MemoryFileSystem fs;
  touch(fs, "/pdk/xschem/sg13g2_pr/sg13_lv_nmos.sym");
  const auto path = schem::SearchPath::parse("/pdk/xschem:/pdk/xschem/sg13g2_pr", kNoEnv, fs);
  const auto resolved = schem::resolve_reference("sg13g2_pr/sg13_lv_nmos.sym", "/work", path, fs);
  CHECK(resolved.via == schem::ResolvedVia::search_path);
  CHECK(resolved.file == "/pdk/xschem/sg13g2_pr/sg13_lv_nmos.sym");
}

TEST_CASE("the SCHEM-PR-05 references resolve by the reference steps", kReferences) {
  Pr05 pr;
  REQUIRE(pr.path.directories() == std::vector<platform::Path>{"/work/input/libA", "/work/input/libB", ".",
                                                               "/opt/ref/share/xschem/xschem_library"});
  REQUIRE(pr.path.aliases() == std::vector<schem::LibraryAlias>{{"lib", "/work/input/libB"}});

  CHECK(pr.resolve("lib/x.sym").file == "/work/input/libB/x.sym");
  CHECK(pr.resolve("lib/x.sym").via == schem::ResolvedVia::alias);
  CHECK(pr.resolve("devices/res.sym").file == "/opt/ref/share/xschem/xschem_library/devices/res.sym");
  CHECK(pr.resolve("a.sym").file == "/work/input/libA/a.sym");
  CHECK(pr.resolve("x.sym").file == "/work/input/libB/x.sym");
  CHECK(pr.resolve("lib/missing.sym").file == "/work/input/libB/missing.sym");
  CHECK(pr.resolve("//lib//./x.sym").file == "//lib//./x.sym");
  CHECK(pr.resolve("//lib//./x.sym").via == schem::ResolvedVia::absolute);
  CHECK(pr.resolve("").via == schem::ResolvedVia::none);
  CHECK(pr.resolve("nowhere.sym").via == schem::ResolvedVia::fallback);
  CHECK(pr.resolve("nowhere.sym").file == "/work/input/sub/nowhere.sym");

  // The shortest forms Hefesto writes for files it chose itself.
  CHECK(schem::shortest_reference("/work/input/libB/x.sym", "/work/input/sub", pr.path) == "lib/x.sym");
  CHECK(schem::shortest_reference("/work/input/libA/a.sym", "/work/input/sub", pr.path) == "a.sym");
  CHECK(schem::shortest_reference("/opt/ref/share/xschem/xschem_library/devices/res.sym", "/work/input/sub",
                                  pr.path) == "devices/res.sym");
  CHECK(schem::shortest_reference("/work/input/sub/x.sym", "/work/input/sub", pr.path) == "x.sym");
  CHECK(schem::shortest_reference("/elsewhere/y.sym", "/work/input/sub", pr.path) == "/elsewhere/y.sym");
}

TEST_CASE("shortest reference: bare name in the document folder, else absolute", kReferences) {
  const schem::SearchPath path{{"/lib"}};
  CHECK(schem::shortest_reference("/lib/sub/a.sym", "/doc", path) == "sub/a.sym");
  CHECK(schem::shortest_reference("/doc/b.sym", "/doc", path) == "b.sym");
  CHECK(schem::shortest_reference("/library/c.sym", "/doc", path) == "/library/c.sym");
}

TEST_CASE("Relative reference resolved against the schematic folder", kReferences) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Pr05 pr;
    CHECK(pr.resolve("../x.sym").file == "/work/input/x.sym");
    CHECK(pr.resolve("../x.sym").via == schem::ResolvedVia::relative);
    CHECK(pr.resolve("./x.sym").file == "/work/input/sub/x.sym");
    CHECK(pr.resolve("./x.sym").folder == "/work/input/sub");

    schem::Document top;
    top.path = "/work/input/sub/top.sch";
    top.instances.push_back({"../x.sym", {0, 0}, 0, 0, "name=x1"});
    diag::CollectingSink sink;
    schem::SymbolCache cache;
    const schem::ResolveContext context{pr.path, pr.fs, &sink, compat};
    for (int round = 0; round < 2; ++round) {
      (void)cache.resolve({"../x.sym"}, top, context);
      (void)cache.resolve({"./x.sym"}, top, context);
    }
    const auto all = sink.take();
    const auto relative = with_code(all, "SCHEM-LIB-001");
    REQUIRE(relative.size() == 2);
    CHECK(relative[0].text ==
          "Relative symbol reference ../x.sym resolved against the schematic folder /work/input/sub; xschem resolves "
          "it against its start folder");
    CHECK(relative[1].text.find("./x.sym") != std::string::npos);
    CHECK(with_code(all, "SCHEM-LIB-003").empty());  // not a missing symbol
    CHECK(top.instances[0].symbol == "../x.sym");
  }
}

TEST_CASE("Dot element is the schematic folder", kSearchPath) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    platform::MemoryFileSystem fs;
    touch(fs, "/a/other.sym");
    touch(fs, "/w/s/y.sym");
    touch(fs, "/w/s/top.sch");
    const auto path = schem::SearchPath::parse("/a:.", kNoEnv, fs);
    const auto resolved = schem::resolve_reference("y.sym", "/w/s", path, fs);
    CHECK(resolved.file == "/w/s/y.sym");
    CHECK(resolved.via == schem::ResolvedVia::document_folder);

    schem::Document top;
    top.path = "/w/s/top.sch";
    diag::CollectingSink sink;
    schem::SymbolCache cache;
    (void)cache.resolve({"y.sym"}, top, schem::ResolveContext{path, fs, &sink, compat});
    const auto relative = with_code(sink.take(), "SCHEM-LIB-001");
    REQUIRE(relative.size() == 1);
    CHECK(relative[0].text.find("y.sym") != std::string::npos);
    CHECK(relative[0].text.find("/w/s;") != std::string::npos);
  }
}

TEST_CASE("Missing folder dropped", kSearchPath) {
  platform::MemoryFileSystem fs;
  touch(fs, "/a/x.sym");
  touch(fs, "/b/y.sym");
  const auto path = schem::SearchPath::parse(":/a:/missing:/b", kNoEnv, fs);
  CHECK(path.directories() == std::vector<platform::Path>{"/a", "/b"});
}

TEST_CASE("search path elements: comments, aliases, ~ and environment references", kSearchPath) {
  platform::MemoryFileSystem fs;
  touch(fs, "/home/u/lib/x.sym");
  touch(fs, "/pdk/sky130A/libs.tech/xschem/a.sym");
  touch(fs, "/odd|dir/z.sym");
  const std::map<std::string, std::string> env{{"HOME", "/home/u"}, {"PDK_ROOT", "/pdk"}, {"PDK", "sky130A"}};
  const auto path = schem::SearchPath::parse(
      "#/commented: ~/lib :${PDK_ROOT}/$PDK/libs.tech/xschem|sky : /odd\\|dir : $UNDEFINED/x", env, fs);
  CHECK(path.directories() ==
        std::vector<platform::Path>{"/home/u/lib", "/pdk/sky130A/libs.tech/xschem", "/odd|dir"});
  CHECK(path.aliases() == std::vector<schem::LibraryAlias>{{"sky", "/pdk/sky130A/libs.tech/xschem"}});
}

TEST_CASE("an empty search path is the schematic folder", kSearchPath) {
  platform::MemoryFileSystem fs;
  CHECK(schem::SearchPath::parse("", kNoEnv, fs).directories() == std::vector<platform::Path>{"."});
  CHECK(schem::SearchPath::parse(":/missing:#x", kNoEnv, fs).directories() == std::vector<platform::Path>{"."});
}

TEST_CASE("the settings search path is prepended", kSearchPath) {
  const schem::SearchPath rc{{"/pdk/a"}, {{"p", "/pdk/a"}}};
  const schem::SearchPath settings{{"/mine"}};
  const auto both = rc.prepended(settings);
  CHECK(both.directories() == std::vector<platform::Path>{"/mine", "/pdk/a"});
  CHECK(both.aliases() == rc.aliases());
}

TEST_CASE("Web reference not fetched", kReferences) {
  platform::MemoryFileSystem fs;
  const schem::SearchPath path;
  schem::Document top;
  top.path = "/w/top.sch";
  top.instances.push_back({"https://example.com/lib/amp.sym", {0, 0}, 0, 0, "name=x1"});
  diag::CollectingSink sink;
  schem::SymbolCache cache;
  const schem::ResolveContext context{path, fs, &sink};
  const schem::Symbol& symbol = cache.resolve({top.instances[0].symbol}, top, context);
  CHECK(symbol.placeholder);
  CHECK(symbol.pins.empty());
  (void)cache.resolve({"https://example.com/lib/amp.sym"}, top, context);
  CHECK(cache.resolve({"http://example.com/b.sym"}, top, context).placeholder);
  const auto warnings = sink.take();
  REQUIRE(warnings.size() == 2);
  CHECK(warnings[0].code == "SCHEM-LIB-002");
  CHECK(warnings[0].text.find("https://example.com/lib/amp.sym") != std::string::npos);
  CHECK(warnings[1].text.find("http://example.com/b.sym") != std::string::npos);
  CHECK(top.instances[0].symbol == "https://example.com/lib/amp.sym");  // written back unchanged
  CHECK(schem::resolve_reference("https://example.com/lib/amp.sym", "/w", path, fs).file.empty());
}

TEST_CASE("Missing PDK symbol", kMissing) {
  platform::MemoryFileSystem fs;
  schem::SearchPath path = schem::SearchPath::parse("", kNoEnv, fs);
  schem::Document top;
  top.path = "/w/top.sch";
  top.instances.push_back({"sky130_fd_pr/nfet_01v8.sym", {0, 0}, 0, 0, "name=M1"});
  diag::CollectingSink sink;
  schem::SymbolCache cache;
  for (int i = 0; i < 2; ++i) {
    const schem::Symbol& symbol = cache.resolve({"sky130_fd_pr/nfet_01v8.sym"}, top, {path, fs, &sink});
    CHECK(symbol.placeholder);
    CHECK(symbol.pins.empty());
  }
  const auto warnings = sink.take();
  REQUIRE(warnings.size() == 1);
  CHECK(warnings[0].code == "SCHEM-LIB-003");
  CHECK(warnings[0].text == "Symbol not found: sky130_fd_pr/nfet_01v8.sym");
  CHECK(cache.size() == 0);  // misses are not cached
  CHECK(top.instances[0].symbol == "sky130_fd_pr/nfet_01v8.sym");

  // A later change of the search path re-resolves the reference.
  touch(fs, "/pdk/xschem/sky130_fd_pr/nfet_01v8.sym", "v {xschem version=3.4.8RC file_version=1.3}\nK {type=nmos}\n");
  path = schem::SearchPath::parse("/pdk/xschem", kNoEnv, fs);
  const auto resolved = schem::resolve_reference("sky130_fd_pr/nfet_01v8.sym", "/w", path, fs);
  CHECK(resolved.file == "/pdk/xschem/sky130_fd_pr/nfet_01v8.sym");
  const schem::Symbol& found = cache.resolve({"sky130_fd_pr/nfet_01v8.sym"}, top, {path, fs, &sink});
  CHECK(with_code(sink.take(), "SCHEM-LIB-003").empty());
  CHECK_FALSE(found.placeholder);
  CHECK(found.file == "/pdk/xschem/sky130_fd_pr/nfet_01v8.sym");
}

TEST_CASE("an unreadable symbol file gives the placeholder once with SCHEM-LIB-004", kMissing) {
  platform::MemoryFileSystem fs;
  // FORMAT's reader never fails ("Malformed input"), so the file is made
  // unreadable instead: /lib/broken.sym exists as a directory.
  touch(fs, "/lib/broken.sym/inside", "");
  const schem::SearchPath path{{"/lib"}};
  schem::Document top;
  diag::CollectingSink sink;
  schem::SymbolCache cache;
  CHECK(cache.resolve({"broken.sym"}, top, {path, fs, &sink}).placeholder);
  CHECK(cache.resolve({"broken.sym"}, top, {path, fs, &sink}).placeholder);
  const auto read = with_code(sink.take(), "SCHEM-LIB-004");
  REQUIRE(read.size() == 1);
  CHECK(read[0].text.find("/lib/broken.sym") != std::string::npos);
}

TEST_CASE("embedded symbols are used before files", kCache) {
  platform::MemoryFileSystem fs;
  const schem::SearchPath path;
  schem::Document top;
  top.path = "/w/top.sch";
  schem::Document inline_symbol;
  inline_symbol.kind = schem::FileKind::symbol;
  inline_symbol.boxes.push_back({5, {-2.5, -2.5}, {2.5, 2.5}, "name=A"});
  top.embedded.push_back({"res.sym", inline_symbol});
  schem::SymbolCache cache;
  const schem::Symbol& first = cache.resolve({"res.sym"}, top, {path, fs});
  CHECK(first.embedded);
  CHECK_FALSE(first.placeholder);
  CHECK(first.pins.size() == 1);
  CHECK(&cache.resolve({"res.sym"}, top, {path, fs}) == &first);
  cache.clear();
  CHECK(cache.resolve({"res.sym"}, top, {path, fs}).embedded);
}
