// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (openspec/changes/m5-mask-gds-cif/design.md D3):
// content_stamps answers an empty table until GDSR ports the structure scan
// and the FNV-1a stamp (tasks.md section 9). Owner: GDSR.

#include "hefesto/gds/read/stamps.hpp"

namespace hefesto::gds {

StampTable content_stamps(std::span<const std::byte> /*bytes*/) {
  return {};
}

}  // namespace hefesto::gds
