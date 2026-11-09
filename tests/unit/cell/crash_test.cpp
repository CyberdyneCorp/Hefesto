// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cell-io-safety crash backups, recovery and archives, through the `crash`,
// `archive` and `crashbackups` commands (in-memory files, in-process locks
// shared through one registry so two sessions see each other's locks).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string_view>
#include <memory>
#include <string>
#include <vector>

#include "command_session.hpp"
#include "hefesto/cell/crash.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/paint.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
using cell::test::CommandSession;
using cell::test::has_text;
using cell::test::last_text;
using Path = hefesto::platform::Path;

namespace {

const Path kDir = "/state/crash";

/// A session whose crash backup directory is kDir and whose locks are
/// shared with every other session made from `registry`.
std::unique_ptr<CommandSession> make_session(const std::shared_ptr<cell::LockRegistry>& registry) {
  auto s = std::make_unique<CommandSession>(cell::make_in_process_lock_service(registry));
  cell::cell_session(s->editor).backup_directory = kDir;
  return s;
}

/// Loads `name` from /work with metal1 paint and marks it modified.
db::CellDef& modified_cell(CommandSession& s, const std::string& name) {
  s.write("/work/" + name + ".mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  REQUIRE(s.run("load", {name}).ok());
  db::CellDef* def = s.editor.cells.find(name);
  REQUIRE(def != nullptr);
  db::mark_modified(*def);
  return *def;
}

std::vector<std::string> names_in(hefesto::platform::FileSystem& fs, const Path& dir) {
  std::vector<std::string> out;
  if (auto listed = fs.list(dir)) {
    for (const Path& p : listed.value()) {
      out.push_back(p.filename().string());
    }
  }
  return out;
}

/// Records the host timer calls.
class FakeTimer final : public cell::BackupTimerHost {
 public:
  explicit FakeTimer(std::vector<std::string>& log) : log_(log) {}
  void schedule(std::int64_t delay_ms) override { log_.push_back("schedule " + std::to_string(delay_ms)); }
  void cancel() override { log_.push_back("cancel"); }

 private:
  std::vector<std::string>& log_;
};

/// A memory host with change times, owners and running processes.
class RecoveryHost final : public cell::FileHost {
 public:
  explicit RecoveryHost(hefesto::platform::FileSystem& fs) : inner_(cell::make_memory_file_host(fs, "/work")) {}
  bool is_symlink(const Path& p) override { return inner_->is_symlink(p); }
  bool is_directory(const Path& p) override { return inner_->is_directory(p); }
  std::optional<bool> same_file(const Path& a, const Path& b) override { return inner_->same_file(a, b); }
  Path current_directory() override { return inner_->current_directory(); }
  bool writable(const Path& p) override { return inner_->writable(p); }
  std::optional<std::uint32_t> permissions(const Path& p) override { return inner_->permissions(p); }
  bool set_permissions(const Path& p, std::uint32_t m) override { return inner_->set_permissions(p, m); }
  std::optional<std::string> git_head(const Path& d) override { return inner_->git_head(d); }
  bool owned_by_current_user(const Path& p) override { return !foreign.contains(p.generic_string()); }
  std::optional<std::int64_t> change_time(const Path& p) override {
    const auto it = times.find(p.generic_string());
    return it == times.end() ? std::nullopt : std::optional{it->second};
  }
  std::optional<bool> process_running(std::int64_t pid) override { return running.contains(pid); }

  std::map<std::string, std::int64_t> times;
  std::set<std::string> foreign;
  std::set<std::int64_t> running;

 private:
  std::unique_ptr<cell::FileHost> inner_;
};

}  // namespace

// --- crash save -----------------------------------------------------------------------

TEST_CASE("crash save writes the modified cells and creates the session files",
          "[cap:cell-io-safety][req:Crash backup policy][req:Crash backup location and session lock]"
          "[cap:cmd-hierarchy][req:Crash backup command]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  (void)modified_cell(*s, "top");
  const auto r = s->run("crash", {"save"});
  REQUIRE(r.ok());
  const cell::CellSession& cs = cell::cell_session(s->editor);
  REQUIRE(cs.session_id.size() == 32);
  const Path backup = kDir / (cs.session_id + ".backup");
  const Path lock = kDir / (cs.session_id + ".lock");
  CHECK(s->files().exists(lock));
  CHECK(s->editor.locks->holds(lock));
  const std::string text = s->files().read(backup).value();
  CHECK(text.starts_with("file /work/top\nmagic\ntech tiny\n"));
  CHECK(text.ends_with("<< end >>\nend\n"));
  CHECK(has_text(r, "Created database crash recovery file " + backup.generic_string()));
}

TEST_CASE("crash save with nothing modified creates no file", "[cap:cell-io-safety][req:Crash backup policy]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  s->write("/work/top.mag", "magic\ntech tiny\ntimestamp 5\n<< end >>\n");
  REQUIRE(s->run("load", {"top"}).ok());
  REQUIRE(s->run("crash", {"save"}).ok());
  CHECK(names_in(s->files(), kDir).empty());
  CHECK(cell::cell_session(s->editor).session_id.empty());
}

TEST_CASE("crash save skips read-only, internal and not-found cells", "[cap:cell-io-safety][req:Crash backup policy]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  db::CellDef& ro = modified_cell(*s, "ro");
  ro.flags().no_edit = true;
  db::CellDef& keep = modified_cell(*s, "keep");
  (void)keep;
  REQUIRE(s->run("crash", {"save", "/work/bk"}).ok());
  const std::string text = s->files().read("/work/bk").value();
  CHECK(text.find("file /work/keep\n") != std::string::npos);
  CHECK(text.find("file /work/ro\n") == std::string::npos);
}

TEST_CASE("an explicit crash save target is used and kept at exit",
          "[cap:cell-io-safety][req:Crash backup policy][req:Crash backup location and session lock]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  (void)modified_cell(*s, "a");
  const auto r = s->run("crash", {"save", "/work/bk"});
  REQUIRE(r.ok());
  CHECK(has_text(r, "Created database crash recovery file /work/bk"));  // Q-CELL-29: the new name
  CHECK(s->files().exists("/work/bk"));
  const std::string id = cell::cell_session(s->editor).session_id;
  CHECK(s->files().exists(kDir / (id + ".lock")));  // the lock still marks the session
  cell::end_crash_session(s->editor, s->files());
  CHECK(s->files().exists("/work/bk"));
  CHECK_FALSE(s->files().exists(kDir / (id + ".lock")));
}

TEST_CASE("normal exit removes the session files", "[cap:cell-io-safety][req:Crash backup location and session lock]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  (void)modified_cell(*s, "a");
  REQUIRE(s->run("crash", {"save"}).ok());
  CHECK(names_in(s->files(), kDir).size() == 2);
  cell::end_crash_session(s->editor, s->files());
  CHECK(names_in(s->files(), kDir).empty());
  CHECK_FALSE(cell::cell_session(s->editor).backup_file.has_value());
}

TEST_CASE("the crash command's usage", "[cap:cmd-hierarchy][req:Crash backup command]") {
  CommandSession s;
  CHECK(s.run("crash").ok());
  const auto bad = s.run("crash", {"bogus"});
  REQUIRE_FALSE(bad.ok());
  CHECK(last_text(bad) == "Usage: crash save|recover [filename]");
  CHECK_FALSE(s.run("crash", {"save", "a", "b"}).ok());
  CHECK(s.run("crash", {"sa", "/work/x"}).ok());  // keyword-matched
}

TEST_CASE("the backup directory falls back to the platform location", "[cap:cell-io-safety][req:Crash backup location and session lock]") {
  CommandSession s;
  s.editor.path_variables.variable = [](std::string_view name) -> std::optional<std::string> {
    if (name == "HOME") {
      return "/home/ana";
    }
    return std::nullopt;
  };
  s.editor.path_variables.home_of = [](std::string_view) -> std::optional<std::string> { return "/home/ana"; };
  db::CellDef& a = modified_cell(s, "a");
  (void)a;
  REQUIRE(s.run("crash", {"save"}).ok());
#if defined(__APPLE__)
  const Path dir = "/home/ana/Library/Application Support/Hefesto/CrashBackups";
#else
  const Path dir = "/home/ana/.local/state/hefesto/crash-backups";
#endif
  CHECK(cell::cell_session(s.editor).backup_directory == dir);
  CHECK(names_in(s.files(), dir).size() == 2);
}

// --- crash recover ----------------------------------------------------------------------

TEST_CASE("a new session recovers an explicit crash backup", "[cap:cmd-hierarchy][req:Crash backup command]") {
  const auto registry = cell::make_lock_registry();
  auto first = make_session(registry);
  first->write("/work/a.mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n"
                              "<< metal2 >>\nrect 10 10 12 12\n<< end >>\n");
  REQUIRE(first->run("load", {"a"}).ok());
  db::mark_modified(*first->editor.cells.find("a"));
  REQUIRE(first->run("crash", {"save", "/work/bk"}).ok());

  auto second = make_session(registry);
  second->write("/work/bk", first->files().read("/work/bk").value());
  REQUIRE(second->run("crash", {"recover", "/work/bk"}).ok());
  db::CellDef* restored = second->editor.cells.find("a");
  REQUIRE(restored != nullptr);
  CHECK(restored->bbox() == db::Rect{0, 0, 12, 12});
  CHECK(restored->flags().modified);  // Q-CELL-33
  CHECK(restored->file_path() == "/work/a");  // Q-CELL-32
  CHECK_FALSE(second->files().exists("/work/bk"));
}

TEST_CASE("recovery is refused while the session has a backup file", "[cap:cell-io-safety][req:Crash recovery]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  (void)modified_cell(*s, "a");
  REQUIRE(s->run("crash", {"save"}).ok());
  const auto r = s->run("crash", {"recover"});
  REQUIRE_FALSE(r.ok());
  CHECK(last_text(r) == "Error:  Backup file in use for current session.");
}

TEST_CASE("recovery skips live sessions and takes an abandoned backup",
          "[cap:cell-io-safety][req:Crash recovery][req:Crash backup location and session lock]") {
  const auto registry = cell::make_lock_registry();
  auto live = make_session(registry);
  (void)modified_cell(*live, "livecell");
  REQUIRE(live->run("crash", {"save"}).ok());
  const std::string live_id = cell::cell_session(live->editor).session_id;

  auto fresh = make_session(registry);
  auto& fs = fresh->files();
  fresh->write(kDir / (live_id + ".backup"), live->files().read(kDir / (live_id + ".backup")).value());
  fresh->write(kDir / (live_id + ".lock"), "");
  const std::string crashed = "0f1e2d3c4b5a69788796a5b4c3d2e1f0";
  fresh->write(kDir / (crashed + ".backup"),
               "file /p/top\nmagic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 2 2\n<< end >>\nend\n");
  fresh->write(kDir / (crashed + ".lock"), "");

  std::vector<Path> asked;
  cell::cell_session(fresh->editor).confirm_recovery = [&asked](const Path& p) {
    asked.push_back(p);
    return true;
  };
  REQUIRE(fresh->run("crash", {"recover"}).ok());
  CHECK(asked == std::vector<Path>{kDir / (crashed + ".backup")});
  CHECK(fresh->editor.cells.find("top") != nullptr);
  CHECK(fresh->editor.cells.find("livecell") == nullptr);
  CHECK_FALSE(fs.exists(kDir / (crashed + ".backup")));
  CHECK_FALSE(fs.exists(kDir / (crashed + ".lock")));
  CHECK(fs.exists(kDir / (live_id + ".backup")));
}

TEST_CASE("cancel at the recovery prompt reads nothing", "[cap:cell-io-safety][req:Crash recovery]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  s->write(kDir / "abc.backup", "file top\nmagic\ntech tiny\ntimestamp 5\n<< end >>\nend\n");
  cell::cell_session(s->editor).confirm_recovery = [](const Path&) { return false; };
  REQUIRE(s->run("crash", {"recover"}).ok());
  CHECK(s->editor.cells.find("top") == nullptr);
  CHECK(s->files().exists(kDir / "abc.backup"));
}

TEST_CASE("recovery candidates: newest first, the user's only, Magic backups of ended processes",
          "[cap:cell-io-safety][req:Crash recovery][req:Import of Magic crash backups]") {
  hefesto::platform::MemoryFileSystem fs;
  RecoveryHost host{fs};
  auto locks = cell::make_in_process_lock_service();
  for (const char* f : {"/c/old.backup", "/c/new.backup", "/c/theirs.backup", "/c/mine.backup", "/c/notes.txt",
                        "/t/MAG100.abcdef", "/t/MAG200.ghijkl", "/t/MAGx.zzz", "/t/other"}) {
    REQUIRE(fs.write_atomic(f, "x").ok());
  }
  host.times = {{"/c/old.backup", 10}, {"/c/new.backup", 30}, {"/t/MAG100.abcdef", 20}, {"/t/MAG200.ghijkl", 40}};
  host.foreign = {"/c/theirs.backup"};
  host.running = {200};
  const auto found = cell::find_recovery_candidates(fs, host, *locks, cell::RecoveryScan{"/c", "mine", Path{"/t"}});
  std::vector<std::string> order;
  for (const auto& c : found) {
    order.push_back(c.backup.generic_string());
  }
  CHECK(order == std::vector<std::string>{"/c/new.backup", "/t/MAG100.abcdef", "/c/old.backup"});
  CHECK(found[1].magic);
  CHECK(found[1].lock.empty());
  CHECK(found[0].lock == Path{"/c/new.lock"});
}

TEST_CASE("Magic backups are not offered when the host cannot test processes",
          "[cap:cell-io-safety][req:Import of Magic crash backups]") {
  hefesto::platform::MemoryFileSystem fs;
  const auto host = cell::make_memory_file_host(fs);
  auto locks = cell::make_in_process_lock_service();
  REQUIRE(fs.write_atomic("/t/MAG100.abcdef", "x").ok());
  CHECK(cell::find_recovery_candidates(fs, *host, *locks, cell::RecoveryScan{"/c", "", Path{"/t"}}).empty());
}

TEST_CASE("startup recovery imports a Magic backup on the desktop", "[cap:cell-io-safety][req:Import of Magic crash backups]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  auto host = std::make_unique<RecoveryHost>(s->files());
  cell::CellSession& cs = cell::cell_session(s->editor);
  cs.host = std::move(host);
  cs.import_magic_backups = true;
  s->editor.path_variables.variable = [](std::string_view name) -> std::optional<std::string> {
    return name == "TMPDIR" ? std::optional<std::string>{"/var/folders/x/T/"} : std::nullopt;
  };
  s->write("/var/folders/x/T/MAG1234.abcdef", "file /p/top.mag\nmagic\ntech tiny\ntimestamp 5\n<< end >>\nend top\n");
  REQUIRE(s->run("crash", {"recover"}).ok());
  CHECK(s->editor.cells.find("top") != nullptr);
  CHECK_FALSE(s->files().exists("/var/folders/x/T/MAG1234.abcdef"));
}

// --- backup reading ---------------------------------------------------------------------

TEST_CASE("a partial backup keeps the cells read before the error", "[cap:cell-io-safety][req:Backup and archive reading]") {
  cell::test::CellIo io;
  const std::string text =
      "file /p/a\nmagic\ntech tiny\ntimestamp 5\n<< end >>\nfile /p/b\nmagic\ntech tiny\ntimestamp 5\n";
  const auto read = cell::read_backup(io.ctx, text, cell::BackupReadOptions{});
  REQUIRE_FALSE(read.ok());
  CHECK(read.error().text == "Error in backup file ; partial restore only!");
  CHECK(io.library.find("a") != nullptr);
  CHECK(io.library.find("a")->flags().available);
}

TEST_CASE("backup records read with a technology mismatch ignored", "[cap:cell-io-safety][req:Backup and archive reading]") {
  cell::test::CellIo io;
  cell::BackupReadOptions options;
  options.kind = "archive";
  options.file_name = "lib";
  const auto read = cell::read_backup(io.ctx, "file x\nmagic\ntech other\ntimestamp 5\n<< end >>\nend x\n", options);
  REQUIRE(read.ok());
  CHECK(read.value() == "x");
  CHECK_FALSE(io.library.find("x")->flags().modified);
  CHECK(io.library.find("x")->file_path().empty());  // a bare name is not a path
}

namespace {

std::size_t count_code(const hefesto::cmd::CommandResult& r, std::string_view code) {
  return static_cast<std::size_t>(std::count_if(r.diagnostics.begin(), r.diagnostics.end(),
                                                [&](const auto& d) { return d.code == code; }));
}

const std::string kMalformedRecord = "file /p/foo\nmagic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 x y\n"
                                     "<< end >>\nend foo\n";

}  // namespace

TEST_CASE("a format error in a crash backup is reported once", "[cap:cell-io-safety][req:Backup and archive reading]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  s->write("/work/bad.backup", kMalformedRecord);
  const auto r = s->run("crash", {"recover", "/work/bad.backup"});
  CHECK_FALSE(r.ok());
  CHECK(count_code(r, cell::codes::kFormatError) == 1);
}

TEST_CASE("a format error in an archive is reported once", "[cap:cell-io-safety][req:Backup and archive reading]") {
  CommandSession s;
  s.write("/work/bad.mag", kMalformedRecord);
  const auto r = s.run("archive", {"read", "/work/bad.mag"});
  CHECK_FALSE(r.ok());
  CHECK(count_code(r, cell::codes::kFormatError) == 1);
}

// --- archive ------------------------------------------------------------------------------

TEST_CASE("an archive round trip restores every cell", "[cap:cmd-hierarchy][req:Archive command][cap:cell-io-safety][req:Archive files]") {
  CommandSession s;
  s.write("/work/sub.mag", "magic\ntech tiny\ntimestamp 3\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  s.write("/work/top.mag", "magic\ntech tiny\ntimestamp 9\nuse sub sub_0\ntimestamp 3\n"
                           "transform 1 0 0 0 1 0\nbox 0 0 4 4\n<< end >>\n");
  REQUIRE(s.run("load", {"sub"}).ok());  // load reads only the named cell
  REQUIRE(s.run("load", {"top"}).ok());
  const auto w = s.run("archive", {"writeall", "/work/arch.mag"});
  REQUIRE(w.ok());
  CHECK(has_text(w, "Created database archive file /work/arch.mag"));

  CommandSession fresh;
  fresh.write("/work/arch.mag", s.files().read("/work/arch.mag").value());
  REQUIRE(fresh.run("archive", {"read", "/work/arch"}).ok());  // Q-CELL-34: .mag appended
  db::CellDef* top = fresh.editor.cells.find("top");
  db::CellDef* sub = fresh.editor.cells.find("sub");
  REQUIRE(top != nullptr);
  REQUIRE(sub != nullptr);
  CHECK(sub->bbox() == db::Rect{0, 0, 4, 4});
  REQUIRE(top->uses().size() == 1);
  CHECK(top->uses().front()->def == sub);
  CHECK(sub->flags().dereference);
  CHECK_FALSE(top->flags().modified);
}

TEST_CASE("archive write includes unmodified cells; read-only ones only with writeall",
          "[cap:cell-io-safety][req:Archive files]") {
  CommandSession s;
  s.write("/work/top.mag", "magic\ntech tiny\ntimestamp 9\n<< end >>\n");
  s.write("/work/pdkcell.mag", "magic\ntech tiny\ntimestamp 9\n<< end >>\n");
  REQUIRE(s.run("load", {"top"}).ok());
  REQUIRE(s.run("load", {"pdkcell"}).ok());
  s.editor.cells.find("pdkcell")->flags().no_edit = true;
  REQUIRE(s.run("archive", {"write", "/work/w.mag"}).ok());
  REQUIRE(s.run("archive", {"writeall", "/work/wa.mag"}).ok());
  const std::string w = s.files().read("/work/w.mag").value();
  const std::string wa = s.files().read("/work/wa.mag").value();
  CHECK(w.find("file /work/top\n") != std::string::npos);
  CHECK(w.find("pdkcell") == std::string::npos);
  CHECK(wa.find("file /work/pdkcell\n") != std::string::npos);
}

TEST_CASE("readref keeps recorded paths; a wrong argument count stops",
          "[cap:cmd-hierarchy][req:Archive command]") {
  CommandSession s;
  s.write("/work/l.mag", "file /lib/x\nmagic\ntech tiny\ntimestamp 5\n<< end >>\nend\n");
  REQUIRE(s.run("archive", {"readref", "/work/l.mag"}).ok());
  CHECK_FALSE(s.editor.cells.find("x")->flags().dereference);
  CHECK(s.editor.cells.find("x")->file_path() == "/lib/x");
  const auto bad = s.run("archive", {"read"});
  REQUIRE_FALSE(bad.ok());
  CHECK(last_text(bad) == "Usage: archive write|writeall|read|readref filename");
  const auto missing = s.run("archive", {"read", "/work/none"});
  REQUIRE_FALSE(missing.ok());
  CHECK(last_text(missing) == "Cannot open archive file \"/work/none\"");
}

// --- crashbackups timer -----------------------------------------------------------------------

TEST_CASE("crashbackups drives the host timer", "[cap:ui-gui-managers][req:Crash backup timer]") {
  CommandSession s;
  std::vector<std::string> log;
  cell::CellSession& cs = cell::cell_session(s.editor);
  cs.timer = std::make_unique<FakeTimer>(log);
  REQUIRE(s.run("crashbackups", {"start"}).ok());
  REQUIRE(s.run("crashbackups", {"stop"}).ok());
  REQUIRE(s.run("crashbackups", {"resume"}).ok());
  REQUIRE(s.run("crashbackups", {"disable"}).ok());
  REQUIRE(s.run("crashbackups", {"resume"}).ok());
  CHECK(log == std::vector<std::string>{"schedule 60000", "cancel", "schedule 60000", "cancel", "cancel"});
  CHECK_FALSE(s.run("crashbackups", {"sta"}).ok());  // exact options, as tools.tcl
}

TEST_CASE("a fired backup timer saves and reschedules, or halts without an interval",
          "[cap:ui-gui-managers][req:Crash backup timer][cap:cell-io-safety][req:Crash backup policy]") {
  const auto registry = cell::make_lock_registry();
  auto s = make_session(registry);
  std::vector<std::string> log;
  cell::CellSession& cs = cell::cell_session(s->editor);
  cs.timer = std::make_unique<FakeTimer>(log);
  (void)modified_cell(*s, "a");
  REQUIRE(s->run("crashbackups").ok());
  cell::run_backup_timer(*s->dispatcher);
  CHECK(s->files().exists(kDir / (cs.session_id + ".backup")));
  CHECK(log == std::vector<std::string>{"schedule 60000", "schedule 60000"});
  cs.backup_interval_ms = 0;
  cell::run_backup_timer(*s->dispatcher);
  CHECK(cs.backup_timer == cell::CellSession::BackupTimer::stopped);
  CHECK(log.size() == 2);
  cell::run_backup_timer(*s->dispatcher);  // stopped: nothing happens
  CHECK(log.size() == 2);
}
