// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wiring tool's buttons (wiring-tool "Wiring tool buttons";
// ui-tools-and-input "Other tools' buttons"): the default bindings of the
// `wiring` macro table and `trackwire`, the native form of Magic's Tcl
// procedure magic::trackwire that they run. Front ends run
// `*bypass wire show` on pointer motion while EditorState::wire.tracking is
// set (Magic rebinds <Motion> for that). Ported from Magic VLSI 8.3.694
// (fc5daa9e) tcltk/tools.tcl:519-584 (magic::enable_tools) and 590-640
// (magic::trackwire), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md). Owner: WIRE.

#include <array>
#include <string>
#include <vector>

#include "hefesto/wiring/commands.hpp"
#include "hefesto/wiring/diag_codes.hpp"
#include "wiring/commands_internal.hpp"

namespace hefesto::wiring {

namespace {

constexpr std::array<ToolBinding, 9> kBindings = {{
    {"Control_Button1", "trackwire current"},
    {"Button1", "trackwire pick"},
    {"Button2", "trackwire done"},
    {"Button3", "trackwire cancel"},
    {"Shift_Button1", "wire incr type ; wire show"},
    {"Shift_Button2", "wire switch"},
    {"Shift_Button3", "wire decr type ; wire show"},
    {"Button4", "wire incr width ; wire show"},
    {"Button5", "wire decr width ; wire show"},
}};

constexpr int kTrackingCursor = 21;  // `cursor 21` while tracking
constexpr int kWiringCursor = 19;    // the wiring tool's glyph

// `cursor N` through the registry, best effort: the glyph is cosmetic and
// the command may not be available in every front end.
void set_cursor(const WireSession& s, int glyph) {
  if (const cmd::CommandSpec* spec = s.ctx->registry().find("cursor")) {
    (void)spec->handler(*s.ctx, cmd::Invocation{"cursor", {std::to_string(glyph)}, cmd::Origin::internal, {}});
  }
}

// Diagnostics of a step are reported; the procedure goes on regardless, as
// the Tcl procedure does with TxError output.
void run_step(const WireSession& s, const cmd::CommandResult& r) {
  for (const diag::Diagnostic& d : r.diagnostics) {
    s.ctx->emit(d);
  }
}

// `trackwire current`: wire type/width kept, the box a width x width square
// with its lower-left corner at the cursor.
void start_current(const WireSession& s) {
  WireState& w = s.ed->wire;
  pick_type(s, w.type, w.width);
  if (const auto c = cursor(*s.ed)) {
    s.ed->box = db::Rect{c->point.x, c->point.y, c->point.x + w.width, c->point.y + w.width};
    s.ed->box_root = c->window->root();
  } else if (s.ed->box) {
    s.ed->box = db::Rect{s.ed->box->x0, s.ed->box->y0, s.ed->box->x0 + w.width, s.ed->box->y0 + w.width};
  }
}

void start_tracking(const WireSession& s, std::string_view option) {
  if (option == "done") {
    run_step(s, add_contact(s, std::nullopt, 0));
    return;
  }
  if (option == "pick") {
    run_step(s, pick_type_at(s, std::nullopt));
  } else if (option == "current") {
    start_current(s);
  } else {
    return;
  }
  s.ed->wire.tracking = true;
  set_cursor(s, kTrackingCursor);
}

void continue_tracking(const WireSession& s, std::string_view option) {
  if (option != "cancel") {
    run_step(s, add_leg(s, std::nullopt, LegMode::choose));
  }
  if (option == "done" || option == "cancel") {
    clear_selection(s);
    s.ed->wire.tracking = false;
    set_cursor(s, kWiringCursor);
  }
}

// magic::trackwire window ?option?: the window word is accepted and ignored
// (the pointer's window is EditorState::command_point's); a single word that
// is an option is the option.
std::string_view option_of(const std::vector<std::string>& args) {
  if (args.size() == 2) {
    return args[1];
  }
  if (args.size() == 1) {
    for (const std::string_view o : {"pick", "current", "done", "cancel"}) {
      if (args[0] == o) {
        return o;
      }
    }
  }
  return {};
}

cmd::CommandResult cmd_trackwire(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  auto s = wire_session(ctx);
  if (!s.ok()) {
    return cmd::CommandResult::failure(s.error());
  }
  if (inv.args.size() > 2) {
    return fail(codes::kUsage, "Usage: trackwire ?window? pick|current|done|cancel");
  }
  const std::string_view option = option_of(inv.args);
  if (s.value().ed->wire.tracking) {
    continue_tracking(s.value(), option);
  } else {
    start_tracking(s.value(), option);
  }
  return cmd::CommandResult::success();
}

}  // namespace

std::span<const ToolBinding> tool_bindings() noexcept {
  return kBindings;
}

bool register_trackwire(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"trackwire", "trackwire ?window? pick|current|done|cancel",
                                    "Runs the wiring tool's button actions (Magic's magic::trackwire).",
                                    cmd_trackwire})
              .has_value();
}

}  // namespace hefesto::wiring
