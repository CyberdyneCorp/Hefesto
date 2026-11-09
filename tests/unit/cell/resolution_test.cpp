// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cell-library-resolution: duplicate-cell conflicts on read, rebasing,
// standard-cell library and technology discovery, and the automatic
// technology discovery bound to `load` (in-memory files).

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "cell_fixture.hpp"
#include "command_session.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/paths.hpp"
#include "hefesto/cell/search_path.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
using cell::test::CellIo;
using cell::test::CommandSession;

namespace {

constexpr std::string_view kInvA = "magic\ntech tiny\ntimestamp 3\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n";
constexpr std::string_view kInvB = "magic\ntech tiny\ntimestamp 4\n<< metal2 >>\nrect 0 0 6 6\n<< end >>\n";

std::string parent_using(std::string_view path) {
  return "magic\ntech tiny\ntimestamp 9\nuse inv inv_0 " + std::string{path} +
         "\ntimestamp 3\ntransform 1 0 0 0 1 0\nbox 0 0 4 4\n<< end >>\n";
}

hefesto::platform::MemoryFileSystem& memory(CellIo& io) {
  return dynamic_cast<hefesto::platform::MemoryFileSystem&>(io.ctx.fs);
}

/// `inv` loaded from /a/inv.mag.
db::CellDef& load_inv_from_a(CellIo& io) {
  REQUIRE(memory(io).write_atomic("/a/inv.mag", std::string{kInvA}).ok());
  io.paths.path = {"/a"};
  auto inv = cell::load_cell(io.ctx, "inv");
  REQUIRE(inv.ok());
  REQUIRE(inv.value()->file_path() == "/a/inv");
  return *inv.value();
}

bool sink_has(const CellIo& io, std::string_view text) {
  for (const auto& d : io.sink.snapshot()) {
    if (d.text == text) {
      return true;
    }
  }
  return false;
}

/// A memory host whose git HEAD is fixed per directory and whose
/// `symlinks` paths are symbolic links.
class GitHost final : public cell::FileHost {
 public:
  GitHost(hefesto::platform::FileSystem& fs, std::map<std::string, std::string> heads)
      : inner_(cell::make_memory_file_host(fs, "/work")), heads_(std::move(heads)) {}
  bool is_symlink(const hefesto::platform::Path& p) override {
    return symlinks.contains(p.generic_string()) || inner_->is_symlink(p);
  }
  bool is_directory(const hefesto::platform::Path& p) override { return inner_->is_directory(p); }
  std::optional<bool> same_file(const hefesto::platform::Path& a, const hefesto::platform::Path& b) override {
    return inner_->same_file(a, b);
  }
  hefesto::platform::Path current_directory() override { return inner_->current_directory(); }
  bool writable(const hefesto::platform::Path& p) override { return inner_->writable(p); }
  std::optional<std::uint32_t> permissions(const hefesto::platform::Path& p) override { return inner_->permissions(p); }
  bool set_permissions(const hefesto::platform::Path& p, std::uint32_t m) override {
    return inner_->set_permissions(p, m);
  }
  std::optional<std::string> git_head(const hefesto::platform::Path& dir) override {
    const auto it = heads_.find(dir.generic_string());
    return it == heads_.end() ? std::nullopt : std::optional{it->second};
  }

  std::set<std::string> symlinks;

 private:
  std::unique_ptr<cell::FileHost> inner_;
  std::map<std::string, std::string> heads_;
};

std::string read_tiny_tech() {
  std::ifstream in{std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech", std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace

// --- duplicate-cell conflicts -----------------------------------------------------

TEST_CASE("a true duplicate renames the original definition",
          "[cap:cell-library-resolution][req:Duplicate cell conflicts on read]") {
  CellIo io;
  std::set<const db::CellDef*> renamed;
  io.ctx.renamed_duplicates = &renamed;
  db::CellDef& original = load_inv_from_a(io);
  REQUIRE(memory(io).write_atomic("/b/inv.mag", std::string{kInvB}).ok());
  db::CellDef& top = io.read("top", parent_using("/b"));
  CHECK(original.name() == "inv__0");
  CHECK(original.file_path() == "/a/inv");
  db::CellDef* fresh = io.library.find("inv");
  REQUIRE(fresh != nullptr);
  CHECK(fresh != &original);
  CHECK(fresh->file_path() == "/b/inv.mag");
  REQUIRE(top.uses().size() == 1);
  CHECK(top.uses().front()->def == fresh);
  CHECK(renamed.contains(&original));
  CHECK(sink_has(io, "Duplicate cell in top:  Instance of cell inv is from path /b but cell was previously read "
                     "from /a."));
  CHECK(sink_has(io, "Cell name conflict:  Renaming original cell to inv__0."));
}

TEST_CASE("the rename picks the smallest unused suffix", "[cap:cell-library-resolution][req:Duplicate cell conflicts on read]") {
  CellIo io;
  REQUIRE(io.library.create("inv__0", 4).ok());
  db::CellDef& original = load_inv_from_a(io);
  REQUIRE(memory(io).write_atomic("/b/inv.mag", std::string{kInvB}).ok());
  (void)io.read("top", parent_using("/b"));
  CHECK(original.name() == "inv__1");
}

TEST_CASE("a stale use path keeps the existing definition",
          "[cap:cell-library-resolution][req:Duplicate cell conflicts on read]") {
  CellIo io;
  db::CellDef& original = load_inv_from_a(io);
  REQUIRE(memory(io).write_atomic("/b/other.mag", std::string{kInvB}).ok());  // /b exists, no inv.mag
  db::CellDef& top = io.read("top", parent_using("/b"));
  CHECK(original.name() == "inv");
  REQUIRE(top.uses().size() == 1);
  CHECK(top.uses().front()->def == &original);
  CHECK(sink_has(io, "Bad cell path in top:  Instance of cell inv specifies path /b, which does not contain the "
                     "cell.  Cell previously read from /a will be used instead."));
}

TEST_CASE("a use path to a missing directory says it does not exist",
          "[cap:cell-library-resolution][req:Duplicate cell conflicts on read]") {
  CellIo io;
  (void)load_inv_from_a(io);
  (void)io.read("top", parent_using("/nowhere"));
  CHECK(sink_has(io, "Bad cell path in top:  Instance of cell inv specifies path /nowhere, which does not exist.  "
                     "Cell previously read from /a will be used instead."));
}

TEST_CASE("the same directory through another spelling is no conflict",
          "[cap:cell-library-resolution][req:Duplicate cell conflicts on read]") {
  CellIo io;
  db::CellDef& original = load_inv_from_a(io);
  REQUIRE(memory(io).write_atomic("/mid/x.mag", std::string{kInvB}).ok());
  db::CellDef& top = io.read("top", parent_using("/mid/../a"));
  CHECK(original.name() == "inv");
  CHECK(top.uses().front()->def == &original);
  CHECK_FALSE(io.has_code(cell::codes::kUsePath));
}

TEST_CASE("matching git HEAD commits make two directories equivalent",
          "[cap:cell-library-resolution][req:Duplicate cell conflicts on read][req:Host mechanisms for resolution]") {
  CellIo io;
  GitHost host{io.ctx.fs, {{"/a", "c0ffee"}, {"/b", "c0ffee"}, {"/c", "decade"}}};
  cell::CellIoContext ctx{io.ctx.fs, io.clock, io.library, io.ctx.tech, io.grid,    io.paths, io.vars, io.sink,
                          host,      io.stamps, nullptr,  nullptr,      true,  true, nullptr,  {},      {}};
  REQUIRE(memory(io).write_atomic("/a/inv.mag", std::string{kInvA}).ok());
  REQUIRE(memory(io).write_atomic("/b/inv.mag", std::string{kInvB}).ok());
  REQUIRE(memory(io).write_atomic("/c/inv.mag", std::string{kInvB}).ok());
  io.paths.path = {"/a"};
  auto inv = cell::load_cell(ctx, "inv");
  REQUIRE(inv.ok());
  auto same = io.library.create("same", 4);
  REQUIRE(same.ok());
  REQUIRE(cell::read_mag(ctx, *same.value(), parent_using("/b")).ok());
  CHECK(inv.value()->name() == "inv");
  CHECK(sink_has(io, "Cells inv in /a and /b have matching git repository commits and can be considered equivalent."));
  auto other = io.library.create("other", 4);
  REQUIRE(other.ok());
  REQUIRE(cell::read_mag(ctx, *other.value(), parent_using("/c")).ok());
  CHECK(inv.value()->name() == "inv__0");  // different commits: a true duplicate
}

TEST_CASE("a parent being dereferenced skips the conflict checks",
          "[cap:cell-library-resolution][req:Dereferencing][req:Duplicate cell conflicts on read]") {
  CellIo io;
  db::CellDef& original = load_inv_from_a(io);
  REQUIRE(memory(io).write_atomic("/b/inv.mag", std::string{kInvB}).ok());
  auto created = io.library.create("top", 4);
  REQUIRE(created.ok());
  cell::ReadOptions options;
  options.dereference = true;
  REQUIRE(cell::read_mag(io.ctx, *created.value(), parent_using("/b"), options).ok());
  CHECK(original.name() == "inv");
  CHECK_FALSE(io.has_code(cell::codes::kUsePath));
}

// --- rebasing -----------------------------------------------------------------------

TEST_CASE("rebasing rewrites relative paths for a new working directory",
          "[cap:cell-library-resolution][req:Rebasing paths on working-directory change]") {
  CHECK(cell::rebase_path("lib/inv", "/home/u/a", "/home/u/b") == "../a/lib/inv");
  CHECK(cell::rebase_path("../lib/inv", "/home/u/a", "/home/u/b") == "../lib/inv");
  CHECK(cell::rebase_path("lib/inv", "/home/u", "/home/u/a/deep") == "../../lib/inv");
  CHECK(cell::rebase_path("inv", "/x", "/x") == "inv");
}

TEST_CASE("rebasing leaves rooted and over-climbing paths alone",
          "[cap:cell-library-resolution][req:Rebasing paths on working-directory change]") {
  CHECK(cell::rebase_path("/abs/inv", "/home/u/a", "/home/u/b") == "/abs/inv");
  CHECK(cell::rebase_path("~/inv", "/home/u/a", "/home/u/b") == "~/inv");
  CHECK(cell::rebase_path("$PDK/inv", "/home/u/a", "/home/u/b") == "$PDK/inv");
  CHECK(cell::rebase_path("../../../../inv", "/home/u", "/tmp") == "../../../../inv");
}

// --- path normalization and substitution ------------------------------------------

#define NORMALIZE_TAGS "[cap:cell-library-resolution][req:Path normalization algorithms]"

TEST_CASE("collapse keeps '..' after a symbolic link", NORMALIZE_TAGS) {
  hefesto::platform::MemoryFileSystem fs;
  GitHost host{fs, {}};
  host.symlinks.insert("/a/link");
  CHECK(cell::collapse_path("/a/link/../b", host) == "/a/link/../b");
  CHECK(cell::collapse_path("/a/dir/../b", host) == "/a/b");
  CHECK(cell::collapse_path("/../x", host) == "/x");          // '..' after the root is dropped
  CHECK(cell::collapse_path("a/./../..//b", host) == "../b");  // empty and '.' components dropped
  CHECK(cell::collapse_path("a/..", host) == ".");             // an empty result is '.'
}

TEST_CASE("absComps, relDir, absDir and isDirPrefix", NORMALIZE_TAGS) {
  hefesto::platform::MemoryFileSystem fs;
  GitHost host{fs, {}};  // working directory /work
  CHECK(cell::relative_dir("/h/u/proj", "/h/u/other", host) == std::optional<std::string>{"../other"});
  CHECK(cell::relative_dir("/h/u", "/h/u", host) == std::optional<std::string>{""});
  CHECK(cell::relative_dir("lib", "/work/x", host) == std::optional<std::string>{"../x"});
  CHECK_FALSE(cell::relative_dir("~/a", "/h/u", host).has_value());  // absComps fails for ~ and $
  CHECK_FALSE(cell::relative_dir("/a/b", "/c/d", host).has_value());  // no common component
  host.symlinks.insert("/h/u/proj");
  CHECK_FALSE(cell::relative_dir("/h/u/proj", "/h/u/other", host).has_value());  // link below the common part

  const auto comps = cell::absolute_comps("../lib", host);
  REQUIRE(comps.has_value());
  CHECK(cell::join_path(*comps) == "/lib");  // a leading '..' pops the working directory
  CHECK_FALSE(cell::absolute_comps("a/../b", host).has_value());
  CHECK_FALSE(cell::absolute_comps("$PDK/x", host).has_value());

  CHECK(cell::absolute_dir("/abs/./d", host) == std::optional<std::string>{"/abs/./d"});  // unchanged
  CHECK(cell::absolute_dir("lib/../mag", host) == std::optional<std::string>{"/work/mag"});
  CHECK(cell::absolute_dir("", host) == std::optional<std::string>{"/work"});
  CHECK_FALSE(cell::absolute_dir("~/x", host).has_value());

  CHECK(cell::is_dir_prefix("/a/b", "/a/b/c"));
  CHECK_FALSE(cell::is_dir_prefix("/a/b", "a/b/c"));  // absolute vs relative
  CHECK(cell::is_dir_prefix("", "x/y"));
  CHECK_FALSE(cell::is_dir_prefix("/a/bc", "/a/b"));
}

#define SUBST_TAGS "[cap:cell-library-resolution][req:Path variable substitution on write]"

namespace {

cell::PathVariables vars_of(std::map<std::string, std::string> values, std::string home) {
  cell::PathVariables vars;
  vars.variable = [table = std::move(values)](std::string_view name) -> std::optional<std::string> {
    const auto it = table.find(std::string{name});
    return it == table.end() ? std::nullopt : std::optional{it->second};
  };
  vars.home_of = [home_dir = std::move(home)](std::string_view user) -> std::optional<std::string> {
    return user.empty() ? std::optional{home_dir} : std::nullopt;
  };
  return vars;
}

}  // namespace

TEST_CASE("PDK variables are substituted first, in order", SUBST_TAGS) {
  const auto vars = vars_of({{"PDK_ROOT", "/usr/share/pdk"}}, "/home/u");
  CHECK(cell::substitute_path("/usr/share/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag", "", vars, true) ==
        "$PDK_ROOT/sky130A/libs.ref/sky130_fd_sc_hd/mag");
  CHECK(cell::substitute_path("/usr/share/pdk", "", vars, true) == "$PDK_ROOT");
  // Q-CELL-20: a path prefix, not a character prefix.
  CHECK(cell::substitute_path("/usr/share/pdk2/x", "", vars, true) == "/usr/share/pdk2/x");
  const auto both = vars_of({{"PDKPATH", "/p/sky130A"}, {"PDK_ROOT", "/p"}}, "/home/u");
  CHECK(cell::substitute_path("/p/sky130A/libs.ref", "", both, true) == "$PDKPATH/libs.ref");
  CHECK(cell::substitute_path("/p/gf180/libs.ref", "", both, true) == "$PDK_ROOT/gf180/libs.ref");
}

TEST_CASE("the home rule tests the child's directory in compat mode, else the string", SUBST_TAGS) {
  const auto vars = vars_of({}, "/home/u");
  // Magic tests the child's recorded directory (Q-CELL-21).
  CHECK(cell::substitute_path("/home/u/lib", "/home/u/lib", vars, true) == "~/lib");
  CHECK(cell::substitute_path("/home/u/lib", "/elsewhere/lib", vars, true) == "/home/u/lib");
  CHECK(cell::substitute_path("/home/u/lib", "/elsewhere/lib", vars, false) == "~/lib");
  CHECK(cell::substitute_path("/home/user2/lib", "/home/user2/lib", vars, true) == "/home/user2/lib");
  CHECK(cell::substitute_path("rel/lib", "/home/u/rel/lib", vars, true) == "rel/lib");  // not absolute
}

// --- discovery ----------------------------------------------------------------------

TEST_CASE("library discovery appends the directories holding .mag files",
          "[cap:cell-library-resolution][req:Standard-cell library path discovery]") {
  hefesto::platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/opt/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag/inv.mag", "x").ok());
  REQUIRE(fs.write_atomic("/opt/pdk/sky130A/libs.ref/sky130_fd_sc_hd/lef/inv.lef", "x").ok());
  REQUIRE(fs.write_atomic("/opt/pdk/sky130A/libs.ref/sky130_fd_io/mag/pad.mag", "x").ok());
  const auto host = cell::make_memory_file_host(fs);
  CHECK(cell::discover_library_paths(fs, *host, "/opt/pdk/sky130A/libs.ref") ==
        std::vector<std::string>{"/opt/pdk/sky130A/libs.ref/sky130_fd_io/mag",
                                 "/opt/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag"});
}

TEST_CASE("library discovery stops below depth 10", "[cap:cell-library-resolution][req:Standard-cell library path discovery]") {
  hefesto::platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/r/1/2/3/4/5/6/7/8/9/10/ok.mag", "x").ok());
  REQUIRE(fs.write_atomic("/r/1/2/3/4/5/6/7/8/9/10/11/deep.mag", "x").ok());
  const auto host = cell::make_memory_file_host(fs);
  CHECK(cell::discover_library_paths(fs, *host, "/r") == std::vector<std::string>{"/r/1/2/3/4/5/6/7/8/9/10"});
}

TEST_CASE("technology discovery visits the entry named like the technology first",
          "[cap:cell-library-resolution][req:Automatic technology discovery]") {
  hefesto::platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/opt/pdk/aaa/magic/sky130A.tech", "x").ok());
  REQUIRE(fs.write_atomic("/opt/pdk/sky130A/libs.tech/magic/sky130A.tech", "x").ok());
  const auto host = cell::make_memory_file_host(fs);
  CHECK(cell::find_technology_dir(fs, *host, "/opt/pdk", "sky130A") ==
        hefesto::platform::Path{"/opt/pdk/sky130A/libs.tech/magic"});
  CHECK_FALSE(cell::find_technology_dir(fs, *host, "/opt/pdk", "gf180").has_value());
  CHECK(cell::libs_ref_root("/opt/pdk/sky130A/libs.tech/magic") == "/opt/pdk/sky130A/libs.ref");
  CHECK_FALSE(cell::libs_ref_root("/usr/share/tech").has_value());
}

TEST_CASE("technology search roots come from the PDK variables, then the fixed directory",
          "[cap:cell-library-resolution][req:Automatic technology discovery]") {
  cell::PathVariables vars;
  vars.variable = [](std::string_view name) -> std::optional<std::string> {
    if (name == "PDKPATH") {
      return "/p1";
    }
    if (name == "PDK_ROOT") {
      return "/p2";
    }
    if (name == "PDKROOT") {
      return "/p1";  // a repeat adds nothing
    }
    return std::nullopt;
  };
  CHECK(cell::technology_search_roots(vars) ==
        std::vector<hefesto::platform::Path>{"/p1", "/p2", "/usr/local/share/pdk"});
}

TEST_CASE("load discovers, loads and registers the technology a cell names",
          "[cap:cell-library-resolution][req:Automatic technology discovery][req:Standard-cell library path discovery]") {
  CommandSession s;
  s.editor.technology = hefesto::tech::builtin_technology();
  s.editor.path_variables.variable = [](std::string_view name) -> std::optional<std::string> {
    return name == "PDK_ROOT" ? std::optional<std::string>{"/opt/pdk"} : std::nullopt;
  };
  s.write("/opt/pdk/tiny/libs.tech/magic/tiny.tech", read_tiny_tech());
  s.write("/opt/pdk/tiny/libs.ref/lib1/mag/nand.mag", "magic\ntech tiny\ntimestamp 1\n<< end >>\n");
  s.write("/work/a.mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  const auto r = s.run("load", {"a"});
  REQUIRE(r.ok());
  CHECK(s.editor.technology->name() == "tiny");
  CHECK(cell::test::has_text(r, "Loading technology tiny"));
  CHECK(s.editor.search_paths.system == std::vector<std::string>{"/opt/pdk/tiny/libs.tech/magic"});
  CHECK(s.editor.search_paths.library == std::vector<std::string>{"/opt/pdk/tiny/libs.ref/lib1/mag"});
  CHECK(cell::test::has_text(r, "Cell path is now \"/opt/pdk/tiny/libs.ref/lib1/mag\""));
  REQUIRE(s.editor.edit_cell != nullptr);
  CHECK(s.editor.edit_cell->bbox() == db::Rect{0, 0, 4, 4});
}

TEST_CASE("a technology that cannot be found fails the read",
          "[cap:cell-library-resolution][req:Automatic technology discovery]") {
  CommandSession s;
  s.editor.technology = hefesto::tech::builtin_technology();
  s.write("/work/a.mag", "magic\ntech nosuch\ntimestamp 5\n<< end >>\n");
  const auto r = s.run("load", {"a"});
  CHECK(cell::test::has_text(r, "Loading technology nosuch"));
  CHECK(cell::test::has_text(r, "Error in loading technology file"));
  CHECK(s.editor.technology->name() == "minimum");
}

TEST_CASE("paint in a loaded cell prevents switching technologies",
          "[cap:cell-library-resolution][req:Automatic technology discovery]") {
  CommandSession s;
  s.editor.path_variables.variable = [](std::string_view name) -> std::optional<std::string> {
    return name == "PDK_ROOT" ? std::optional<std::string>{"/opt/pdk"} : std::nullopt;
  };
  s.write("/opt/pdk/other/other.tech", "tech\n  other\nend\n");
  s.write("/work/p.mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  s.write("/work/q.mag", "magic\ntech other\ntimestamp 5\n<< end >>\n");
  REQUIRE(s.run("load", {"p"}).ok());
  const auto r = s.run("load", {"q"});
  CHECK(s.editor.technology->name() == "tiny");
  CHECK_FALSE(cell::test::has_text(r, "Loading technology other"));
  CHECK(cell::test::has_text(r, "Use command \"tech load\" if you want to switch technologies, or use"));
}
