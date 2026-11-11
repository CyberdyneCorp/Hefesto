// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Every public header of the desktop shell, compiled in a target that has no
// Raven include path (raven::raven is a PRIVATE dependency of the shell). If a
// header included Raven, this file would not compile.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/shell/assets.hpp"
#include "hefesto/shell/backup_timer.hpp"
#include "hefesto/shell/canvas_controller.hpp"
#include "hefesto/shell/canvas_overlay.hpp"
#include "hefesto/shell/command_bar_model.hpp"
#include "hefesto/shell/command_sink.hpp"
#include "hefesto/shell/console_session.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/design_tokens.hpp"
#include "hefesto/shell/drc_link.hpp"
#include "hefesto/shell/drc_models.hpp"
#include "hefesto/shell/editing_models.hpp"
#include "hefesto/shell/file_picker_host.hpp"
#include "hefesto/shell/gui_script.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/key_bindings_sheet.hpp"
#include "hefesto/shell/label_tool.hpp"
#include "hefesto/shell/manager_views.hpp"
#include "hefesto/shell/menu_bar_model.hpp"
#include "hefesto/shell/minimap.hpp"
#include "hefesto/shell/options.hpp"
#include "hefesto/shell/panels_layout.hpp"
#include "hefesto/shell/pcell_dialog_model.hpp"
#include "hefesto/shell/pdk_job.hpp"
#include "hefesto/shell/pdk_panel_model.hpp"
#include "hefesto/shell/screen_models.hpp"
#include "hefesto/shell/script_consent_model.hpp"
#include "hefesto/shell/shell.hpp"
#include "hefesto/shell/tcl_console.hpp"
#include "hefesto/shell/text_helper_view.hpp"
#include "hefesto/shell/text_key_filter.hpp"
#include "hefesto/shell/theme_tokens.hpp"
#include "hefesto/shell/toolbar_views.hpp"
#include "hefesto/shell/view_commands.hpp"
#include "hefesto/shell/view_models.hpp"
#include "hefesto/shell/wiring_hud.hpp"

#if defined(RAVEN_H) || defined(RAVEN_CORE_H)
#error "a public header of the desktop shell pulled in Raven"
#endif

#if __has_include(<Raven.h>)
#error "Raven's include path leaked out of hefesto_shell_desktop"
#endif

TEST_CASE("shell public headers are Raven-free", "[cap:hefesto-architecture][req:App shells per platform]") {
  SUCCEED("compiled without Raven");
}
