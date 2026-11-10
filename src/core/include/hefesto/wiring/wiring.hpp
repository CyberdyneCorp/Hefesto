// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wiring tool's data (capability wiring-tool; design.md "Area
// contracts" WIRE): the technology `wiring` section, read from the raw
// section the technology keeps (tech::Technology::raw_sections), the
// per-session wire state (current type, width, last leg direction, tool
// tracking) and the pure geometry of legs, contacts, chunks and centerline
// paths that the `wire` command paints.
// (Magic ref: wiring/wireTech.c, wiring/wireOps.c, wiring/wireUndo.c,
// select/selCreate.c:569-671 selFindChunk, cif/CIFrdpt.c:339-603)
//
// Owner: WIRE (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::undo {
class Event;
}

namespace hefesto::wiring {

// ---- wiring technology section ---------------------------------------------

/// One `contact` line (seven or nine fields). Dimensions are in internal
/// units after the grid scaling of "wiring technology section" (multiply,
/// then truncating divide); the `scalefactor` division is applied when the
/// contact is used (WiringSection::scaled).
struct WiringContact {
  tech::TileType type = tech::kSpace;
  db::Coord size = 0;
  tech::TileType layer1 = tech::kSpace;
  db::Coord surround1 = 0;
  db::Coord extend1 = 0;
  tech::TileType layer2 = tech::kSpace;
  db::Coord surround2 = 0;
  db::Coord extend2 = 0;

  friend bool operator==(const WiringContact&, const WiringContact&) = default;
};

struct WiringSection {
  /// In REVERSE file order (each line is prepended): the search order.
  std::vector<WiringContact> contacts;
  /// `scalefactor` (default 1; the last line wins, as in Magic): divides
  /// the contact dimensions, rounding up, when a contact is placed.
  std::int64_t scale_factor = 1;

  /// `contact` with every dimension divided by scale_factor, rounding up
  /// (WireAddContact's conSize/conSurround/conExtend).
  [[nodiscard]] WiringContact scaled(const WiringContact& contact) const;
};

/// Reads the lines of one `wiring` section. Line errors (Magic's TechError
/// texts) are reported to `sink` as WIRE-TECH-001 with the line's location
/// and the line is ignored. `grid` scales the dimensions.
[[nodiscard]] WiringSection parse_wiring_lines(std::span<const tech::RawLine> lines, const tech::Technology& tech,
                                               const tech::GridScale& grid = {},
                                               diag::DiagnosticSink* sink = nullptr);

/// Reads every `wiring` section of `tech` in file order (a repeated section
/// restarts the table, as WireTechInit does); an empty section when absent.
[[nodiscard]] diag::Outcome<WiringSection> parse_wiring_section(const tech::Technology& tech,
                                                                const tech::GridScale& grid = {},
                                                                diag::DiagnosticSink* sink = nullptr);

/// DRCGetDefaultLayerWidth over a freshly compiled default `drc` style at
/// `grid` (drc::default_width; 0 when there is none). The wire commands
/// read the session's current style instead (drc::default_width on
/// `editor.drc.rules`), as Magic reads DRCCurStyle; this overload serves
/// callers without a session.
[[nodiscard]] db::Coord default_layer_width(const tech::Technology& tech, tech::TileType type,
                                            const tech::GridScale& grid = {});

// ---- session state ----------------------------------------------------------

/// The direction of the last leg (none: no leg yet, or the type changed).
enum class LegDirection : std::uint8_t { none, north, south, east, west };

/// The wiring parameters undo/redo restores (Magic's WireUndoEvent).
struct WireParams {
  std::optional<tech::TileType> type;
  db::Coord width = 0;
  LegDirection direction = LegDirection::none;

  friend bool operator==(const WireParams&, const WireParams&) = default;
};

/// The session's wire state (editor::EditorState::wire).
struct WireState {
  /// The wiring type; nullopt until `wire type` (or switch/increment/
  /// decrement) sets one: "no material" in both magic-compat modes
  /// (Q-LEGACY-51).
  std::optional<tech::TileType> type;
  db::Coord width = 0;
  LegDirection last_direction = LegDirection::none;
  /// The wiring tool is tracking (between `trackwire pick|current` and
  /// `trackwire done|cancel`): front ends run `*bypass wire show` on every
  /// pointer motion while it is set (Magic's <Motion> binding).
  bool tracking = false;
  /// The values last handed to the undo log (WireRememberForUndo's
  /// wireOld* statics, initially space / 2 / north).
  WireParams remembered{std::nullopt, 2, LegDirection::north};

  [[nodiscard]] WireParams params() const { return WireParams{type, width, last_direction}; }
  void set_params(const WireParams& p) {
    type = p.type;
    width = p.width;
    last_direction = p.direction;
  }
};

/// The undo event of a wiring-parameter change: undo restores `before`,
/// redo `after` (and both update WireState::remembered).
[[nodiscard]] std::unique_ptr<undo::Event> make_wire_event(const WireParams& before, const WireParams& after);

/// WireRememberForUndo: records the change from `remembered` to the current
/// parameters in editor.undo and remembers them.
void remember_for_undo(editor::EditorState& editor);

// ---- leg, contact and chunk geometry (root coordinates) ------------------

/// How `wire leg|horizontal|vertical` picks the leg direction.
enum class LegMode : std::uint8_t { choose, horizontal, vertical };

struct LegPlan {
  db::Rect area;  // the paint of the new leg
  LegDirection direction = LegDirection::none;
};

/// WireAddLeg's geometry: the leg from `box` towards `point` with width
/// `width`; nullopt when the point lies within the box's span on the leg
/// axis ("nothing is painted").
[[nodiscard]] std::optional<LegPlan> plan_leg(const db::Rect& box, const db::Point& point, db::Coord width,
                                              LegMode mode);

/// The box after a leg: a width x width square at the tip of `leg`,
/// extended back to the selected chunk `chunk` when the leg is shorter.
[[nodiscard]] db::Rect leg_tip_box(const LegPlan& leg, const db::Rect& chunk, db::Coord width);

/// WirePickType's box: a width x width square at the point area
/// `point_area` (1 x 1), shifted inside `chunk` where it sticks out.
[[nodiscard]] db::Rect pick_box(const db::Rect& point_area, const db::Rect& chunk, db::Coord width);

/// One rectangle of a contact placement.
struct ContactPaint {
  tech::TileType type = tech::kSpace;
  db::Rect area;
};

struct ContactPlan {
  /// Paint in edit-cell coordinates, in Magic's order (contact, surround 1,
  /// surround 2, extension 1, extension 2), and their union.
  std::vector<ContactPaint> paint;
  db::Rect changed;
  /// Selection in root coordinates (contact and surrounds).
  std::vector<ContactPaint> select;
  /// The new box in root coordinates.
  db::Rect box;
};

/// WireAddContact's geometry for the contact `scaled` (dimensions already
/// divided by the scalefactor) placed from `old_type` to the new wire of
/// `new_width`, at the end of the leg marked by `old_leg` (root
/// coordinates) whose direction was `old_dir`. `to_edit` maps root to
/// edit-cell coordinates. nullopt when `scaled` does not join `old_type`.
[[nodiscard]] std::optional<ContactPlan> plan_contact(const WiringContact& scaled, tech::TileType old_type,
                                                      const db::Rect& old_leg, LegDirection old_dir,
                                                      db::Coord new_width, const db::Transform& to_edit);

/// selFindChunk: the largest rectangle inside `search` containing
/// `contained` with no tile of `wrong` on `plane` (largest smaller side,
/// then larger side), unless `best` (the chunk to beat, from a smaller
/// search) is at least as large; nullopt when there is none.
[[nodiscard]] std::optional<db::Rect> find_chunk(const db::Plane& plane, const tech::TypeMask& wrong,
                                                 const db::Rect& search, const db::Rect& contained,
                                                 std::optional<db::Rect> best = std::nullopt);

// ---- `wire segment` --------------------------------------------------------

/// CIFPaintWirePath: the polygons of the centerline path `points` (in half
/// internal units, already in the order Magic's PaintWireList builds them)
/// of width `width` (internal units), with half-width end caps unless
/// `endcap` is false. Vertices are in internal units. Reversal and mitre
/// warnings (WIRE-PATH-001/002) go to `sink`; a mitre-limit violation
/// truncates the route there.
[[nodiscard]] std::vector<std::vector<db::Point>> wire_path_polygons(std::span<const db::Point> points,
                                                                     db::Coord width, bool endcap,
                                                                     diag::DiagnosticSink* sink = nullptr);

/// The file form of `wire segment`: one `x y` pair per line (lines read as
/// fgets with a 128-byte buffer does), each coordinate parsed at twice the
/// internal resolution with `units`. WIRE-FILE-002 "Bad coordinate pair at
/// <file_label> line <n>" when a line lacks a coordinate.
[[nodiscard]] diag::Outcome<std::vector<db::Point>> read_segment_points(std::string_view text,
                                                                        std::string_view file_label,
                                                                        const cmd::CoordinateUnits& units,
                                                                        diag::DiagnosticSink* sink = nullptr);

/// cmdScaleCoord(text, relative, is_x, scale): the coordinate times
/// `scale`, rounded half away from zero; a word that is not a number warns
/// "Coordinate value cannot be parsed:  assuming 0" and yields 0.
[[nodiscard]] db::Coord scale_coord(std::string_view text, const cmd::CoordinateUnits& units, bool relative,
                                    bool is_x, int scale, diag::DiagnosticSink* sink = nullptr);

}  // namespace hefesto::wiring
