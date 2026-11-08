// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/compat/compat_settings.hpp"

namespace hefesto::compat {

namespace {
std::string tag(const char* name, bool on) {
  return std::string{name} + (on ? "=on" : "=off");
}
}  // namespace

std::string magic_compat_header_tag(const CompatSettings& settings) {
  return tag("magic-compat", settings.magic_compat);
}

std::string netgen_compat_header_tag(const CompatSettings& settings) {
  return tag("netgen-compat", settings.netgen_compat);
}

}  // namespace hefesto::compat
