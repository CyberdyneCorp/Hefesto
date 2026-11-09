// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the offline PDK pack manifest (`pack.json`, read by
// `pdk import`) and the install progress text (PDK-PROGRESS-001, parsed by
// front ends) (capability hefesto-pdk-manager; hefesto-testing-strategy
// "Parser fuzzing"). The first byte selects the reader.

#include <cstddef>
#include <cstdint>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/packs.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace pdk = hefesto::pdk;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  if (mode % 2U == 0U) {
    (void)pdk::parse_pack_manifest(text);
  } else {
    (void)pdk::parse_install_progress(hefesto::diag::make(pdk::codes::kProgress, std::string{text}));
  }
  return 0;
}
