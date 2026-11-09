// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit-free view models of the Desktop-Layout screen
// (docs/design/mockups/Desktop-Layout.dc.html; capability hefesto-app-ux
// "Main window layout"; hefesto-design-system "Layer colors come from the
// PDK, not the theme", "Status is never color-only", "Typography"). The Raven
// layer only draws what these functions return, so the content of every
// panel is unit-tested without a window.
//
// Owner: SHELL (openspec/changes/m1-core-viewer/design.md, D2 and D6).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/shell/design_tokens.hpp"
#include "hefesto/shell/theme_tokens.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::shell {

// --- units -----------------------------------------------------------------

/// How internal database units are shown. `internal` internal units equal
/// `lambda` lambda (tech::GridScale). Microns are shown when the technology
/// gives a micron scale; until then (M1: no cifoutput scale) values are in
/// lambda and the status bar says "units: lambda".
struct UnitScale {
  std::int64_t internal = 1;
  std::int64_t lambda = 1;
  std::optional<double> microns_per_lambda;

  [[nodiscard]] double to_display(double internal_units) const noexcept;
  [[nodiscard]] std::string_view unit_name() const noexcept { return microns_per_lambda ? "um" : "lambda"; }
};

/// Microns per lambda from the technology's first `cifoutput` style
/// (Magic's CIFGetOutputScale: scalefactor / (100 × expander), the scale
/// factor being in centimicrons); nullopt without a cifoutput style.
[[nodiscard]] std::optional<double> microns_per_lambda(const std::optional<tech::CifGridInfo>& cif);

/// A display-unit value: microns with 3 decimals ("1.165"), lambda with up
/// to 2 decimals and no trailing zeros ("12", "0.5").
[[nodiscard]] std::string format_length(double internal_units, const UnitScale& scale);

// --- text fitting --------------------------------------------------------------

/// The width of a text in logical units (a painter's or runtime's text_width).
using TextMeasure = std::function<float(std::string_view)>;

/// `text` when it fits `max_width`; otherwise its longest prefix (whole UTF-8
/// code points) followed by "…" that fits, or "…" alone.
[[nodiscard]] std::string ellipsize(std::string_view text, float max_width, const TextMeasure& measure);

/// Like ellipsize, but the "…" replaces the middle: about a third of what
/// fits is kept from the start and the rest from the end, so names sharing a
/// long prefix ("sky130_fd_sc_hd__nand2_1", "…__nor2_1") stay distinct.
[[nodiscard]] std::string ellipsize_middle(std::string_view text, float max_width, const TextMeasure& measure);

/// `text` broken into lines of at most `columns` code points, after a space
/// or a '/' when one falls in the second half of the line, else hard;
/// continuation lines start with `indent` spaces. Embedded LFs start new lines.
[[nodiscard]] std::vector<std::string> wrap_columns(std::string_view text, std::size_t columns, std::size_t indent);
/// `text` broken into lines no wider than `max_width` (same break rules).
[[nodiscard]] std::vector<std::string> wrap_to_width(std::string_view text, float max_width, const TextMeasure& measure);

// --- accessibility -------------------------------------------------------------

/// A toggle's accessible name with its state ("Box tool, pressed"): Raven
/// 0.2.2 has no accessible-state API, so the state goes in the name.
[[nodiscard]] std::string toggle_accessible_name(std::string_view label, bool pressed);
/// A checkbox's accessible name with its state ("Accept the PDK license, checked").
[[nodiscard]] std::string checkbox_accessible_name(std::string_view label, bool checked);

/// What a screen reader is told of a problem: errors interrupt (assertive),
/// warnings wait (polite), info is not announced.
struct Announcement {
  std::string text;
  bool assertive = false;
};
[[nodiscard]] std::optional<Announcement> problem_announcement(StatusKind kind, std::string_view status);

// --- status chips ------------------------------------------------------------

/// A status indicator: shape + text + color, never color only.
struct StatusChip {
  StatusKind kind = StatusKind::info;
  std::string text;

  /// "● DRC clean": the shape glyph, a space, the text.
  [[nodiscard]] std::string label() const;
};

/// M1 has no DRC (M3) or LVS (M9): the chips say so with the info shape.
[[nodiscard]] StatusChip drc_chip_m1();
[[nodiscard]] StatusChip lvs_chip_m1();

/// The shape for a diagnostic severity word ("error", "warning", "info", "note").
[[nodiscard]] StatusKind status_for_severity(std::string_view severity) noexcept;

/// A diagnostics-panel line: "<shape> <formatted diagnostic>".
[[nodiscard]] std::string diagnostic_line(StatusKind kind, std::string_view text);

/// A status-bar message with its shape.
struct LineStatus {
  StatusKind kind = StatusKind::info;
  std::string text;  // "<shape> <text>"
};

/// The status-bar message of a command line, so that a raised problem reads
/// the same whichever way the line ended ("■ error: CODE: text"): its first
/// error problem, or its first warning when the line succeeded; else the
/// result ("■ <message>" for a failure, "● <result>" for success).
[[nodiscard]] LineStatus command_line_status(bool ok, std::string_view result,
                                             const std::optional<LineStatus>& problem);

/// Problems reported outside the command bar (style loading, startup, the
/// canvas): counted by severity so the status bar can show them, since the
/// Diagnostics tab may be collapsed ("Status is never color-only").
class DiagnosticTally {
 public:
  /// Records one diagnostic; returns the status-bar message for it
  /// ("▲ text" / "■ text") or an empty string for info.
  std::string note(StatusKind kind, std::string_view text);
  /// "■ 1 error", "■ 2 errors · ▲ 1 warning", "▲ 3 warnings"; nullopt when clean.
  [[nodiscard]] std::optional<StatusChip> chip() const;
  [[nodiscard]] int errors() const noexcept { return errors_; }
  [[nodiscard]] int warnings() const noexcept { return warnings_; }
  void clear() noexcept { errors_ = warnings_ = 0; }

 private:
  int errors_ = 0;
  int warnings_ = 0;
};

/// The window's sink on the core Dispatcher: every diagnostic becomes a
/// Diagnostics-panel line with its shape, and warnings and errors are
/// counted and turned into a status-bar message, so a failure outside the
/// command bar is never invisible.
class ProblemSink final : public diag::DiagnosticSink {
 public:
  /// `panel_line`: "<shape> <formatted diagnostic>"; `status`: the status-bar
  /// message, empty for info.
  using Listener = std::function<void(StatusKind kind, const std::string& panel_line, const std::string& status)>;
  explicit ProblemSink(Listener listener) : listener_(std::move(listener)) {}
  void emit(const diag::Diagnostic& diagnostic) override;
  [[nodiscard]] const DiagnosticTally& tally() const noexcept { return tally_; }

 private:
  Listener listener_;
  DiagnosticTally tally_;
};

// --- top bar -------------------------------------------------------------------

struct HeaderInput {
  std::string cell;                      // edit or root cell; empty: none loaded
  std::string technology;                // tech name ("minimum" before a PDK)
  std::optional<std::string> pdk;        // variant, e.g. "sky130A"
  std::optional<std::string> pdk_version;  // short form, e.g. "1.0.471" or "abcdef1"
  bool magic_compat = true;
};

struct HeaderText {
  std::string title;     // "inverter_demo" or "No cell loaded"
  std::string subtitle;  // "sky130A · 1.0.471 · magic-compat on"
};

[[nodiscard]] HeaderText header_text(const HeaderInput& input);

/// The short PDK version shown in the top bar from a technology's
/// `version` statements: the last word of the first statement, cut before
/// the first '-' ("1.0.493-0-g0fe599b" -> "1.0.493"); nullopt when absent.
[[nodiscard]] std::optional<std::string> short_tech_version(const std::vector<std::string>& version_lines);

/// The disabled controls of later milestones keep the mockup's structure;
/// each tooltip names the milestone.
struct DeferredControl {
  std::string_view label;
  std::string_view tooltip;
};
[[nodiscard]] const std::vector<DeferredControl>& deferred_view_tabs();  // Schematic, 3D, Waveforms
[[nodiscard]] const std::vector<DeferredControl>& deferred_actions();    // Extract, Simulate, Ask agent

// --- tool rail -------------------------------------------------------------------

struct ToolButton {
  std::string_view icon;   // icon id in assets/icons/<icon>.svg
  std::string_view label;  // accessible name ("Box tool")
  bool enabled = false;
  std::string_view tooltip;
};

/// The tool rail of the mockup, top to bottom. M1 enables the box tool
/// (active) and zoom to fit; the editing tools arrive in M2.
[[nodiscard]] const std::vector<ToolButton>& tool_rail();

// --- layer palette ------------------------------------------------------------------

struct LayerRow {
  tech::TileType type = tech::kSpace;
  std::string name;
  std::string plane;
  bool visible = true;
  bool contact = false;
  Rgba fill;      // PDK color at the style's opacity (never a theme color)
  Rgba outline;   // PDK color, opaque
  bool stippled = false;
  bool crossed = false;   // contact-style cross
  bool dashed = false;    // outline-only style (wells)

  /// The meta text at the right of the row: the plane, or "hidden".
  [[nodiscard]] std::string meta() const { return visible ? plane : std::string("hidden"); }
  /// "metal1, plane metal1, visible" (screen readers; hefesto-app-ux "VoiceOver on layer palette").
  [[nodiscard]] std::string accessible_name() const;
  /// The command a visibility toggle issues: "see metal1" or "see no metal1".
  [[nodiscard]] std::string toggle_command() const;
};

/// Rows for the palette from display::layer_palette(), in technology order,
/// filtered by a case-insensitive substring of the name or plane.
[[nodiscard]] std::vector<LayerRow> layer_rows(const std::vector<display::LayerSwatch>& swatches,
                                               const tech::TypeMask& visible, std::string_view filter);

// --- hierarchy ------------------------------------------------------------------------

struct HierarchyRow {
  int depth = 0;
  std::string text;               // "▾ inverter_demo", "▸ nfet_01v8 (X1)", "via1_2x1 ×4"
  bool expandable = false;        // instances whose contents can be shown (not the root)
  bool expanded = false;          // a group: every instance is expanded
  const db::CellUse* use = nullptr;  // null for the root and for grouped rows
  std::vector<db::Rect> boxes;    // each instance's bounding box in root coordinates
  std::string suffix;             // the tail of `text` that tells rows apart: " ×4 · 1 open", " (X1) [4×2]"
  bool partial = false;           // a group with some, not all, instances expanded
  /// The direct children of the row's instance in root coordinates (for an
  /// array: of its first element), unexpanded after an expansion so that a
  /// row opens one level only.
  std::vector<db::Rect> child_boxes;

  /// `text` without `suffix` (the marker and the cell name).
  [[nodiscard]] std::string_view name_part() const;
  /// `text` plus its state: ", expanded", ", collapsed" or ", partly expanded".
  [[nodiscard]] std::string accessible_name() const;

  /// The commands that toggle the row's instances through the Dispatcher
  /// (Magic's `expand` / `unexpand` act on the box): for each instance the
  /// box is set to it (internal units) and `expand surround` or
  /// `unexpand surround` acts on it. `expand surround` expands every
  /// instance inside the box at any depth (DBExpandAll), so an expansion is
  /// followed by `unexpand surround` on each direct child's box: the row
  /// opens one level, as Magic's `expand toggle` of a selected cell does.
  /// `restore_box` (internal units), when given, is set back at the end.
  /// Empty for rows that cannot expand.
  [[nodiscard]] std::string toggle_command(const std::optional<db::Rect>& restore_box = std::nullopt) const;
};

/// How many leading top-bar groups to hide so the bar fits `available`
/// logical pixels: `full_width` is the bar with every group shown and
/// `group_widths` the widths the groups free, in the order they give way
/// (the disabled deferred actions, the DRC/LVS chips, the view tabs). Open
/// and PDKs are never hidden (hefesto-design-system "Screen patterns follow
/// the reference mockups", minimum window size).
[[nodiscard]] std::size_t top_bar_hidden_groups(float available, float full_width,
                                                const std::vector<float>& group_widths);

/// How a hierarchy row is drawn in `width` (after its indent): the name part,
/// ellipsized in the middle so that the suffix always shows right after it.
struct HierarchyRowLayout {
  std::string name;  // name_part(), possibly ellipsized
  float suffix_x = 0.0F;  // where the suffix starts, relative to the name
  std::string suffix;     // the suffix, ellipsized only when it is wider than half the row
};
[[nodiscard]] HierarchyRowLayout hierarchy_row_layout(const HierarchyRow& row, float width, const TextMeasure& measure);

using ExpandedTest = std::function<bool(const db::CellUse&)>;

/// The tree of `root`: the root row, then its instances depth-first. Leaf
/// instances of the same definition are grouped into one "<def> ×N" row (as
/// the mockup does); arrays show "[cols×rows]". Every instance row can be
/// expanded (▸ / ▾: an unexpanded instance shows only its bounding box);
/// children of an instance are listed when `expanded(use)` is true. At most
/// `max_rows` rows.
[[nodiscard]] std::vector<HierarchyRow> hierarchy_rows(const db::CellDef& root, const ExpandedTest& expanded,
                                                       std::size_t max_rows = 2000);

/// The Layers panel's text when it has no row: no technology (`technology`
/// empty or "minimum"), a technology without display styles, or a filter
/// that matches nothing.
[[nodiscard]] std::string layer_empty_hint(std::string_view technology, bool has_styles);

// --- inspector ------------------------------------------------------------------------

struct InspectorRow {
  std::string key;
  std::string value;  // JetBrains Mono
};

struct InspectorView {
  std::string section;   // "BOX", "CELL" or "SELECTION"
  std::string title;
  std::string subtitle;
  std::vector<InspectorRow> rows;
};

/// The box as the edit cell's window shows it. As in Magic, the box belongs to
/// one root cell (`box_root`; none means the edit cell) and is not shown in a
/// window on another cell, so after `load` of a new cell the old box is hidden.
[[nodiscard]] std::optional<db::Rect> visible_box(const std::optional<db::Rect>& box, const db::CellDef* box_root,
                                                  const db::CellDef* edit_cell);

/// M1 inspects the box (or, without a box, the edit cell); selection is M2.
[[nodiscard]] InspectorView inspector_view(const std::optional<db::Rect>& box, const db::CellDef* cell,
                                           const UnitScale& scale);

// --- status bar -------------------------------------------------------------------------

struct StatusBarInput {
  std::optional<db::Point> cursor;  // internal units
  std::optional<db::Rect> box;
  std::string edit_cell;
  UnitScale scale;
  std::string snap = "lambda";  // the `snap` command's mode
  StatusChip drc = drc_chip_m1();
  std::string message;  // the last result or problem, first line, with its shape
  std::optional<StatusChip> problems;  // DiagnosticTally::chip()
};

struct StatusBarText {
  std::string cursor;  // "x 1.165  y 1.480 um"
  std::string box;     // "box 0.170 × 1.650"
  std::string edit;    // "edit: inverter_demo"
  std::string snap;    // "snap: lambda · units: um"
  std::string drc;     // "● DRC idle · 0 errors"
  std::string problems;  // "■ 1 error" (empty when clean)
  std::string message; // first line of the last result, at most 96 characters
};

[[nodiscard]] StatusBarText status_bar_text(const StatusBarInput& input);

/// The status bar's accessible name: "Status: <message>. <problems>. <drc>"
/// (the painted text, read by screen readers).
[[nodiscard]] std::string status_bar_accessible_name(const StatusBarText& text);

}  // namespace hefesto::shell
