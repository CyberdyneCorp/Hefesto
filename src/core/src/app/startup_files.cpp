// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Installation root, search paths and startup files (app-startup
// "Installation root and search paths", "Startup file order"). The file
// order and names follow Magic 8.3.694 (fc5daa9e) utils/main.c:835-1135
// (Tcl build, normative per D-7) and utils/paths.h:30-73; DBGetTech
// follows database/DBio.c:6311-6338. Copyright (C) 1985, 1990 Regents of
// the University of California, see THIRD_PARTY_NOTICES.md.

#include "hefesto/app/startup.hpp"

#include <algorithm>

namespace hefesto::app {

namespace {

using platform::Path;

void add_if_present(platform::FileSystem& fs, std::vector<StartupFile>& out, const Path& path, std::string role,
                    bool deprecated = false) {
  if (fs.exists(path)) {
    out.push_back(StartupFile{path, std::move(role), deprecated});
  }
}

bool same_directory(const Path& a, const Path& b) {
  return !a.empty() && a.lexically_normal() == b.lexically_normal();
}

// The home file: ~/<rcname>, else ~/.magic (deprecated).
void add_home_file(platform::FileSystem& fs, const StartupOptions& options, const StartupEnvironment& env,
                   std::vector<StartupFile>& out) {
  const std::string rc = options.rc_name();
  if (env.home.empty() || Path{rc}.is_absolute()) {
    return;
  }
  if (fs.exists(env.home / rc)) {
    out.push_back(StartupFile{env.home / rc, "user", false});
    return;
  }
  add_if_present(fs, out, env.home / ".magic", "user", true);
}

// The local file: <rcname> from the working directory (skipped in HOME
// unless rcname is absolute), else ./.magic and magic_setup for .magicrc.
void add_local_file(platform::FileSystem& fs, const StartupOptions& options, const StartupEnvironment& env,
                    std::vector<StartupFile>& out) {
  const std::string rc = options.rc_name();
  const bool absolute = Path{rc}.is_absolute();
  if (!absolute && same_directory(env.working_directory, env.home)) {
    return;
  }
  const Path file = absolute ? Path{rc} : env.working_directory / rc;
  if (fs.exists(file)) {
    out.push_back(StartupFile{file, "local", false});
    return;
  }
  if (rc != ".magicrc") {
    return;
  }
  if (fs.exists(env.working_directory / ".magic")) {
    out.push_back(StartupFile{env.working_directory / ".magic", "local", true});
    return;
  }
  add_if_present(fs, out, env.working_directory / "magic_setup", "local");
}

}  // namespace

platform::Path resolve_install_root(const std::optional<std::string>& cad_root_env, const platform::Path& builtin_prefix) {
  if (cad_root_env && !cad_root_env->empty()) {
    return Path{*cad_root_env};
  }
  return builtin_prefix;
}

std::vector<std::string> system_library_path() {
  return {".", "$CAD_ROOT/magic/sys", "$CAD_ROOT/magic/sys/current"};
}

std::vector<std::string> cell_library_path(const std::optional<std::string>& technology) {
  std::vector<std::string> out;
  if (technology && !technology->empty()) {
    out.push_back("$CAD_ROOT/magic/" + *technology);
  }
  out.emplace_back("$CAD_ROOT/magic/sys/current");
  out.emplace_back("$CAD_ROOT/magic/tutorial");
  return out;
}

std::optional<std::string> mag_technology(std::string_view text) {
  const auto first_end = text.find('\n');
  if (first_end == std::string_view::npos || text.substr(0, first_end) != "magic") {
    return std::nullopt;
  }
  std::string_view line = text.substr(first_end + 1);
  // std::find, not string_view::find: GCC 11 reports a bogus
  // -Wstringop-overread for memchr on the substring here.
  line = std::string_view{line.begin(), std::find(line.begin(), line.end(), '\n')};
  if (!line.starts_with("tech ")) {
    return std::nullopt;
  }
  line.remove_prefix(5);
  while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
    line.remove_prefix(1);
  }
  if (line.ends_with('\r')) {
    line.remove_suffix(1);
  }
  return std::string{line};
}

std::vector<StartupFile> startup_files(platform::FileSystem& fs, const StartupOptions& options,
                                       const StartupEnvironment& env) {
  std::vector<StartupFile> out;
  if (!env.install_root.empty()) {
    add_if_present(fs, out, env.install_root / "magic" / "sys" / ".initrc", "pre");
    add_if_present(fs, out, env.install_root / "magic" / "sys" / ".magicrc", "system");
  }
  if (env.pdk_startup_file) {
    add_if_present(fs, out, *env.pdk_startup_file, "pdk");
  }
  if (!options.no_rc) {
    add_home_file(fs, options, env, out);
    add_local_file(fs, options, env, out);
  }
  return out;
}

}  // namespace hefesto::app
