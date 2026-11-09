// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the MODEL commands.

#include "db/command_util.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <utility>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/diag/codes.hpp"

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

void warn(const Session& s, std::string text) {
  if (s.ctx != nullptr) {
    s.ctx->emit(diag::make(codes::kWarning, std::move(text)));
  }
}

constexpr std::string_view kUnitsSetting = "units";

bool units_internal(const Session& s) {
  const auto v = s.editor->settings.value_in(app::SettingLayer::session, kUnitsSetting);
  return v && *v == "internal";
}

void set_units_internal(const Session& s) {
  s.editor->settings.set(app::SettingLayer::session, kUnitsSetting, "internal");
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
  cmd::CoordinateUnits u;
  u.internal_per = s.editor->grid.internal;
  u.lambda_per = s.editor->grid.lambda;
  switch (units_internal(s) ? Snap::internal : snap_mode(s)) {
    case Snap::internal:
      u.default_unit = cmd::DefaultUnit::internal;
      break;
    case Snap::user:
      u.default_unit = cmd::DefaultUnit::user;
      break;
    case Snap::lambda:
      break;
  }
  if (const auto& cif = s.tech->cif_output_grid()) {
    // The style as read; Magic also rescales it with `scalegrid` (open: M4 mask styles).
    u.cif_scale = std::array<std::int64_t, 2>{cif->scale_factor, cif->expander};
  }
  if (const view::LayoutWindow* w = s.editor->windows.current()) {
    const Rect g = w->grid().spacing;
    u.grid = std::array<std::int64_t, 4>{g.x0, g.y0, g.x1, g.y1};
  }
  return u;
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

cmd::Value coord_value(const Session& s, Coord value) {
  if (units_internal(s) || snap_mode(s) == Snap::internal) {
    return cmd::Value{std::int64_t{value}};
  }
  const std::int64_t num = std::int64_t{value} * s.editor->grid.lambda;
  if (num % s.editor->grid.internal == 0) {
    return cmd::Value{num / s.editor->grid.internal};
  }
  return cmd::Value{static_cast<double>(num) / static_cast<double>(s.editor->grid.internal)};
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
  int found = -2;
  int index = 0;
  for (const std::string_view choice : choices) {
    if (choice == word) {
      return index;
    }
    if (!word.empty() && choice.starts_with(word)) {
      found = found == -2 ? index : -1;
    }
    ++index;
  }
  return found;
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
