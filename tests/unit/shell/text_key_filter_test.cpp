// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Regression tests for the Raven 0.2.2 text-field key bugs found by the M2
// live-input check (task 10.1; docs/decisions/raven-go-no-go.md): on macOS
// the arrow, Home/End, Page and function keys inserted AppKit's private-use
// characters U+F700-U+F8FF in the command bar, and a held key typed once.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/text_key_filter.hpp"

using namespace hefesto::shell;

namespace {

constexpr std::uint32_t kUpArrowText = 0xF700U;     // NSUpArrowFunctionKey
constexpr std::uint32_t kLeftArrowText = 0xF702U;   // NSLeftArrowFunctionKey
constexpr std::uint32_t kF35Text = 0xF726U;         // NSF35FunctionKey
constexpr std::uint32_t kLastPrivateUse = 0xF8FFU;  // AppKit reserves up to here

}  // namespace

TEST_CASE("function keys insert no text in text fields",
          "[cap:hefesto-input-and-touch][req:Command entry on every platform]") {
  TextKeyFilter filter(true);
  CHECK(is_function_key_text(kUpArrowText));
  CHECK(is_function_key_text(kLastPrivateUse));
  CHECK_FALSE(is_function_key_text(0xF6FFU));
  CHECK_FALSE(is_function_key_text(0xF900U));
  // ← in the command bar: the key moves the caret, its text is dropped.
  CHECK(filter.key_down(raven_key::kLeft, false, true).empty());
  CHECK_FALSE(filter.text(kLeftArrowText));
  filter.key_up(raven_key::kLeft);
  CHECK(filter.key_down(raven_key::kF1, false, true).empty());
  CHECK_FALSE(filter.text(kF35Text));
  // Ordinary text, accents and emoji pass.
  CHECK(filter.text('a'));
  CHECK(filter.text(0x00E9U));
  CHECK(filter.text(0x1F600U));
}

TEST_CASE("a held key repeats its text where the platform does not",
          "[cap:hefesto-input-and-touch][req:Command entry on every platform]") {
  SECTION("macOS: each repeat retypes the press's text in a text field") {
    TextKeyFilter filter(true);
    CHECK(filter.key_down('A', false, true).empty());
    CHECK(filter.text('a'));
    CHECK(filter.key_down('A', true, true) == "a");
    // The retyped text comes back through the hook: it is not recorded again.
    CHECK(filter.text('a'));
    CHECK(filter.key_down('A', true, true) == "a");
    filter.key_up('A');
    CHECK(filter.key_down('A', true, true).empty());  // released: nothing held
  }
  SECTION("UTF-8 of a non-ASCII key") {
    TextKeyFilter filter(true);
    CHECK(filter.key_down('E', false, true).empty());
    CHECK(filter.text(0x00E9U));
    CHECK(filter.key_down('E', true, true) == "\xC3\xA9");
  }
  SECTION("no text for a held arrow key, a control key, or outside a text field") {
    TextKeyFilter filter(true);
    CHECK(filter.key_down(raven_key::kLeft, false, true).empty());
    CHECK_FALSE(filter.text(kLeftArrowText));
    CHECK(filter.key_down(raven_key::kLeft, true, true).empty());
    CHECK(filter.key_down(raven_key::kBackspace, false, true).empty());
    CHECK(filter.text(0x7FU));  // Raven's text field drops it itself
    CHECK(filter.key_down(raven_key::kBackspace, true, true).empty());
    CHECK(filter.key_down('A', false, false).empty());
    CHECK(filter.text('a'));
    CHECK(filter.key_down('A', true, false).empty());  // the canvas repeats the key, not text
  }
  SECTION("another key pressed while one is held") {
    TextKeyFilter filter(true);
    CHECK(filter.key_down('A', false, true).empty());
    CHECK(filter.text('a'));
    CHECK(filter.key_down('B', false, true).empty());
    CHECK(filter.text('b'));
    CHECK(filter.key_down('A', true, true).empty());  // only the newest key repeats
    CHECK(filter.key_down('B', true, true) == "b");
    filter.key_up('A');                                 // releasing the older key keeps B held
    CHECK(filter.key_down('B', true, true) == "b");
  }
  SECTION("X11 and Wayland repeat the text themselves") {
    TextKeyFilter filter(false);
    CHECK(filter.key_down('A', false, true).empty());
    CHECK(filter.text('a'));
    CHECK(filter.key_down('A', true, true).empty());
  }
}
