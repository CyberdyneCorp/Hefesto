// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The highlight layer (capability ui-feedback-and-highlights, requirement
// "Highlight layer and clients"; design.md D6). Per layout window it keeps
// the screen areas whose highlights must be erased and the root areas whose
// highlights must be redrawn; at the end of every command
// (editor::post_command) redraw() goes through each window: it erases the
// pending erase areas and calls each highlight client, in registration
// order, over the pending redraw areas. Clients do not draw pixels: each one
// appends display::HighlightItem primitives (root coordinates) to the
// window's display::HighlightScene, which the renderer draws over the layout
// (render::to_batches) and the shell's overlay completes with text.
//
// Built-in clients, in Magic's registration order: box, feedback, user
// elements, crosshair, then selection (registered on its first display).
// Later areas (wiring preview, rulers) add theirs with add_client(); at most
// 10 clients exist. The `*watch` tile overlay is not a client: it is part of
// the window redisplay and forces a full redraw (view/tile_overlay.hpp).
// (Magic ref: dbwind/DBWhlights.c:86-450, dbwind/DBWprocs.c:1068-1117,
// select/selDisplay.c:90-370, utils/main.c:742-797)
//
// Owner: VIEW (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::view {

/// "There SHALL be at most 10 highlight clients."
inline constexpr std::size_t kMaxHighlightClients = 10;

/// Magic's GR_CROSSSIZE: half the size of the cross drawn for points, in pixels.
inline constexpr std::int32_t kCrossSize = 5;

/// What a client draws for: the session, the window and the root area whose
/// highlights must be redrawn (the whole surface for a full rebuild).
struct HighlightContext {
  const editor::EditorState& editor;
  const LayoutWindow& window;
  db::Rect area;
};

/// A highlight client appends its items overlapping `ctx.area` to `out`.
using HighlightClient = std::function<void(const HighlightContext& ctx, display::HighlightScene& out)>;

/// The highlight work of one window at the end of a command.
struct HighlightFrame {
  std::vector<ScreenRect> erase;  // screen areas whose highlights were erased
  std::vector<db::Rect> redraw;   // root areas whose highlights were redrawn
  display::HighlightScene scene;  // what the clients drew there (styles resolved)
};

class HighlightLayer {
 public:
  /// Registers the built-in clients box, feedback, user elements and
  /// crosshair (the selection client is registered on its first display).
  HighlightLayer();

  /// DBWHLRedraw: requests a redraw of `area` (root coordinates of `root`)
  /// in every window showing `root`; with `erase`, the old highlights are
  /// erased first. The window areas are derived at the end of the command
  /// (highlight_areas()).
  void request(const db::CellDef* root, const db::Rect& area, bool erase);
  /// The same for one window only (DBWHLRedrawPrepWindow).
  void request_window(WindowId window, const db::Rect& area, bool erase);

  /// Registers a client after the existing ones; false past
  /// kMaxHighlightClients or when `name` is taken.
  bool add_client(std::string name, HighlightClient client);
  [[nodiscard]] std::size_t client_count() const noexcept { return clients_.size(); }
  /// Client names in registration (= drawing) order.
  [[nodiscard]] std::vector<std::string> client_names() const;

  /// The areas requested for `root` (not yet grown), or nullptr.
  [[nodiscard]] const std::vector<db::Rect>* pending(const db::CellDef* root) const;
  [[nodiscard]] bool has_pending() const noexcept { return !by_root_.empty() || !by_window_.empty(); }

  /// The end-of-command pass: for each window with pending areas, runs the
  /// clients over them, stores the result as that window's frame, notifies
  /// Change::highlights and clears the pending areas. A window with a
  /// `*watch` overlay and any change gets a whole-window redisplay.
  void redraw(editor::EditorState& editor);
  /// Scaffold form kept for editor::post_command until it passes the
  /// session (open item): notifies the windows with pending areas and clears
  /// them, without running the clients.
  void redraw(WindowManager& windows);

  /// The last frame of `window` (nullptr when none), and taking it.
  [[nodiscard]] const HighlightFrame* frame(WindowId window) const;
  [[nodiscard]] HighlightFrame take_frame(WindowId window);

  /// Runs every client over `area` of `window` (the full rebuild a retained
  /// renderer uses after Change::highlights). Registers the selection client
  /// on the first non-empty selection shown.
  [[nodiscard]] display::HighlightScene build(const editor::EditorState& editor, const LayoutWindow& window,
                                              const db::Rect& area);
  /// The same over the window's whole surface.
  [[nodiscard]] display::HighlightScene build(const editor::EditorState& editor, const LayoutWindow& window);

 private:
  struct Client {
    std::string name;
    HighlightClient draw;
  };
  struct Request {
    db::Rect area;
    bool erase = false;
  };
  void ensure_selection_client(const editor::EditorState& editor);
  [[nodiscard]] std::vector<Request> requests_for(const LayoutWindow& window) const;

  std::vector<Client> clients_;
  bool selection_registered_ = false;
  std::map<const db::CellDef*, std::vector<db::Rect>> by_root_areas_;  // for pending()
  std::map<const db::CellDef*, std::vector<Request>> by_root_;
  std::map<WindowId, std::vector<Request>> by_window_;
  std::map<WindowId, HighlightFrame> frames_;
};

/// What one redraw request means in `window` (dbwhlRedrawFunc): a
/// zero-width or zero-height area is first widened by one unit on each
/// side; with `erase`, the screen area grown by the window's label extent
/// and the cross size is erased; the redraw area is that area grown once
/// more, back in root coordinates, plus one unit.
struct HighlightAreas {
  std::optional<ScreenRect> erase;
  db::Rect redraw;
};
[[nodiscard]] HighlightAreas highlight_areas(const LayoutWindow& window, const db::Rect& area, bool erase);

// --- built-in clients (exposed for tests and for front ends that compose) --

/// The box: its outline in the box style (1), medium highlights (2) when the
/// box is outside the edit cell, a cross for a zero-size box.
void draw_box_highlights(const HighlightContext& ctx, display::HighlightScene& out);
/// The feedback areas of the window's root ("Feedback display").
void draw_feedback_highlights(const HighlightContext& ctx, display::HighlightScene& out);
/// The window root's display elements (rectangles, lines, texts).
void draw_element_highlights(const HighlightContext& ctx, display::HighlightScene& out);
/// The crosshair: a horizontal and a vertical line through the point.
void draw_crosshair_highlights(const HighlightContext& ctx, display::HighlightScene& out);
/// The selection in solid white outlines (outline highlights, 6): the edges
/// between different material, split-tile diagonals, label rectangles and
/// the bounding boxes and names of selected cells, offset by the floating
/// selection's offset. Only in windows whose root is the selection root.
void draw_selection_highlights(const HighlightContext& ctx, display::HighlightScene& out);

}  // namespace hefesto::view
