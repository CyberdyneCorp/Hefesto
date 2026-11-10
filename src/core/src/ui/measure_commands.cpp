// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ruler, measure, unmeasure and genkey (ui-box-stack-and-measure "Ruler
// annotation", "Measure and unmeasure", "Layer key annotation";
// Q-SCRIPT-16/17: the box is read in internal units whatever `units` says).
// Native versions of the tools.tcl procs; the annotations are drawing
// elements made through the `element` command. Owner: DISPATCH.
// (Magic ref: tcltk/tools.tcl:322-510)

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <set>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/state.hpp"
#include "ui/command_util.hpp"

namespace hefesto::ui::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

// Tcl's integer division (rounds toward minus infinity).
std::int64_t floor_div(std::int64_t a, std::int64_t b) {
  const std::int64_t q = a / b;
  return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

struct BoxValues {
  std::int64_t llx = 0, lly = 0, urx = 0, ury = 0;
};

// `box values` in internal units.
diag::Outcome<BoxValues> internal_box(CommandContext& ctx, editor::EditorState& editor) {
  const InternalUnits internal{editor};
  CommandResult box = run_nested(ctx, {"box", "values"});
  if (!box.ok()) {
    return box.diagnostics.empty() ? diag::make(codes::kUsage, "no box") : box.diagnostics.front();
  }
  const std::vector<std::string> v = value_words(box.value);
  std::array<std::int64_t, 4> n{};
  for (std::size_t i = 0; i < n.size(); ++i) {
    const auto parsed = i < v.size() ? parse_int(v[i]) : std::nullopt;
    if (!parsed) {
      return diag::make(codes::kUsage, "box values are not integers: " + box.value.to_string());
    }
    n.at(i) = *parsed;
  }
  return BoxValues{n[0], n[1], n[2], n[3]};
}

bool horizontal(const BoxValues& b, std::string_view orient) {
  if (orient == "auto") {
    return (b.urx - b.llx) > (b.ury - b.lly);
  }
  return orient == "horizontal";
}

std::string num(std::int64_t v) {
  return std::to_string(v);
}

// One ruler's element commands in tools.tcl's order.
std::vector<std::vector<std::string>> ruler_elements(const BoxValues& b, bool horiz, const std::string& text,
                                                     int k) {
  const std::string id = std::to_string(k);
  const std::int64_t tclr = value_words(cmd::Value{text}).empty() ? 0 : (horiz ? 4 : 2);
  const std::int64_t mmx = floor_div(b.llx + b.urx, 2);
  const std::int64_t mmy = floor_div(b.lly + b.ury, 2);
  // Along the ruler: lo/hi are the box edges, m1/m2 the gap around the text.
  const std::int64_t lo = horiz ? b.llx : b.lly;
  const std::int64_t hi = horiz ? b.urx : b.ury;
  const std::int64_t mid = horiz ? mmx : mmy;
  std::int64_t m1 = mid - tclr;
  std::int64_t m2 = mid + tclr;
  if (m1 == lo) {
    m1 = lo - 2;
  }
  if (m2 == hi) {
    m2 = hi + 2;
  }
  const auto line = [&](std::string_view name, std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1) {
    return std::vector<std::string>{"element", "add", "line", std::string{name} + "_" + id, "black",
                                    num(x0),   num(y0), num(x1), num(y1)};
  };
  std::vector<std::vector<std::string>> out;
  if (horiz) {
    out = {line("l1", b.llx, b.lly, b.llx, b.ury), line("l4", b.urx, b.lly, b.urx, b.ury),
           line("l2", b.llx, mmy, m1, mmy), line("l3", m2, mmy, b.urx, mmy)};
  } else {
    out = {line("l1", b.llx, b.lly, b.urx, b.lly), line("l4", b.llx, b.ury, b.urx, b.ury),
           line("l2", mmx, b.lly, mmx, m1), line("l3", mmx, m2, mmx, b.ury)};
  }
  if (tclr > 0) {
    out.push_back({"element", "add", "text", "t_" + id, "black", num(mmx), num(mmy), text});
  }
  const std::string_view back = horiz ? "arrowleft" : "arrowbottom";
  const std::string_view fwd = horiz ? "arrowright" : "arrowtop";
  out.push_back({"element", "configure", "l2_" + id, "flags", std::string{lo < m1 ? back : fwd}});
  out.push_back({"element", "configure", "l3_" + id, "flags", std::string{hi > m2 ? fwd : back}});
  return out;
}

CommandResult run_all(CommandContext& ctx, const std::vector<std::vector<std::string>>& commands) {
  for (const auto& words : commands) {
    const CommandResult r = run_forwarded(ctx, words);
    if (!r.ok()) {
      return r;
    }
  }
  return CommandResult::success();
}

CommandResult draw_ruler(CommandContext& ctx, editor::EditorState& editor, const std::string& text,
                         std::string_view orient) {
  UiState& ui = state(editor);
  ++ui.rulers;
  const auto box = internal_box(ctx, editor);
  if (!box.ok()) {
    return CommandResult::failure(box.error());
  }
  const InternalUnits internal{editor};
  return run_all(ctx, ruler_elements(box.value(), horizontal(box.value(), orient), text, ui.rulers));
}

CommandResult ruler(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.size() > 2) {
    return raise(codes::kUsage, "wrong # args: should be \"ruler ?text? ?orient?\"");
  }
  const std::string text = inv.args.empty() ? std::string{} : inv.args[0];
  const std::string orient = inv.args.size() > 1 ? inv.args[1] : std::string{"auto"};
  return draw_ruler(ctx, *editor.value(), text, orient);
}

std::string format_microns(double value) {
  std::array<char, 64> buf{};
  std::snprintf(buf.data(), buf.size(), "%g um", value);
  return buf.data();
}

CommandResult measure(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.size() > 1) {
    return raise(codes::kUsage, "wrong # args: should be \"measure ?orient?\"");
  }
  editor::EditorState& ed = *editor.value();
  const std::string orient = inv.args.empty() ? std::string{"auto"} : inv.args[0];
  const auto box = internal_box(ctx, ed);
  if (!box.ok()) {
    return CommandResult::failure(box.error());
  }
  const bool horiz = orient == "auto" ? horizontal(box.value(), orient) : orient == "horizontal";
  const BoxValues& b = box.value();
  const double extent = static_cast<double>(horiz ? b.urx - b.llx : b.ury - b.lly);
  return draw_ruler(ctx, ed, format_microns(microns_per_unit(ed) * extent), horiz ? "horizontal" : "vertical");
}

// unmeasure: the ruler index of an element name, as tools.tcl's unanchored
// `switch -regexp` takes it; empty when the name is not a ruler element.
std::string ruler_index(const std::string& name) {
  const auto digit = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };
  for (std::size_t i = 0; i + 3 < name.size(); ++i) {
    if (name[i] == 'l' && name[i + 1] >= '1' && name[i + 1] <= '4' && name[i + 2] == '_' && digit(name[i + 3])) {
      return name.substr(3);
    }
  }
  for (std::size_t i = 0; i + 2 < name.size(); ++i) {
    if (name[i] == 't' && name[i + 1] == '_' && digit(name[i + 2])) {
      return name.substr(2);
    }
  }
  return {};
}

CommandResult unmeasure(CommandContext& ctx, const Invocation& /*inv*/) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  const CommandResult inbox = run_forwarded(ctx, {"element", "inbox"});
  if (!inbox.ok()) {
    return inbox;
  }
  std::set<std::string> indices;
  for (const std::string& name : value_words(inbox.value)) {
    if (std::string index = ruler_index(name); !index.empty()) {
      indices.insert(std::move(index));
    }
  }
  std::vector<std::vector<std::string>> deletions;
  for (const std::string& m : indices) {
    for (const std::string_view prefix : {"t_", "l1_", "l2_", "l3_", "l4_"}) {
      deletions.push_back({"element", "delete", std::string{prefix} + m});
    }
  }
  return run_all(ctx, deletions);
}

// The center coordinate of genkey: integer division when both values are
// integers (Tcl expr), otherwise a real.
std::string center(const std::string& a, const std::string& b) {
  const auto ia = parse_int(a);
  const auto ib = parse_int(b);
  if (ia && ib) {
    return std::to_string(floor_div(*ia + *ib, 2));
  }
  return cmd::Value{(std::strtod(a.c_str(), nullptr) + std::strtod(b.c_str(), nullptr)) / 2.0}.to_string();
}

CommandResult genkey(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.empty() || inv.args.size() > 2) {
    return raise(codes::kUsage, "wrong # args: should be \"genkey layer ?keysize?\"");
  }
  const std::string& layer = inv.args[0];
  const std::string size = inv.args.size() > 1 ? inv.args[1] : std::string{"4"};
  CommandResult r = run_all(ctx, {{"box", "size", size, size}, {"paint", layer}});
  if (!r.ok()) {
    return r;
  }
  UiState& ui = state(*editor.value());
  const std::string id = "key" + std::to_string(++ui.keys);
  r = run_forwarded(ctx, {"box", "move", "e", size});
  const CommandResult values = r.ok() ? run_nested(ctx, {"box", "values"}) : r;
  if (!values.ok()) {
    return values;
  }
  const std::vector<std::string> v = value_words(values.value);
  if (v.size() < 4) {
    return CommandResult::failure(diag::make(codes::kUsage, "box values: " + values.value.to_string()));
  }
  return run_all(ctx, {{"element", "add", "text", id, "white", center(v[2], v[0]), center(v[3], v[1]), layer},
                       {"element", "configure", id, "flags", "east"}});
}

}  // namespace

bool register_measure_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "ruler", "ruler ?text? ?auto|horizontal|vertical?",
                "Draws a dimension marker (drawing elements) inside the box, with the text in its middle.", ruler);
  ok = add(registry, "measure", "measure ?auto|horizontal|vertical?",
           "Draws a ruler labelled with the box width or height in microns.", measure) &&
       ok;
  ok = add(registry, "unmeasure", "unmeasure", "Deletes every ruler with an element inside the box.", unmeasure) &&
       ok;
  ok = add(registry, "genkey", "genkey layer ?keysize?",
           "Paints a keysize square of the layer, moves the box east and labels the key with the layer name.",
           genkey) &&
       ok;
  return ok;
}

}  // namespace hefesto::ui::detail
