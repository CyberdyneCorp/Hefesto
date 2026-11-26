// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology `mzrouter` section reader
// (capability router-maze "mzrouter section grammar";
// hefesto-testing-strategy "Parser fuzzing"; m10-plot-route design D20).
// The input is the body of an `mzrouter` section, split into lines and
// tokens as the technology reader keeps raw sections, read against the
// wiring test technology (tests/data/wiring/wire.tech).
//
// Each input is read, then derived (MZAfterTech without DRC rules) in both
// magic-compat settings, and every route-type reference of every style is
// checked to be in range: a statement that failed validation must leave
// nothing half-registered (Q-LEGACY-31, -39).
//
// Owner: MZTECH.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/route/tech/styles.hpp"
#include "hefesto/tech/loader.hpp"
#include "section_lines.hpp"

namespace {

std::shared_ptr<const hefesto::tech::Technology> technology() {
  static const auto tech = [] {
    hefesto::platform::MemoryFileSystem fs;
    hefesto::diag::NullSink sink;
    auto loaded = hefesto::tech::load_technology_text(hefesto::fuzz::source_file("tests/data/wiring/wire.tech"),
                                                      "/fuzz/wire.tech", fs, hefesto::tech::LoadOptions{}, sink);
    return loaded.ok() ? loaded.value() : hefesto::tech::builtin_technology();
  }();
  return tech;
}

// Every route-type reference indexes an existing layer or contact.
void check_styles(const hefesto::route::MazeStyles& styles) {
  using Kind = hefesto::route::RouteTypeRef::Kind;
  for (const hefesto::route::MazeStyle& style : styles.styles) {
    if (style.types.size() != style.layers.size() + style.contacts.size()) {
      std::abort();
    }
    for (const hefesto::route::RouteTypeRef ref : style.types) {
      const std::size_t size = ref.kind == Kind::layer ? style.layers.size() : style.contacts.size();
      if (ref.index >= size) {
        std::abort();
      }
    }
    for (const hefesto::route::RouteContact& contact : style.contacts) {
      if (contact.layer1 >= style.layers.size() || contact.layer2 >= style.layers.size()) {
        std::abort();
      }
    }
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto section = hefesto::fuzz::raw_section("mzrouter", hefesto::fuzz::as_text(data, size), "/fuzz/wire.tech");
  hefesto::diag::NullSink sink;
  const hefesto::route::MazeStyles styles = hefesto::route::read_mzrouter_section(&section, *technology(), sink);
  (void)hefesto::route::find_style(styles, "irouter");
  check_styles(styles);
  for (const bool magic_compat : {true, false}) {
    hefesto::route::MazeStyles derived = styles;
    hefesto::route::derive_after_load(derived, *technology(), nullptr, hefesto::lef::LayerTable{}, magic_compat, sink);
    check_styles(derived);
  }
  return 0;
}
