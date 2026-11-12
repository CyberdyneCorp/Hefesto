// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The parameter dialog procedures of toolkit.tcl on DialogModel (Magic ref:
// tcltk/toolkit.tcl:811-865,1285-1771): gencell_dialog, gencell_getparams,
// gencell_setparams, the add_* builders, add_dependency (Q-SCRIPT-25),
// add_check_callbacks, update_dialog and gencell_update. Where Tk raises an
// error because `.params` does not exist, the same error is raised here.
//
// Owner: PCELL (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <string>
#include <vector>

#include "hefesto/pcell/framework/dialog.hpp"
#include "pcell/framework/toolkit.hpp"

namespace hefesto::pcell::detail {

/// magic::gencell_dialog instname type library parameters.
[[nodiscard]] cmd::CommandResult open_dialog(Toolkit& tk, std::string instance, std::string type,
                                             std::string library, ParameterDict parameters);

/// magic::gencell_getparams: pname -> magic::<pname>_val of every entry,
/// checkbox and select widget in grid order; an error without a dialog.
[[nodiscard]] diag::Outcome<ParameterDict> dialog_parameters(Toolkit& tk);

/// magic::gencell_setparams: writes `parameters` into the dialog (no-op
/// without one); an error when a value widget's key is missing.
cmd::CommandResult push_dialog_parameters(Toolkit& tk, const ParameterDict& parameters);

/// The Apply / Okay buttons of an edit dialog for `instance`.
void set_edit_buttons(DialogModel& dialog, const std::string& instance, const std::string& type,
                      const std::string& library);

/// A builder (`add_entry` ...): `args` as given to the Tcl procedure.
[[nodiscard]] cmd::CommandResult add_widget(Toolkit& tk, WidgetKind kind, const std::vector<std::string>& args);

/// magic::add_dependency callback type library ?pname ...?
[[nodiscard]] cmd::CommandResult add_dependency(Toolkit& tk, const std::string& callback, const std::string& type,
                                                const std::string& library, std::vector<std::string> parameters);

/// magic::add_check_callbacks type library.
[[nodiscard]] cmd::CommandResult add_check_callbacks(Toolkit& tk, const std::string& type,
                                                     const std::string& library);

/// magic::update_dialog callback pname type library.
[[nodiscard]] cmd::CommandResult update_dialog(Toolkit& tk, const std::string& callback, const std::string& pname,
                                               std::string type, const std::string& library);

/// magic::gencell_update ?command?: the `select` tag.
[[nodiscard]] cmd::CommandResult gencell_update(Toolkit& tk, const std::string& command);

}  // namespace hefesto::pcell::detail
