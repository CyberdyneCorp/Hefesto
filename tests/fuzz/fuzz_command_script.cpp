// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the M0 command-script reader (capability
// hefesto-testing-strategy, requirement "Parser fuzzing"). Besides crashes,
// hangs and memory errors (ASan/UBSan), it checks two properties:
//   - a parse error is always one of the CMD-PARSE codes;
//   - re-quoting every parsed word and parsing again yields the same words,
//     which is what the command log relies on for replay.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "hefesto/cmd/script.hpp"

namespace {

std::string requote(const hefesto::cmd::ParsedScript& parsed) {
  std::string text;
  for (const auto& command : parsed.commands) {
    for (std::size_t i = 0; i < command.words.size(); ++i) {
      text += i == 0 ? "" : " ";
      text += hefesto::cmd::quote_word(command.words[i]);
    }
    text += '\n';
  }
  return text;
}

bool same_words(const hefesto::cmd::ParsedScript& a, const hefesto::cmd::ParsedScript& b) {
  if (a.commands.size() != b.commands.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.commands.size(); ++i) {
    if (a.commands[i].words != b.commands[i].words) {
      return false;
    }
  }
  return true;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view text(reinterpret_cast<const char*>(data), size);
  const auto parsed = hefesto::cmd::parse_script(text, "fuzz");
  if (parsed.error) {
    if (parsed.error->code.rfind("CMD-PARSE-", 0) != 0) {
      std::abort();
    }
    return 0;
  }
  const auto reparsed = hefesto::cmd::parse_script(requote(parsed));
  if (reparsed.error || !same_words(parsed, reparsed)) {
    std::abort();
  }
  return 0;
}
