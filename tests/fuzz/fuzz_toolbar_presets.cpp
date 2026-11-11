// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the toolbar presets file `<tech>_toolbars.tcl`, read
// as data and never evaluated (capability ui-gui-managers "Layer toolbar",
// "Toolbar order presets file"; Q-SCRIPT-11, Q-SCRIPT-28), and for the DRC
// report file read by drc_load_report ("DRC manager and DRC report file").
// The first byte selects the reader; the rest is the file.
//
// Written by GUIMGR; tests/fuzz/ is shared (INTEGRATION, design.md D2).

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/gui/drc_report.hpp"
#include "hefesto/gui/toolbar_presets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace gui = hefesto::gui;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  if ((mode & 1U) != 0) {
    for (const auto& error : gui::parse_drc_report(text)) {
      (void)error.boxes.size();
    }
    return 0;
  }
  const gui::PresetFile file = gui::parse_toolbar_presets(text);
  (void)gui::skipped_diagnostics(file, "fuzz_toolbars.tcl");
  (void)file.names();
  if (const gui::ToolbarPreset* current = file.find("current_toolbar")) {
    (void)gui::rewrite_current_toolbar(text, current->layers);
  }
  return 0;
}
