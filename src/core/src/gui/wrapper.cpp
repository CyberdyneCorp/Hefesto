// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wrapper layout window (see wrapper.hpp).
// (Magic ref: tcltk/wrapper.tcl:233-268,533-729,1050-1584, tcltk/tools.tcl:267-295)

#include "hefesto/gui/wrapper.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <memory>

#include "gui/gui_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/diag_codes.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/pdk/widgets.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/view/window.hpp"
#include "ui/command_util.hpp"

namespace hefesto::gui {

namespace {

void finish_wrapper(cmd::CommandContext& ctx, editor::EditorState& editor, script::Interpreter& interp,
                    const std::string& framename, std::string_view cell);


using cmd::CommandContext;
using cmd::CommandResult;
using Kind = WrapperMenuItem::Kind;

WrapperMenuItem item(std::string label, std::string command) {
  return WrapperMenuItem{Kind::command, std::move(label), std::move(command), {}, {}, true};
}

WrapperMenuItem check(std::string label, std::string variable, std::string command) {
  return WrapperMenuItem{Kind::check, std::move(label), std::move(command), std::move(variable), {}, true};
}

WrapperMenuItem separator() {
  return WrapperMenuItem{Kind::separator, {}, {}, {}, {}, true};
}

WrapperMenu file_menu() {
  return {"File",
          {item("Open...", "magic::promptload magic"), item("Save...", "magic::promptsave magic"), separator(),
           item("Flush changes", "magic::flush"), separator(), item("Read GDS", "magic::promptload gds"),
           item("Write GDS", "magic::promptsave gds"), separator(),
           item("Save All and Quit", "magic::saveallandquit"), item("Quit", "magic::quit")}};
}

WrapperMenu edit_menu() {
  return {"Edit",
          {item("Delete", "delete"), separator(), item("Select Area", "select area"),
           item("Select Clear", "select clear"), separator(), item("Undo", "magic::undo"),
           item("Redo", "magic::redo"), separator(),
           // Q-SCRIPT-36 (fix): Magic's item runs the non-existent `magic::clock`.
           item("Rotate 90 degree", "magic::clockwise 90"), item("Mirror   Up/Down", "magic::upsidedown"),
           item("Mirror Left/Right", "magic::sideways"), separator(), item("Move Right", "move right 1"),
           item("Move Left", "move left 1"), item("Move Up", "move up 1"), item("Move Down", "move down 1"),
           separator(), item("Stretch Right", "stretch right 1"), item("Stretch Left", "stretch left 1"),
           item("Stretch Up", "stretch up 1"), item("Stretch Down", "stretch down 1"), separator(),
           item("Text ...", "magic::update_texthelper")}};
}

WrapperMenu cell_menu() {
  return {"Cell",
          {item("New...", "magic::prompt_dialog new"), item("Save as...", "magic::prompt_dialog save"),
           item("Select", "magic::select cell"), item("Place Instance", "magic::promptload getcell"), separator(),
           item("Down hierarchy", "magic::pushstack"), item("Up   hierarchy", "magic::popstack"), separator(),
           item("Edit", "magic::edit"), separator(),
           item("Delete", "magic::cellname delete [magic::cellname list window]"), separator(),
           item("Expand Toggle", "magic::expand toggle"), item("Expand", "magic::expand"),
           item("Unexpand", "magic::unexpand"), separator(), item("Lock   Cell", "magic::instance lock"),
           item("Unlock Cell", "magic::instance unlock")}};
}

WrapperMenu window_menu(const std::string& frame) {
  WrapperMenu m{"Window",
                {item("Clone", "magic::openwrapper [magic::cellname list window]"),
                 item("New", "magic::openwrapper"), item("Set Editable", "pushbox ; select top cell ; edit ; popbox"),
                 item("Close", "closewrapper " + frame), separator(), item("Full View", "magic::view"),
                 item("Redraw", "magic::redraw"), item("Zoom Out", "magic::zoom 2"),
                 item("Zoom In", "magic::zoom 0.5"), item("Zoom Box", "magic::findbox zoom"), separator(),
                 item("Grid on", "magic::grid on"), item("Grid off", "magic::grid off"),
                 item("Snap-to-grid on", "magic::snap on"), item("Snap-to-grid off", "magic::snap off"),
                 item("Measure box", "magic::box"), separator(),
                 item("Report internal units", "magic::units internal"),
                 item("Report lambda units", "magic::units lambda"),
                 item("Report micron units", "magic::units microns"), item("Report grid units", "magic::units grid"),
                 check("Display units", "Opts(printunits)",
                       "if { $Opts(printunits) } { magic::units print } else { magic::units noprint }"),
                 separator()}};
  for (const char* grid : {"0.05um", "0.10um", "0.50um", "1.00um", "5.00um", "10.0um"}) {
    m.items.push_back(item(std::string{"Set grid "} + grid, std::string{"magic::grid "} + grid));
  }
  return m;
}

WrapperMenu layers_menu(const std::string& layoutframe) {
  return {"Layers",
          {item("Protect Base Layers", "magic::tech revert"), item("Unlock  Base Layers", "magic::tech unlock *"),
           separator(), item("Clear Feedback", "magic::feedback clear"), separator(),
           item("Reorder layers", "magic::reorderToolbar " + layoutframe)}};
}

WrapperMenu drc_menu() {
  return {"Drc",
          {item("DRC update", "drc check; drc why"), item("DRC report", "drc why"),
           item("DRC Find next error", "drc find; findbox zoom"), separator(),
           item("DRC Fast", "drc style drc(fast)"), item("DRC Complete", "drc style drc(full)")}};
}

WrapperMenu options_menu(const std::string& frame, const std::string& layoutframe, bool maze_helper) {
  WrapperMenu m{"Options",
                {check("Toolbar", "Winopts(" + frame + ",toolbar)", "magic::maketoolbar " + layoutframe),
                 check("Toolbar Hide Locked", "Opts(hidelocked)", "magic::maketoolbar " + layoutframe),
                 check("Cell Manager", "Opts(cellmgr)", "magic::cellmanager create"),
                 check("Library Manager", "Opts(libmgr)", "magic::libmanager create"),
                 check("Tech Manager", "Opts(techmgr)", "magic::techmanager create"),
                 check("DRC Manager", "Opts(drcmgr)", "magic::drcmanager create"),
                 check("Netlist Window", "Opts(netlist)", "magic::specialopen netlist"),
                 check("Colormap Window", "Opts(colormap)", "magic::specialopen color"),
                 check("3D Display", "Opts(wind3d)", "magic::render3d"),
                 check("Window Command Entry", "Winopts(" + frame + ",cmdentry)", "addcommandentry " + frame),
                 check("Crosshair", "Opts(crosshair)", "crosshair off")}};
  if (maze_helper) {
    m.items.push_back(check("Maze Router", "Opts(mazehelper)", "magic::mazehelper"));
  }
  return m;
}

std::string trim_blanks(std::string_view text) {
  const std::size_t start = text.find_first_not_of(" \t\n");
  if (start == std::string_view::npos) {
    return {};
  }
  const std::size_t end = text.find_last_not_of(" \t\n");
  return std::string{text.substr(start, end - start + 1)};
}

std::string strip_magic(std::string_view text) {
  std::string out;
  for (std::size_t i = 0; i < text.size();) {
    if (text.substr(i, 7) == "magic::") {
      i += 7;
    } else {
      out += text[i++];
    }
  }
  return out;
}

bool is_true(const std::optional<cmd::Value>& value) {
  if (!value) {
    return false;
  }
  const std::string text = value->to_string();
  return text != "0" && text != "false" && text != "no" && text != "off" && !text.empty();
}

std::string var_text(const script::Interpreter& interp, std::string_view name) {
  const auto value = interp.get_var(name);
  return value ? value->to_string() : std::string{};
}

// `catch {eval $script}` inside magic::openwrapper: the procedure's locals
// (framename, layoutframe, winname, cell) and globals are visible.
void run_hook(CommandContext& ctx, script::Interpreter& interp, const std::string& script,
              const std::array<std::string, 4>& locals) {
  static constexpr std::array<const char*, 4> kNames = {"framename", "layoutframe", "winname", "cell"};
  for (std::size_t i = 0; i < kNames.size(); ++i) {
    (void)interp.set_var(std::string{"::__hefesto_wrapper("} + kNames[i] + ")", cmd::Value{locals[i]});
  }
  (void)interp.set_var("::__hefesto_wrapper(script)", cmd::Value{script});
  const CommandResult r = interp.evaluate(
      "namespace eval ::magic {}\n"
      "::apply {{} {global lwindow owindow Opts Winopts\n"
      " foreach __v {framename layoutframe winname cell} {set $__v $::__hefesto_wrapper($__v)}\n"
      " unset __v\n"
      " set __s $::__hefesto_wrapper(script)\n"
      " unset ::__hefesto_wrapper\n"
      " eval $__s} ::magic}",
      cmd::Origin::internal);
  (void)interp.evaluate("unset -nocomplain ::__hefesto_wrapper", cmd::Origin::internal);
  if (!r.ok()) {
    ctx.emit(diag::make_native(codes::kWrapperHook, "Wrapper callback failed (ignored): " +
                                                        (r.diagnostics.empty() ? std::string{} : r.diagnostics[0].text)));
  }
}

// Q-SCRIPT-13 (fix): `Winopts(F, callback)` as Magic reads it, then the
// key without the space, which Magic never runs.
void run_hooks(CommandContext& ctx, script::Interpreter& interp, const std::array<std::string, 4>& locals) {
  const std::string& frame = locals[0];
  for (const std::string& name :
       {std::string{"Opts(callback)"}, "Winopts(" + frame + ", callback)", "Winopts(" + frame + ",callback)"}) {
    if (const auto script = interp.get_var(name)) {
      run_hook(ctx, interp, script->to_string(), locals);
    }
  }
}

// "Window close and key overrides": `o` and `O` while they are the plain
// window commands.
void override_window_keys(CommandContext& ctx) {
  if (detail::run(ctx, {"macro", "list", "o"}) == "openwindow") {
    (void)detail::run(ctx, {"macro", "o",
                            "incr owindow ;set rpt [cursor screen] ;set rptx [lindex $rpt 0] ;"
                            "set rpty [lindex $rpt 1] ;set Winopts(.owindow$owindow,geometry) 500x500+$rptx+$rpty ;"
                            "openwrapper [$Opts(focus)*.magic cellname list window] .owindow$owindow ;"
                            ".owindow$owindow*.magic view [box values]"});
  }
  if (detail::run(ctx, {"macro", "list", "O"}) == "closewindow") {
    (void)detail::run(ctx, {"macro", "O", "closewrapper $Opts(focus)"});
  }
}

// The window named `name` is opened on `cell` (Magic's
// `magic::openwindow $cell $winname`; an empty cell opens the window as
// `openwindow` without arguments does, then names it).
CommandResult open_named_window(CommandContext& ctx, editor::EditorState& editor, std::string_view cell,
                                const std::string& name) {
  if (!cell.empty()) {
    return ui::detail::run_forwarded(ctx, {"openwindow", std::string{cell}, name});
  }
  std::vector<view::WindowId> before;
  editor.windows.for_each([&](view::LayoutWindow& w) { before.push_back(w.id()); });
  CommandResult opened = ui::detail::run_forwarded(ctx, {"openwindow"});
  if (opened.ok()) {
    editor.windows.for_each([&](view::LayoutWindow& w) {
      if (std::find(before.begin(), before.end(), w.id()) == before.end()) {
        w.set_name(name);
      }
    });
  }
  return opened;
}

// The front end's file picker at the time of a request (none: cancelled), so
// that a picker set after the wrapper opened is used.
class SessionPicker final : public platform::FilePicker {
 public:
  explicit SessionPicker(editor::EditorState& editor) : editor_(editor) {}
  [[nodiscard]] std::optional<platform::Path> pick(const platform::FilePickerRequest& request) override {
    return editor_.file_picker != nullptr ? editor_.file_picker->pick(request) : std::nullopt;
  }

 private:
  editor::EditorState& editor_;
};

// The PDK widget statements on `frame` edit the session's menu model and ask
// its file picker (Q-SCRIPT-42; SHELL-A round 2 open item 1): what Magic's Tk
// frame widgets are to the PDK's menu callbacks.
void intercept_widgets(editor::EditorState& editor, script::Interpreter& interp, const std::string& frame) {
  script::ProjectInterpreter& project = editor.script;
  if (project.session_picker == nullptr) {
    project.session_picker = std::make_unique<SessionPicker>(editor);
  }
  auto interceptor =
      std::make_unique<script::WidgetInterceptor>(project.menu_model, *project.session_picker, project.events);
  (void)interceptor->install(interp, frame);
  project.interceptors.push_back(std::move(interceptor));
}

// The common tail of openwrapper and adopt_wrapper (wrapper.tcl:1495-1584):
// per-frame options, the window keys, widget interception, the hooks.
void finish_wrapper(cmd::CommandContext& ctx, editor::EditorState& editor, script::Interpreter& interp,
                    const std::string& framename, std::string_view cell) {
  if (!interp.get_var("Winopts(" + framename + ",toolbar)")) {
    (void)interp.set_var("Winopts(" + framename + ",toolbar)", cmd::Value{std::int64_t{1}});
  }
  if (!interp.get_var("Winopts(" + framename + ",cmdentry)")) {
    (void)interp.set_var("Winopts(" + framename + ",cmdentry)", cmd::Value{var_text(interp, "Opts(cmdentry)")});
  }
  override_window_keys(ctx);
  intercept_widgets(editor, interp, framename);
  run_hooks(ctx, interp, {framename, framename + ".pane.top", wrapper_window_name(framename), std::string{cell}});
}

}  // namespace

const std::vector<std::pair<std::string, std::string>>& wrapper_option_defaults() {
  static const std::vector<std::pair<std::string, std::string>> kDefaults = {
      {"techmgr", "0"},  {"target", "default"}, {"netlist", "0"},   {"colormap", "0"},       {"wind3d", "0"},
      {"crosshair", "0"}, {"hidelocked", "0"},  {"hidespecial", "0"}, {"toolbar", "0"},       {"toolscale", "1.0"},
      {"drc", "1"},       {"autobuttontext", "1"}, {"cmdentry", "0"}};
  return kDefaults;
}

void init_wrapper_options(script::Interpreter& interp) {
  for (const auto& [name, value] : wrapper_option_defaults()) {
    const std::string var = "Opts(" + name + ")";
    if (!interp.get_var(var)) {
      (void)interp.set_var(var, cmd::Value{value});
    }
  }
}

std::string wrapper_window_name(std::string_view frame) {
  return std::string{frame} + ".pane.top.magic";
}

CommandResult open_wrapper(CommandContext& ctx, std::string_view cell, std::string_view frame) {
  editor::EditorState* editor = ctx.editor();
  script::Interpreter* interp = editor != nullptr ? editor->script.interp.get() : nullptr;
  if (interp == nullptr) {
    return CommandResult::failure(diag::make(codes::kWrapper, "openwrapper needs the project's script interpreter"));
  }
  init_wrapper_options(*interp);
  const std::string lwindow = var_text(*interp, "lwindow");
  if (lwindow.empty() || lwindow == "0") {
    (void)detail::run(ctx, {"windowcaption", "off"});
    (void)detail::run(ctx, {"windowscrollbars", "off"});
    (void)detail::run(ctx, {"windowborder", "off"});
  }
  std::string framename{frame};
  if (framename.empty()) {
    const long long next = (lwindow.empty() ? 0 : std::stoll(lwindow)) + 1;
    (void)interp->set_var("lwindow", cmd::Value{static_cast<std::int64_t>(next)});
    framename = ".layout" + std::to_string(next);
  }
  const std::string winname = wrapper_window_name(framename);
  if (editor->windows.find_named(winname) != nullptr) {
    return CommandResult::failure(diag::make(codes::kWrapper, "window name \"" + framename + "\" already exists"));
  }
  if (!interp->get_var("Opts(scale)")) {
    (void)interp->set_var("Opts(scale)", cmd::Value{std::int64_t{1}});  // no font metrics in the core
  }
  CommandResult opened = open_named_window(ctx, *editor, cell, winname);
  if (!opened.ok()) {
    return opened;
  }
  finish_wrapper(ctx, *editor, *interp, framename, cell);
  return CommandResult::success(cmd::Value{winname});
}

CommandResult adopt_wrapper(CommandContext& ctx, std::string_view frame) {
  editor::EditorState* editor = ctx.editor();
  script::Interpreter* interp = editor != nullptr ? editor->script.interp.get() : nullptr;
  if (interp == nullptr) {
    return CommandResult::failure(diag::make(codes::kWrapper, "openwrapper needs the project's script interpreter"));
  }
  view::LayoutWindow* window = nullptr;
  editor->windows.for_each([&](view::LayoutWindow& w) {
    if (window == nullptr) {
      window = &w;
    }
  });
  if (window == nullptr) {
    return CommandResult::failure(diag::make(codes::kWrapper, "no layout window to wrap"));
  }
  init_wrapper_options(*interp);
  const std::string framename{frame};
  window->set_name(wrapper_window_name(framename));
  if (const auto lwindow = var_text(*interp, "lwindow"); lwindow.empty() || lwindow == "0") {
    (void)interp->set_var("lwindow", cmd::Value{std::int64_t{1}});
  }
  if (!interp->get_var("Opts(scale)")) {
    (void)interp->set_var("Opts(scale)", cmd::Value{std::int64_t{1}});
  }
  finish_wrapper(ctx, *editor, *interp, framename, {});
  return CommandResult::success(cmd::Value{wrapper_window_name(framename)});
}

CommandResult close_wrapper(CommandContext& ctx, std::string_view frame) {
  editor::EditorState* editor = ctx.editor();
  if (editor == nullptr) {
    return detail::no_session(ctx);
  }
  const std::string winname = wrapper_window_name(frame);
  if (editor->windows.find_named(winname) == nullptr) {
    return CommandResult::failure(diag::make(codes::kWrapper, "bad window path name \"" + std::string{frame} + "\""));
  }
  if (script::Interpreter* interp = editor->script.interp.get()) {
    if (var_text(*interp, "Opts(target)") == winname) {
      (void)interp->set_var("Opts(target)", cmd::Value{"default"});
    }
  }
  return ui::detail::run_forwarded(ctx, {"closewindow", winname});
}

WrapperMenuModel wrapper_menus(std::string_view frame, bool maze_helper) {
  const std::string f{frame};
  const std::string layoutframe = f + ".pane.top";
  WrapperMenuModel model;
  model.menus = {file_menu(),        edit_menu(), cell_menu(), window_menu(f), layers_menu(layoutframe),
                 drc_menu(),         options_menu(f, layoutframe, maze_helper)};
  return model;
}

std::string accelerator_key_name(std::string_view key) {
  static constexpr std::array<std::pair<std::string_view, std::string_view>, 6> kMap = {
      {{"Control_", "^"}, {"XK_", ""}, {"less", "<"}, {"more", ">"}, {"comma", ","}, {"question", "?"}}};
  std::string out;
  for (std::size_t i = 0; i < key.size();) {
    bool mapped = false;
    for (const auto& [from, to] : kMap) {  // Tcl's `string map`: first matching key at each position
      if (key.substr(i, from.size()) == from) {
        out += to;
        i += from.size();
        mapped = true;
        break;
      }
    }
    if (!mapped) {
      out += key[i++];
    }
  }
  return out;
}

void apply_accelerators(WrapperMenuModel& menus, const std::vector<std::pair<std::string, std::string>>& reverse) {
  std::map<std::string, std::string, std::less<>> keys;  // `dict create`: the last key of a text wins
  for (const auto& [text, key] : reverse) {
    keys[trim_blanks(strip_magic(text))] = key;
  }
  for (WrapperMenu& menu : menus.menus) {
    for (WrapperMenuItem& entry : menu.items) {
      if (entry.kind != Kind::command) {
        continue;
      }
      const auto found = keys.find(trim_blanks(strip_magic(entry.command)));
      entry.accelerator = found == keys.end() ? std::string{} : "(" + accelerator_key_name(found->second) + ")";
    }
  }
}

WrapperMenuModel wrapper_menu_model(CommandContext& ctx, std::string_view frame) {
  WrapperMenuModel model = wrapper_menus(frame);
  script::Interpreter* interp = ctx.editor() != nullptr ? ctx.editor()->script.interp.get() : nullptr;
  if (interp != nullptr && !is_true(interp->get_var("Opts(autobuttontext)"))) {
    return model;
  }
  const std::vector<cmd::Value> flat = detail::elements_of(detail::run_value(ctx, {"macro", "list", "-reverse"}));
  std::vector<std::pair<std::string, std::string>> reverse;
  for (std::size_t i = 0; i + 1 < flat.size(); i += 2) {
    reverse.emplace_back(flat[i].to_string(), flat[i + 1].to_string());
  }
  apply_accelerators(model, reverse);
  return model;
}

std::string wrapper_caption(const std::vector<std::string>& window_caption, std::string_view tool,
                            std::string_view technology) {
  std::string root = window_caption.empty() ? std::string{} : window_caption[0];
  std::string edit;
  if (window_caption.size() > 2 && window_caption[1] == "EDITING") {
    edit = window_caption[2];
  } else {
    for (const std::string& word : window_caption) {
      edit += (edit.empty() ? "" : " ") + word;
    }
  }
  return "Loaded: " + root + " Editing: " + edit + " Tool: " + std::string{tool} +
         "    Technology: " + std::string{technology};
}

std::string box_position_label(const std::optional<std::array<double, 4>>& box_microns) {
  if (!box_microns) {
    return "no box";
  }
  const auto& b = *box_microns;
  char text[160];
  std::snprintf(text, sizeof text, "box (%+g %+g) to (%+g %+g) microns", b[0], b[1], b[2], b[3]);
  return text;
}

std::string pointer_position_label(double x, double y, const std::optional<std::array<double, 2>>& box_ll) {
  char text[160];
  if (box_ll) {
    std::snprintf(text, sizeof text, "(%+g %+g) %+g %+g microns", x, y, x - (*box_ll)[0], y - (*box_ll)[1]);
  } else {
    std::snprintf(text, sizeof text, "(%+g %+g) microns", x, y);
  }
  return text;
}

DrcButton drc_button(bool busy, long long total_count) {
  if (busy) {
    return {"DRC", DrcButton::Colour::yellow};
  }
  return {"DRC=" + std::to_string(total_count), total_count > 0 ? DrcButton::Colour::red : DrcButton::Colour::green};
}

std::string command_entry_prompt(std::string_view frame) {
  return std::string{frame} + ">";
}

std::string command_entry_line(std::string_view frame, std::string_view line) {
  return wrapper_window_name(frame) + " " + std::string{line};
}

}  // namespace hefesto::gui
