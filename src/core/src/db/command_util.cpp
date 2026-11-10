// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the MODEL commands.

#include "db/command_util.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <string>
#include <utility>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/editor/coordinate_units.hpp"

namespace hefesto::db::cmdutil {

namespace {


std::vector<std::string_view> split_layers(std::string_view text) {
  std::vector<std::string_view> out;
  std::size_t pos = 0;
  while (pos <= text.size()) {
    const std::size_t end = text.find_first_of(", ", pos);
    const std::string_view part = text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
    if (!part.empty()) {
      out.push_back(part);
    }
    if (end == std::string_view::npos) {
      break;
    }
    pos = end + 1;
  }
  return out;
}

tech::TypeMask all_user_types(const tech::Technology& tech) {
  tech::TypeMask m;
  for (std::size_t t = tech::kFirstUserType; t < tech.user_type_end(); ++t) {
    m.set(static_cast<tech::TileType>(t));
  }
  return m;
}

}  // namespace

diag::Outcome<Session> session(cmd::CommandContext& ctx) {
  auto ed = cmd::require_editor(ctx);
  if (!ed.ok()) {
    return ed.error();
  }
  Session s;
  s.editor = ed.value();
  s.tech = s.editor->technology.get();
  s.magic_compat = ctx.project().compat.magic_compat;
  s.ctx = &ctx;
  if (s.tech == nullptr) {
    return diag::make(diag::codes::kCmdSession, "the session has no technology");
  }
  return s;
}

diag::Outcome<CellDef*> edit_cell(const Session& s) {
  if (s.editor->edit_cell == nullptr) {
    return diag::make(codes::kNoEditCell, "no edit cell (use `load` first)");
  }
  return s.editor->edit_cell;
}

diag::Outcome<Rect> box(const Session& s) {
  if (!s.editor->box) {
    return diag::make(codes::kNoBox, "Box tool must be present");
  }
  return *s.editor->box;
}

Snap snap_mode(const Session& s) {
  return s.editor->snap;
}

void set_snap_mode(const Session& s, Snap mode) {
  s.editor->snap = mode;
}

void print(const Session& s, std::string text) {
  if (s.ctx != nullptr) {
    s.ctx->emit(diag::make(codes::kReport, std::move(text)));
  }
}

void print_block(const Session& s, std::string text) {
  if (text.ends_with('\n')) {
    text.pop_back();
  }
  print(s, std::move(text));
}

void warn(const Session& s, std::string text) {
  if (s.ctx != nullptr) {
    s.ctx->emit(diag::make(codes::kWarning, std::move(text)));
  }
}

bool units_internal(const Session& s) {
  return s.editor->units.unit == cmd::InputUnit::internal;
}

void set_units_internal(const Session& s) {
  s.editor->units.unit = cmd::InputUnit::internal;
}

std::string_view snap_name(Snap mode) noexcept {
  switch (mode) {
    case Snap::internal:
      return "internal";
    case Snap::user:
      return "user";
    case Snap::lambda:
      break;
  }
  return "lambda";
}

diag::Outcome<std::int64_t> parse_int(std::string_view text) {
  std::int64_t v = 0;
  const char* first = text.data();
  const char* last = text.data() + text.size();
  if (first != last && *first == '+') {
    ++first;
  }
  const auto [ptr, ec] = std::from_chars(first, last, v);
  if (ec != std::errc{} || ptr != last || first == last) {
    return bad_argument("\"" + std::string{text} + "\" is not an integer");
  }
  return v;
}

cmd::CoordinateUnits coordinate_units(const Session& s) {
  return editor::coordinate_units(*s.editor, s.magic_compat);
}

diag::Outcome<Coord> parse_distance(const Session& s, std::string_view text, cmd::Axis axis, bool relative) {
  diag::DiagnosticSink* sink = s.ctx != nullptr ? &s.ctx->sink() : nullptr;
  const auto v = cmd::parse_distance(text, coordinate_units(s), cmd::DistanceRequest{axis, relative, 1}, sink);
  if (!v.ok()) {
    return bad_argument(v.error().text);
  }
  return static_cast<Coord>(v.value());
}

diag::Outcome<Coord> parse_coord(const Session& s, std::string_view text) {
  // Shared units (cmd::parse_coordinate); MODEL commands take only values on
  // the internal grid.
  const auto value = cmd::parse_coordinate(text, coordinate_units(s));
  if (!value.ok()) {
    return bad_argument(value.error().text);
  }
  const double v = value.value();
  if (v != std::floor(v)) {
    return bad_argument("\"" + std::string{text} + "\" is not on the internal grid");
  }
  if (v < static_cast<double>(kPlaneMin) || v > static_cast<double>(kPlaneMax)) {
    return diag::make(codes::kGeometryOutsidePlane, "coordinate " + std::string{text} + " is outside the plane");
  }
  const auto internal = static_cast<std::int64_t>(v);
  if (internal < kPlaneMin || internal > kPlaneMax) {
    return diag::make(codes::kGeometryOutsidePlane, "coordinate " + std::to_string(internal) + " is outside the plane");
  }
  return static_cast<Coord>(internal);
}

std::int32_t scaled_coord(const Session& s, std::string_view text, int scale, bool is_x, bool relative) {
  diag::DiagnosticSink* sink = s.ctx != nullptr ? &s.ctx->sink() : nullptr;
  const cmd::DistanceRequest request{is_x ? cmd::Axis::x : cmd::Axis::y, relative, scale};
  const auto value = cmd::parse_distance(text, coordinate_units(s), request, sink);
  if (!value.ok()) {
    warn(s, value.error().text);
    return 0;
  }
  return value.value();
}

bool str_is_int(std::string_view text) noexcept {
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    text.remove_prefix(1);
  }
  return std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

bool str_is_numeric(std::string_view text) noexcept {
  const std::string copy{text};
  char* end = nullptr;
  (void)std::strtod(copy.c_str(), &end);
  return end != copy.c_str();
}

std::int32_t c_atoi(std::string_view text) noexcept {
  const std::string copy{text};
  const long v = std::strtol(copy.c_str(), nullptr, 10);
  return static_cast<std::int32_t>(std::clamp<long>(v, INT32_MIN, INT32_MAX));
}

std::int32_t default_font_size(const Session& s) noexcept {
  const std::int64_t l1 = s.editor->grid.internal;
  return static_cast<std::int32_t>(s.magic_compat ? l1 : 8 * l1);
}

void label_changed(const Session& s, const CellDef& def, const Label& label) {
  const Rect area = label.font >= 0 && label.text_bbox ? include(label.rect, *label.text_bbox) : label.rect;
  s.editor->windows.area_changed(def, expand(area, 1), std::nullopt);
}

std::size_t erase_labels_by_content(CellDef& def, const Rect& rect, std::string_view text) {
  return std::erase_if(def.labels(), [&](const Label& l) { return l.rect == rect && l.text == text; });
}

void update_font_label(const Session& s, Label& label) {
  const display::OutlineFont* font = label.font >= 0 ? s.editor->fonts.font_of(label) : nullptr;
  if (font == nullptr) {
    label.text_bbox.reset();
    return;
  }
  const display::FontLabelGeometry geo = display::font_label_geometry(label, *font, s.magic_compat);
  label.text_bbox = geo.bbox;
  label.rotation = geo.rotation;
}

cmd::Value report_value(const Session& s, std::int64_t value, cmd::Axis axis, cmd::ValueKind kind) {
  const cmd::UnitsState& u = s.editor->units;
  if (u.unit == cmd::InputUnit::default_units || (u.unit == cmd::InputUnit::internal && !u.print)) {
    return cmd::Value{value};
  }
  return cmd::Value{cmd::format_value(value, u, coordinate_units(s), kind, axis)};
}

cmd::Value coord_value(const Session& s, Coord value, cmd::Axis axis) {
  return report_value(s, value, axis, cmd::ValueKind::length);
}

diag::Outcome<tech::TypeMask> parse_layers(const tech::Technology& tech, std::string_view text) {
  tech::TypeMask mask;
  for (const std::string_view name : split_layers(text)) {
    if (name == "*") {
      mask |= all_user_types(tech);
      continue;
    }
    if (name == "labels" || name == "label") {
      mask.set(kLabelsPseudoType);
      continue;
    }
    auto t = parse_layer(tech, name);
    if (!t.ok()) {
      return t.error();
    }
    mask.set(t.value());
  }
  return mask;
}

diag::Outcome<tech::TileType> parse_layer(const tech::Technology& tech, std::string_view text) {
  const auto found = tech.find_type(text);
  if (!found.ok()) {
    const char* why = found.error == tech::LookupError::ambiguous ? "ambiguous" : "unknown";
    return diag::make(codes::kBadLayer, std::string{why} + " layer \"" + std::string{text} + "\"");
  }
  return *found.id;
}

diag::Diagnostic bad_argument(std::string text) {
  return diag::make(codes::kBadArgument, std::move(text));
}

cmd::CommandResult fail(const diag::Diagnostic& d) {
  return cmd::CommandResult::failure(d);
}

CellUse* current_use(const Session& s) {
  if (s.editor->edit_cell == nullptr) {
    return nullptr;
  }
  const std::string& id = s.editor->current_use;
  return id.empty() ? nullptr : find_use(*s.editor->edit_cell, id);
}

void set_current_use(const Session& s, const CellUse* use) {
  s.editor->current_use = use == nullptr ? std::string{} : use->id;
}

int lookup(std::string_view word, std::initializer_list<std::string_view> choices) {
  return lookup_in(word, std::span<const std::string_view>{choices.begin(), choices.size()});
}

int lookup_in(std::string_view word, std::span<const std::string_view> choices) {
  const cmd::LookupResult r = cmd::lookup(word, choices);
  switch (r.match) {
    case cmd::Match::found:
      return static_cast<int>(r.index);
    case cmd::Match::ambiguous:
      return -1;
    case cmd::Match::unrecognized:
      break;
  }
  return -2;
}

std::string join_args(const std::vector<std::string>& args, std::size_t first) {
  std::string out;
  for (std::size_t i = first; i < args.size(); ++i) {
    if (!out.empty()) {
      out += ' ';
    }
    out += args[i];
  }
  return out;
}

bool add(cmd::CommandRegistry& registry, std::string name, std::string synopsis, std::string help,
         cmd::Handler handler) {
  return !registry.add(cmd::CommandSpec{std::move(name), std::move(synopsis), std::move(help), std::move(handler)});
}

}  // namespace hefesto::db::cmdutil
