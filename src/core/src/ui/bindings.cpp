// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The default bindings (ui-tools-and-input "Default key bindings";
// ui-gui-managers "Tool modes"). Owner: DISPATCH.
// (Magic ref: magic/proto.magicrc.in:11-318, tcltk/tools.tcl:297-313,
// tcltk/tools.tcl:512-584)

#include "hefesto/ui/bindings.hpp"

#include <array>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/client.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/ui/macros.hpp"
#include "hefesto/ui/tools.hpp"
#include "hefesto/wiring/commands.hpp"

namespace hefesto::ui {

namespace {

constexpr StartupBinding macro(std::string_view table, std::string_view key, std::string_view text) {
  return StartupBinding{table, key, text, false, {}, {}, false};
}
constexpr StartupBinding imacro(std::string_view table, std::string_view key, std::string_view text) {
  return StartupBinding{table, key, text, true, {}, {}, false};
}
constexpr StartupBinding copy_to(std::string_view name) {
  return StartupBinding{{}, {}, {}, false, name, {}, false};
}
constexpr StartupBinding tool(std::string_view name) {
  return StartupBinding{{}, {}, {}, false, {}, name, false};
}
constexpr StartupBinding wiring_bindings() {
  return StartupBinding{{}, {}, {}, false, {}, {}, true};
}

// The system startup file (new macros, Tcl build with XLIB, no readline),
// then tools.tcl: the `+`/`-` auto-increment keys and magic::enable_tools.
// An empty text deletes the binding.
// clang-format off
// A C array: deducing std::array over a table this long can exceed Clang's
// fold-expression limit with libstdc++ (256 elements).
constexpr StartupBinding kStartup[] = {
    macro("", "a", "select visible"),
    macro("", "A", "select more visible"),
    macro("", "^A", "select less visible"),
    macro("", "b", "box"),
    macro("", "B", "findbox"),
    macro("", "c", "copy"),
    macro("", "d", "delete"),
    macro("", "^D", "erase $"),
    macro("", "e", "edit"),
    macro("", "f", "sideways"),
    macro("", "F", "upsidedown"),
    macro("", "g", "grid"),
    macro("", "G", "grid 2"),
    macro("", "i", "select cell"),
    macro("", "I", "select more cell"),
    macro("", "Control_XK_i", "select less cell"),
    imacro("", "l", "label "),
    macro("", "L", "shell ls"),
    macro("", "^L", "redraw"),
    macro("", "m", "move"),
    macro("", "M", "stretch"),
    macro("", "^N", ""),
    macro("", "o", "openwindow"),
    macro("", "O", "closewindow"),
    imacro("", "p", "paint "),
    macro("", "Control_Shift_XK_q", "quit"),
    macro("", "r", "clockwise"),
    macro("", "R", "clockwise 270"),
    macro("", "^R", "clockwise 180"),
    macro("", "s", "select"),
    macro("", "S", "select more"),
    macro("", "^S", "select less"),
    macro("", "Control_Shift_XK_s", "undo ; select"),
    macro("", "u", "undo"),
    macro("", "U", "redo"),
    macro("", "v", "view"),
    macro("", "V", "xview"),
    macro("", "w", "writeall"),
    macro("", "W", "writeall force"),
    macro("", "x", "expand"),
    macro("", "X", "unexpand"),
    macro("", "^X", "expand toggle"),
    macro("", "z", "zoom .5"),
    macro("", "Z", "zoom 2"),
    macro("", "^Z", "findbox zoom"),
    macro("", "Control_Shift_XK_z", "center"),
    macro("", "?", "drc why"),
    macro("", "!", "drc find; findbox zoom; zoom 16"),
    macro("", "/", "select area; what ; select clear"),
    macro("", ",", "select clear"),
    macro("", " ", "tool"),
    imacro("", "XK_colon", ":"),
    imacro("", "XK_semicolon", ":"),
    macro("", "Shift_XK_space", "tool box"),
    macro("", "Control_XK_space", "tool wiring"),
    macro("", "XK_Left", "scroll l .1 w"),
    macro("", "Shift_XK_Left", "scroll l 1 w"),
    macro("", "Control_XK_Left", "box grow w 1i"),
    macro("", "Control_Shift_XK_Left", "box shrink e 1i"),
    macro("", "XK_Right", "scroll r .1 w"),
    macro("", "Shift_XK_Right", "scroll r 1 w"),
    macro("", "Control_XK_Right", "box grow e 1i"),
    macro("", "Control_Shift_XK_Right", "box shrink w 1i"),
    macro("", "XK_Up", "scroll u .1 w"),
    macro("", "Shift_XK_Up", "scroll u 1 w"),
    macro("", "Control_XK_Up", "box grow n 1i"),
    macro("", "Control_Shift_XK_Up", "box shrink s 1i"),
    macro("", "XK_Down", "scroll d .1 w"),
    macro("", "Shift_XK_Down", "scroll d 1 w"),
    macro("", "Control_XK_Down", "box grow s 1i"),
    macro("", "Control_Shift_XK_Down", "box shrink n 1i"),
    macro("", "XK_KP_Delete", "box size 0 0"),
    macro("", "XK_KP_Insert", "box size 4l 4l"),
    macro("", "XK_KP_0", "box size 7l 2l"),
    macro("", "Shift_XK_KP_0", "box size 7l 2l"),
    macro("", "XK_0", "box size 7l 2l"),
    macro("", "Control_XK_KP_0", "box size 2l 7l"),
    macro("", "Control_XK_KP_Insert", "box size 2l 7l"),
    macro("", "XK_KP_End", "move sw 1i"),
    macro("", "XK_KP_Down", "move d 1i"),
    macro("", "XK_KP_2", "stretch d 1i"),
    macro("", "Shift_XK_KP_2", "stretch d 1"),
    macro("", "Shift_XK_KP_Down", "move d 1"),
    macro("", "Control_XK_KP_Down", "stretch d 1i"),
    macro("", "XK_2", "stretch d 1i"),
    macro("", "XK_KP_Next", "move se 1i"),
    macro("", "XK_KP_Left", "move l 1i"),
    macro("", "XK_KP_4", "stretch l 1i"),
    macro("", "Shift_XK_KP_4", "stretch l 1"),
    macro("", "Shift_XK_KP_Left", "move l 1"),
    macro("", "Control_XK_KP_Left", "stretch l 1i"),
    macro("", "XK_4", "stretch l 1i"),
    macro("", "XK_KP_Begin", "findbox zoom"),
    macro("", "XK_KP_5", "findbox"),
    macro("", "Shift_XK_KP_5", "findbox"),
    macro("", "XK_5", "findbox"),
    macro("", "XK_KP_Right", "move r 1i"),
    macro("", "XK_KP_6", "stretch r 1i"),
    macro("", "Shift_XK_KP_6", "stretch r 1"),
    macro("", "Shift_XK_KP_Right", "move r 1"),
    macro("", "Control_XK_KP_Right", "stretch r 1i"),
    macro("", "XK_6", "stretch r 1i"),
    macro("", "XK_KP_Home", "move nw 1i"),
    macro("", "XK_KP_Up", "move u 1i"),
    macro("", "XK_KP_8", "stretch u 1i"),
    macro("", "Shift_XK_KP_8", "stretch u 1"),
    macro("", "Shift_XK_KP_Up", "move u 1"),
    macro("", "Control_XK_KP_Up", "stretch u 1i"),
    macro("", "XK_8", "stretch u 1i"),
    macro("", "XK_KP_Prior", "move ne 1i"),
    macro("", "XK_Pointer_Button4", "scroll u .05 w"),
    macro("", "XK_Pointer_Button5", "scroll d .05 w"),
    macro("", "Shift_XK_Pointer_Button4", "scroll l .05 w"),
    macro("", "Shift_XK_Pointer_Button5", "scroll r .05 w"),
    macro("", "Control_XK_Pointer_Button4", "zoom 1.6"),
    macro("", "Control_XK_Pointer_Button5", "zoom 0.625"),
    macro("", "XK_F1", "paint ndiff"),
    macro("", "XK_F2", "paint pdiff"),
    macro("", "XK_F3", "paint poly"),
    macro("", "XK_F4", "paint poly2"),
    macro("", "XK_F5", "paint m1"),
    macro("", "XK_F6", "paint m2"),
    macro("", "XK_F7", "paint m3"),
    macro("", "XK_F8", "paint m4"),
    macro("", "XK_F9", "paint ndc"),
    macro("", "XK_F10", "paint pdc"),
    macro("", "XK_F11", "paint pc"),
    macro("", "XK_F12", "paint via"),
    macro("", "Control_Button1", "*bypass box move bl cursor"),
    macro("", "Control_Button2", "*bypass paint cursor"),
    macro("", "Control_Button3", "*bypass box corner ur cursor"),
    macro("", "Button1", "*bypass box move bl cursor"),
    macro("", "Shift_Button1", "*bypass box corner bl cursor"),
    macro("", "Button2", "*bypass paint cursor"),
    macro("", "Shift_Button2", "*bypass erase cursor"),
    macro("", "Button3", "*bypass box corner ur cursor"),
    macro("", "Shift_Button3", "*bypass box move ur cursor"),
    macro("color", "Button1", "pushbutton left"),
    macro("color", "Button2", "pushbutton middle"),
    macro("color", "Button3", "pushbutton right"),
    macro("color", "u", "undo"),
    macro("color", "U", "redo"),
    macro("color", "plus", "color next"),
    macro("color", "minus", "color last"),
    macro("netlist", "Button1", "pushbutton left"),
    macro("netlist", "Button2", "pushbutton middle"),
    macro("netlist", "Button3", "pushbutton right"),
    macro("wind3d", "L", "level up"),
    macro("wind3d", "l", "level down"),
    macro("wind3d", "C", "cif"),
    macro("wind3d", " ", "defaults"),
    macro("wind3d", "^L", "refresh"),
    macro("wind3d", "Z", "zoom 2.0 1 rel"),
    macro("wind3d", "z", "zoom 0.5 1 rel"),
    macro("wind3d", "1", "view 0 10 0 rel"),
    macro("wind3d", "2", "view 0 -10 0 rel"),
    macro("wind3d", "3", "view 10 0 0 rel"),
    macro("wind3d", "4", "view -10 0 0 rel"),
    macro("wind3d", "5", "view 0 0 10 rel"),
    macro("wind3d", "6", "view 0 0 -10 rel"),
    macro("wind3d", "7", "view 0 1 0 rel"),
    macro("wind3d", "8", "view 0 -1 0 rel"),
    macro("wind3d", "9", "view 1 0 0 rel"),
    macro("wind3d", "0", "view -1 0 0 rel"),
    macro("wind3d", "XK_Up", "scroll 0 -0.25 0 rel"),
    macro("wind3d", "XK_Down", "scroll 0 0.25 0 rel"),
    macro("wind3d", "XK_Left", "scroll 0.25 0 0 rel"),
    macro("wind3d", "XK_Right", "scroll -0.25 0 0 rel"),
    macro("wind3d", "XK_minus", "view 0 0 1 rel"),
    macro("wind3d", "XK_equal", "view 0 0 -1 rel"),
    macro("wind3d", "XK_greater", "zoom 1 2.0 rel"),
    macro("wind3d", "XK_less", "zoom 1 0.5 rel"),
    macro("", "XK_plus", "autoincr 1"),
    macro("", "XK_minus", "autoincr -1"),
    macro("", "XK_greater", "pushstack"),
    macro("", "XK_less", "popstack"),
    macro("", "space", "tool"),
    macro("", "Shift_space", "tool box"),
    copy_to("wiring"),
    copy_to("nettool"),
    copy_to("pick"),
    tool("wiring"),
    wiring_bindings(),
    tool("nettool"),
    macro("", "Button1", "netlist select"),
    macro("", "Button2", "netlist join"),
    macro("", "Button3", "netlist terminal"),
    macro("", "Shift_Button1", ""),
    macro("", "Shift_Button2", ""),
    macro("", "Shift_Button3", ""),
    macro("", "Button4", "scroll u .05 w"),
    macro("", "Button5", "scroll d .05 w"),
    tool("pick"),
    macro("", "Button1", "keepselect %W"),
    macro("", "Shift_Button2", "startselect %W copy"),
    macro("", "Button2", "startselect %W pick"),
    macro("", "Button3", "cancelselect %W"),
    macro("", "Shift_Button1", "box corner bl cursor"),
    macro("", "Shift_Button3", "box move ur cursor"),
    macro("", "Button4", "scroll u .05 w"),
    macro("", "Button5", "scroll d .05 w"),
    tool("box"),
};
// clang-format on

void define(editor::EditorState& editor, std::string_view table, std::string_view key_name, std::string_view text,
            bool interactive) {
  const auto key = parse_key_name(key_name);
  if (!key.ok()) {
    return;
  }
  if (text.empty()) {
    (void)editor.macros.erase(table, key.value());
    return;
  }
  editor.macros.define(table, key.value(), std::string{text}, std::nullopt, interactive);
}

void apply(editor::EditorState& editor, const StartupBinding& line) {
  if (line.wiring_bindings) {
    for (const wiring::ToolBinding& b : wiring::tool_bindings()) {
      define(editor, "wiring", b.key, b.text, false);
    }
    return;
  }
  if (!line.copy_to.empty()) {
    editor.macros.copy(macro_table_for(editor, "layout"), line.copy_to);
    return;
  }
  if (!line.tool.empty()) {
    (void)editor.tools.set_current(line.tool);
    return;
  }
  if (const ClientInfo* client = find_client_exact(line.table); client != nullptr && !client_available(*client)) {
    return;  // a module this build lacks (wind3d, netlist): Magic's `macro` ignores its bindings
  }
  const std::string table = line.table.empty() ? macro_table_for(editor, "layout") : std::string{line.table};
  define(editor, table, line.key, line.text, line.interactive);
}

}  // namespace

std::span<const StartupBinding> startup_bindings() noexcept {
  return kStartup;
}

void install_defaults(editor::EditorState& editor) {
  for (Tool& t : builtin_tools()) {
    (void)editor.tools.add(std::move(t));
  }
  (void)editor.tools.set_current("box");
  for (const StartupBinding& line : kStartup) {
    apply(editor, line);
  }
  (void)editor.tools.set_current("box");
  state(editor).cursor_glyph = 0;
}

void ensure_defaults(editor::EditorState& editor) {
  if (editor.tools.tools().empty()) {
    install_defaults(editor);
  }
}

std::string substitute_window(std::string_view text, std::string_view window) {
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '%' && i + 1 < text.size() && text[i + 1] == 'W') {
      out += window;
      ++i;
    } else {
      out += text[i];
    }
  }
  return out;
}

}  // namespace hefesto::ui
