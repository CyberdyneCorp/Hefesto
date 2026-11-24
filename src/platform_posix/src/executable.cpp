// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/platform_posix/executable.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace hefesto::platform_posix {

platform::Path executable_dir() {
  std::error_code ec;
#if defined(__APPLE__)
  std::uint32_t size = 0;
  (void)_NSGetExecutablePath(nullptr, &size);
  std::string buffer(size, '\0');
  if (size == 0 || _NSGetExecutablePath(buffer.data(), &size) != 0) return {};
  const platform::Path executable{buffer.c_str()};
#else
  const platform::Path executable = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec) return {};
#endif
  const platform::Path canonical = std::filesystem::weakly_canonical(executable, ec);
  const platform::Path dir = (ec ? executable : canonical).parent_path();
  return dir.is_absolute() ? dir : platform::Path{};
}

}  // namespace hefesto::platform_posix
