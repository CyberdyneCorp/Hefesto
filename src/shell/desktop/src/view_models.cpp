// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/view_models.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>

#include "hefesto/db/transform.hpp"
#include "hefesto/diag/format.hpp"

namespace hefesto::shell {

namespace {

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool matches(const display::LayerSwatch& swatch, const std::string& filter) {
  return filter.empty() || lower(swatch.name).find(filter) != std::string::npos ||
         lower(swatch.plane).find(filter) != std::string::npos;
}

Rgba style_color(const display::ResolvedStyle& s, float alpha) {
  return Rgba{s.r, s.g, s.b, alpha};
}

/// Fill and outline of a swatch from the styles the canvas draws the layer
/// with: the first filled style gives the fill, any outline style the edge.
void apply_styles(LayerRow& row, const std::vector<display::ResolvedStyle>& styles) {
  bool filled = false;
  for (const display::ResolvedStyle& s : styles) {
    row.crossed = row.crossed || s.fill == display::FillKind::cross;
    if (!filled && (s.fill == display::FillKind::solid || s.fill == display::FillKind::stipple)) {
      // The mockup draws swatches at about half the layer's opacity so the
      // outline stays readable; the hue is always the PDK's.
      row.fill = style_color(s, std::clamp(s.a * 0.55F, 0.2F, 0.6F));
      row.outline = style_color(s, 1.0F);
      row.stippled = s.fill == display::FillKind::stipple;
      filled = true;
    }
  }
  if (!filled && !styles.empty()) {
    row.fill = style_color(styles.front(), 0.15F);
    row.outline = style_color(styles.front(), 1.0F);
    row.dashed = styles.front().fill == display::FillKind::outline;
  }
}

std::string trim_number(double value, int decimals) {
  char buffer[48];
  std::snprintf(buffer, sizeof buffer, "%.*f", decimals, value);
  std::string text = buffer;
  if (text.find('.') != std::string::npos) {
    while (!text.empty() && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
  }
  return text == "-0" ? "0" : text;
}

std::string array_suffix(const db::ArrayInfo& a) {
  if (!a.is_array()) {
    return {};
  }
  const int cols = std::abs(a.xhi - a.xlo) + 1;
  const int rows = std::abs(a.yhi - a.ylo) + 1;
  return " [" + std::to_string(cols) + "×" + std::to_string(rows) + "]";
}

/// Every instance can be expanded (Magic shows an unexpanded instance as
/// its bounding box only), unless its definition was not found.
bool can_expand(const db::CellUse& use) {
  return use.def != nullptr && !use.def->flags().not_found;
}

struct TreeBuilder {
  const ExpandedTest& expanded;
  std::size_t max_rows;
  std::vector<HierarchyRow> rows;

  [[nodiscard]] bool full() const { return rows.size() >= max_rows; }
  [[nodiscard]] bool is_open(const db::CellUse& use) const { return can_expand(use) && expanded && expanded(use); }

  /// One row for the leaf instances of one definition: open when all are,
  /// partial (collapsed marker, "· N open") when only some are.
  void add_group(const std::vector<const db::CellUse*>& uses, int depth, const std::string& prefix) {
    HierarchyRow row{depth, {}, true, true, nullptr, {}, {}, false};
    std::size_t open = 0;
    for (const db::CellUse* use : uses) {
      open += is_open(*use) ? 1U : 0U;
      row.paths.push_back(prefix + use->id);
    }
    row.expanded = open == uses.size();
    row.partial = open > 0 && !row.expanded;
    row.suffix = " ×" + std::to_string(uses.size()) + (row.partial ? " · " + std::to_string(open) + " open" : "");
    row.text = std::string(row.expanded ? "▾ " : "▸ ") + uses.front()->def->name() + row.suffix;
    rows.push_back(std::move(row));
  }

  void add_use(const db::CellUse& use, int depth, const std::string& prefix) {
    const bool expandable = can_expand(use);
    const bool open = is_open(use);
    const std::string name = use.def != nullptr ? use.def->name() : std::string("(missing)");
    const std::string marker = expandable ? (open ? "▾ " : "▸ ") : "";
    std::string suffix = " (" + use.id + ")" + array_suffix(use.array);
    const std::string path = prefix + use.id;
    rows.push_back(HierarchyRow{depth, marker + name + suffix, expandable, open, &use, {path}, std::move(suffix), false});
    if (open && !use.def->uses().empty()) {
      add_children(*use.def, depth + 1, path + "/");
    }
  }

  /// Leaf instances (no children, not arrays) of one definition collapse into
  /// one counted row at the position of the first; the others keep their order.
  void add_children(const db::CellDef& def, int depth, const std::string& prefix) {
    std::map<const db::CellDef*, std::vector<const db::CellUse*>> leaves;
    for (const auto& use : def.uses()) {
      if (is_groupable(*use)) leaves[use->def].push_back(use.get());
    }
    for (const auto& use : def.uses()) {
      if (full()) return;
      const auto group = is_groupable(*use) ? leaves.find(use->def) : leaves.end();
      if (group == leaves.end() || group->second.size() < 2) {
        add_use(*use, depth, prefix);
      } else if (group->second.front() == use.get()) {
        add_group(group->second, depth, prefix);
      }
    }
  }

  static bool is_groupable(const db::CellUse& use) {
    return can_expand(use) && use.def->uses().empty() && !use.array.is_array();
  }
};

}  // namespace

// --- units -----------------------------------------------------------------

double UnitScale::to_display(double internal_units) const noexcept {
  const double lambdas = internal_units * static_cast<double>(lambda) / static_cast<double>(internal > 0 ? internal : 1);
  return microns_per_lambda ? lambdas * *microns_per_lambda : lambdas;
}

std::optional<double> microns_per_lambda(const std::optional<tech::CifGridInfo>& cif) {
  if (!cif || cif->scale_factor <= 0 || cif->expander <= 0) {
    return std::nullopt;
  }
  return static_cast<double>(cif->scale_factor) / (100.0 * static_cast<double>(cif->expander));
}

std::string format_length(double internal_units, const UnitScale& scale) {
  const double value = scale.to_display(internal_units);
  if (scale.microns_per_lambda) {
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%.3f", value);
    return buffer;
  }
  return trim_number(value, 2);
}

// --- status ------------------------------------------------------------------

std::string StatusChip::label() const {
  return std::string(status_glyph(kind)) + " " + text;
}

StatusChip drc_chip_m1() {
  return StatusChip{StatusKind::info, "DRC not run"};
}

StatusChip lvs_chip_m1() {
  return StatusChip{StatusKind::info, "LVS not run"};
}

StatusKind status_for_severity(std::string_view severity) noexcept {
  if (severity == "error" || severity == "fatal") return StatusKind::error;
  if (severity == "warning") return StatusKind::warning;
  return StatusKind::info;
}

// --- text fitting --------------------------------------------------------------

namespace {

/// Bytes of the UTF-8 code point that starts with `lead`.
std::size_t utf8_length(char lead) noexcept {
  const auto c = static_cast<unsigned char>(lead);
  if (c >= 0xF0U) return 4;
  if (c >= 0xE0U) return 3;
  if (c >= 0xC0U) return 2;
  return 1;
}

/// Byte offsets of the code point starts of `text`, plus its size.
std::vector<std::size_t> code_point_starts(std::string_view text) {
  std::vector<std::size_t> starts;
  for (std::size_t i = 0; i < text.size(); i += utf8_length(text[i])) starts.push_back(i);
  starts.push_back(text.size());
  return starts;
}

/// Where the line that starts at code point `first` ends: the most code
/// points that `fits`, moved back to just after a space or '/' when one falls
/// in the second half of the line. At least one code point.
std::size_t wrap_break(std::string_view text, const std::vector<std::size_t>& cps, std::size_t first,
                       const std::function<bool(std::string_view)>& fits) {
  const std::size_t end = cps.size() - 1;
  std::size_t last = first + 1;
  while (last < end && fits(text.substr(cps[first], cps[last + 1] - cps[first]))) ++last;
  if (last == end) return last;
  for (std::size_t i = last; i > first + (last - first) / 2; --i) {
    const char before = text[cps[i] - 1];
    if (before == ' ' || before == '/') return i;
  }
  return last;
}

/// Wraps each LF-separated paragraph of `text`; continuation lines start
/// with `indent` spaces and must satisfy `fits` with them.
std::vector<std::string> wrap_lines(std::string_view text, std::size_t indent,
                                    const std::function<bool(std::string_view)>& fits) {
  std::vector<std::string> lines;
  std::size_t pos = 0;
  for (bool more = true; more;) {
    const std::size_t lf = text.find('\n', pos);
    more = lf != std::string_view::npos;
    const std::string_view paragraph = text.substr(pos, more ? lf - pos : std::string_view::npos);
    const std::vector<std::size_t> cps = code_point_starts(paragraph);
    std::size_t first = 0;
    do {
      const std::string pad(lines.empty() ? 0 : indent, ' ');
      const auto padded_fits = [&](std::string_view line) { return fits(pad + std::string(line)); };
      const std::size_t end = cps.size() == 1 ? 0 : wrap_break(paragraph, cps, first, padded_fits);
      lines.push_back(pad + std::string(paragraph.substr(cps[first], cps[end] - cps[first])));
      first = end;
    } while (first < cps.size() - 1);
    pos = lf + 1;
  }
  return lines;
}

}  // namespace

std::string ellipsize(std::string_view text, float max_width, const TextMeasure& measure) {
  if (measure(text) <= max_width) {
    return std::string(text);
  }
  const std::vector<std::size_t> starts = code_point_starts(text);
  // Binary search for the most code points that fit with the ellipsis.
  std::size_t lo = 0;
  std::size_t hi = starts.size() - 1;
  while (lo < hi) {
    const std::size_t mid = (lo + hi + 1) / 2;
    if (measure(std::string(text.substr(0, starts[mid])) + "…") <= max_width) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  return std::string(text.substr(0, starts[lo])) + "…";
}

std::string ellipsize_middle(std::string_view text, float max_width, const TextMeasure& measure) {
  if (measure(text) <= max_width) {
    return std::string(text);
  }
  const std::vector<std::size_t> starts = code_point_starts(text);
  const std::size_t count = starts.size() - 1;
  if (count == 0) {
    return {};
  }
  const auto shortened = [&](std::size_t kept) {
    const std::size_t head = kept / 3;
    return std::string(text.substr(0, starts[head])) + "…" + std::string(text.substr(starts[count - (kept - head)]));
  };
  // Binary search for the most code points that fit with the ellipsis.
  std::size_t lo = 0;
  std::size_t hi = count - 1;
  while (lo < hi) {
    const std::size_t mid = (lo + hi + 1) / 2;
    if (measure(shortened(mid)) <= max_width) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  return shortened(lo);
}

std::vector<std::string> wrap_columns(std::string_view text, std::size_t columns, std::size_t indent) {
  const std::size_t width = std::max<std::size_t>(columns, indent + 8);
  return wrap_lines(text, indent, [width](std::string_view line) { return code_point_starts(line).size() - 1 <= width; });
}

std::vector<std::string> wrap_to_width(std::string_view text, float max_width, const TextMeasure& measure) {
  return wrap_lines(text, 0, [&](std::string_view line) { return measure(line) <= max_width; });
}

// --- accessibility -------------------------------------------------------------

std::string toggle_accessible_name(std::string_view label, bool pressed) {
  return std::string(label) + (pressed ? ", pressed" : ", not pressed");
}

std::optional<db::Rect> visible_box(const std::optional<db::Rect>& box, const db::CellDef* box_root,
                                    const db::CellDef* edit_cell) {
  if (box_root != nullptr && box_root != edit_cell) {
    return std::nullopt;
  }
  return box;
}

std::string checkbox_accessible_name(std::string_view label, bool checked) {
  return std::string(label) + (checked ? ", checked" : ", not checked");
}

std::optional<Announcement> problem_announcement(StatusKind kind, std::string_view status) {
  if (status.empty() || (kind != StatusKind::error && kind != StatusKind::warning)) {
    return std::nullopt;
  }
  const std::string_view word = kind == StatusKind::error ? "Error: " : "Warning: ";
  // The status line starts with its shape glyph and a space: read the words.
  const std::string prefix = std::string(status_glyph(kind)) + " ";
  const std::string_view body = status.starts_with(prefix) ? status.substr(prefix.size()) : status;
  return Announcement{std::string(word) + std::string(body), kind == StatusKind::error};
}

std::string diagnostic_line(StatusKind kind, std::string_view text) {
  return std::string(status_glyph(kind)) + " " + std::string(text);
}

LineStatus command_line_status(bool ok, std::string_view result, const std::optional<LineStatus>& problem) {
  if (problem && (ok || problem->kind == StatusKind::error)) return *problem;
  const StatusKind kind = ok ? StatusKind::success : StatusKind::error;
  return LineStatus{kind, diagnostic_line(kind, result)};
}

std::string DiagnosticTally::note(StatusKind kind, std::string_view text) {
  if (kind == StatusKind::error) {
    ++errors_;
  } else if (kind == StatusKind::warning) {
    ++warnings_;
  } else {
    return {};
  }
  const std::string_view first_line = text.substr(0, text.find('\n'));
  return diagnostic_line(kind, first_line);
}

std::optional<StatusChip> DiagnosticTally::chip() const {
  const auto count = [](int n, std::string_view word) {
    return std::to_string(n) + " " + std::string(word) + (n == 1 ? "" : "s");
  };
  if (errors_ > 0) {
    std::string text = count(errors_, "error");
    if (warnings_ > 0) text += " · " + diagnostic_line(StatusKind::warning, count(warnings_, "warning"));
    return StatusChip{StatusKind::error, text};
  }
  if (warnings_ > 0) return StatusChip{StatusKind::warning, count(warnings_, "warning")};
  return std::nullopt;
}

void ProblemSink::emit(const diag::Diagnostic& d) {
  const StatusKind kind = status_for_severity(diag::severity_name(d.severity));
  const std::string text = diag::format_text(d);
  const std::string status = tally_.note(kind, text);
  if (listener_) listener_(kind, diagnostic_line(kind, text), status);
}

// --- top bar -------------------------------------------------------------------

HeaderText header_text(const HeaderInput& in) {
  HeaderText out;
  out.title = in.cell.empty() ? "No cell loaded" : in.cell;
  std::string sub = in.pdk ? *in.pdk : (in.technology.empty() ? std::string("minimum") : in.technology);
  if (in.pdk && in.pdk_version) {
    sub += " · " + *in.pdk_version;
  }
  sub += in.magic_compat ? " · magic-compat on" : " · magic-compat off";
  out.subtitle = sub;
  return out;
}

std::optional<std::string> short_tech_version(const std::vector<std::string>& version_lines) {
  if (version_lines.empty()) {
    return std::nullopt;
  }
  std::string_view line = version_lines.front();
  while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())) != 0) line.remove_suffix(1);
  const std::size_t space = line.find_last_of(" \t");
  std::string_view word = space == std::string_view::npos ? line : line.substr(space + 1);
  word = word.substr(0, word.find('-'));
  if (word.empty()) {
    return std::nullopt;
  }
  return std::string(word);
}

const std::vector<DeferredControl>& deferred_view_tabs() {
  static const std::vector<DeferredControl> tabs = {
      {"Schematic", "Schematic editor: arrives in M8"},
      {"3D", "3D inspection: arrives in M10"},
      {"Waveforms", "Waveforms: arrive in M8"},
  };
  return tabs;
}

const std::vector<DeferredControl>& deferred_actions() {
  static const std::vector<DeferredControl> actions = {
      {"Extract", "Extraction: arrives in M7"},
      {"Simulate", "Simulation: arrives in M9"},
      {"Ask agent", "AI agent: arrives in M11"},
  };
  return actions;
}

const std::vector<ToolButton>& tool_rail() {
  static const std::vector<ToolButton> tools = {
      {"box", "Box tool", true, "Box tool (active)"},
      {"select", "Select", false, "Select: arrives in M2"},
      {"paint", "Paint", false, "Paint: arrives in M2"},
      {"wire", "Wire", false, "Wire: arrives in M2"},
      {"label", "Label", false, "Label: arrives in M2"},
      {"measure", "Measure", false, "Measure: arrives in M2"},
      {"place", "Place device", false, "Place device: arrives in M2"},
      {"fit", "Zoom to fit", true, "Zoom to fit (F)"},
  };
  return tools;
}

// --- layer palette ------------------------------------------------------------------

std::string LayerRow::accessible_name() const {
  return name + ", plane " + plane + (visible ? ", visible" : ", hidden");
}

std::string LayerRow::toggle_command() const {
  return visible ? "see no " + name : "see " + name;
}

std::vector<LayerRow> layer_rows(const std::vector<display::LayerSwatch>& swatches, const tech::TypeMask& visible,
                                 std::string_view filter) {
  const std::string needle = lower(filter);
  std::vector<LayerRow> rows;
  for (const display::LayerSwatch& swatch : swatches) {
    if (!matches(swatch, needle)) {
      continue;
    }
    LayerRow row;
    row.type = swatch.type;
    row.name = swatch.name;
    row.plane = swatch.plane;
    row.contact = swatch.contact;
    row.visible = visible.test(swatch.type);
    apply_styles(row, swatch.styles);
    rows.push_back(std::move(row));
  }
  return rows;
}

// --- hierarchy ------------------------------------------------------------------------

std::vector<HierarchyRow> hierarchy_rows(const db::CellDef& root, const ExpandedTest& expanded,
                                         std::size_t max_rows) {
  TreeBuilder builder{expanded, max_rows, {}};
  const bool has_children = !root.uses().empty();
  builder.rows.push_back(
      HierarchyRow{0, (has_children ? "▾ " : "") + root.name(), false, has_children, nullptr, {}, {}, false});
  builder.add_children(root, 1, "");
  return builder.rows;
}

std::vector<db::Rect> splice_areas(const std::vector<db::Rect>& areas, std::size_t max_areas) {
  if (areas.size() <= max_areas) {
    return areas;
  }
  db::Rect all = areas.front();
  for (const db::Rect& r : areas) {
    all = db::Rect{std::min(all.x0, r.x0), std::min(all.y0, r.y0), std::max(all.x1, r.x1), std::max(all.y1, r.y1)};
  }
  return {all};
}

std::string_view HierarchyRow::name_part() const {
  return std::string_view(text).substr(0, text.size() - std::min(suffix.size(), text.size()));
}

std::string HierarchyRow::accessible_name() const {
  if (!expandable && !expanded) {
    return text;
  }
  if (partial) {
    return text + ", partly expanded";
  }
  return text + (expanded ? ", expanded" : ", collapsed");
}

std::size_t top_bar_hidden_groups(float available, float full_width, const std::vector<float>& group_widths) {
  std::size_t hidden = 0;
  float width = full_width;
  while (width > available && hidden < group_widths.size()) {
    width -= group_widths[hidden];
    ++hidden;
  }
  return hidden;
}

HierarchyRowLayout hierarchy_row_layout(const HierarchyRow& row, float width, const TextMeasure& measure) {
  // The suffix stays whole while the name keeps at least half the width;
  // a longer suffix (long instance ids) is ellipsized instead of the name.
  const float suffix_w = measure(row.suffix);
  const float name_w = measure(row.name_part());
  const float name_room = std::max(0.0F, width - suffix_w);
  const float min_name = std::min(name_w, width / 2.0F);
  HierarchyRowLayout out;
  out.name = ellipsize_middle(row.name_part(), std::max(name_room, min_name), measure);
  out.suffix_x = measure(out.name);
  out.suffix = ellipsize_middle(row.suffix, std::max(0.0F, width - out.suffix_x), measure);
  return out;
}

std::string HierarchyRow::toggle_command() const {
  if (!expandable) {
    return {};
  }
  // A path word the script reader keeps whole (as typed paths are quoted).
  const auto word = [](const std::string& path) {
    if (path.find_first_of(" \t\"\\{}[]$;") == std::string::npos) return path;
    std::string out = "\"";
    for (const char c : path) {
      if (c == '"' || c == '\\' || c == '$' || c == '[' || c == ']') out += '\\';
      out += c;
    }
    return out + "\"";
  };
  std::string out;
  for (const std::string& path : paths) {
    out += (out.empty() ? "" : "; ") + std::string(expanded ? "unexpand instance " : "expand instance ") + word(path);
  }
  return out;
}

std::string layer_empty_hint(std::string_view technology, bool has_styles) {
  if (has_styles) {
    return "No layer matches the filter";
  }
  if (technology.empty() || technology == "minimum") {
    return "Load a technology with display styles";
  }
  return "No display styles found for " + std::string(technology) + " (see Diagnostics)";
}

// --- inspector ------------------------------------------------------------------------

InspectorView inspector_view(const std::optional<db::Rect>& box, const db::CellDef* cell, const UnitScale& scale) {
  InspectorView view;
  const std::string unit(scale.unit_name());
  if (box) {
    view.section = "BOX";
    view.title = "Box";
    view.subtitle = cell != nullptr ? "in edit cell " + cell->name() : "no edit cell";
    view.rows = {{"Width", format_length(static_cast<double>(box->width()), scale) + " " + unit},
                 {"Height", format_length(static_cast<double>(box->height()), scale) + " " + unit},
                 {"Lower left", format_length(box->x0, scale) + ", " + format_length(box->y0, scale)},
                 {"Upper right", format_length(box->x1, scale) + ", " + format_length(box->y1, scale)}};
    return view;
  }
  view.section = "CELL";
  if (cell == nullptr) {
    view.title = "Nothing loaded";
    view.subtitle = "Open a .mag file or load a cell";
    return view;
  }
  const db::Rect& b = cell->bbox();
  view.title = cell->name();
  view.subtitle = cell->file_path().empty() ? std::string("not saved") : cell->file_path().filename().string();
  view.rows = {{"Width", format_length(static_cast<double>(b.width()), scale) + " " + unit},
               {"Height", format_length(static_cast<double>(b.height()), scale) + " " + unit},
               {"Lower left", format_length(b.x0, scale) + ", " + format_length(b.y0, scale)},
               {"Instances", std::to_string(cell->uses().size())},
               {"Labels", std::to_string(cell->labels().size())}};
  return view;
}

// --- status bar -------------------------------------------------------------------------

StatusBarText status_bar_text(const StatusBarInput& in) {
  StatusBarText out;
  const std::string unit(in.scale.unit_name());
  if (in.cursor) {
    out.cursor = "x " + format_length(in.cursor->x, in.scale) + "  y " + format_length(in.cursor->y, in.scale) + " " +
                 unit;
  } else {
    out.cursor = "x –  y – " + unit;
  }
  out.box = in.box ? "box " + format_length(static_cast<double>(in.box->width()), in.scale) + " × " +
                         format_length(static_cast<double>(in.box->height()), in.scale)
                   : std::string("box –");
  out.edit = "edit: " + (in.edit_cell.empty() ? std::string("(none)") : in.edit_cell);
  out.snap = "snap: " + in.snap + " · units: " + unit;
  out.drc = in.drc.label();
  out.problems = in.problems ? in.problems->label() : std::string();
  const std::string_view first_line = std::string_view(in.message).substr(0, in.message.find('\n'));
  constexpr std::size_t kMaxMessage = 96;
  out.message = first_line.size() > kMaxMessage ? std::string(first_line.substr(0, kMaxMessage - 1)) + "…"
                                                : std::string(first_line);
  return out;
}

std::string status_bar_accessible_name(const StatusBarText& text) {
  std::string out = "Status";
  for (const std::string* part : {&text.message, &text.problems, &text.drc}) {
    if (part->empty()) continue;
    out += out == "Status" ? ": " : ". ";
    out += *part;
  }
  return out;
}

}  // namespace hefesto::shell
