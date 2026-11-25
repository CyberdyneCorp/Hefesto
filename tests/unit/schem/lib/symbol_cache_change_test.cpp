// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Change detection of the symbol cache (capability
// schematic-symbols-and-libraries "Symbol cache and reload"; tasks.md 7.2):
// SymbolCache::forget_changed() over platform::FileSystem::modified_time,
// on the local file system and through the script sandbox.
//
// Owner: CMD (the cache change detection of M8 round 5).

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace schem = hefesto::schem;
namespace platform = hefesto::platform;

namespace {

constexpr const char* kCache = "[cap:schematic-symbols-and-libraries][req:Symbol cache and reload]";

class Folder {
 public:
  Folder() {
    static std::atomic<int> counter{0};
    path_ = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
            ("hefesto-symcache-" + std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1)));
    std::filesystem::create_directories(path_);
  }
  ~Folder() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  Folder(const Folder&) = delete;
  Folder& operator=(const Folder&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }
  void write(const std::string& name, const std::string& text) const {
    std::ofstream(path_ / name, std::ios::binary) << text;
  }
  // Rewrites `name` with the modification time moved on by two seconds.
  void rewrite(const std::string& name, const std::string& text) const {
    const auto before = std::filesystem::last_write_time(path_ / name);
    write(name, text);
    std::filesystem::last_write_time(path_ / name, before + std::chrono::seconds{2});
  }

 private:
  std::filesystem::path path_;
};

std::string symbol(const std::string& type) {
  return "v {xschem version=3.4.8RC file_version=1.3}\nK {type=" + type + "}\nL 4 0 0 10 0 {}\n";
}

std::string type_of(const schem::Symbol& s) {
  return schem::get_token(s.property(), "type").value_or("<none>");
}

}  // namespace

TEST_CASE("LocalFileSystem reports modification times; unknown for a missing file", kCache) {
  Folder folder;
  folder.write("a.sym", symbol("x"));
  platform::LocalFileSystem fs;
  const auto time = fs.modified_time(folder.path() / "a.sym");
  REQUIRE(time.has_value());
  const bool same = *time == std::filesystem::last_write_time(folder.path() / "a.sym");
  CHECK(same);
  CHECK_FALSE(fs.modified_time(folder.path() / "missing.sym").has_value());
  // A file system without times reports unknown (the default).
  platform::MemoryFileSystem memory;
  REQUIRE(memory.write_atomic("/m/a.sym", "x").ok());
  CHECK_FALSE(memory.modified_time("/m/a.sym").has_value());
}

TEST_CASE("the script sandbox forwards modification times inside its roots only", kCache) {
  Folder folder;
  Folder outside;
  folder.write("a.sym", symbol("x"));
  outside.write("b.sym", symbol("x"));
  platform::LocalFileSystem local;
  hefesto::script::SandboxPolicy policy;
  policy.granted_roots.push_back(folder.path());
  hefesto::script::SandboxedFileSystem sandboxed(local, policy);
  const bool forwarded = sandboxed.modified_time(folder.path() / "a.sym") == local.modified_time(folder.path() / "a.sym");
  CHECK(forwarded);
  CHECK(local.modified_time(folder.path() / "a.sym").has_value());
  CHECK_FALSE(sandboxed.modified_time(outside.path() / "b.sym").has_value());
}

TEST_CASE("a changed symbol file is read again; an unchanged one is kept", kCache) {
  Folder folder;
  folder.write("a.sym", symbol("one"));
  folder.write("b.sym", symbol("keep"));
  platform::LocalFileSystem fs;
  schem::SearchPath path = schem::SearchPath::parse(folder.path().generic_string(), {}, fs);
  const schem::ResolveContext context{path, fs, nullptr, true};
  schem::Document from;
  schem::SymbolCache cache;
  CHECK(type_of(cache.resolve(schem::Reference{"a.sym"}, from, context)) == "one");
  const schem::Symbol* kept = &cache.resolve(schem::Reference{"b.sym"}, from, context);
  CHECK(cache.forget_changed(fs) == 0);

  folder.rewrite("a.sym", symbol("two"));
  CHECK(type_of(cache.resolve(schem::Reference{"a.sym"}, from, context)) == "one");  // until asked
  CHECK(cache.forget_changed(fs) == 1);
  CHECK(type_of(cache.resolve(schem::Reference{"a.sym"}, from, context)) == "two");
  CHECK(&cache.resolve(schem::Reference{"b.sym"}, from, context) == kept);
  CHECK(cache.forget_changed(fs) == 0);
}

TEST_CASE("a symbol file that changes size or disappears is dropped too", kCache) {
  Folder folder;
  folder.write("a.sym", symbol("one"));
  platform::LocalFileSystem fs;
  schem::SearchPath path = schem::SearchPath::parse(folder.path().generic_string(), {}, fs);
  const schem::ResolveContext context{path, fs, nullptr, true};
  schem::Document from;
  schem::SymbolCache cache;
  (void)cache.resolve(schem::Reference{"a.sym"}, from, context);
  // Same modification time, other size.
  const auto time = std::filesystem::last_write_time(folder.path() / "a.sym");
  folder.write("a.sym", symbol("longer"));
  std::filesystem::last_write_time(folder.path() / "a.sym", time);
  CHECK(cache.forget_changed(fs) == 1);
  CHECK(type_of(cache.resolve(schem::Reference{"a.sym"}, from, context)) == "longer");
  std::filesystem::remove(folder.path() / "a.sym");
  CHECK(cache.forget_changed(fs) == 1);
  CHECK(cache.resolve(schem::Reference{"a.sym"}, from, context).placeholder);
  CHECK(cache.forget_changed(fs) == 0);  // a missing symbol is not cached
}
