// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Every public header of the desktop shell, compiled in a target that has no
// Raven include path (raven::raven is a PRIVATE dependency of the shell). If a
// header included Raven, this file would not compile.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/shell/assets.hpp"
#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/canvas_overlay.hpp"
#include "hefesto/shell/command_bar_model.hpp"
#include "hefesto/shell/command_sink.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/design_tokens.hpp"
#include "hefesto/shell/options.hpp"
#include "hefesto/shell/pdk_panel_model.hpp"
#include "hefesto/shell/shell.hpp"
#include "hefesto/shell/theme_tokens.hpp"
#include "hefesto/shell/view_commands.hpp"
#include "hefesto/shell/view_models.hpp"

#if defined(RAVEN_H) || defined(RAVEN_CORE_H)
#error "a public header of the desktop shell pulled in Raven"
#endif

#if __has_include(<Raven.h>)
#error "Raven's include path leaked out of hefesto_shell_desktop"
#endif

TEST_CASE("shell public headers are Raven-free", "[cap:hefesto-architecture][req:App shells per platform]") {
  SUCCEED("compiled without Raven");
}
