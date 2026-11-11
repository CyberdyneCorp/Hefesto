// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/drc_models.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <utility>

namespace hefesto::shell {

namespace {

constexpr std::string_view kSpecial = " \t\n\"\\{}[]$;";

bool balanced_braces(std::string_view text) {
  int depth = 0;
  for (const char c : text) {
    depth += c == '{' ? 1 : (c == '}' ? -1 : 0);
    if (depth < 0) return false;
  }
  return depth == 0 && (text.empty() || text.back() != '\\');
}

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::string plural(std::size_t n, std::string_view one, std::string_view many) {
  return group_thousands(n) + " " + std::string(n == 1 ? one : many);
}

/// `index` wrapped into 1..n.
std::int64_t wrap(std::int64_t index, std::size_t n) {
  const auto count = static_cast<std::int64_t>(n);
  if (index < 1) return count;
  if (index > count) return 1;
  return index;
}

std::string internal_word(db::Coord v) {
  return std::to_string(v) + "i";
}

}  // namespace

std::string command_word(std::string_view text) {
  if (!text.empty() && text.find_first_of(kSpecial) == std::string_view::npos) return std::string(text);
  if (balanced_braces(text)) return "{" + std::string(text) + "}";
  std::string out = "\"";
  for (const char c : text) {
    if (c == '"' || c == '\\' || c == '$' || c == '[' || c == ']') out += '\\';
    out += c;
  }
  return out + "\"";
}

std::string drc_ignore_command(std::string_view text) {
  return "drc ignore " + command_word(text);
}

std::string drc_style_command(std::string_view name) {
  return "drc style " + command_word(name);
}

std::string drc_rule_tag(std::string_view why_text) {
  const std::size_t close = why_text.rfind(')');
  const std::size_t open = close == std::string_view::npos ? close : why_text.rfind('(', close);
  if (open == std::string_view::npos || close <= open + 1) return std::string(why_text);
  return std::string(why_text.substr(open + 1, close - open - 1));
}

std::string group_thousands(std::size_t n) {
  std::string digits = std::to_string(n);
  for (std::size_t i = digits.size(); i > 3; i -= 3) digits.insert(i - 3, ",");
  return digits;
}

// --- status ---------------------------------------------------------------------------

DrcState drc_state(const DrcStatusInput& in) noexcept {
  if (!in.background_on) return DrcState::off;
  if (in.busy) return DrcState::checking;
  if (in.cell.empty() || !in.has_rules || !in.checked) return DrcState::not_run;
  return in.error_tiles > 0 ? DrcState::errors : DrcState::clean;
}

namespace {

// ", style X" / " · style X" / " · X" after a status text, or nothing without a style.
std::string with_style(const std::string& style, std::string_view separator) {
  return style.empty() ? std::string() : std::string(separator) + style;
}

void describe_off(const DrcStatusInput& in, const std::string& errors, DrcStatusView& v) {
  v.chip = {StatusKind::info, "DRC off"};
  v.status_bar = {StatusKind::info, in.checked ? "DRC off · " + errors + ", last check" : "DRC off"};
  v.accessible_name = "DRC: background checking off. Open the DRC panel";
  v.tooltip = "Background DRC is off (drc off): painted areas are not marked for checking; Check now runs drc check; drc catchup";
}

void describe_checking(const DrcStatusInput& in, DrcStatusView& v) {
  v.chip = {StatusKind::info, "DRC checking…"};
  v.status_bar = {StatusKind::info, "DRC checking…" + with_style(in.style, " · ")};
  v.accessible_name = "DRC: checking" + with_style(in.style, ", style ") + ". Open the DRC panel";
  v.tooltip = "The background checker is busy" + with_style(in.cell, " in ");
}

void describe_not_run(const DrcStatusInput& in, DrcStatusView& v) {
  v.chip = {StatusKind::info, "DRC not run"};
  v.status_bar = {StatusKind::info, "DRC not run"};
  v.accessible_name = "DRC: not run. Open the DRC panel";
  if (in.cell.empty()) {
    v.tooltip = "No cell open";
  } else if (!in.has_rules) {
    v.tooltip = "The technology has no DRC rules";
  } else {
    v.tooltip = in.cell + " has not been checked since it was opened · Check now";
  }
}

void describe_errors(const DrcStatusInput& in, const std::string& errors, DrcStatusView& v) {
  const std::string count = group_thousands(in.error_tiles);
  v.chip = {StatusKind::error, count + (in.error_tiles == 1 ? " DRC error" : " DRC errors")};
  v.status_bar = {StatusKind::error, "DRC " + errors + with_style(in.style, " · ")};
  v.accessible_name = "DRC: " + errors + with_style(in.style, ", style ") + ". Open the DRC panel";
  v.tooltip = plural(in.error_tiles, "error tile", "error tiles") + " in " + in.cell +
              with_style(in.style, " · style ") + " · ! next error";
}

void describe_clean(const DrcStatusInput& in, DrcStatusView& v) {
  v.chip = {StatusKind::success, "DRC clean"};
  v.status_bar = {StatusKind::success, "DRC 0 errors" + with_style(in.style, " · ")};
  v.accessible_name = "DRC: clean, 0 errors" + with_style(in.style, ", style ") + ". Open the DRC panel";
  v.tooltip = "No error tiles in " + in.cell + with_style(in.style, " · style ");
}

}  // namespace

DrcStatusView drc_status_view(const DrcStatusInput& in) {
  DrcStatusView v;
  v.state = drc_state(in);
  const std::string errors = plural(in.error_tiles, "error", "errors");
  switch (v.state) {
    case DrcState::off: describe_off(in, errors, v); break;
    case DrcState::checking: describe_checking(in, v); break;
    case DrcState::not_run: describe_not_run(in, v); break;
    case DrcState::errors: describe_errors(in, errors, v); break;
    case DrcState::clean: describe_clean(in, v); break;
  }
  v.announcement = v.status_bar.text;
  return v;
}

std::vector<MenuEntry> drc_chip_menu(const DrcMenuInput& in) {
  std::vector<MenuEntry> out;
  out.push_back({"Open DRC panel", std::string(kDrcOpenPanelCommand), false, false, true, false});
  out.push_back({"Check now      drc check; drc catchup", std::string(kDrcRecheckCommand), false, false,
                 in.has_root && !in.busy, false});
  out.push_back({"Find next error      !", std::string(kDrcFindNextCommand), false, false, in.has_root, false});
  out.push_back({"Why at the box      ?", std::string(kDrcWhyCommand), false, false, in.has_root, false});
  out.push_back({"Background checking      drc on / off", in.background_on ? "drc off" : "drc on", false,
                 in.background_on, true, true});
  bool first = true;
  for (const std::string& style : in.styles) {
    out.push_back({"Style: " + style, drc_style_command(style), false, style == in.style, !in.busy, first});
    first = false;
  }
  return out;
}

// --- stepping -------------------------------------------------------------------------

std::string DrcStepper::label() const {
  if (n == 0) return "0 errors";
  return (k ? "#" + std::to_string(*k) : std::string("–")) + " of " + group_thousands(n);
}

std::string DrcStepper::accessible_name() const {
  if (n == 0) return "No error tiles";
  return k ? "Error " + std::to_string(*k) + " of " + std::to_string(n) : "No error found yet, " + std::to_string(n) + " in all";
}

std::string DrcStepper::next_command() const {
  return "drc find " + std::to_string(k ? wrap(*k + 1, n) : 1) + "; findbox zoom";
}

std::string DrcStepper::prev_command() const {
  return "drc find " + std::to_string(k ? wrap(*k - 1, n) : static_cast<std::int64_t>(n)) + "; findbox zoom";
}

std::optional<DrcFindReport> parse_find_report(const std::vector<std::string>& printed) {
  constexpr std::string_view kHead = "Error area #";
  std::optional<DrcFindReport> report;
  for (const std::string& line : printed) {
    if (line.starts_with(kHead) && line.ends_with(":")) {
      const std::string number = line.substr(kHead.size(), line.size() - kHead.size() - 1);
      char* end = nullptr;
      const long long n = std::strtoll(number.c_str(), &end, 10);
      if (end != nullptr && *end == '\0' && !number.empty()) {
        report = DrcFindReport{n, {}};
        continue;
      }
    }
    if (report) report->whys.push_back(line);
  }
  return report;
}

bool is_drc_option(std::string_view name, const std::vector<std::string>& args, std::string_view option,
                   bool allow_list) {
  if (name != "drc" && name != "magic::drc" && name != "::magic::drc") return false;
  std::size_t i = 0;
  if (!args.empty() && args.front().starts_with("list")) {
    if (!allow_list) return false;
    i = 1;
  }
  if (i >= args.size() || args[i].empty()) return false;
  // Magic's Lookup takes a unique prefix; the shell only needs the options
  // that no other option shares a prefix with at that length.
  return option.starts_with(lower(args[i]));
}

// --- browser --------------------------------------------------------------------------

void DrcBrowserModel::set_list(std::vector<DrcGroup> groups) {
  std::vector<bool> expanded(groups.size(), false);
  for (std::size_t i = 0; i < groups.size(); ++i) {
    for (std::size_t j = 0; j < groups_.size(); ++j) {
      if (groups_[j].text == groups[i].text && expanded_[j]) expanded[i] = true;
    }
  }
  groups_ = std::move(groups);
  expanded_ = std::move(expanded);
  has_list_ = true;
  rebuild();
}

void DrcBrowserModel::clear_list() {
  groups_.clear();
  expanded_.clear();
  has_list_ = false;
  rebuild();
}

void DrcBrowserModel::sort(DrcSortKey key, bool descending) {
  sort_key_ = key;
  descending_ = descending;
  rebuild();
}

void DrcBrowserModel::set_filter(std::string text) {
  filter_ = lower(text);
  rebuild();
}

namespace {

// The browser's group order: by area count or by text, ascending or not;
// discovery order (the index) breaks ties.
bool group_before(const std::vector<DrcGroup>& groups, std::size_t a, std::size_t b, DrcSortKey key,
                  bool descending) {
  const DrcGroup& ga = groups[a];
  const DrcGroup& gb = groups[b];
  if (key == DrcSortKey::count && ga.areas.size() != gb.areas.size()) {
    return descending == (ga.areas.size() > gb.areas.size());
  }
  if (key == DrcSortKey::text && ga.text != gb.text) return descending == (ga.text > gb.text);
  return a < b;
}

}  // namespace

bool DrcBrowserModel::listed(std::size_t group, bool ignored) const {
  const DrcGroup& g = groups_[group];
  return g.ignored == ignored && (filter_.empty() || lower(g.text).find(filter_) != std::string::npos);
}

std::vector<std::size_t> DrcBrowserModel::ordered(bool ignored) const {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < groups_.size(); ++i) {
    if (listed(i, ignored)) out.push_back(i);
  }
  std::stable_sort(out.begin(), out.end(), [this](std::size_t a, std::size_t b) {
    return group_before(groups_, a, b, sort_key_, descending_);
  });
  return out;
}

void DrcBrowserModel::rebuild() {
  rows_.clear();
  for (const std::size_t g : ordered(false)) {
    rows_.push_back({DrcRowKind::group, g, 0});
    if (!expanded_[g]) continue;
    for (std::size_t a = 0; a < groups_[g].areas.size(); ++a) rows_.push_back({DrcRowKind::area, g, a});
  }
  const std::vector<std::size_t> ignored = ordered(true);
  if (ignored.empty()) return;
  rows_.push_back({DrcRowKind::ignored_heading, 0, 0});
  for (const std::size_t g : ignored) rows_.push_back({DrcRowKind::ignored_group, g, 0});
}

std::string DrcBrowserModel::cell_text(std::size_t index, int column) const {
  if (index >= rows_.size()) return {};
  const DrcRow& r = rows_[index];
  const DrcGroup& g = groups_.at(r.kind == DrcRowKind::ignored_heading ? 0 : r.group);
  switch (r.kind) {
    case DrcRowKind::group:
      if (column == 0) return std::string(expanded_[r.group] ? "▾ " : "▸ ") + g.text;
      return std::string(status_glyph(StatusKind::error)) + " " + group_thousands(g.areas.size());
    case DrcRowKind::area: {
      if (column == 1) return {};
      const db::Rect& a = g.areas[r.area];
      return "    " + std::to_string(r.area + 1) + "   " + format_length(a.x0, scale_) + " " +
             format_length(a.y0, scale_) + " " + format_length(a.x1, scale_) + " " + format_length(a.y1, scale_);
    }
    case DrcRowKind::ignored_heading:
      if (column == 1) return {};
      return "IGNORED · " + std::to_string(ignored_count()) + " · Clear ignores ⏎";
    case DrcRowKind::ignored_group:
      if (column == 0) return "⊘ " + g.text;
      return std::string(status_glyph(StatusKind::info)) + " " + group_thousands(g.areas.size());
  }
  return {};
}

std::string DrcBrowserModel::row_accessible_name(std::size_t index) const {
  if (index >= rows_.size()) return {};
  const DrcRow& r = rows_[index];
  switch (r.kind) {
    case DrcRowKind::group:
      return groups_[r.group].text + ", " + plural(groups_[r.group].areas.size(), "area", "areas") +
             (expanded_[r.group] ? ", expanded" : ", collapsed");
    case DrcRowKind::area: return "Error area " + cell_text(index, 0).substr(4);
    case DrcRowKind::ignored_heading:
      return "Ignored rules, " + std::to_string(ignored_count()) + ". Activate to clear ignores";
    case DrcRowKind::ignored_group:
      return "Ignored: " + groups_[r.group].text + ", " + plural(groups_[r.group].areas.size(), "area", "areas");
  }
  return {};
}

bool DrcBrowserModel::toggle(std::size_t index) {
  if (index >= rows_.size() || rows_[index].kind != DrcRowKind::group) return false;
  const std::size_t g = rows_[index].group;
  expanded_[g] = !expanded_[g];
  rebuild();
  return true;
}

std::optional<std::string> DrcBrowserModel::command(std::size_t index) const {
  if (index >= rows_.size()) return std::nullopt;
  const DrcRow& r = rows_[index];
  if (r.kind == DrcRowKind::ignored_heading) return std::string(kDrcClearIgnoresCommand);
  if (r.kind != DrcRowKind::area) return std::nullopt;
  const db::Rect& a = groups_[r.group].areas[r.area];
  // Internal units ("i"): exact whatever the `units` setting.
  return "box values " + internal_word(a.x0) + " " + internal_word(a.y0) + " " + internal_word(a.x1) + " " +
         internal_word(a.y1) + "; " + std::string(kDrcZoomCommand);
}

std::optional<std::size_t> DrcBrowserModel::row_at(const db::Rect& box) const {
  for (std::size_t i = 0; i < rows_.size(); ++i) {
    const DrcRow& r = rows_[i];
    if (r.kind == DrcRowKind::area && db::overlap(groups_[r.group].areas[r.area], box)) return i;
  }
  return std::nullopt;
}

std::size_t DrcBrowserModel::area_count() const noexcept {
  std::size_t n = 0;
  for (const DrcGroup& g : groups_) n += g.ignored ? 0 : g.areas.size();
  return n;
}

std::size_t DrcBrowserModel::why_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(groups_.begin(), groups_.end(), [](const DrcGroup& g) { return !g.ignored; }));
}

std::size_t DrcBrowserModel::ignored_count() const noexcept {
  return groups_.size() - why_count();
}

StatusKind DrcBrowserModel::summary_kind(std::size_t error_tiles) const noexcept {
  if (error_tiles > 0) return StatusKind::error;
  return has_list_ ? StatusKind::success : StatusKind::info;  // not checked: no "clean" shape
}

std::string DrcBrowserModel::summary(std::size_t error_tiles) const {
  std::string out = std::string(status_glyph(summary_kind(error_tiles))) + " " +
                    plural(error_tiles, "error tile", "error tiles");
  if (!has_list_) return out;
  out += " · " + plural(area_count(), "area", "areas") + " · " + plural(why_count(), "why-text", "why-texts");
  if (ignored_count() > 0) out += " · " + std::to_string(ignored_count()) + " ignored";
  return out;
}

DrcBrowserState drc_browser_state(DrcState status, const DrcBrowserModel& list, bool stale) noexcept {
  if (status == DrcState::checking) return DrcBrowserState::running;
  if (!list.has_list()) return DrcBrowserState::not_checked;
  if (stale) return DrcBrowserState::stale;
  return list.why_count() == 0 ? DrcBrowserState::clean : DrcBrowserState::populated;
}

std::optional<DrcBrowserNotice> drc_browser_notice(DrcBrowserState state, std::string_view cell,
                                                   std::string_view style) {
  const std::string in_cell = cell.empty() ? std::string() : " in " + std::string(cell);
  switch (state) {
    case DrcBrowserState::not_checked:
      return DrcBrowserNotice{StatusKind::info, "Not checked yet",
                              "Background DRC is off, or the cell has not been checked. Check runs select top cell; "
                              "drc check; drc catchup."};
    case DrcBrowserState::clean:
      return DrcBrowserNotice{StatusKind::success, "No DRC errors" + in_cell,
                              "0 error tiles under " + std::string(style) +
                                  ". drc listall why printed No errors found."};
    case DrcBrowserState::running:
      return DrcBrowserNotice{StatusKind::info, "Checking" + in_cell + "…",
                              "The list is rebuilt when the checker goes idle."};
    case DrcBrowserState::stale:
      return DrcBrowserNotice{StatusKind::warning, "Out of date",
                              "The layout changed after this list was built. It refreshes when checking finishes; "
                              "with DRC off, use Re-check."};
    case DrcBrowserState::populated: return std::nullopt;
  }
  return std::nullopt;
}

std::string_view recheck_label(DrcBrowserState state) noexcept {
  return state == DrcBrowserState::not_checked ? "Check" : "Re-check";
}

std::vector<MenuEntry> drc_style_menu(const std::vector<std::string>& styles, std::string_view current) {
  std::vector<MenuEntry> out;
  for (const std::string& s : styles) out.push_back({s, drc_style_command(s), false, s == current, true, false});
  return out;
}

// --- inspector, popover, notice -------------------------------------------------------

DrcInspectorView drc_inspector_view(const DrcInspectorInput& in) {
  DrcInspectorView v;
  v.title = in.found ? "DRC ERROR AREA #" + std::to_string(in.found->index) + " OF " + std::to_string(in.error_tiles)
                     : std::string("DRC ERROR AREA");
  if (!in.found || !in.box) {
    v.empty = in.error_tiles > 0 ? "Use Next, or click an area in the DRC panel" : "No error area under the box";
    return v;
  }
  const std::string glyph(status_glyph(StatusKind::error));
  if (!in.found->whys.empty()) v.headline = glyph + " " + in.found->whys.front();
  for (std::size_t i = 1; i < in.found->whys.size(); ++i) v.whys.push_back(glyph + " " + in.found->whys[i]);
  const db::Rect& b = *in.box;
  const std::string unit(in.scale.unit_name());
  v.rows.emplace_back("Cell", in.cell);
  v.rows.emplace_back("Area", format_length(b.x0, in.scale) + " " + format_length(b.y0, in.scale) + " " +
                                  format_length(b.x1, in.scale) + " " + format_length(b.y1, in.scale));
  v.rows.emplace_back("Size", format_length(static_cast<double>(b.width()), in.scale) + " × " +
                                  format_length(static_cast<double>(b.height()), in.scale) + " " + unit);
  v.rows.emplace_back("Style", in.style);
  return v;
}

std::vector<DeferredRow> drc_deferred_rows() {
  return {{"Measured vs required", "M11", "Measured versus required values arrive in M11"},
          {"Nets", "M7", "The nets of an error arrive with the net browser in M7"},
          {"Suggested fix · Preview fix", "M11", "Suggested fixes arrive with the agent in M11"},
          {"Sign-off", "M13", "The sign-off summary arrives in M13"},
          {"Fix with agent", "M11", "Fix with agent arrives in M11"}};
}

std::optional<DrcWhyPopover> drc_why_popover(std::string_view name, const std::vector<std::string>& args,
                                             const std::vector<std::string>& printed) {
  if (args.size() != 1 || !is_drc_option(name, args, "why")) return std::nullopt;
  DrcWhyPopover p;
  p.title = "DRC why · box";
  p.clean = printed.size() == 1 && printed.front() == "No errors found.";
  p.rows = printed;
  if (p.rows.empty()) {
    p.rows.emplace_back("No errors found.");
    p.clean = true;
  }
  return p;
}

std::optional<DrcJobNotice> drc_job_notice(const DrcJobInput& in) {
  if (!in.enabled || in.window_active || in.seconds < kLongJobSeconds) return std::nullopt;
  DrcJobNotice n;
  n.title = "DRC finished" + (in.cell.empty() ? std::string() : " · " + in.cell);
  const std::string style = in.style.empty() ? std::string() : " · " + in.style;
  const auto secs = static_cast<long long>(std::lround(in.seconds));
  if (in.error_tiles == 0) {
    n.body = std::string(status_glyph(StatusKind::success)) + " No DRC errors" + style;
  } else {
    n.body = std::string(status_glyph(StatusKind::error)) + " " + plural(in.error_tiles, "error", "errors") + style;
  }
  n.body += " · " + std::to_string(secs) + " s";
  return n;
}

bool notifications_enabled(const std::optional<std::string>& setting) noexcept {
  return !setting || (*setting != "off" && *setting != "0" && *setting != "false");
}

}  // namespace hefesto::shell
