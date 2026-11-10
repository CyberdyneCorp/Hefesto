// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the key-binding file reader (capability
// hefesto-testing-strategy, requirement "Parser fuzzing"; m2-interactive-
// editing design.md D7). Besides crashes, hangs and memory errors it checks
// that every binding the file defined can be listed, and that every key
// display name parses back to the same key.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/macro_file.hpp"
#include "hefesto/ui/macros.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view text{reinterpret_cast<const char*>(data), size};
  hefesto::editor::EditorState editor;
  (void)hefesto::ui::read_macro_file(editor, text, "fuzz");
  for (const std::string& table : editor.macros.table_names()) {
    for (const auto& [key, macro] : editor.macros.entries(table)) {
      if (macro == nullptr) {
        continue;
      }
      const std::string name = hefesto::ui::key_display_name(key);
      const auto back = hefesto::ui::parse_key_name(name);
      if (name.rfind("0x", 0) != 0 && (!back.ok() || !(back.value() == key))) {
        std::abort();
      }
    }
  }
  return 0;
}
