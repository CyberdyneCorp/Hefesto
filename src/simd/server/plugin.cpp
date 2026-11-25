// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMD.

#include "server/plugin.hpp"

#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>

#include <climits>
#include <string>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace hefesto::simd {

namespace {

bool exists(const std::string& path) { return ::access(path.c_str(), R_OK) == 0; }

}  // namespace

std::string adapter_file_name() {
#if defined(__APPLE__)
  return "hefesto-ngspice-adapter.dylib";
#else
  return "hefesto-ngspice-adapter.so";
#endif
}

std::vector<std::string> adapter_candidates(const std::string& executable_dir) {
  if (executable_dir.empty()) {
    return {};
  }
  return {executable_dir + "/../lib/" + adapter_file_name(), executable_dir + "/" + adapter_file_name()};
}

std::string executable_dir() {
  char buffer[PATH_MAX];  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  std::string path;
#if defined(__APPLE__)
  std::uint32_t size = sizeof buffer;
  if (::_NSGetExecutablePath(buffer, &size) == 0) {
    path = buffer;
  }
#else
  const ssize_t n = ::readlink("/proc/self/exe", buffer, sizeof buffer - 1);
  if (n > 0) {
    path.assign(buffer, static_cast<std::size_t>(n));
  }
#endif
  char resolved[PATH_MAX];  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  if (path.empty() || ::realpath(path.c_str(), resolved) == nullptr) {
    return {};
  }
  path = resolved;
  const std::size_t slash = path.rfind('/');
  return slash == std::string::npos ? std::string{} : path.substr(0, slash);
}

LoadedPlugin load_adapter(const std::string& explicit_path, const std::vector<std::string>& candidates) {
  LoadedPlugin plugin;
  if (!explicit_path.empty()) {
    plugin.path = explicit_path;
  } else {
    for (const std::string& candidate : candidates) {
      if (exists(candidate)) {
        plugin.path = candidate;
        break;
      }
    }
  }
  if (plugin.path.empty()) {
    plugin.error = "engine adapter " + adapter_file_name() + " not found next to hefesto-simd";
    return plugin;
  }
  void* handle = ::dlopen(plugin.path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (handle == nullptr) {
    const char* why = ::dlerror();
    plugin.error = "cannot load " + plugin.path + ": " + (why != nullptr ? why : "unknown error");
    return plugin;
  }
  auto entry = reinterpret_cast<hefesto_engine_entry_fn>(::dlsym(handle, HEFESTO_ENGINE_ENTRY));  // NOLINT
  const hefesto_engine_api* api = entry != nullptr ? entry() : nullptr;
  if (api == nullptr || api->abi_version != HEFESTO_ENGINE_ABI_VERSION) {
    plugin.error = plugin.path + " is not a hefesto_engine_v1 plugin";
    return plugin;
  }
  plugin.api = api;
  return plugin;
}

}  // namespace hefesto::simd
