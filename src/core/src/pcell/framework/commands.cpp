// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl-visible procedures of toolkit.tcl as commands of the hidden
// cmd::kScriptClient table (reachable as `magic::<name>`; m4-scripting
// design.md D3/D8): argument checking with Tcl's proc messages, then the
// ported procedure. The toolkit menu procedures (toolkit.tcl:61-105) edit
// FrameworkState::menus.

#include <functional>
#include <random>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/stubs.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/gencell.hpp"
#include "hefesto/pcell/framework/naming.hpp"
#include "hefesto/pcell/framework/netlist.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/script/interp.hpp"
#include "pcell/framework/dialog_ops.hpp"

namespace hefesto::pcell {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using detail::Toolkit;

using ToolkitHandler = std::function<CommandResult(Toolkit&, const std::vector<std::string>&)>;

struct Arity {
  std::size_t min = 0;
  std::size_t max = 0;  // SIZE_MAX: `args`
};

constexpr std::size_t kAny = static_cast<std::size_t>(-1);

bool arity_ok(const Arity& arity, std::size_t count) {
  return count >= arity.min && count <= arity.max;
}

CommandResult raise(const diag::Diagnostic& error) {
  return CommandResult::raised(error);
}

diag::Outcome<ParameterDict> dict_argument(const std::string& text) {
  return ParameterDict::from_tcl(text);
}

// A procedure that needs the interpreter (device procedures).
cmd::CommandSpec toolkit_command(std::string name, std::string params, Arity arity, ToolkitHandler body) {
  std::string usage = "magic::" + name + (params.empty() ? "" : " " + params);
  std::string synopsis = name + (params.empty() ? "" : " " + params);
  return {std::move(name), std::move(synopsis), "gencell framework procedure (toolkit.tcl).",
          [usage, arity, body = std::move(body)](CommandContext& ctx, const Invocation& inv) {
            if (!arity_ok(arity, inv.args.size())) {
              return detail::wrong_args(usage);
            }
            auto tk = Toolkit::open(ctx);
            if (!tk.ok()) {
              return raise(tk.error());
            }
            return body(tk.value(), inv.args);
          }};
}

using PlainHandler = std::function<CommandResult(CommandContext&, const std::vector<std::string>&)>;

// A procedure that needs no interpreter.
cmd::CommandSpec plain_command(std::string name, std::string params, Arity arity, PlainHandler body) {
  std::string usage = "magic::" + name + (params.empty() ? "" : " " + params);
  std::string synopsis = name + (params.empty() ? "" : " " + params);
  return {std::move(name), std::move(synopsis), "gencell framework procedure (toolkit.tcl).",
          [usage, arity, body = std::move(body)](CommandContext& ctx, const Invocation& inv) {
            if (!arity_ok(arity, inv.args.size())) {
              return detail::wrong_args(usage);
            }
            return body(ctx, inv.args);
          }};
}

std::string arg(const std::vector<std::string>& a, std::size_t i, std::string fallback = {}) {
  return i < a.size() ? a[i] : fallback;
}

std::vector<std::string> rest(const std::vector<std::string>& a, std::size_t from) {
  return from < a.size() ? std::vector<std::string>{a.begin() + static_cast<std::ptrdiff_t>(from), a.end()}
                         : std::vector<std::string>{};
}

// ---- toolkit menus ---------------------------------------------------------------

diag::Outcome<FrameworkState*> framework_state(CommandContext& ctx) {
  auto editor = cmd::require_editor(ctx);
  if (!editor.ok()) {
    return editor.error();
  }
  return &editor.value()->script.gencell;
}

ToolkitMenu* find_menu(FrameworkState& state, const std::string& frame, const std::string& library) {
  for (auto& menu : state.menus.menus) {
    if (menu.frame == frame && menu.library == library) {
      return &menu;
    }
  }
  return nullptr;
}

CommandResult add_menu_item(CommandContext& ctx, const std::string& frame, const std::string& library,
                            ToolkitItem item) {
  auto state = framework_state(ctx);
  if (!state.ok()) {
    return raise(state.error());
  }
  ToolkitMenu* menu = find_menu(*state.value(), frame, library);
  if (menu == nullptr) {
    return detail::script_error("invalid command name \"" + frame + ".titlebar.mbuttons." + library + ".toolmenu\"");
  }
  menu->items.push_back(std::move(item));
  return CommandResult::success();
}

CommandResult add_toolkit_menu(CommandContext& ctx, const std::vector<std::string>& a) {
  auto state = framework_state(ctx);
  if (!state.ok()) {
    return raise(state.error());
  }
  const std::string library = arg(a, 2, "toolkit");
  if (find_menu(*state.value(), a[0], library) != nullptr) {
    return detail::script_error("window name \"" + library + "\" already exists in parent");
  }
  state.value()->menus.menus.push_back({a[0], library, a[1], {}});
  return CommandResult::success();
}

CommandResult add_toolkit_button(CommandContext& ctx, const std::vector<std::string>& a) {
  const std::string library = arg(a, 3, "toolkit");
  const std::string command = "magic::gencell " + library + "::" + a[2] + " {} " + merge_tcl_list(rest(a, 4));
  return add_menu_item(ctx, a[0], library, {ToolkitItem::Kind::command, a[1], command});
}

CommandResult add_toolkit_command(CommandContext& ctx, const std::vector<std::string>& a) {
  const std::string library = arg(a, 3, "toolkit");
  const std::string command = a[2] + " " + merge_tcl_list(rest(a, 4));
  return add_menu_item(ctx, a[0], library, {ToolkitItem::Kind::command, a[1], command});
}

CommandResult add_toolkit_separator(CommandContext& ctx, const std::vector<std::string>& a) {
  return add_menu_item(ctx, a[0], arg(a, 1, "toolkit"), {ToolkitItem::Kind::separator, {}, {}});
}

// ---- naming ------------------------------------------------------------------------

CommandResult murmur3(CommandContext&, const std::vector<std::string>& a) {
  std::uint32_t seed = 0;
  if (a.size() > 1) {
    try {
      seed = static_cast<std::uint32_t>(std::stoul(a[1]));
    } catch (const std::exception&) {
      return detail::script_error("expected integer but got \"" + a[1] + "\"");
    }
  }
  return CommandResult::success(static_cast<std::int64_t>(gencell_hash(a[0], seed)));
}

CommandResult hash_of(CommandContext&, const std::vector<std::string>& a) {
  auto p = dict_argument(a[0]);
  if (!p.ok()) {
    return raise(p.error());
  }
  return CommandResult::success(gencell_suffix(p.value()));
}

CommandResult random_name(Toolkit& tk, const std::vector<std::string>& a) {
  const auto draw = [&tk](int range) {
    const auto r = tk.evaluate("expr {int(rand() * " + std::to_string(range) + ")}");
    try {
      return std::stoi(detail::value_text(r.value));
    } catch (const std::exception&) {
      return 0;
    }
  };
  const auto exists = [&tk](const std::string& name) {
    const std::string r = tk.query({"cellname", "list", "exists", name});
    return !r.empty() && r != "0";
  };
  return CommandResult::success(random_gencell_name(a[0], draw, exists));
}

// ---- gencell entry points --------------------------------------------------------

CommandResult gencell_command(CommandContext& ctx, const std::vector<std::string>& a) {
  return gencell(ctx, GencellRequest{a[0], arg(a, 1), rest(a, 2)});
}

CommandResult create_command(Toolkit& tk, const std::vector<std::string>& a) {
  auto p = dict_argument(a[2]);
  if (!p.ok()) {
    return raise(p.error());
  }
  return create(tk.context(), a[0], a[1], p.value(), arg(a, 3, "0"));
}

CommandResult change_command(Toolkit& tk, const std::vector<std::string>& a) {
  auto p = dict_argument(a[3]);
  if (!p.ok()) {
    return raise(p.error());
  }
  return change(tk.context(), a[0], a[1], a[2], p.value());
}

CommandResult defaults_command(Toolkit& tk, const std::vector<std::string>& a) {
  auto p = dict_argument(a[2]);
  if (!p.ok()) {
    return raise(p.error());
  }
  return defaults(tk.context(), a[0], a[1], p.value());
}

CommandResult dialog_command(Toolkit& tk, const std::vector<std::string>& a) {
  auto p = dict_argument(a[3]);
  if (!p.ok()) {
    return raise(p.error());
  }
  return detail::open_dialog(tk, a[0], a[1], a[2], p.value());
}

CommandResult getparams_command(Toolkit& tk, const std::vector<std::string>&) {
  auto p = detail::dialog_parameters(tk);
  if (!p.ok()) {
    return raise(p.error());
  }
  return CommandResult::success(p.value().to_tcl());
}

CommandResult setparams_command(Toolkit& tk, const std::vector<std::string>& a) {
  auto p = dict_argument(a[0]);
  if (!p.ok()) {
    return raise(p.error());
  }
  return detail::push_dialog_parameters(tk, p.value());
}

CommandResult netlist_command(Toolkit& tk, const std::vector<std::string>& a) {
  return netlist_to_layout(tk.context(), platform::Path{a[0]}, a[1]);
}

ToolkitHandler builder(WidgetKind kind) {
  return [kind](Toolkit& tk, const std::vector<std::string>& a) { return detail::add_widget(tk, kind, a); };
}

std::vector<cmd::CommandSpec> framework_commands() {
  std::vector<cmd::CommandSpec> specs;
  specs.push_back(plain_command("add_toolkit_menu", "framename button_text ?library?", {2, 3}, add_toolkit_menu));
  specs.push_back(plain_command("add_toolkit_button", "framename button_text gencell_type ?library? ?arg ...?",
                                {3, kAny}, add_toolkit_button));
  specs.push_back(plain_command("add_toolkit_command", "framename button_text command ?library? ?arg ...?",
                                {3, kAny}, add_toolkit_command));
  specs.push_back(plain_command("add_toolkit_separator", "framename ?library?", {1, 2}, add_toolkit_separator));
  specs.push_back(toolkit_command("netlist_to_layout", "netfile library", {2, 2}, netlist_command));
  specs.push_back(plain_command("gencell", "gencell_name ?instname? ?arg ...?", {1, kAny}, gencell_command));
  specs.push_back(toolkit_command("gencell_makecell", "gencell_fullname ?arg ...?", {1, kAny},
                                  [](Toolkit& tk, const std::vector<std::string>& a) {
                                    return makecell(tk.context(), a[0], rest(a, 1));
                                  }));
  specs.push_back(toolkit_command("gencell_getparams", "", {0, 0}, getparams_command));
  specs.push_back(toolkit_command("gencell_setparams", "parameters", {1, 1}, setparams_command));
  specs.push_back(toolkit_command("gencell_change", "instname gencell_type library parameters", {4, 4},
                                  change_command));
  specs.push_back(toolkit_command("get_gencell_name", "gencell_type", {1, 1}, random_name));
  specs.push_back(plain_command("get_gencell_hash", "parameters", {1, 1}, hash_of));
  specs.push_back(toolkit_command("gencell_create", "gencell_type library parameters ?orient?", {3, 4},
                                  create_command));
  specs.push_back(toolkit_command("add_entry", "pname ptext parameters", {0, kAny}, builder(WidgetKind::entry)));
  specs.push_back(toolkit_command("add_check_callbacks", "gencell_type library", {2, 2},
                                  [](Toolkit& tk, const std::vector<std::string>& a) {
                                    return detail::add_check_callbacks(tk, a[0], a[1]);
                                  }));
  specs.push_back(toolkit_command("add_dependency", "callback gencell_type library ?arg ...?", {3, kAny},
                                  [](Toolkit& tk, const std::vector<std::string>& a) {
                                    return detail::add_dependency(tk, a[0], a[1], a[2], rest(a, 3));
                                  }));
  specs.push_back(toolkit_command("update_dialog", "callback pname gencell_type library", {4, 4},
                                  [](Toolkit& tk, const std::vector<std::string>& a) {
                                    return detail::update_dialog(tk, a[0], a[1], a[2], a[3]);
                                  }));
  specs.push_back(toolkit_command("add_checkbox", "pname ptext parameters", {0, kAny}, builder(WidgetKind::checkbox)));
  specs.push_back(toolkit_command("add_message", "pname ptext parameters ?color?", {0, kAny},
                                  builder(WidgetKind::message)));
  specs.push_back(toolkit_command("add_selectlist", "pname ptext all_values parameters ?itext?", {0, kAny},
                                  builder(WidgetKind::selectlist)));
  specs.push_back(toolkit_command("add_selectindex", "pname ptext all_values parameters ?ival?", {0, kAny},
                                  builder(WidgetKind::selectindex)));
  specs.push_back(toolkit_command("gencell_defaults", "gencell_type library parameters", {3, 3}, defaults_command));
  specs.push_back(toolkit_command("gencell_update", "?command?", {0, 1},
                                  [](Toolkit& tk, const std::vector<std::string>& a) {
                                    return detail::gencell_update(tk, arg(a, 0));
                                  }));
  specs.push_back(toolkit_command("gencell_dialog", "instname gencell_type library parameters", {4, 4},
                                  dialog_command));
  specs.push_back(plain_command("murmur3_32", "key ?seed?", {1, 2}, murmur3));
  specs.push_back(plain_command("normalize_value", "value", {1, 1}, [](CommandContext&, const std::vector<std::string>& a) {
    return CommandResult::success(normalize_value(a[0]));
  }));
  return specs;
}

}  // namespace

const std::vector<std::string>& framework_command_names() {
  // The magic:: procedures of tcltk/toolkit.tcl that scripts call (its
  // private helpers move_forward_by_width, get_and_move_inst,
  // create_new_pin, generate_layout_add, gencell_change_orig and
  // updateParamsScrollRegion are not commands), in toolkit.tcl order.
  static const std::vector<std::string> kNames = {
      "add_toolkit_menu", "add_toolkit_button", "add_toolkit_command", "add_toolkit_separator",
      "netlist_to_layout", "gencell", "gencell_makecell", "gencell_getparams", "gencell_setparams",
      "gencell_change", "get_gencell_name", "get_gencell_hash", "gencell_create", "add_entry",
      "add_check_callbacks", "add_dependency", "update_dialog", "add_checkbox", "add_message",
      "add_selectlist", "add_selectindex", "gencell_defaults", "gencell_update", "gencell_dialog",
      "murmur3_32", "normalize_value"};
  return kNames;
}

bool register_pcell_commands(cmd::CommandRegistry& registry) {
  bool ok = true;
  for (auto& spec : framework_commands()) {
    ok = !registry.add(std::move(spec), cmd::kScriptClient).has_value() && ok;
  }
  return ok;
}

}  // namespace hefesto::pcell
