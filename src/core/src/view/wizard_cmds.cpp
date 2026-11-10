// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The window-client and layout wizard commands VIEW provides (capability
// ui-wizard-and-debug): `*watch` (tile structure overlay, drawn by
// tile_overlay_scene), `*coord`, `*winddebug`, `*files`, `*crash`,
// `*grstats`, `*pause` and `*winddump`. Messages follow
// Magic 8.3.694 commands/CmdWizard.c and windows/windCmdAM.c, Copyright (C)
// 1985, 1990 Regents of the University of California, used under Magic's
// permissive license (see THIRD_PARTY_NOTICES.md). Owner: VIEW.

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if __has_include(<sys/stat.h>)
#include <sys/stat.h>
#define HEFESTO_HAVE_FSTAT 1
#endif

#include "hefesto/cmd/prompt.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/view/color_window.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "hefesto/view/tile_overlay.hpp"
#include "view/command_util.hpp"

namespace hefesto::view {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using detail::fail;
using detail::failure;
using detail::print;

// --- *watch ----------------------------------------------------------------------

/// The watched cell and its transform to the window's root (CmdWatch).
TileOverlay watch_target(const editor::EditorState& e, const LayoutWindow& w) {
  TileOverlay o;
  if (e.edit_cell != nullptr) {
    o.def = e.edit_cell;
    o.to_root = e.edit_root != nullptr ? e.edit_transform : db::kIdentity;
  } else {
    o.def = w.root();
  }
  return o;
}

CommandResult unknown_plane(const editor::EditorState& e, const std::string& name) {
  std::string text = "Unrecognized plane: " + name + ".  Legal names are:";
  for (tech::PlaneId p = 0; p < e.technology->plane_count(); ++p) {
    const std::string_view n = e.technology->plane_name(p);
    if (!n.empty()) {
      text += "\n\t" + std::string(n);
    }
  }
  return fail(codes::kWatchPlane, std::move(text));
}

CommandResult watch_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = detail::pointed(ctx, "Gee, you don't seem like a wizard!\nCursor not in a layout window.");
  if (!t) {
    return failure(t.error());
  }
  editor::EditorState& e = t.value().editor;
  LayoutWindow& w = t.value().window;
  TileOverlay o;
  for (std::size_t i = 1; i < inv.args.size(); ++i) {
    if (inv.args[i] == "demo") {
      o.demo = true;
    } else if (inv.args[i] == "types") {
      o.types = true;
    } else {
      return fail(codes::kUsage, "Gee, you don't sound like a wizard!\nUsage: " + inv.name + " [plane] [demo] [types]");
    }
  }
  if (inv.args.empty()) {
    e.tile_overlays.erase(w.id());
  } else {
    const auto plane = e.technology->find_plane(inv.args[0]);
    if (!plane.ok()) {
      return unknown_plane(e, inv.args[0]);
    }
    const TileOverlay target = watch_target(e, w);
    o.plane = *plane.id;
    o.def = target.def;
    o.to_root = target.to_root;
    e.tile_overlays[w.id()] = o;
  }
  w.mark_redisplay_all();  // WindAreaChanged(w, NULL)
  return CommandResult::success();
}

// --- *coord ----------------------------------------------------------------------

std::string rect_text(const db::Rect& r) {
  return "(" + std::to_string(r.x0) + "," + std::to_string(r.y0) + ")::(" + std::to_string(r.x1) + "," +
         std::to_string(r.y1) + ")";
}

/// EditRootDef: the root the edit cell is edited in.
const db::CellDef* edit_root_def(const editor::EditorState& e) {
  return e.edit_root != nullptr ? e.edit_root : e.edit_cell;
}

/// One `*coord` line: the root rectangle and, when `root` is the edit root,
/// the same rectangle in edit coordinates.
std::string coord_line(const editor::EditorState& e, std::string_view label, const db::CellDef* root,
                       const db::Rect& r) {
  std::string line = std::string(label) + "\tr=" + rect_text(r);
  if (root != nullptr && root == edit_root_def(e)) {
    line += "\te=" + rect_text(db::apply(db::invert(e.edit_transform), r));
  }
  return line;
}

/// The box and the cell it belongs to (ToolGetBox).
std::optional<std::pair<const db::CellDef*, db::Rect>> box_of(const editor::EditorState& e) {
  if (!e.box) {
    return std::nullopt;
  }
  const db::CellDef* root = e.box_root != nullptr ? e.box_root : e.edit_cell;
  return std::make_pair(root, *e.box);
}

CommandResult coord_cmd(CommandContext& ctx, const Invocation&) {
  auto s = detail::session(ctx);
  if (!s) {
    return failure(s.error());
  }
  const editor::EditorState& e = *s.value();
  std::string text;
  const LayoutWindow* point_window = nullptr;
  if (e.command_point) {
    point_window = e.windows.find(e.command_point->window);
    if (point_window != nullptr) {
      text += coord_line(e, "Point:", point_window->root(), e.command_point->pixel_area) + "\n";
    }
  }
  if (const auto box = box_of(e)) {
    text += coord_line(e, "Box:", box->first, box->second) + "\n";
  }
  const db::CellDef* root = point_window != nullptr ? point_window->root() : edit_root_def(e);
  const db::Rect root_rect = point_window != nullptr && root != nullptr ? root->bbox() : db::Rect{0, 0, 1, 1};
  text += coord_line(e, "Root cell:", root, root_rect) + "\n";
  if (e.edit_cell != nullptr) {
    const db::Rect edit = e.edit_cell->bbox();
    text += "Edit cell:";
    if (root == edit_root_def(e)) {
      text += "\tr=" + rect_text(db::apply(e.edit_transform, edit));
    }
    text += "\te=" + rect_text(edit);
  }
  if (!text.empty() && text.back() == '\n') {
    text.pop_back();
  }
  print(ctx, std::move(text));
  return CommandResult::success();
}

// --- *winddebug, *files ------------------------------------------------------------

CommandResult winddebug_cmd(CommandContext& ctx, const Invocation& inv) {
  if (!inv.args.empty()) {
    return fail(codes::kUsage, "Usage:  *winddebug");
  }
  auto s = detail::session(ctx);
  if (!s) {
    return failure(s.error());
  }
  WindowManager& wm = s.value()->windows;
  wm.set_print_commands(!wm.print_commands());
  // Magic writes it with TxError (stderr) and fails nothing.
  ctx.emit(diag::make(codes::kErrorReport, std::string("Window command debugging set to ") +
                                          (wm.print_commands() ? "TRUE" : "FALSE")));
  return CommandResult::success();
}

#ifdef HEFESTO_HAVE_FSTAT
std::string_view file_type(mode_t mode) {
  switch (mode & S_IFMT) {
    case S_IFDIR:
      return "directory";
    case S_IFCHR:
      return "character special";
    case S_IFBLK:
      return "block special";
    case S_IFREG:
      return "regular";
    case S_IFLNK:
      return "symbolic link";
    case S_IFSOCK:
      return "socket";
    default:
      return "unknown";
  }
}
#endif

CommandResult files_cmd(CommandContext& ctx, const Invocation&) {
  constexpr int kNumFd = 20;
  int open = 0;
  int unopen = 0;
  std::string text;
#ifdef HEFESTO_HAVE_FSTAT
  for (int fd = 0; fd < kNumFd; ++fd) {
    struct stat buf {};
    if (fstat(fd, &buf) != 0) {
      if (errno == EBADF) {
        ++unopen;
      } else {
        text += "file descriptor " + std::to_string(fd) + ": " + std::strerror(errno) + "\n";
      }
      continue;
    }
    text += "file descriptor " + std::to_string(fd) + ": open  (type: '" + std::string(file_type(buf.st_mode)) +
            "', inode number " + std::to_string(buf.st_ino) + ")\n";
    ++open;
  }
#else
  unopen = kNumFd;
#endif
  text += std::to_string(open) + " open files, " + std::to_string(unopen) + " unopened file descriptors left";
  ctx.emit(diag::make(codes::kErrorReport, std::move(text)));  // TxError, as Magic (UI-G-081)
  return CommandResult::success();
}


// --- *crash, *grstats, *pause, *winddump (windCmdAM.c, windCmdNR.c, windDebug.c) ---

CommandResult crash_cmd(CommandContext& ctx, const Invocation& inv) {
  if (!inv.args.empty()) {
    return fail(codes::kUsage, "Usage:  *crash");
  }
  print(ctx, "OK -- crashing...");
  std::fflush(nullptr);
  std::abort();  // niceabort(): a deliberate core dump (it also exercises crash recovery)
}

std::string screen_text(const ScreenRect& r) {
  return "(" + std::to_string(r.x0) + ", " + std::to_string(r.y0) + ") (" + std::to_string(r.x1) + ", " +
         std::to_string(r.y1) + ")";
}

std::string surface_text(const db::Rect& r) {
  return "(" + std::to_string(r.x0) + ", " + std::to_string(r.y0) + ") (" + std::to_string(r.x1) + ", " +
         std::to_string(r.y1) + ")";
}

CommandResult grstats_cmd(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty() || inv.args.size() > 2) {
    return fail(codes::kUsage, "Usage: grstats num [ style ]");
  }
  const auto count = detail::parse_int(inv.args[0]);
  if (!count || (inv.args.size() == 2 && !detail::parse_int(inv.args[1]))) {
    return fail(codes::kUsage, "Count & style must be numeric");
  }
  auto target = detail::pointed(ctx);
  if (!target) {
    return failure(target.error());
  }
  // The headless core has no graphics device: each pass marks the whole
  // window for redisplay, and no clip boxes are counted.
  const auto start = std::chrono::steady_clock::now();
  for (std::int64_t i = 0; i < *count; ++i) {
    target.value().window.mark_redisplay_all();
  }
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
  const long long elapsed = static_cast<long long>(us.count());
  print(ctx, "[" + std::to_string(elapsed) + " uS elapsed]\n0 rectangles, " + std::to_string(elapsed) +
                 " uS, " + std::to_string(elapsed) + " uS/rectangle, " +
                 std::to_string(1000000 / std::max<long long>(1, elapsed)) + " rects/sec");
  return CommandResult::success();
}

CommandResult pause_cmd(CommandContext& ctx, const Invocation& inv) {
  auto s = detail::session(ctx);
  if (!s) {
    return failure(s.error());
  }
  std::string question;
  for (std::size_t i = 0; i < inv.args.size(); ++i) {
    question += inv.args[i] + (i + 1 == inv.args.size() ? "  " : " ");
  }
  question += "Pausing: type <cr> to continue: ";
  print(ctx, question);
  static const std::array<std::string, 1> kContinue{"continue"};
  (void)cmd::ask(s.value()->prompt, question, kContinue, 0);  // TxGetLine: the front end waits
  return CommandResult::success();
}

CommandResult winddump_cmd(CommandContext& ctx, const Invocation& inv) {
  if (!inv.args.empty()) {
    return fail(codes::kUsage, "Usage:  *winddump");
  }
  auto s = detail::session(ctx);
  if (!s) {
    return failure(s.error());
  }
  // windDump without the client function pointers, which mean nothing outside Magic.
  std::string text = "\n\n------------ Clients ----------";
  for (const std::string& client : ctx.registry().clients()) {
    char line[64];
    std::snprintf(line, sizeof line, "\n'%10s'", client.c_str());
    text += line;
  }
  text += "\n";
  s.value()->windows.for_each([&](LayoutWindow& w) {
    const ViewMapping& m = w.mapping();
    text += "\nWindow " + std::to_string(w.id()) + ": '" + w.caption() + "'\n  Client layout\n  Screen area " +
            screen_text(m.screen) + "\n  Frame area " + screen_text(w.frame()) + "\n  Surface area " +
            surface_text(m.surface) + " \n";
  });
  for (const ColorWindow& cw : s.value()->color_map.windows) {
    text += "\nWindow " + std::to_string(cw.id) + ": '" + cw.caption + "'\n  Client color\n  Frame area " +
            screen_text(cw.frame) + "\n";
  }
  print(ctx, std::move(text));
  return CommandResult::success();
}

}  // namespace

// --- tile overlay scene --------------------------------------------------------------

display::HighlightScene tile_overlay_scene(const editor::EditorState& editor, const LayoutWindow& window,
                                           const db::Rect& area) {
  display::HighlightScene scene;
  const auto it = editor.tile_overlays.find(window.id());
  if (it == editor.tile_overlays.end() || it->second.def == nullptr ||
      it->second.plane >= it->second.def->plane_count()) {
    return scene;
  }
  const TileOverlay& o = it->second;
  const db::Rect local = db::apply(db::invert(o.to_root), area);
  const display::StyleSet* styles = editor.styles ? &*editor.styles : nullptr;
  (void)o.def->plane(o.plane).enumerate(local, tech::TypeMask::all(), [&](const db::TileView& t) {
    const db::Rect r = db::apply(o.to_root, t.area);
    display::HighlightItem box;
    box.shape = display::HighlightShape::outline;
    box.style = kStyleDrawTile;
    box.rect = r;
    display::HighlightItem label = box;
    label.shape = display::HighlightShape::text;
    label.text = o.types ? std::string(editor.technology->type_name(t.type, tech::NameForm::short_name))
                         : std::to_string(t.area.x0) + "," + std::to_string(t.area.y0);
    scene.items.push_back(box);
    scene.items.push_back(label);
    return true;
  });
  for (display::HighlightItem& item : scene.items) {
    display::resolve_highlight(item, styles);
  }
  return scene;
}

namespace detail {

bool register_wizard_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "*watch", "*watch [plane] [demo] [types]",
                "Draws the tile structure of a plane of the edit cell over the layout (developer overlay).", watch_cmd);
  ok = add(registry, "*coord", "*coord", "Prints the point, box, root and edit cell rectangles.", coord_cmd) && ok;
  ok = add(registry, "*winddebug", "*winddebug", "Toggles printing of every dispatched command.", winddebug_cmd) && ok;
  ok = add(registry, "*files", "*files", "Lists the status of the open file descriptors.", files_cmd) && ok;
  ok = add(registry, "*crash", "*crash", "Prints OK -- crashing... and aborts with a core dump.", crash_cmd) && ok;
  ok = add(registry, "*grstats", "*grstats num [style]", "Redraws the window num times and prints the time taken.",
           grstats_cmd) &&
       ok;
  ok = add(registry, "*pause", "*pause [args]", "Prints the arguments and waits for a line.", pause_cmd) && ok;
  ok = add(registry, "*winddump", "*winddump", "Prints the clients and the window list.", winddump_cmd) && ok;
  return ok;
}

}  // namespace detail

}  // namespace hefesto::view
