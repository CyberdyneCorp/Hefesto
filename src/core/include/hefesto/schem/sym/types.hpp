// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol types and what each one means for the editor and for schematics
// used as symbols (capability schematic-symbols-and-libraries "Symbol
// types").
//
// Owner: SYMLIB.

#pragma once

#include <string>
#include <string_view>

#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::schem {

/// The `type` key of the symbol property; empty when there is none.
[[nodiscard]] std::string symbol_type(const Symbol& symbol);

/// `subcircuit` or `primitive`: the editor can descend into the symbol's
/// schematic. Every other type, devices such as `nmos` included, refuses.
[[nodiscard]] bool can_descend(std::string_view type) noexcept;

/// `ipin`, `opin` or `iopin`: a schematic port.
[[nodiscard]] bool is_port_type(std::string_view type) noexcept;

/// The pin direction of a port type: `in`, `out`, `inout`; empty otherwise.
[[nodiscard]] std::string_view port_direction(std::string_view type) noexcept;

/// The types whose instances a schematic placed as a symbol leaves out of
/// the symbol graphics (`logo`, `netlist_commands`, `launcher`, ...).
[[nodiscard]] bool ignored_in_schematic_symbol(std::string_view type) noexcept;

/// True when `type` is none of the types of the "Symbol types" table, so the
/// symbol is a device netlisted through its `format`.
[[nodiscard]] bool is_device_type(std::string_view type) noexcept;

}  // namespace hefesto::schem
