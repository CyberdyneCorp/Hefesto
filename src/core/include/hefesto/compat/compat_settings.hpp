// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Reference-tool compatibility settings (capability hefesto-magic-compat-mode,
// requirements "magic-compat setting for value-changing quirks",
// "Reference-tool compatibility settings", "netgen-compat setting for netgen
// quirks"). Each setting is independent of the others.

#pragma once

#include <string>

namespace hefesto::compat {

struct CompatSettings {
  bool magic_compat = true;
  bool netgen_compat = true;

  friend bool operator==(const CompatSettings&, const CompatSettings&) = default;
};

/// The tag that every netlist and `.ext` writer puts in its header comment:
/// "magic-compat=on" or "magic-compat=off". The writer adds its own comment
/// leader (e.g. "* " for SPICE).
[[nodiscard]] std::string magic_compat_header_tag(const CompatSettings& settings);

/// "netgen-compat=on" or "netgen-compat=off", for LVS reports. netgen-compat
/// never appears in layout, extraction or netlist output.
[[nodiscard]] std::string netgen_compat_header_tag(const CompatSettings& settings);

}  // namespace hefesto::compat
