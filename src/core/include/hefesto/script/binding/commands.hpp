// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The commands of the binding area (openspec/changes/m4-scripting/design.md
// D3/D6): `tag` (scripting-api "Command tag callbacks") and `cif` (the
// `scale`, `list`/`listall istyle|ostyle` queries of Q-SCRIPT-7, backed by
// the M3 cifoutput catalog; the rest of `cif` arrives with M5). Called by
// cmd::register_all_commands.
//
// Owner: BINDING (design D2). `cif` lives in
// src/core/src/script/binding/cif_command.cpp.

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::script {

/// Registers `tag` and `cif`. SCAFFOLD: both are CORE-NOTIMPL-004 stubs.
bool register_binding_commands(cmd::CommandRegistry& registry);

/// Registers `cif` in the layout client.
bool register_cif_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::script
