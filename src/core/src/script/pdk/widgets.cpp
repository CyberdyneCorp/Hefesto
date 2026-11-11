// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Widget interception for PDK menu callbacks (Q-SCRIPT-34, Q-SCRIPT-42,
// D-32; scripting-pdk-integration "Wrapper widget operations in PDK menu
// callbacks"). Magic's File menu is ported from Magic 8.3.694 (fc5daa9e)
// tcltk/wrapper.tcl:1226-1244. Copyright (C) 1985, 1990 Regents of the
// University of California, see THIRD_PARTY_NOTICES.md.

#include "hefesto/script/pdk/widgets.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <iterator>
#include <optional>
#include <utility>

namespace hefesto::script {

namespace {

constexpr std::string_view kSkipped = "SCRIPT-PDK-004";

// `::hefesto::pdk::widget kind file line procedure words...`: where the words start.
constexpr std::size_t kWordsAt = 4;

// The Tcl side: every intercepted statement reaches the native
// `::hefesto::pdk::widget <kind> <file> <line> <proc> <words...>`. The
// location is the innermost frame Tcl knows a file for (TIP 280: also the
// body of a procedure defined in a sourced file), else the file being
// sourced; the procedure is the innermost procedure frame (`proc` key) that
// is not one of the interception procedures.
constexpr std::string_view kCommonScript = R"tcl(
namespace eval ::hefesto::pdk {}
proc ::hefesto::pdk::ours {name} {
    if {$name eq "::unknown" || [string match ::hefesto::pdk::* $name]} {return 1}
    return [string match "::hefesto::pdk::forward *" [string trim [info body $name]]]
}
proc ::hefesto::pdk::where {} {
    set caller ""
    for {set level [expr {[info frame] - 1}]} {$level > 0} {incr level -1} {
        set frame [info frame $level]
        set name [expr {[dict exists $frame proc] ? [dict get $frame proc] : ""}]
        if {$name ne "" && [::hefesto::pdk::ours $name]} {continue}
        if {$caller eq ""} {set caller $name}
        if {[dict exists $frame file] && [dict exists $frame line]} {
            return [list [dict get $frame file] [dict get $frame line] $caller]
        }
    }
    return [list [info script] 0 $caller]
}
proc ::hefesto::pdk::forward {kind words} {
    lassign [::hefesto::pdk::where] file line procedure
    ::hefesto::pdk::widget $kind $file $line $procedure {*}$words
}
proc ::tk_getOpenFile {args} {
    return [::hefesto::pdk::open_file {*}$args]
}
if {[info commands ::hefesto::pdk::unknown_next] eq "" && [info commands ::unknown] ne ""} {
    rename ::unknown ::hefesto::pdk::unknown_next
}
proc ::unknown {args} {
    if {[string index [lindex $args 0] 0] eq "."} {
        return [::hefesto::pdk::forward skip $args]
    }
    if {[info commands ::hefesto::pdk::unknown_next] ne ""} {
        return [uplevel 1 [list ::hefesto::pdk::unknown_next {*}$args]]
    }
    return -code error "invalid command name \"[lindex $args 0]\""
}
)tcl";

// A Tcl list of `words`.
std::string tcl_list(const std::vector<std::string>& words) {
  cmd::Value::List list;
  for (const auto& word : words) {
    list.emplace_back(word);
  }
  return cmd::Value{std::move(list)}.to_string();
}

// `proc <name> {args} {::hefesto::pdk::forward <kind> [linsert $args 0 <name>]}`.
std::string forwarding_proc(std::string_view name, std::string_view kind) {
  const std::string quoted = tcl_list({std::string{name}});
  return "proc " + quoted + " {args} {::hefesto::pdk::forward " + std::string{kind} + " [linsert $args 0 " + quoted +
         "]}\n";
}

std::uint32_t parse_line(std::string_view text) {
  std::uint32_t line = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), line);
  return error == std::errc{} && end == text.data() + text.size() ? line : 0;
}

// `-label`/`-command` of a Tk menu entry; the other options are ignored.
MenuItem entry_item(const std::vector<std::string>& words, std::size_t type_at) {
  MenuItem item;
  item.kind = words[type_at] == "separator" ? MenuItem::Kind::separator : MenuItem::Kind::command;
  for (std::size_t i = type_at + 1; i + 1 < words.size(); i += 2) {
    if (words[i] == "-label") {
      item.label = words[i + 1];
    } else if (words[i] == "-command") {
      item.command = words[i + 1];
    }
  }
  return item;
}

bool supported_type(std::string_view type) {
  return type == "command" || type == "separator";
}

// A Tk menu index usable for insert: a number, `end` or `last`.
std::optional<std::size_t> menu_index(std::string_view word, std::size_t size) {
  if (word == "end" || word == "last") {
    return size;
  }
  std::size_t index = 0;
  const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), index);
  if (error != std::errc{} || end != word.data() + word.size()) {
    return std::nullopt;
  }
  return index;
}

}  // namespace

std::vector<MenuItem>& MenuModel::menu(std::string_view name) {
  const auto it = std::find(order_.begin(), order_.end(), name);
  if (it != order_.end()) {
    return items_[static_cast<std::size_t>(std::distance(order_.begin(), it))];
  }
  order_.emplace_back(name);
  return items_.emplace_back();
}

void MenuModel::append(std::string_view menu_name, MenuItem item) {
  menu(menu_name).push_back(std::move(item));
}

void MenuModel::insert(std::string_view menu_name, std::size_t index, MenuItem item) {
  auto& items = menu(menu_name);
  const std::size_t at = std::min(index, items.size());
  items.insert(items.begin() + static_cast<std::ptrdiff_t>(at), std::move(item));
}

const std::vector<MenuItem>& MenuModel::items(std::string_view menu_name) const {
  static const std::vector<MenuItem> kNone;
  const auto it = std::find(order_.begin(), order_.end(), menu_name);
  if (it == order_.end()) {
    return kNone;
  }
  return items_[static_cast<std::size_t>(std::distance(order_.begin(), it))];
}

void MenuModel::clear() noexcept {
  order_.clear();
  items_.clear();
}

std::vector<MenuItem> magic_file_menu() {
  using Kind = MenuItem::Kind;
  return {
      {Kind::command, "Open...", "magic::promptload magic", true},
      {Kind::command, "Save...", "magic::promptsave magic", true},
      {Kind::separator, "", "", true},
      {Kind::command, "Flush changes", "magic::flush", true},
      {Kind::separator, "", "", true},
      {Kind::command, "Read GDS", "magic::promptload gds", true},
      {Kind::command, "Write GDS", "magic::promptsave gds", true},
      {Kind::separator, "", "", true},
      {Kind::command, "Save All and Quit", "magic::saveallandquit", true},
      {Kind::command, "Quit", "magic::quit", true},
  };
}

std::span<const std::string_view> skipped_tk_commands() {
  static constexpr std::array<std::string_view, 43> kNames = {
      "bell",          "bind",           "bindtags",      "button",       "canvas",         "checkbutton",
      "clipboard",     "destroy",        "entry",         "event",        "focus",          "font",
      "frame",         "grab",           "image",         "labelframe",   "listbox",        "lower",
      "menu",          "menubutton",     "message",       "option",       "pack",           "panedwindow",
      "place",         "radiobutton",    "raise",         "scale",        "scrollbar",      "selection",
      "spinbox",       "text",           "tk",            "tk_chooseColor", "tk_chooseDirectory", "tk_dialog",
      "tk_getSaveFile", "tk_messageBox", "tk_popup",      "tkwait",       "toplevel",       "winfo",
      "wm",
  };
  return kNames;
}

WidgetInterceptor::WidgetInterceptor(MenuModel& menus, platform::FilePicker& picker, ScriptEvents& events)
    : menus_(menus), picker_(picker), events_(events) {}

void WidgetInterceptor::skipped(const cmd::Invocation& inv) {
  // inv.args: kind, file, line, procedure, words...
  const std::vector<std::string> words(inv.args.begin() + kWordsAt, inv.args.end());
  SkippedStatement statement{tcl_list(words), ScriptLocation{inv.args[1], parse_line(inv.args[2])}, inv.args[3]};
  events_.publish_skipped_statement(statement);
  if (sink_ != nullptr) {
    std::string text = "Skipped widget statement (no Tk in Hefesto): " + statement.statement;
    if (!statement.procedure.empty()) {
      text += " (in " + statement.procedure + ")";
    }
    sink_->emit(diag::make(kSkipped, text, statement.location));
  }
}

cmd::CommandResult WidgetInterceptor::menu_command(std::string_view menu, const cmd::Invocation& inv) {
  const std::vector<std::string> words(inv.args.begin() + kWordsAt, inv.args.end());  // path, subcommand, ...
  if (words.size() >= 3 && words[1] == "add" && supported_type(words[2])) {
    menus_.append(menu, entry_item(words, 2));
    events_.publish_menu_model_changed();
    return cmd::CommandResult::success();
  }
  if (menu == "File" && menus_.items(menu).empty()) {
    for (auto& item : magic_file_menu()) {
      menus_.append(menu, std::move(item));
    }
  }
  const auto index = words.size() >= 4 && words[1] == "insert" && supported_type(words[3])
                         ? menu_index(words[2], menus_.items(menu).size())
                         : std::nullopt;
  if (!index) {
    skipped(inv);
    return cmd::CommandResult::success();
  }
  menus_.insert(menu, *index, entry_item(words, 3));
  events_.publish_menu_model_changed();
  return cmd::CommandResult::success();
}

cmd::CommandResult WidgetInterceptor::open_file(const cmd::Invocation& inv) {
  platform::FilePickerRequest request;
  for (std::size_t i = 0; i + 1 < inv.args.size(); i += 2) {
    const std::string& option = inv.args[i];
    const std::string& value = inv.args[i + 1];
    if (option == "-title") {
      request.title = value;
    } else if (option == "-initialdir") {
      request.initial_directory = value;
    } else if (option == "-initialfile") {
      request.initial_file = value;
    } else if (option == "-filetypes") {
      request.file_types.emplace_back(value, std::string{});  // the Tcl list, as given
    }
  }
  const auto chosen = picker_.pick(request);
  return cmd::CommandResult::success(chosen ? chosen->string() : std::string{});
}

void WidgetInterceptor::define_common(Interpreter& interp) {
  if (common_defined_ && interp.has_command("::hefesto::pdk::widget")) {
    return;
  }
  interp.define_command("::hefesto::pdk::widget", [this](const cmd::Invocation& inv) {
    if (inv.args.size() <= kWordsAt) {
      return cmd::CommandResult::success();
    }
    const std::string& kind = inv.args[0];
    if (kind == "drc") {
      return menu_command("Drc", inv);
    }
    if (kind == "file") {
      return menu_command("File", inv);
    }
    if (kind == "titlebar" && inv.args.size() > kWordsAt + 1 && inv.args[kWordsAt + 1] == "cget") {
      return cmd::CommandResult::failure(
          diag::make("SCRIPT-TCL-001", "invalid command name \"" + inv.args[kWordsAt] + "\""));
    }
    skipped(inv);
    return cmd::CommandResult::success();
  });
  interp.define_command("::hefesto::pdk::open_file", [this](const cmd::Invocation& inv) { return open_file(inv); });
  std::string script{kCommonScript};
  for (const std::string_view name : skipped_tk_commands()) {
    if (!interp.has_command(name)) {
      script += forwarding_proc(name, "skip");
    }
  }
  static_cast<void>(interp.evaluate(script, cmd::Origin::internal));
  common_defined_ = true;
}

cmd::CommandResult WidgetInterceptor::install(Interpreter& interp, std::string_view frame_path) {
  define_common(interp);
  const std::string frame{frame_path};
  std::string script;
  for (const std::string& layout : {frame, frame + ".pane.top"}) {
    script += forwarding_proc(layout + ".titlebar", "titlebar");
    script += forwarding_proc(layout + ".titlebar.mbuttons.drc.toolmenu", "drc");
    script += forwarding_proc(layout + ".titlebar.mbuttons.file.toolmenu", "file");
  }
  return interp.evaluate(script, cmd::Origin::internal);
}

}  // namespace hefesto::script
