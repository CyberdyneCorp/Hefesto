// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `feedback` command (capability ui-feedback-and-highlights, requirement
// "feedback command"; Q-UI-23 style names, Q-UI-24 `i`-suffixed save file).
// Messages follow Magic 8.3.694 commands/CmdFI.c (CmdFeedback), Copyright (C)
// 1985, 1990 Regents of the University of California, used under Magic's
// permissive license (see THIRD_PARTY_NOTICES.md). Owner: VIEW.

#include <array>
#include <cmath>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/db/polygon.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "hefesto/view/feedback.hpp"
#include "view/command_util.hpp"

namespace hefesto::view::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

enum Option : int { kAdd, kClear, kCount, kFind, kHelp, kSave, kWhy };
constexpr std::array<std::string_view, 7> kOptions = {"add", "clear", "count", "find", "help", "save", "why"};

constexpr std::array<std::string_view, 7> kOptionHelp = {
    "add text [style] [points...]\tcreate new feedback area over box",
    "clear [substring]\t\tclear all or selected feedback info",
    "count\t\t\t\tcount # feedback entries",
    "find [nth]\t\t\tput box over next [or nth] entry",
    "help\t\t\t\tprint this message",
    "save file\t\t\tsave feedback areas in file",
    "why\t\t\t\tprint all feedback messages under box",
};

/// The five named styles of `feedback add` and `feedback save`, in Lookup order.
constexpr std::array<std::string_view, 5> kStyleNames = {"dotted", "medium", "outline", "pale", "solid"};
constexpr std::array<std::size_t, 5> kStyleIndices = {kStyleDottedHighlights, kStyleMediumHighlights,
                                                      kStyleOutlineHighlights, kStylePaleHighlights,
                                                      kStyleSolidHighlights};
constexpr std::size_t kTechBeginStyles = 52;  // TECHBEGINSTYLES

CommandResult bad_usage(CommandContext& ctx) {
  print(ctx, "Wrong number of arguments for \"feedback\" command.\nType \":feedback help\" for help.");
  return CommandResult::success();
}

/// GrStyleTable[style].outline, with Magic's fixed highlight styles when the
/// session has no display styles.
std::int32_t style_outline(const editor::EditorState& e, std::size_t style) {
  if (e.styles && style < e.styles->table.styles.size()) {
    return e.styles->table.styles[style].outline;
  }
  if (style == kStyleOutlineHighlights) return 0xFF;
  if (style == kStyleDottedHighlights) return 0xCC;
  return 0;
}

/// The window the box is in and the box in its root (ToolGetBoxWindow).
std::optional<std::pair<LayoutWindow*, db::Rect>> box_window(const editor::EditorState& e) {
  std::optional<std::pair<LayoutWindow*, db::Rect>> found;
  e.windows.for_each([&](LayoutWindow& w) {
    if (!found) {
      if (const auto box = box_in_window(e, w)) found = std::make_pair(&w, *box);
    }
  });
  return found;
}

/// The style argument of `feedback add`; nullopt (and the error emitted)
/// when it is not a style; `points_from` is set to 3 for a numeric argument.
struct StyleArg {
  std::size_t style = kStylePaleHighlights;
  std::size_t points_from = 3;  // argument index of the first point (Magic's pstart - 1)
};

diag::Outcome<StyleArg> parse_style(const editor::EditorState& e, const std::vector<std::string>& args) {
  StyleArg out;
  if (args.size() < 3) {
    return out;
  }
  const std::string& word = args[2];
  const int i = lookup_nocase(word, kStyleNames);
  if (i >= 0) {
    out.style = kStyleIndices[static_cast<std::size_t>(i)];
    return out;
  }
  if (parse_number(word, true)) {
    out.points_from = 2;
    return out;
  }
  const auto index = e.styles ? display::style_from_name(e.styles->table, word) : std::nullopt;
  if (!index) {
    return diag::make(codes::kFeedback, word + " isn't a valid display style.  Try one of:\n"
                                               "    dotted, pale, medium, solid, outline,\n"
                                               "    or a long name from the .dstyle file");
  }
  if (*index < 1) {  // Q-UI-23: technology styles (>= 52) are accepted
    return diag::make(codes::kFeedback,
                      "Numbered styles must be between 1 and " + std::to_string(kTechBeginStyles - 1));
  }
  out.style = *index;
  return out;
}

/// cmdScaleCoord(w, arg, FALSE, is_x, FEEDMAGNIFY).
db::Coord scaled_point_coord(CommandContext& ctx, const editor::EditorState& e, const std::string& text, bool is_x) {
  const auto v = cmd::parse_coordinate(text, coordinate_units(e), is_x ? cmd::Axis::x : cmd::Axis::y, false,
                                       &ctx.sink());
  if (!v) {
    ctx.emit(diag::make(codes::kCoordinate, "Coordinate value cannot be parsed:  assuming 0"));
    return 0;
  }
  return static_cast<db::Coord>(std::round(v.value() * kFeedbackMagnify));
}

/// A line (or point) from a to b, with the TT_SIDE / TT_DIRECTION hack for
/// an angled line.
FeedbackStyle line_style(std::size_t style, db::Point a, db::Point b) {
  FeedbackStyle s{style};
  if (a.x != b.x && a.y != b.y) {
    s.side = true;
    s.direction = (a.x > b.x && a.y < b.y) || (a.x < b.x && a.y > b.y);
  }
  return s;
}

struct AddTarget {
  editor::EditorState& editor;
  const db::CellDef& root;
  std::string text;
};

void add_area(AddTarget& t, const db::Rect& r, std::int32_t scale, FeedbackStyle style) {
  (void)t.editor.feedback.add(t.editor.windows, t.root, r, scale, style, t.text, &t.editor.highlights);
}

void add_lines(AddTarget& t, std::size_t style, const std::vector<db::Point>& pts) {
  for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
    add_area(t, db::canonical(db::Rect{pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y}), kFeedbackMagnify,
             line_style(style, pts[i], pts[i + 1]));
  }
}

CommandResult add_polygon(AddTarget& t, std::size_t style, const std::vector<db::Point>& pts) {
  auto pieces = db::decompose_polygon(pts);
  if (!pieces) {
    return failure(pieces.error());
  }
  for (const db::PolygonPiece& piece : pieces.value()) {
    FeedbackStyle s{style};
    if (piece.triangle) {
      s.diagonal = true;
      s.side = piece.triangle->right_side;
      s.direction = piece.triangle->diagonal == db::Diagonal::nw_se;
    }
    add_area(t, piece.area, kFeedbackMagnify, s);
  }
  if (pieces.value().empty()) {
    // Degenerate polygon: its edges as lines, in an outlined style.
    add_lines(t, style_outline(t.editor, style) == 0 ? kStyleOutlineHighlights : style, pts);
  }
  return CommandResult::success();
}

CommandResult add_points(CommandContext& ctx, editor::EditorState& e, const std::vector<std::string>& args,
                         std::size_t from, std::size_t style) {
  const LayoutWindow* w = e.windows.current();
  if (w == nullptr || w->root() == nullptr) {
    return CommandResult::success();
  }
  std::vector<db::Point> pts;
  for (std::size_t i = from; i + 1 < args.size(); i += 2) {
    pts.push_back(db::Point{scaled_point_coord(ctx, e, args[i], true), scaled_point_coord(ctx, e, args[i + 1], false)});
  }
  AddTarget t{e, *w->root(), args[1]};
  if (pts.size() > 2) {
    return add_polygon(t, style, pts);
  }
  // One point or one line: the style must be outlined to be visible.
  const std::size_t s = style_outline(e, style) == 0 ? kStyleOutlineHighlights : style;
  const db::Point b = pts.size() == 2 ? pts[1] : pts[0];
  add_area(t, db::canonical(db::Rect{pts[0].x, pts[0].y, b.x, b.y}), kFeedbackMagnify, line_style(s, pts[0], b));
  return CommandResult::success();
}

CommandResult add_cmd(CommandContext& ctx, editor::EditorState& e, const std::vector<std::string>& args) {
  const auto style = parse_style(e, args);
  if (!style) {
    return failure(style.error());
  }
  const std::size_t from = style.value().points_from;
  if (args.size() > from) {
    if (((args.size() - from) & 1U) != 0) {
      return bad_usage(ctx);
    }
    return add_points(ctx, e, args, from, style.value().style);
  }
  if (args.size() < 2) {
    return bad_usage(ctx);
  }
  const auto bw = box_window(e);
  if (!bw || bw->first->root() == nullptr) {
    return CommandResult::success();
  }
  AddTarget t{e, *bw->first->root(), args[1]};
  add_area(t, bw->second, 1, FeedbackStyle{style.value().style});
  return CommandResult::success();
}

CommandResult find_cmd(CommandContext& ctx, editor::EditorState& e, const std::vector<std::string>& args) {
  FeedbackList& list = e.feedback;
  if (list.size() == 0) {
    print(ctx, "There are no feedback areas right now.");
    return CommandResult::success();
  }
  std::size_t nth = list.find_cursor() + 1;
  if (args.size() == 2) {
    const long long n = std::atoll(args[1].c_str());
    if (n <= 0 || static_cast<std::size_t>(n) > list.size()) {
      ctx.emit(diag::make(codes::kFeedbackRange,
                          "Sorry, but only feedback areas 1-" + std::to_string(list.size()) + " exist."));
      nth = 1;
    } else {
      nth = static_cast<std::size_t>(n);
    }
  } else if (nth > list.size()) {
    nth = 1;
  }
  list.set_find_cursor(nth);
  const FeedbackArea& a = list.areas()[nth - 1];
  if (e.box) {
    e.highlights.request(e.box_root != nullptr ? e.box_root : e.edit_cell, *e.box, true);
  }
  e.box = a.area;
  e.box_root = nullptr;
  e.windows.for_each([&](LayoutWindow& w) {
    if (w.root() == a.root) e.box_root = w.root();
  });
  e.highlights.request(a.root, a.area, false);
  print(ctx, "Feedback #" + std::to_string(nth) + ": " + a.message());
  return CommandResult::success();
}

std::string save_text(const FeedbackList& list) {
  std::string out;
  for (const FeedbackArea& a : list.areas()) {
    const db::Rect& r = a.area;
    out += "box " + std::to_string(r.x0) + "i " + std::to_string(r.y0) + "i " + std::to_string(r.x1) + "i " +
           std::to_string(r.y1) + "i\nfeedback add \"";
    for (const char c : a.message()) {
      if (c == '"') out += '\\';
      out += c;
    }
    out += '"';
    for (std::size_t j = 0; j < kStyleIndices.size(); ++j) {
      if (a.style == FeedbackStyle{kStyleIndices[j]}) {
        out += " " + std::string(kStyleNames[j]);
        break;
      }
    }
    out += '\n';
  }
  return out;
}

CommandResult save_cmd(CommandContext& ctx, const editor::EditorState& e, const std::string& file) {
  const auto written = ctx.services().files.write_atomic(file, save_text(e.feedback));
  if (!written) {
    return fail(codes::kFeedbackFile, "Can't open file " + file + ".");
  }
  return CommandResult::success();
}

CommandResult why_cmd(CommandContext& ctx, const editor::EditorState& e) {
  const auto bw = box_window(e);
  if (!bw) {
    return CommandResult::success();
  }
  const db::Rect& box = bw->second;
  std::set<std::string> seen;
  std::string text;
  for (const FeedbackArea& a : e.feedback.areas()) {
    const db::Rect& r = a.area;
    const bool overlap = box.x0 < r.x1 && r.x0 < box.x1 && box.y0 < r.y1 && r.y0 < box.y1;
    if (a.root == bw->first->root() && overlap && seen.insert(a.message()).second) {
      text += (text.empty() ? "" : "\n") + a.message();
    }
  }
  if (!text.empty()) {
    print(ctx, std::move(text));
  }
  return CommandResult::success();
}

CommandResult help_cmd(CommandContext& ctx) {
  std::string text = "Feedback commands have the form \"feedback option\",\nwhere option is one of:";
  for (const std::string_view line : kOptionHelp) {
    text += "\n" + std::string(line);
  }
  print(ctx, std::move(text));
  return CommandResult::success();
}

CommandResult run_option(CommandContext& ctx, editor::EditorState& e, int option, const std::vector<std::string>& a) {
  const std::size_t argc = a.size() + 1;  // Magic's tx_argc
  switch (option) {
    case kAdd:
      return add_cmd(ctx, e, a);
    case kClear:
      if (argc > 3) return bad_usage(ctx);
      (void)e.feedback.clear(argc == 3 ? std::optional<std::string_view>(a[1]) : std::nullopt, &e.highlights);
      e.feedback.set_find_cursor(0);
      return CommandResult::success();
    case kCount:
      if (argc != 2) return bad_usage(ctx);
      return CommandResult::success(cmd::Value{static_cast<std::int64_t>(e.feedback.size())});
    case kFind:
      return argc > 3 ? bad_usage(ctx) : find_cmd(ctx, e, a);
    case kHelp:
      return argc > 2 ? bad_usage(ctx) : help_cmd(ctx);
    case kSave:
      return argc != 3 ? bad_usage(ctx) : save_cmd(ctx, e, a[1]);
    default:
      return argc > 2 ? bad_usage(ctx) : why_cmd(ctx, e);
  }
}

CommandResult feedback_cmd(CommandContext& ctx, const Invocation& inv) {
  auto e = session(ctx);
  if (!e) {
    return failure(e.error());
  }
  if (inv.args.empty()) {
    return bad_usage(ctx);
  }
  const int option = lookup_nocase(inv.args[0], kOptions);
  if (option < 0) {
    // CmdFeedback's list (with `save` twice, Q-UI-23: replicated).
    return fail(codes::kFeedback, inv.args[0] +
                                      " isn't a valid feedback option.  Try one of:\n"
                                      "    add        find\n    clear      help\n    count      save\n    save");
  }
  return run_option(ctx, *e.value(), option, inv.args);
}

}  // namespace

bool register_feedback_command(cmd::CommandRegistry& registry) {
  return add(registry, "feedback", "feedback option [args]",
             "Adds, counts, finds, saves, explains or clears feedback areas (`feedback help`).", feedback_cmd);
}

}  // namespace hefesto::view::detail
