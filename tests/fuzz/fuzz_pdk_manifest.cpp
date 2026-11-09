// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the PDK catalog and manifest readers (capability
// hefesto-pdk-manager; hefesto-testing-strategy "Parser fuzzing"). The first
// byte selects the reader: family table, family manifest, version manifest,
// GitHub releases list or one GitHub release.

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/pdk/catalog.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace pdk = hefesto::pdk;
  static const pdk::Catalog kCatalog = pdk::Catalog::builtin();
  const auto [mode, json] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  switch (mode % 5U) {
    case 0:
      (void)pdk::Catalog::from_json(json);
      break;
    case 1:
      (void)pdk::parse_family_manifest(json);
      break;
    case 2:
      (void)pdk::parse_version_manifest(json);
      break;
    case 3:
      if (const pdk::Family* family = kCatalog.find_family("sky130"); family != nullptr) {
        (void)pdk::parse_github_releases(json, *family);
      }
      break;
    default:
      (void)pdk::parse_github_release_assets(json);
      break;
  }
  return 0;
}
