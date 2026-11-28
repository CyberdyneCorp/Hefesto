// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The top bar's Extract button (openspec/changes/m7-extraction task 16.1,
// design D19): enabled only with an extract style and a root, disabled with
// a tooltip naming what is missing, a click runs `extract all` as typed.
// Includes extract_button.hpp without Raven (public-header rule).
// Owner: SHELL.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/shell/extract_button.hpp"

namespace shell = hefesto::shell;

#define EXTRACT_BUTTON_TAGS "[cap:hefesto-app-ux][req:Main window layout]"

namespace {

shell::ExtractButtonInput ready() {
  shell::ExtractButtonInput in;
  in.technology = "exttest";
  in.style = "exttest";
  in.root = "top";
  return in;
}

}  // namespace

TEST_CASE("Extract is enabled with an extract style and a root, and runs extract all", EXTRACT_BUTTON_TAGS) {
  const shell::ExtractButtonView view = shell::extract_button_view(ready());
  CHECK(view.enabled);
  CHECK(view.tooltip == "Extract top: extract all (style exttest)");
  CHECK(shell::extract_click_line(view) == std::optional<std::string>{"extract all"});
  CHECK(shell::extract_button_status(view) == "hefesto: extract button enabled: Extract top: extract all (style exttest)");
}

TEST_CASE("Extract is disabled with a tooltip when the technology has no extract style", EXTRACT_BUTTON_TAGS) {
  shell::ExtractButtonInput in = ready();
  in.technology = "minimum";
  in.style.reset();
  const shell::ExtractButtonView view = shell::extract_button_view(in);
  CHECK_FALSE(view.enabled);
  CHECK(view.tooltip ==
        "Extract: technology minimum has no extract style (no extract section); load a technology with one");
  CHECK_FALSE(shell::extract_click_line(view).has_value());  // a click does nothing
  CHECK(shell::extract_button_status(view).rfind("hefesto: extract button disabled: ", 0) == 0);
}

// Regression (review of PR #141): the tooltip read "technology  has no
// extract style" with an empty name when no technology was loaded.
TEST_CASE("Extract is disabled with its own tooltip when no technology is loaded", EXTRACT_BUTTON_TAGS) {
  shell::ExtractButtonInput in = ready();
  in.technology.clear();
  in.style.reset();
  const shell::ExtractButtonView view = shell::extract_button_view(in);
  CHECK(view == shell::ExtractButtonView{false, "Extract: no technology loaded; load a technology with an extract section"});
  CHECK_FALSE(shell::extract_click_line(view).has_value());
}

TEST_CASE("Extract is disabled without a root and while a job runs", EXTRACT_BUTTON_TAGS) {
  shell::ExtractButtonInput no_root = ready();
  no_root.root.reset();
  CHECK(shell::extract_button_view(no_root) ==
        shell::ExtractButtonView{false, "Extract: no cell in the active window"});
  shell::ExtractButtonInput running = ready();
  running.job_running = true;
  const shell::ExtractButtonView view = shell::extract_button_view(running);
  CHECK_FALSE(view.enabled);
  CHECK(view.tooltip == "Extract: a job is running; Interrupt it from its card or with Ctrl-C");
}

TEST_CASE("the Extract button follows the session's extract style", EXTRACT_BUTTON_TAGS) {
  hefesto::editor::EditorState editor;
  shell::ExtractButtonInput in = shell::extract_button_input(editor, false);
  CHECK(in.technology == editor.technology->name());
  CHECK_FALSE(in.root.has_value());  // no window yet
  auto style = std::make_shared<hefesto::extract::ExtStyle>();
  style->name = "lambda";
  editor.extract.current = style;
  in = shell::extract_button_input(editor, true);
  CHECK(in.style == std::optional<std::string>{"lambda"});
  CHECK(in.job_running);
  editor.extract.current.reset();
  CHECK_FALSE(shell::extract_button_input(editor, false).style.has_value());
}
