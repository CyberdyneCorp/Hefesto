// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl-visible GUI manager commands (see commands.hpp). The managers
// have no widgets in the core: `cellmanager`, `libmanager`, `drcmanager`,
// `techmanager` and `make_texthelper` accept Magic's options and install
// their scripts' tags on `create`; SHELL observes the commands and renders
// the models of managers.hpp.
// (Magic ref: tcltk/wrapper.tcl, cellmgr.tcl, libmgr.tcl, drcmgr.tcl,
// texthelper.tcl, toolbar.tcl)

#include "hefesto/gui/commands.hpp"

#include "gui/gui_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/diag_codes.hpp"
#include "hefesto/gui/drc_report.hpp"
#include "hefesto/gui/managers.hpp"
#include "hefesto/gui/prompts.hpp"
#include "hefesto/gui/tk_model.hpp"
#include "hefesto/gui/toolbar_presets.hpp"
#include "hefesto/gui/wrapper.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::gui {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

const std::string& arg(const Invocation& inv, std::size_t i) {
  static const std::string kEmpty;
  return i < inv.args.size() ? inv.args[i] : kEmpty;
}

CommandResult cmd_drc_save_report(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() > 2) {
    return detail::wrong_args("magic::drc_save_report ?cellname? ?outfile?");
  }
  return drc_save_report(ctx, arg(inv, 0), arg(inv, 1));
}

CommandResult cmd_drc_load_report(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() > 1) {
    return detail::wrong_args("magic::drc_load_report ?drc_file?");
  }
  return drc_load_report(ctx, arg(inv, 0));
}

CommandResult cmd_openwrapper(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() > 2) {
    return detail::wrong_args("magic::openwrapper ?cell? ?framename?");
  }
  return open_wrapper(ctx, arg(inv, 0), arg(inv, 1));
}

CommandResult cmd_closewrapper(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return detail::wrong_args("magic::closewrapper framename");
  }
  return close_wrapper(ctx, inv.args[0]);
}

// hefesto-cli's headless widget model (wrapper mode): the manager's window.
CommandResult manager_widgets(CommandContext& ctx, std::optional<ManagerWindow> window, const std::string& option) {
  script::Interpreter* interp = ctx.editor() != nullptr ? ctx.editor()->script.interp.get() : nullptr;
  if (!window || interp == nullptr || !has_tk_model(*interp)) {
    return CommandResult::success();
  }
  CommandResult r = update_manager_widgets(ctx, *interp, *window, option);
  if (!r.ok()) {
    const std::string text = r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text;
    return CommandResult::raised(diag::make(codes::kTkError, text));
  }
  return CommandResult::success();
}

// `<manager> ?option?`: `create` installs the tags of the manager's script
// once (Magic installs them when the script is sourced); every option is
// otherwise answered by SHELL, which renders the model (techmanager
// initall included: Q-SCRIPT-33).
CommandResult manager(CommandContext& ctx, const Invocation& inv, std::string_view usage,
                      std::optional<GuiScript> script, std::optional<ManagerWindow> window = std::nullopt) {
  if (inv.args.size() > 1) {
    return detail::wrong_args(usage);
  }
  if (script && arg(inv, 0) == "create" && ctx.editor() != nullptr) {
    script::TagTable& tags = ctx.editor()->script.tags;
    const auto& first = gui_script_tags(*script).front();
    const auto existing = tags.get(first.first);
    if (!existing || existing->find(first.second) == std::string::npos) {
      install_gui_tags(tags, *script);
    }
  }
  return manager_widgets(ctx, window, inv.args.empty() ? std::string{"update"} : inv.args[0]);
}

// texthelper.tcl's `proc magic::make_texthelper { mgrpath }`: one argument.
CommandResult make_texthelper(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return detail::wrong_args("magic::make_texthelper mgrpath");
  }
  return manager(ctx, inv, "magic::make_texthelper mgrpath", GuiScript::text_helper);
}

std::string opt(CommandContext& ctx, std::string_view name) {
  const script::Interpreter* interp = ctx.editor() != nullptr ? ctx.editor()->script.interp.get() : nullptr;
  const auto value = interp != nullptr ? interp->get_var(name) : std::nullopt;
  return value ? value->to_string() : std::string{};
}

CommandResult cmd_maketoolbar(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return detail::wrong_args("magic::maketoolbar framename");
  }
  if (ctx.editor() == nullptr) {
    return detail::no_session(ctx);
  }
  ToolbarOptions options;
  options.hide_special = opt(ctx, "Opts(hidespecial)") == "1";
  options.hide_locked = opt(ctx, "Opts(hidelocked)") == "1";
  script::Interpreter* interp = ctx.editor()->script.interp.get();
  if (interp != nullptr && has_tk_model(*interp)) {
    CommandResult built = build_toolbar_widgets(*interp, inv.args[0], [&] { return build_toolbar(ctx, options); });
    if (!built.ok()) {  // a Tcl error of the procedure, as Tk's
      const std::string text = built.diagnostics.empty() ? std::string{} : built.diagnostics.front().text;
      return CommandResult::raised(diag::make(codes::kTkError, text));
    }
    return built;
  }
  (void)build_toolbar(ctx, options);
  return CommandResult::success();
}

// The front end's platform::FilePicker (editor::EditorState::file_picker);
// without one the prompts behave as a cancelled dialog.
platform::FilePicker* session_picker(CommandContext& ctx) {
  return ctx.editor() != nullptr ? ctx.editor()->file_picker : nullptr;
}

CommandResult cmd_promptload(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return detail::wrong_args("magic::promptload type");
  }
  return prompt_load(ctx, session_picker(ctx), inv.args[0]);
}

CommandResult cmd_promptsave(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return detail::wrong_args("magic::promptsave type");
  }
  return prompt_save(ctx, session_picker(ctx), inv.args[0]);
}

CommandResult cmd_saveallandquit(CommandContext& ctx, const Invocation& inv) {
  if (!inv.args.empty()) {
    return detail::wrong_args("magic::saveallandquit");
  }
  return save_all_and_quit(ctx, session_picker(ctx));
}

}  // namespace

const std::vector<std::string>& gui_entry_command_names() {
  // wrapper.tcl (openwrapper, closewrapper, techmanager, promptload,
  // promptsave), cellmgr.tcl, libmgr.tcl, drcmgr.tcl, texthelper.tcl,
  // toolbar.tcl.
  static const std::vector<std::string> kNames = {
      "openwrapper", "closewrapper", "techmanager", "promptload", "promptsave", "cellmanager",
      "libmanager",  "drcmanager",   "make_texthelper", "maketoolbar"};
  return kNames;
}

bool register_gui_commands(cmd::CommandRegistry& registry) {
  bool ok = true;
  const auto add = [&](std::string name, std::string synopsis, std::string help, cmd::Handler handler) {
    // Tcl procedures in Magic: exact-name table cmd::kScriptClient.
    ok = !registry.add({std::move(name), std::move(synopsis), std::move(help), std::move(handler)}, cmd::kScriptClient)
              .has_value() &&
         ok;
  };
  const auto managed = [](std::string usage, std::optional<GuiScript> script,
                          std::optional<ManagerWindow> window = std::nullopt) {
    return [usage = std::move(usage), script, window](CommandContext& ctx, const Invocation& inv) {
      return manager(ctx, inv, usage, script, window);
    };
  };
  add("drc_save_report", "drc_save_report [<cell> [<file>]]", "Write the DRC errors of a cell to a report file.",
      cmd_drc_save_report);
  add("drc_load_report", "drc_load_report [<file>]", "Read a DRC report file as feedback areas.",
      cmd_drc_load_report);
  add("openwrapper", "openwrapper [<cell> [<frame>]]", "Open a layout window in a wrapper frame.", cmd_openwrapper);
  add("closewrapper", "closewrapper <frame>", "Close a wrapper frame and its layout window.", cmd_closewrapper);
  add("techmanager", "techmanager [create|init|update]", "Technology manager.",
      managed("magic::techmanager ?option?", std::nullopt, ManagerWindow::technology));
  add("promptload", "promptload magic|gds|cif|getcell", "Load a file chosen in a file dialog.", cmd_promptload);
  add("promptsave", "promptsave magic|force|gds|cif", "Save to a file chosen in a file dialog.", cmd_promptsave);
  add("cellmanager", "cellmanager [create|update]", "Cell manager.",
      managed("magic::cellmanager ?option?", GuiScript::cell_manager, ManagerWindow::cell));
  add("libmanager", "libmanager [create|update]", "Library manager.",
      managed("magic::libmanager ?option?", GuiScript::library_manager, ManagerWindow::library));
  add("drcmanager", "drcmanager [create|update]", "DRC manager.",
      managed("magic::drcmanager ?option?", GuiScript::drc_manager));
  add("make_texthelper", "make_texthelper <window>", "Text helper.",
      make_texthelper);
  add("maketoolbar", "maketoolbar <frame>", "Rebuild the layer toolbar of a wrapper frame.", cmd_maketoolbar);
  add("saveallandquit", "saveallandquit", "Save every modified cell, then quit.", cmd_saveallandquit);
  return ok;
}

}  // namespace hefesto::gui
