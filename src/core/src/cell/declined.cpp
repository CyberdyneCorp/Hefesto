// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The declined read-only layout (hefesto/cell/declined.hpp;
// hefesto-security "Consent before implicit script execution").

#include "hefesto/cell/declined.hpp"

#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/pdk/tech_rc.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "io_session.hpp"

namespace hefesto::cell {

namespace {

constexpr std::string_view kNotLoaded = "SCRIPT-TRUST-006";

/// The definition's recorded path, the identity of a declined record.
platform::Path recorded_file(const db::CellDef& def) {
  return def.file_path();
}

/// Makes `pdk`'s technology current after its script ran: a PDK's rc file
/// loads it itself (`tech load`); otherwise technology auto-discovery loads
/// it (running a `<tech>.magicrc` it finds, the consent answer reused).
bool technology_current(detail::IoSession& s, const std::string& technology) {
  if (s.ed.technology->name() != technology && s.io.load_technology) {
    (void)s.io.load_technology(technology);
    s.io.tech = s.ed.technology.get();  // the reads below use the loaded one
  }
  return s.ed.technology->name() == technology;
}

/// Reads every cell declined with `pdk`'s variant again (flush, which reads
/// with the technology ignored: it is now the current one), then shows the
/// current window's root again so that it becomes the edit cell. When the
/// technology could not be loaded, the decline is restored and the cells
/// stay as they are.
cmd::CommandResult read_declined_again(detail::IoSession& s, const script::PdkIdentity& pdk) {
  if (!technology_current(s, pdk.variant)) {
    s.ed.script.pdk_trust.decline(pdk);
    return cmd::CommandResult::failure(diag::make(
        kNotLoaded, "The " + pdk.variant + " script ran but the technology is \"" + s.ed.technology->name() +
                        "\"; the cells opened without it stay read-only."));
  }
  for (db::CellDef* def : declined_definitions(s.ed, pdk.variant)) {
    s.session.declined.forget(*def);
    // Still no_edit: read_cell's writability check decides again when the
    // file is read; a cell whose file is gone stays read-only.
    (void)detail::flush_cell(s, *def, false);
  }
  const view::LayoutWindow* window = s.ed.windows.current();
  if (window != nullptr && window->root() != nullptr) {
    detail::show_in_window(s.ed, *window->root());
  }
  return cmd::CommandResult::success();
}

}  // namespace

void DeclinedCells::mark(const db::CellDef& def, std::string technology) {
  cells_[&def] = Record{std::move(technology), recorded_file(def)};
}

std::optional<std::string> DeclinedCells::technology_of(const db::CellDef& def) const {
  const auto found = cells_.find(&def);
  if (found == cells_.end() || found->second.file != recorded_file(def)) {
    return std::nullopt;
  }
  return found->second.technology;
}

std::vector<db::CellDef*> declined_definitions(const editor::EditorState& editor, const std::string& technology) {
  std::vector<db::CellDef*> out;
  const DeclinedCells& declined = editor.cell_session.declined;
  editor.cells.for_each([&](db::CellDef& def) {
    const auto tech = declined.technology_of(def);
    if (tech && (technology.empty() || *tech == technology)) {
      out.push_back(&def);
    }
  });
  return out;
}

bool is_declined(const editor::EditorState& editor, const db::CellDef& def) {
  return editor.cell_session.declined.contains(def) && editor.cells.find(def.name()) == &def;
}

cmd::CommandResult reopen_declined(cmd::CommandContext& ctx, const script::PdkIdentity& pdk,
                                   const platform::Path& script) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return cmd::CommandResult::failure(editor.error());
  }
  editor::EditorState& ed = *editor.value();
  ed.script.pdk_trust.forget_decline(pdk);
  cmd::CommandResult result = script::run_tech_magicrc(ed, pdk, script, nullptr);
  if (!result.ok()) {
    return result;  // declined again (SCRIPT-TRUST-001) or no interpreter
  }
  cmd::CommandResult reread = detail::with_session(ctx, [&](detail::IoSession& s) {
    return read_declined_again(s, pdk);
  });
  result.diagnostics.insert(result.diagnostics.end(), reread.diagnostics.begin(), reread.diagnostics.end());
  result.status = reread.status;
  return result;
}

}  // namespace hefesto::cell
