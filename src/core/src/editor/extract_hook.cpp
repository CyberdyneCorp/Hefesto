// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The editor-side extraction-style hook (editor/extract_hook.hpp;
// openspec/changes/m7-extraction/design.md D7, D20). The order follows
// Magic's tech load (ExtTechFinal's micron conversion with the current
// cifoutput scale, then TechLoad's post-load ExtTechScale) and
// DBScaleEverything's ExtTechScale. Until EXTTECH lands load_styles()
// (tasks.md 4.1, 4.6) the table is empty and the current style null.
//
// Owner: INTEGRATION.

#include "hefesto/editor/extract_hook.hpp"

#include <memory>
#include <string_view>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::editor {

namespace {

// The technology's last `extract` section (a repeated section replaces the
// styles of the earlier one), or nullptr.
const tech::RawSection* last_extract_section(const tech::Technology& technology) {
  const tech::RawSection* found = nullptr;
  for (const tech::RawSection& raw : technology.raw_sections()) {
    if (raw.name == "extract") {
      found = &raw;
    }
  }
  return found;
}

}  // namespace

void install_extract_styles(EditorState& editor, diag::DiagnosticSink& sink) {
  extract::ExtractSession& session = editor.extract;
  session.styles = extract::StyleTable{};
  session.current.reset();
  session.overrides = extract::Overrides{};
  const tech::RawSection* section = last_extract_section(*editor.technology);
  if (section == nullptr) {
    return;
  }
  const compat::CompatSettings compat = editor.drc.compat_settings();
  session.styles = extract::load_styles(*section, *editor.technology, sink, compat);
  if (session.styles.names().empty()) {
    return;
  }
  const std::shared_ptr<const extract::ExtStyle> first = session.styles.select(session.styles.names().front());
  if (first == nullptr) {
    return;
  }
  extract::ExtStyle style = first->micron_convert(editor.mask.output_styles.current_style());
  // TechLoad's post-load ExtTechScale(DBLambda[0], DBLambda[1]): distances
  // grow by internal/lambda.
  if (editor.grid.internal != editor.grid.lambda) {
    style = style.rescale(static_cast<std::int32_t>(editor.grid.lambda), static_cast<std::int32_t>(editor.grid.internal),
                          compat);
  }
  session.current = std::make_shared<const extract::ExtStyle>(std::move(style));
}

void rescale_extract_style(EditorState& editor, std::int64_t n, std::int64_t d) {
  extract::ExtractSession& session = editor.extract;
  if (session.current == nullptr) {
    return;
  }
  const compat::CompatSettings compat = editor.drc.compat_settings();
  // The layout grows by n/d, so the style's distances do too:
  // ExtTechScale(d, n), as DBio.c pairs ExtTechScale(1, d) with
  // DBScaleEverything(d, 1).
  session.current = std::make_shared<const extract::ExtStyle>(
      session.current->rescale(static_cast<std::int32_t>(d), static_cast<std::int32_t>(n), compat));
  // The `halo` and `stepsize` overrides scale with the style (Q-EXTRACT-25);
  // EXTCMD applies its rounding rule (Q-EXTRACT-19) when it lands them.
}

}  // namespace hefesto::editor
