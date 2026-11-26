// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ReadEngine: the one read engine behind both parsers (capability
// mask-input-styles; openspec/changes/archive/2026-11-24-m5-mask-gds-cif/design.md D9; Magic's
// CIFrdcl.c, CIFrdutils.c, CIFrdpt.c, CIFrdpoly.c). It keeps per-cell read
// planes and the scale, rescales mid-read (CIFScalePlanes,
// CIFInputRescale), paints the current cell through evaluate_layer
// (CIFPaintCurrent), and handles boundary, mask-hints, tagged and copyup.
// The read holds the exclusive database lock; undo is disabled for it (the
// caller's job: the engine paints through the db primitives).
//
// Units. Read units (RU) are the integer units of the read planes; one
// internal unit (IU) is S RU (InputStyle::scale_factor). Every coordinate
// the ReadSink operations take is in RU; the engine converts to IU with
// scale_coord ("Conversion of read units to internal units"), which may
// subdivide the internal grid of the whole session (every cell, the DRC
// rules, the output styles: Magic's CIFScaleCoord with DBScaleEverything).
//
// Two sets of read planes are kept, as in Magic: the top level's
// (cifEditCellPlanes: CIF geometry outside a definition, painted into
// Options::top by finish()) and the current cell's (cifSubcellPlanes,
// between begin_cell and end_cell).
//
// Layers: `cif_layer` is the index into the style's CIF layer table
// (InputStyle::cif_layers); paint on an unknown index is dropped.
//
// Lower-level operations (for the GDS reader, which composes its own
// structure processing on them: flatten planes, `gds maskhints`, the label
// rules of TEXT records): scale_coord, scale_rect, place, saved planes,
// paint_current, rescale and the read planes themselves.
//
// Contract changes from the scaffold (design D4): the engine keeps its own
// copy of the input style (a read changes its scale; style() returns it for
// the caller to store back with InputStyleSet::replace_current), the
// constructor takes Options, and the lower-level operations above are added.
//
// Owner: INPUT (design D2; the header was created by the M5 scaffold).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/polygon.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/input/sink.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::mask {

/// The non-Manhattan work budget of one read's polygon triangles (Q-MODEL-37,
/// proposed). A triangle painted beside a nearly parallel diagonal (a
/// 45-degree path over a boundary with almost the same edge, as in
/// gf180mcu_fd_io's POWER_RAIL_COR_1) makes DBNMPaintPlane0 descend into a
/// chain of pieces about as long as the triangle divided by the edges'
/// offset: 910 in that cell, above the 512 of a database paint
/// (db::NMBudget, Q-MODEL-35), past which a piece is filled as a rectangle
/// and the read planes' tiling stops being Magic's. A read triangle
/// descends into at most kPieces pieces, drawn from a pool that starts at
/// kPool and grows by kPerTriangle for every triangle a read paints (a
/// polygon's triangles, at most kPolygonGrant per polygon, and every split
/// tile a rescale or flatten copy repaints).
///
/// Work: each piece of a chain works next to the tiles the pieces before it
/// left, so one paint of n pieces costs about n^2 tile steps: at most
/// kPieces^2 (about 1.05M) per paint, against 512^2 (about 0.26M) for a
/// database paint. The pool bounds the pieces of a whole read, so the
/// read's non-Manhattan work stays below kPieces * (kPool + kPerTriangle *
/// triangles): about 1.05M tile steps per triangle read, at most.
///
/// That n^2 holds over a plane of large tiles only. A piece re-stitches
/// every tile along the edges it cuts, so a tall triangle cut beside the
/// chains of earlier paints pays for all of their tiles at every piece:
/// 3.9 x 10^8 steps for one triangle of an 18 KiB CIF file (security.md
/// M5-S10). A read paint therefore also stops after kSteps stitch steps
/// (db::NMBudget::steps, Q-MODEL-38, proposed) and fills the rest of its
/// triangle's area as a rectangle; a gf180mcu_fd_io chain of 910 pieces
/// spends about 10^4.
struct ReadTriangleBudget {
  static constexpr std::size_t kPieces = 1024;
  static constexpr std::uint64_t kSteps = std::uint64_t{kPieces} * kPieces;
  static constexpr std::size_t kPool = std::size_t{1} << 16;
  static constexpr std::size_t kPerTriangle = 1024;
  static constexpr std::size_t kPolygonGrant = std::size_t{1} << 16;
  std::size_t pool = kPool;

  /// Grows the pool for a polygon decomposed into `polygon`'s triangles.
  void add_polygon(const db::PolygonDecomposition& polygon) noexcept;
  /// Grows the pool for a polygon of `triangles` triangles.
  void add_polygon(std::size_t triangles) noexcept {
    pool += triangles < kPolygonGrant / kPerTriangle ? triangles * kPerTriangle : kPolygonGrant;
  }
};

/// How a read-unit coordinate becomes an internal one (COORD_*).
enum class Snap : std::uint8_t {
  exact,      // subdivide the grid if needed
  half_up,    // to the half grid (subdivide beyond it), then a half rounds up (contact top/right)
  half_down,  // the same, a half rounds down (contact bottom/left)
  any,        // round to the nearest unit, ties toward negative infinity
};

class ReadEngine final : public ReadSink {
 public:
  struct Options {
    /// The cell that receives top-level geometry (CIF outside a
    /// definition); nullptr drops it.
    db::CellDef* top = nullptr;
    /// CIFRescaleAllow (`cif rescale` / `gds rescale`): false snaps every
    /// off-grid coordinate instead of subdividing the internal grid.
    bool rescale_allowed = true;
    /// Whether call() copies the child's saved copyup planes into the
    /// caller's read planes: GDS always; CIF only with magic-compat OFF
    /// (Q-MASK-309). Set by the format layer.
    bool copyup_on_call = true;
    /// Messages (MASK-INPUT-010..015). The format layer may route them
    /// through its own warning level and formatting (CIFReadWarning,
    /// CalmaReadError); nullptr drops them.
    diag::DiagnosticSink* sink = nullptr;
  };

  /// A read into `editor`'s cell library with a copy of input style
  /// `style`. The editor's technology and current output style
  /// (CIFCurStyle: Q-MASK-306 and 307 ON) are read; the editor must outlive
  /// the engine.
  ReadEngine(editor::EditorState& editor, const InputStyle& style, const compat::CompatSettings& compat);
  ReadEngine(editor::EditorState& editor, const InputStyle& style, const compat::CompatSettings& compat,
             const Options& options);
  ~ReadEngine() override;

  // --- ReadSink ---------------------------------------------------------------

  /// Starts reading cell `name`, creating it when the library has none.
  [[nodiscard]] diag::Status begin_cell(std::string_view name) override;
  /// Paints a rectangle or polygon ("Polygon to region conversion") on the
  /// current read plane of `cif_layer`.
  [[nodiscard]] diag::Status paint(std::size_t cif_layer, const ReadShape& shape) override;
  /// Adds a label ("Label placement semantics during read"): its area
  /// converted with Snap::any, its type and kind from its CIF layer.
  [[nodiscard]] diag::Status label(const ReadLabel& label) override;
  /// Places `child` (created when missing) with a read-unit translation and
  /// array spacing, after copying its saved copyup planes when
  /// Options::copyup_on_call ("Input operator copyup").
  [[nodiscard]] diag::Status call(std::string_view child, const db::Transform& transform,
                                  const db::ArrayInfo& array) override;
  /// Sets a string property of the current cell.
  [[nodiscard]] diag::Status property(std::string_view key, std::string_view value) override;
  /// Ends the current cell: paint_current, label adjustment, bounding box
  /// (CIFParseFinish). DBAdjustLabels' "Moving label" and "Adjusting size of
  /// label" lines go to Options::sink as MASK-INPUT-019.
  [[nodiscard]] diag::Status end_cell() override;

  /// Ends the read: paints the top-level planes into Options::top (with the
  /// label adjustment of end_cell) and drops the saved copyup planes.
  [[nodiscard]] diag::Status finish();

  // --- Lower-level operations ---------------------------------------------------

  /// Starts reading into `cell` (begin_cell for a cell the caller made).
  [[nodiscard]] diag::Status begin_cell(db::CellDef& cell);
  /// The cell being read, or nullptr at the top level.
  [[nodiscard]] db::CellDef* current_cell() const noexcept;

  /// CIFScaleCoord: `c` (RU) in internal units. May subdivide the internal
  /// grid (Snap::exact, half_*), which changes S; the warning is reported
  /// once per read.
  [[nodiscard]] db::Coord scale_coord(std::int64_t c, Snap snap);
  /// A rectangle converted xtop, ytop, xbot, ybot in that order, the edges
  /// already converted multiplied by any subdivision a later one makes
  /// (cifPaintCurrentFunc). `contact`: top/right half up, bottom/left half
  /// down; otherwise `snap` for all four.
  [[nodiscard]] db::Rect scale_rect(const db::Rect& r, Snap snap, bool contact = false);

  /// CIFInputRescale(n, d) with calmaInputRescale's saved planes: S, M and
  /// every operator distance times n / d, every read plane (top level,
  /// current cell, saved) scaled; "CIF style <name>: units rescaled by
  /// factor of <n> / <d>".
  void rescale(std::int64_t n, std::int64_t d);

  /// CIFPaintCurrent over `planes` (indexed by CIF layer) for `cell`: every
  /// rule in order ("Per-cell layer processing order", "Painting rule
  /// results", the templayer side effects of copyup, boundary and
  /// mask-hints), then every plane cleared.
  void paint_current(std::vector<MaskPlane*>& planes, db::CellDef& cell);

  /// The current read planes (the cell's, or the top level's), indexed by
  /// CIF layer.
  [[nodiscard]] std::vector<MaskPlane*>& read_planes() noexcept;
  /// The non-Manhattan budget the read's polygon triangles draw from
  /// (paint_read_triangle).
  [[nodiscard]] ReadTriangleBudget& triangle_budget() noexcept;
  /// The saved copyup planes of `cell` (nullptr entries for layers without
  /// one), or nullptr when the cell saved none.
  [[nodiscard]] const std::vector<MaskPlane*>* saved_planes(const db::CellDef& cell) const;
  /// Unions the saved planes of `child`, transformed by `transform` (RU)
  /// for each array element (spacing in RU), into the current read planes.
  void copy_saved_planes(const db::CellDef& child, const db::Transform& transform, const db::ArrayInfo& array);
  /// Places `child` in the current cell with an internal-unit transform and
  /// array; `id` empty for a default identifier.
  [[nodiscard]] diag::Outcome<db::CellUse*> place(db::CellDef& child, const db::Transform& transform,
                                                  const db::ArrayInfo& array, std::string_view id = {});

  /// The input style as the read has rescaled it.
  [[nodiscard]] const InputStyle& style() const noexcept;

 private:
  struct State;
  [[nodiscard]] db::CellDef* target_cell() const noexcept;
  [[nodiscard]] diag::Outcome<db::CellDef*> find_or_create(std::string_view name);
  void adjust_labels(db::CellDef& cell);
  void report_off_grid(std::int64_t remainder, std::int64_t denominator, bool snapped);
  void subdivide_grid(std::int64_t den);
  [[nodiscard]] const OutputStyle& evaluation();
  void note_rule(const InputRule& rule);
  void paint_layer_rule(const MaskPlane& result, db::CellDef& cell, tech::TileType type);
  void make_boundary(const db::Rect& tile_area, db::CellDef& cell);
  void store_hints(const MaskPlane& result, db::CellDef& cell, const std::string& name);
  void save_copyup(const MaskPlane& result, const db::CellDef& cell, const LayerOp& copyup);
  void templayer_effects(const InputRule& rule, const MaskPlane& result, db::CellDef& cell);

  editor::EditorState& editor_;
  compat::CompatSettings compat_;
  std::unique_ptr<State> state_;
};

/// "Mid-read subdivision of read units" (CIFParsePoint): `value` is the
/// input already multiplied by the file scale numerator N (and any element
/// scale), `n` is N for the limit test, `d` the denominator Dn, `limit` the
/// rescale limit (`cif limit`, 1..100; out of range means 100). When value
/// is a multiple of d, `factor` is 1 and `value` the quotient; when N*r
/// stays within the limit (r = d / gcd(d, |value|)), `factor` is r (the
/// caller rescales the read units by it, ReadEngine::rescale(r, 1), and
/// sets N *= r) and `value` is value*r/d; otherwise `value` is rounded to
/// nearest with ties toward positive infinity, `factor` is 1 and `rounded`
/// is set ("CIF units at maximum scale; value is rounded"). 64-bit
/// throughout.
struct ReadUnitValue {
  std::int64_t value = 0;
  std::int64_t factor = 1;
  bool rounded = false;
};
[[nodiscard]] ReadUnitValue subdivide_read_unit(std::int64_t value, std::int64_t n, std::int64_t d,
                                                std::int64_t limit) noexcept;

/// Paints a triangle piece of db::decompose_polygon into a read plane as
/// CIFMakeManhattanPath does: DBNMPaintPlane, then DBMergeNMTiles over the
/// triangle's area grown by one (the CIF and GDS polygon readers). The
/// paint descends into at most min(ReadTriangleBudget::kPieces, pool) pieces
/// and takes those it uses from `budget`'s pool.
void paint_read_triangle(MaskPlane& plane, const db::PolygonPiece& piece, bool magic_compat,
                         ReadTriangleBudget& budget);

/// Repaints a split tile into a read plane as DBNMPaintPlane does, without
/// merging: a read-unit rescale (dbScalePlane) or a flatten copy
/// (gdsCopyPaintFunc, the CIF reader's copy of a saved cell). The pool
/// grows by one triangle's grant and the paint draws from it as
/// paint_read_triangle does, so a repaint is not cut short by the 512
/// pieces of a database paint (Q-MODEL-37).
void repaint_read_triangle(MaskPlane& plane, const db::Rect& area, db::Diagonal diagonal, bool right_side,
                           bool magic_compat, ReadTriangleBudget& budget);

}  // namespace hefesto::mask
