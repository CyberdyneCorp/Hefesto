// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit-free view models of the M3 desktop DRC screens (DESIGN boards
// docs/design/mockups/M3-DRC-Status.dc.html and DRC-Browser.dc.html,
// approved 2026-11-10; openspec/changes/m3-drc/design.md D11):
//
//   - the top-bar chip and status-bar segment: not run, checking, clean,
//     N errors, off (precedence off > checking > not run > N errors >
//     clean), N being the root cell's `drc list count total`;
//   - the chip menu (Open DRC panel, Check now, Find next, Why, Background
//     checking, Style);
//   - the DRC results browser: why-text groups with their areas as virtual
//     rows (no widget per row), sorted by count or text, the ignored rules
//     apart, Prev/Next with `#k of n`, Re-check, Ignore, Clear ignores and
//     the style selector;
//   - the error-area inspector, the `drc why` popover and the
//     job-completion notice.
//
// Every action is the core command line it issues (a dispatcher command,
// logged and replayable); the Raven layer only shows what these functions
// return.
//
// Owner: SHELL (openspec/changes/m3-drc/design.md D2, D11).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/shell/screen_models.hpp"
#include "hefesto/shell/view_models.hpp"

namespace hefesto::shell {

// --- commands ------------------------------------------------------------------

/// Re-check and the chip's Check now (Magic's DRC manager selects the top
/// cell first).
inline constexpr std::string_view kDrcRecheckCommand = "select top cell; drc check; drc catchup";
/// The chip's Find next error (Magic's `!` key without its `zoom 16`).
inline constexpr std::string_view kDrcFindNextCommand = "drc find; findbox zoom";
inline constexpr std::string_view kDrcWhyCommand = "drc why";
inline constexpr std::string_view kDrcClearIgnoresCommand = "drc ignore none";
inline constexpr std::string_view kDrcZoomCommand = "findbox zoom";
inline constexpr std::string_view kDrcOpenPanelCommand = "panel drc show";

/// `text` as one word of a command line: unchanged when it has no space or
/// special character, else in braces (or double quotes with escapes when
/// its braces do not balance).
[[nodiscard]] std::string command_word(std::string_view text);

/// `drc ignore <text>`.
[[nodiscard]] std::string drc_ignore_command(std::string_view text);
/// `drc style <name>`.
[[nodiscard]] std::string drc_style_command(std::string_view name);
/// The rule tag in the last parentheses of a why-text ("met1.2" for
/// "Metal1 spacing < 0.14um (met1.2)"); the whole text when it has none.
[[nodiscard]] std::string drc_rule_tag(std::string_view why_text);
/// "12,408": a count with thousands separators.
[[nodiscard]] std::string group_thousands(std::size_t n);

// --- status chip and status bar ------------------------------------------------------

enum class DrcState : std::uint8_t { not_run, checking, clean, errors, off };

struct DrcStatusInput {
  std::string cell;             // the window's root cell (empty: none open)
  bool has_rules = false;       // the technology has a DRC style with rules
  bool background_on = true;    // `drc status` (DRCBackGround on)
  bool busy = false;            // drc::DrcEvents busy
  bool checked = false;         // the root was checked since it was opened, or holds error tiles
  std::size_t error_tiles = 0;  // the root's `drc list count total`
  std::string style;            // the current DRC style
};

/// What the chip and the status-bar segment show for one state.
struct DrcStatusView {
  DrcState state = DrcState::not_run;
  StatusChip chip;        // top bar: "■ 12 DRC errors"
  StatusChip status_bar;  // status bar: "■ DRC 12 errors · drc(full)"
  std::string accessible_name;  // "DRC: 12 errors, style drc(full). Open the DRC panel"
  std::string tooltip;          // "12 error tiles in ota_5t · style drc(full) · ! next error"
  std::string announcement;     // read politely when the state changes

  friend bool operator==(const DrcStatusView&, const DrcStatusView&) = default;
};

/// Precedence off > checking > not run > N errors > clean.
[[nodiscard]] DrcState drc_state(const DrcStatusInput& in) noexcept;
[[nodiscard]] DrcStatusView drc_status_view(const DrcStatusInput& in);

struct DrcMenuInput {
  bool has_root = false;
  bool busy = false;
  bool background_on = true;
  std::vector<std::string> styles;  // `drc listall style`, tech-file order
  std::string style;                // the current one
};

/// The chip's menu (M3-DRC-Status board, tile 2). Raven 0.2.2 menus have no
/// submenus: the styles follow as checked "Style: <name>" entries.
[[nodiscard]] std::vector<MenuEntry> drc_chip_menu(const DrcMenuInput& in);

// --- stepping (#k of n) ---------------------------------------------------------------

/// Prev/Next with an explicit index (Q-DRC-15: the first Next goes to #1
/// under magic-compat too) wrapping n -> 1 and 1 -> n; `k` from the last
/// `drc find`, `n` the error tiles.
struct DrcStepper {
  std::optional<std::int64_t> k;
  std::size_t n = 0;

  [[nodiscard]] bool enabled() const noexcept { return n > 0; }
  /// "#3 of 12", "– of 12" before the first find, "0 errors" when n is 0.
  [[nodiscard]] std::string label() const;
  [[nodiscard]] std::string accessible_name() const;
  /// `drc find <k+1>; findbox zoom` (wrapping); `drc find 1; …` before any find.
  [[nodiscard]] std::string next_command() const;
  [[nodiscard]] std::string prev_command() const;
};

/// What the last `drc find` of a command's printed output reported: the
/// number of its last `Error area #N:` line and the why-texts after it.
struct DrcFindReport {
  std::int64_t index = 0;
  std::vector<std::string> whys;

  friend bool operator==(const DrcFindReport&, const DrcFindReport&) = default;
};
[[nodiscard]] std::optional<DrcFindReport> parse_find_report(const std::vector<std::string>& printed);

/// True when `words` (a command name and its arguments) is a `drc` command
/// whose option is `option` ("find", "why", "style", ...), in any list mode.
[[nodiscard]] bool is_drc_option(std::string_view name, const std::vector<std::string>& args, std::string_view option,
                                 bool allow_list = false);

// --- browser ----------------------------------------------------------------------------

/// One why-text group of `drc listall why` over the root cell.
struct DrcGroup {
  std::string text;
  std::vector<db::Rect> areas;  // root coordinates, discovery order
  bool ignored = false;         // its rule is on the `drc ignore` list

  friend bool operator==(const DrcGroup&, const DrcGroup&) = default;
};

enum class DrcSortKey : std::uint8_t { count, text };

enum class DrcRowKind : std::uint8_t { group, area, ignored_heading, ignored_group };

struct DrcRow {
  DrcRowKind kind = DrcRowKind::group;
  std::size_t group = 0;  // index into groups()
  std::size_t area = 0;   // area rows: index into that group's areas
};

/// What the browser body shows (board tiles "Empty · not checked", "Empty ·
/// clean", "Running", "Stale" and the populated list).
enum class DrcBrowserState : std::uint8_t { not_checked, clean, running, stale, populated };

/// The DRC results browser: the list of `drc listall why` as virtual rows.
/// Groups are collapsed until toggled; the ignored groups follow the others
/// under an "Ignored" heading.
class DrcBrowserModel {
 public:
  static constexpr int kColumnCount = 2;  // why-text (or area), areas

  /// A new list (the expansion of groups with the same text is kept).
  void set_list(std::vector<DrcGroup> groups);
  void clear_list();
  [[nodiscard]] bool has_list() const noexcept { return has_list_; }
  [[nodiscard]] const std::vector<DrcGroup>& groups() const noexcept { return groups_; }

  void set_units(const UnitScale& scale) { scale_ = scale; }
  /// Default: by count, descending (Magic's order is unspecified).
  void sort(DrcSortKey key, bool descending);
  [[nodiscard]] DrcSortKey sort_key() const noexcept { return sort_key_; }
  [[nodiscard]] bool sort_descending() const noexcept { return descending_; }
  /// Keeps the groups whose text contains `text` (case-insensitive).
  void set_filter(std::string text);

  [[nodiscard]] std::size_t row_count() const noexcept { return rows_.size(); }
  [[nodiscard]] const DrcRow& row(std::size_t index) const { return rows_.at(index); }
  /// The text of a cell; empty out of range.
  [[nodiscard]] std::string cell_text(std::size_t row, int column) const;
  /// What a screen reader says for a row ("Metal1 spacing …, 9 areas, collapsed").
  [[nodiscard]] std::string row_accessible_name(std::size_t row) const;

  /// Expands or collapses a group row; false for other rows.
  bool toggle(std::size_t row);
  /// The command a row issues when activated: an area's `box values …;
  /// findbox zoom`, the heading's `drc ignore none`; nullopt for a group
  /// (activating it toggles it).
  [[nodiscard]] std::optional<std::string> command(std::size_t row) const;
  /// The first row overlapping `box`, for the highlight of the found error
  /// (board note 6): an area row of an expanded group, or the group row of a
  /// collapsed (or ignored) group one of whose areas overlaps it.
  [[nodiscard]] std::optional<std::size_t> row_at(const db::Rect& box) const;

  [[nodiscard]] std::size_t area_count() const noexcept;
  [[nodiscard]] std::size_t why_count() const noexcept;
  [[nodiscard]] std::size_t ignored_count() const noexcept;
  /// "■ 12 error tiles · 23 areas · 5 why-texts · 1 ignored".
  [[nodiscard]] std::string summary(std::size_t error_tiles) const;
  /// The summary's shape: ■ errors, ● checked and clean, ◆ no list yet.
  [[nodiscard]] StatusKind summary_kind(std::size_t error_tiles) const noexcept;

 private:
  void rebuild();
  [[nodiscard]] bool row_overlaps(const DrcRow& row, const db::Rect& box) const;
  [[nodiscard]] bool listed(std::size_t group, bool ignored) const;  // in its section and the filter
  [[nodiscard]] std::vector<std::size_t> ordered(bool ignored) const;

  std::vector<DrcGroup> groups_;
  std::vector<bool> expanded_;
  std::vector<DrcRow> rows_;
  UnitScale scale_;
  DrcSortKey sort_key_ = DrcSortKey::count;
  bool descending_ = true;
  std::string filter_;
  bool has_list_ = false;
};

/// The canvas tag on the found error tile (board note 6): "■ #3" and the
/// first why-text of the last `drc find`, beside the box.
struct DrcCanvasTag {
  std::string index;  // "#3"
  std::string why;    // the first why-text (empty when the find printed none)
  db::Rect area;      // the box the find left, root coordinates

  friend bool operator==(const DrcCanvasTag&, const DrcCanvasTag&) = default;
};
/// The tag while the box is still where the last `drc find` put it
/// (`found_box`); nullopt before any find or once the box has moved.
[[nodiscard]] std::optional<DrcCanvasTag> drc_canvas_tag(const std::optional<DrcFindReport>& found,
                                                         const std::optional<db::Rect>& found_box,
                                                         const std::optional<db::Rect>& box);

/// Precedence: running (checker busy) > not checked (no list, or DRC off
/// without one) > stale > clean (nothing but ignored groups) > populated.
[[nodiscard]] DrcBrowserState drc_browser_state(DrcState status, const DrcBrowserModel& list, bool stale) noexcept;

/// The body text of the empty, running and stale states.
struct DrcBrowserNotice {
  StatusKind kind = StatusKind::info;
  std::string title;
  std::string text;
};
[[nodiscard]] std::optional<DrcBrowserNotice> drc_browser_notice(DrcBrowserState state, std::string_view cell,
                                                                 std::string_view style);

/// "Check" before the root was checked, "Re-check" afterwards.
[[nodiscard]] std::string_view recheck_label(DrcBrowserState state) noexcept;

/// The style selector's menu: `drc listall style` entries, the current one checked.
[[nodiscard]] std::vector<MenuEntry> drc_style_menu(const std::vector<std::string>& styles, std::string_view current);

// --- inspector, why popover, notification --------------------------------------------

/// The error area under the box (board tile 8).
struct DrcInspectorInput {
  std::optional<DrcFindReport> found;  // the last `drc find`
  std::size_t error_tiles = 0;
  std::string cell;
  std::optional<db::Rect> box;  // root coordinates
  std::string style;
  UnitScale scale;
};

struct DrcInspectorView {
  std::string title;     // "DRC ERROR AREA #3 OF 12" / "DRC ERROR AREA"
  std::string headline;  // the first why-text, with its shape ("■ Metal1 …")
  std::vector<std::string> whys;  // the others
  std::vector<std::pair<std::string, std::string>> rows;  // Cell, Area, Size, Style
  std::string empty;     // shown when there is no area ("Use Next or click an area")
};
[[nodiscard]] DrcInspectorView drc_inspector_view(const DrcInspectorInput& in);

/// The later-milestone rows of the board (disabled, tooltip names the milestone).
struct DeferredRow {
  std::string label;
  std::string milestone;
  std::string tooltip;
};
[[nodiscard]] std::vector<DeferredRow> drc_deferred_rows();

/// The `drc why` popover at the box (board tile 4): one row per why-text in
/// order of discovery; "No errors found." in the clean case.
struct DrcWhyPopover {
  std::string title;
  std::vector<std::string> rows;
  bool clean = false;
};
/// From a plain `drc why` command and what it printed; nullopt for any other command.
[[nodiscard]] std::optional<DrcWhyPopover> drc_why_popover(std::string_view name, const std::vector<std::string>& args,
                                                           const std::vector<std::string>& printed);

/// The job-completion notice (board tile 5) for a check that ran at least
/// `kLongJobSeconds` while the window was not focused.
inline constexpr double kLongJobSeconds = 5.0;
struct DrcJobNotice {
  std::string title;  // "DRC finished · ota_5t"
  std::string body;   // "■ 12 errors · drc(full) · 48 s"
};
struct DrcJobInput {
  double seconds = 0.0;
  bool window_active = true;
  bool enabled = true;  // the `notifications` setting
  std::string cell;
  std::size_t error_tiles = 0;
  std::string style;
};
[[nodiscard]] std::optional<DrcJobNotice> drc_job_notice(const DrcJobInput& in);
/// The `notifications` setting's value ("off" turns the notices off; anything else or none: on).
[[nodiscard]] bool notifications_enabled(const std::optional<std::string>& setting) noexcept;

}  // namespace hefesto::shell
