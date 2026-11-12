// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-PDK registration functions of the built-in native generators
// (pcell/native/builtin.cpp).
//
// Owner: PCELL (design D2).

#pragma once

#include "hefesto/pcell/native/registry.hpp"

namespace hefesto::pcell::native {

/// The sky130 MOSFET generators (pcell/native/sky130/mos.cpp).
void add_sky130_mosfets(NativeRegistry& registry, const script::PdkIdentity& pdk);
/// The gf180mcu MOSFET generators (pcell/native/gf180mcu/mos.cpp).
void add_gf180mcu_mosfets(NativeRegistry& registry, const script::PdkIdentity& pdk);
/// The IHP sg13g2 MOSFET generators (pcell/native/sg13g2/mos.cpp).
void add_sg13g2_mosfets(NativeRegistry& registry, const script::PdkIdentity& pdk);

}  // namespace hefesto::pcell::native
