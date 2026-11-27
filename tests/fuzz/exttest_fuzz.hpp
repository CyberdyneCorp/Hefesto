// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The synthetic extraction technology of the M7 golden cases
// (tests/fuzz/data/exttest_fuzz.tech: the golden cases' exttest.tech,
// m7-extraction design D21, with a fuller extract section), loaded
// once per magic-compat setting with its first extraction style read and
// micron-converted, for the M7 fuzz targets that need a style
// (fuzz_res_ext_reader; m7-extraction design D22).
//
// Owner: INTEGRATION (tests/fuzz/, design D2).

#pragma once

#include <memory>
#include <string>

#include "fuzz_common.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/tech_file.hpp"

namespace hefesto::fuzz {

struct ExtTestTech {
  std::shared_ptr<const tech::Technology> technology;
  std::shared_ptr<const extract::ExtStyle> style;  // micron-converted; null when the load failed
};

inline ExtTestTech load_exttest(bool magic_compat) {
  ExtTestTech out;
  const std::string text = tech_with_includes(std::string{HEFESTO_SOURCE_DIR} + "/tests/fuzz/data/exttest_fuzz.tech");
  platform::MemoryFileSystem fs;
  diag::NullSink sink;
  tech::LoadOptions options;
  options.magic_compat = magic_compat;
  auto loaded = tech::load_technology_text(text, "/fuzz/exttest.tech", fs, options, sink);
  if (!loaded.ok()) {
    return out;
  }
  out.technology = loaded.value();
  compat::CompatSettings compat;
  compat.magic_compat = magic_compat;
  for (const tech::RawSection& raw : out.technology->raw_sections()) {
    if (raw.name != "extract") {
      continue;
    }
    const auto table = extract::load_styles(raw, *out.technology, sink, compat);
    if (!table.names().empty()) {
      if (const auto style = table.select(table.names().front())) {
        out.style = std::make_shared<const extract::ExtStyle>(style->micron_convert(nullptr));
      }
    }
  }
  return out;
}

/// The technology for `magic_compat`, loaded on first use.
inline const ExtTestTech& exttest(bool magic_compat) {
  static const ExtTestTech on = load_exttest(true);
  static const ExtTestTech off = load_exttest(false);
  return magic_compat ? on : off;
}

}  // namespace hefesto::fuzz
