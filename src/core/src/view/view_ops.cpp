// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// View commands `view`, `xview`, `zoom`, `scroll`, `center` and `findbox`
// (capability ui-windows-and-views). Ported from Magic 8.3.694
// windows/windCmdSZ.c (windViewCmd, windXviewCmd, windZoomCmd,
// windScrollCmd), windows/windCmdAM.c (windCenterCmd) and commands/CmdFI.c
// (CmdFindBox), Copyright (C) 1985, 1990 Regents of the University of
// California, used under Magic's permissive license (see
// THIRD_PARTY_NOTICES.md).

#include <array>
#include <sstream>
#include <vector>

#include "hefesto/db/transform.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "view/command_util.hpp"

namespace hefesto::view::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

/// WindMove plus the minimum-scale warning and the observer notification.
CommandResult move(CommandContext& ctx, Target& t, const db::Rect& area) {
  t.window.move_to(area);
  if (t.window.mapping().at_minimum) {
    ctx.emit(diag::make(codes::kMinimumScale, "Warning:  At minimum scale!"));
  }
  t.editor.windows.notify(t.window.id(), Change::view);
  return CommandResult::success();
}

db::Rect root_bbox(const LayoutWindow& w) {
  return w.root() != nullptr ? w.root()->bbox() : db::Rect{0, 0, 1, 1};
}

/// The four values of `view llx lly urx ury` (or one argument holding them),
/// parsed as x, y, x, y (Q-UI-18).
std::optional<db::Rect> view_area(const std::vector<std::string>& args, const tech::GridScale& grid) {
  std::vector<std::string> words = args;
  if (args.size() == 1) {
    std::istringstream in(args.front());
    words.clear();
    for (std::string w; in >> w;) {
      words.push_back(w);
    }
  }
  if (words.size() != 4) {
    return std::nullopt;
  }
  std::array<db::Coord, 4> v{};
  for (std::size_t i = 0; i < 4; ++i) {
    const auto c = parse_coord(words[i], grid);
    if (!c) {
      return std::nullopt;
    }
    v[i] = *c;
  }
  return db::Rect{v[0], v[1], v[2], v[3]};
}

// The Tcl result of `view get` / `view bbox`: the four coordinates as a list.
cmd::Value rect_list(const db::Rect& r, const tech::GridScale& grid) {
  return cmd::Value{cmd::Value::List{format_coord(r.x0, grid), format_coord(r.y0, grid), format_coord(r.x1, grid),
                                     format_coord(r.y1, grid)}};
}

CommandResult view_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  const auto& grid = t.value().editor.grid;
  if (inv.args.empty()) {
    return move(ctx, t.value(), view_frame(root_bbox(t.value().window)));
  }
  if (inv.args.size() == 1 && inv.args[0] == "get") {
    return CommandResult::success(rect_list(t.value().window.mapping().surface, grid));
  }
  if (inv.args.size() == 1 && inv.args[0] == "bbox") {
    return CommandResult::success(rect_list(root_bbox(t.value().window), grid));
  }
  if (const auto area = view_area(inv.args, grid)) {
    return move(ctx, t.value(), *area);
  }
  return fail(codes::kUsage, "Usage: view [get|bbox|llx lly urx ury]");
}

CommandResult xview_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  if (!inv.args.empty()) {
    return fail(codes::kUsage, "Usage: xview");
  }
  t.value().window.clear_expansion();
  t.value().editor.windows.notify(t.value().window.id(), Change::expansion);
  return move(ctx, t.value(), view_frame(root_bbox(t.value().window)));
}

CommandResult zoom_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  if (inv.args.size() != 1) {
    return fail(codes::kUsage, "Usage: zoom factor");
  }
  // atof(): a non-number reads as 0, which the range check rejects.
  const auto factor = static_cast<float>(parse_number(inv.args[0], true).value_or(0.0));
  if (factor <= 0.0F || factor >= 20.0F) {
    return fail(codes::kZoom, "zoom factor must be between 0 and 20.");
  }
  return move(ctx, t.value(), zoom_area(t.value().window.mapping().surface, factor));
}

/// The (x, y) sign of the content move for a direction (windScrollCmd).
db::Point direction_sign(int pos) {
  switch (pos) {
    case db::kNorth: return {0, -1};
    case db::kSouth: return {0, 1};
    case db::kEast: return {-1, 0};
    case db::kWest: return {1, 0};
    case db::kNorthEast: return {-1, -1};
    case db::kNorthWest: return {1, -1};
    case db::kSouthEast: return {-1, 1};
    case db::kSouthWest: return {1, 1};
    default: return {0, 0};
  }
}

struct ScrollAmount {
  db::Point size;
  bool fractional = true;
};

diag::Outcome<ScrollAmount> scroll_amount(const Target& t, const Invocation& inv) {
  const ViewMapping& m = t.window.mapping();
  db::Rect r{m.screen.x0, m.screen.y0, m.screen.x1, m.screen.y1};
  float amount = 0.5F;
  if (inv.args.size() == 2) {
    const auto x = parse_coord(inv.args[1], t.editor.grid);
    if (!x) {
      return diag::make(codes::kCoordinate, "Bad coordinate \"" + inv.args[1] + "\"");
    }
    return ScrollAmount{{*x, *x}, false};
  }
  if (inv.args.size() == 3) {
    const char unit = inv.args[2].empty() ? '\0' : inv.args[2][0];
    if (unit != 'w' && unit != 'l') {
      return diag::make(codes::kScroll, "Usage: scroll direction [amount [units]]\n  'units' must be one of 'w' "
                                        "(window) or 'l' (layout);");
    }
    if (unit == 'l') {
      r = root_bbox(t.window);
    }
    const auto value = parse_number(inv.args[1], true);
    if (!value) {
      return diag::make(codes::kScroll, "Usage: scroll direction [amount [units]]\n  'amount' is a fractional value.");
    }
    amount = static_cast<float>(*value);
  }
  return ScrollAmount{{static_cast<db::Coord>(static_cast<float>(r.x1 - r.x0) * amount),
                       static_cast<db::Coord>(static_cast<float>(r.y1 - r.y0) * amount)},
                      true};
}

CommandResult scroll_cmd(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty() || inv.args.size() > 3) {
    return fail(codes::kUsage, "Usage: scroll direction [amount [units]]");
  }
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  const int pos = db::parse_position(inv.args[0], false);
  if (pos < 0) {
    // GeoNameToPos(name, FALSE, TRUE) (Magic ref: utils/geometry.c:410-445)
    return fail(codes::kUsage, "\"" + inv.args[0] + "\" " + (pos == -1 ? "is ambiguous." : "is not a valid direction or position.") +
                                   "\nLegal directions/positions are:\n\tbl,bottom,br,center,d,dl,down,dr,e,east,left,n,ne,"
                                   "north,northeast,northwest,nw,right,s,se,south,southeast,southwest,sw,tl,top,tr,u,ul,up,"
                                   "ur,w,west");
  }
  const auto amount = scroll_amount(t.value(), inv);
  if (!amount) {
    return failure(amount.error());
  }
  const db::Point sign = direction_sign(pos);
  const db::Point p{sign.x * amount.value().size.x, sign.y * amount.value().size.y};
  LayoutWindow& w = t.value().window;
  w.set_mapping(amount.value().fractional ? w.mapping().scrolled({0, 0}, p) : w.mapping().scrolled({-p.x, -p.y}, {0, 0}));
  t.value().editor.windows.notify(w.id(), Change::view);
  return CommandResult::success();
}

/// The surface of the same size centred on `p` (windCenterCmd, CmdFindBox).
db::Rect centred(const db::Rect& old, db::Point p) {
  db::Rect r;
  r.x0 = p.x - (old.x1 - old.x0) / 2;
  r.x1 = r.x0 - old.x0 + old.x1;
  r.y0 = p.y - (old.y1 - old.y0) / 2;
  r.y1 = r.y0 - old.y0 + old.y1;
  return r;
}

CommandResult center_axis(CommandContext& ctx, Target& t, const Invocation& inv) {
  const auto frac = parse_number(inv.args[1], true);
  if (!frac) {
    return fail(codes::kCoordinate, "Must specify a fractional value.");
  }
  const db::Rect b = root_bbox(t.window);
  const db::Rect& s = t.window.mapping().surface;
  db::Point offset{0, 0};
  if (inv.args[0][0] == 'h') {
    offset.x = static_cast<db::Coord>(b.x0 + *frac * (b.x1 - b.x0) - (s.x1 + s.x0) / 2);
  } else {
    offset.y = static_cast<db::Coord>(b.y0 + *frac * (b.y1 - b.y0) - (s.y1 + s.y0) / 2);
  }
  t.window.set_mapping(t.window.mapping().scrolled(offset, {0, 0}));
  t.editor.windows.notify(t.window.id(), Change::view);
  (void)ctx;
  return CommandResult::success();
}

CommandResult center_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  const db::Rect& s = t.value().window.mapping().surface;
  if (inv.args.empty()) {
    // No pointer in the core: the point under the cursor is the view centre
    // unless the shell recentres through `center x y`.
    return move(ctx, t.value(), centred(s, db::Point{(s.x0 + s.x1) / 2, (s.y0 + s.y1) / 2}));
  }
  if (inv.args.size() != 2) {
    return fail(codes::kUsage, "Usage: center [x y]\n       center horizontal|vertical f");
  }
  if (!inv.args[0].empty() && (inv.args[0][0] == 'h' || inv.args[0][0] == 'v')) {
    return center_axis(ctx, t.value(), inv);
  }
  const auto x = parse_int(inv.args[0]);
  const auto y = parse_int(inv.args[1]);
  if (!x || !y) {
    return fail(codes::kCoordinate, "Coordinates must be integer values");
  }
  return move(ctx, t.value(), centred(s, db::Point{static_cast<db::Coord>(*x), static_cast<db::Coord>(*y)}));
}

CommandResult findbox_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  if (!t.value().editor.box) {
    return fail(codes::kBox, "Put the box in a window first.");
  }
  const auto box = box_in_window(t.value().editor, t.value().window);
  if (!box) {
    return fail(codes::kBox, "The box is not in the same coordinate system as the window.");
  }
  if (inv.args.empty()) {
    const db::Point c{(box->x0 + box->x1) / 2, (box->y0 + box->y1) / 2};
    return move(ctx, t.value(), centred(t.value().window.mapping().surface, c));
  }
  if (inv.args.size() == 1 && inv.args[0] == "zoom") {
    return move(ctx, t.value(), box_frame(*box));
  }
  return fail(codes::kUsage, "Usage: findbox [zoom]");
}

}  // namespace

bool register_view_zoom(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "view", "view [get|bbox|llx lly urx ury]",
                "Frames the root cell (with a 10% margin), reports the view, or frames an area.", view_cmd);
  ok = add(registry, "zoom", "zoom factor",
           "Rescales the view about its centre: a factor above 1 zooms out, below 1 zooms in.", zoom_cmd) &&
       ok;
  return ok;
}

bool register_view_ops(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "xview", "xview", "Unexpands every instance in the window, then frames the root cell.",
                xview_cmd);
  ok = add(registry, "scroll", "scroll direction [amount [w|l]]", "Pans the view; the layout moves opposite to direction.",
           scroll_cmd) &&
       ok;
  ok = add(registry, "center", "center [x y] | center horizontal|vertical f", "Recentres the view on a point.",
           center_cmd) &&
       ok;
  ok = add(registry, "findbox", "findbox [zoom]", "Centres (or zooms) the view on the box.", findbox_cmd) && ok;
  return ok;
}

}  // namespace hefesto::view::detail
