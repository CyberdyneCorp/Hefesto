// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/version.hpp"

#include <zlib.h>

namespace hefesto {

std::string_view version() noexcept {
  return kVersion;
}

std::string_view linked_zlib_version() noexcept {
  return zlibVersion();
}

}  // namespace hefesto
