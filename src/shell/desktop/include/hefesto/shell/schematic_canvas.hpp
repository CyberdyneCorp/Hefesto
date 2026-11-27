// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The canvas view model of the desktop schematic editor (capability
// schematic-editing "Desktop and touch interaction", "Net highlighting";
// openspec/changes/m8-schematic design D15; board
// docs/design/mockups/Schematic.dc.html, the canvas):
//
// - build_scene draws a schem::Document read-only into a flat display list
//   in schematic coordinates (y down): the document's graphics on their
//   layers, its wires, and each instance's symbol graphics placed by the
//   one orientation transform (FORMAT's transform.hpp), with the symbol
//   looked up through a replaceable SymbolLookup (in the app: the session
//   resolver's symbols, resolved off the UI thread, schematic_symbols.hpp);
// - SchematicViewport maps schematic coordinates to canvas pixels: pan,
//   zoom about a point and zoom to fit, the view state only (no command,
//   no undo step: "Undo and redo").
//
// The scene changes nothing in the document; every edit goes through
// `schematic ...` command lines (schematic_panels.hpp, schematic_bindings.hpp).
// Raven-free and unit-tested in tests/unit/shell/schematic_canvas_test.cpp.
//
// Owner: SHELL (openspec/changes/m8-schematic design D3, D15; task 13.2).

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::shell {

/// What a scene item comes from, for painting and picking.
enum class SceneSource : unsigned char { graphic, wire, symbol, pin };

/// An axis-aligned rectangle in schematic coordinates; empty until a point
/// is added.
struct SceneBox {
  double x1 = 0.0;
  double y1 = 0.0;
  double x2 = 0.0;
  double y2 = 0.0;
  bool empty = true;

  void add(schem::Point p) noexcept;
  void add(const SceneBox& other) noexcept;
  [[nodiscard]] bool contains(schem::Point p) const noexcept;
};

struct SceneSegment {
  schem::Point a;
  schem::Point b;
  int layer = 0;
  SceneSource source = SceneSource::graphic;
  std::size_t index = 0;  // the wire's index, or the instance's
  bool highlighted = false;
};

struct ScenePolygon {
  std::vector<schem::Point> points;  // closed: the last point joins the first
  int layer = 0;
  SceneSource source = SceneSource::graphic;
  std::size_t index = 0;
  bool filled = false;  // the `fill` attribute, or a pin box
};

struct SceneArc {
  schem::Point centre;
  double radius = 0.0;
  double start_angle = 0.0;  // degrees, after the instance's transform
  double sweep_angle = 0.0;
  int layer = 0;
  SceneSource source = SceneSource::graphic;
  std::size_t index = 0;
};

struct SceneText {
  std::string text;  // `@key` tokens replaced by the instance's attribute values
  schem::Point origin;
  int rotation = 0;
  int flip = 0;
  double size = 0.0;  // the record's y scale
  int layer = 0;
  SceneSource source = SceneSource::graphic;
  std::size_t index = 0;
};

struct SceneInstance {
  std::string name;    // the `name` attribute
  std::string symbol;  // the reference as written
  SceneBox box;        // the placed symbol's graphics
  bool placeholder = false;  // the missing-symbol placeholder was drawn
  bool pending = false;      // its symbol is still being resolved (schematic_symbols.hpp)
  bool highlighted = false;
};

/// The display list of one document.
struct SchematicScene {
  std::vector<SceneSegment> segments;
  std::vector<ScenePolygon> polygons;
  std::vector<SceneArc> arcs;
  std::vector<SceneText> texts;
  std::vector<SceneInstance> instances;
  SceneBox box;  // every item

  /// The topmost instance whose box holds `p` (the last drawn), or npos.
  [[nodiscard]] std::size_t instance_at(schem::Point p) const noexcept;
};

/// The symbol of one placed instance (never null: a missing symbol gives
/// the placeholder).
using SymbolLookup = std::function<const schem::Symbol&(const schem::InstanceRecord&)>;

/// The nets to show highlighted (`schematic hilight list`). A wire is drawn
/// highlighted when its `lab` attribute (written by netlisting, Q-SCHEM-29)
/// names one of them, an instance when its name is in `instances`.
struct SceneHighlight {
  std::set<std::string> nets;
  std::set<std::string> instances;
};

/// The scene of `document`. An instance whose lookup gives `pending` (the
/// resolver's pending_symbol(), schematic_symbols.hpp) is marked pending:
/// drawn with the placeholder until its symbol is resolved.
[[nodiscard]] SchematicScene build_scene(const schem::Document& document, const SymbolLookup& lookup,
                                         const SceneHighlight& highlight = {},
                                         const schem::Symbol* pending = nullptr);

/// A lookup through `session.symbols`, resolving through the session's
/// search path and file system (`session.files`), with the warnings to
/// `warnings` (may be null). The session must outlive the lookup. It may
/// run Tcl generators: the desktop canvas never uses it on the UI thread
/// (it draws through SchematicSymbolResolver, schematic_symbols.hpp).
[[nodiscard]] SymbolLookup session_symbol_lookup(schem::Session& session, const schem::Document& document,
                                                 diag::DiagnosticSink* warnings);

/// A canvas position in pixels (origin top left).
struct CanvasPoint {
  double x = 0.0;
  double y = 0.0;
};

/// The canvas view: the schematic point at the canvas centre and the zoom
/// (pixels per schematic unit), clamped to [kMinZoom, kMaxZoom].
class SchematicViewport {
 public:
  static constexpr double kMinZoom = 1.0 / 64.0;
  static constexpr double kMaxZoom = 64.0;
  static constexpr double kFitMargin = 0.05;  // of each side, when fitting

  void set_size(double width, double height) noexcept;
  [[nodiscard]] double width() const noexcept { return width_; }
  [[nodiscard]] double height() const noexcept { return height_; }
  [[nodiscard]] double zoom() const noexcept { return zoom_; }
  [[nodiscard]] schem::Point centre() const noexcept { return centre_; }

  [[nodiscard]] CanvasPoint to_canvas(schem::Point p) const noexcept;
  [[nodiscard]] schem::Point to_schematic(CanvasPoint p) const noexcept;

  /// A drag by (dx, dy) pixels: the drawing follows the pointer.
  void pan_by(double dx, double dy) noexcept;
  /// Zooms by `factor` keeping the schematic point under `at` in place.
  void zoom_at(double factor, CanvasPoint at) noexcept;
  /// Shows all of `box` (no change when it is empty or the canvas has no size).
  void fit(const SceneBox& box) noexcept;

 private:
  double width_ = 0.0;
  double height_ = 0.0;
  schem::Point centre_;
  double zoom_ = 1.0;
};

/// Canvas texts follow the zoom (M8 round 7: they kept one size, so they
/// overlapped once a fit zoomed out). A text is kTextUnitHeight schematic
/// units tall per unit of its record's y scale (a scale of 0 or below counts
/// as kDefaultTextScale): a Hefesto display choice, the exact xschem font
/// metrics are not established (clean room).
inline constexpr double kTextUnitHeight = 50.0;
inline constexpr double kDefaultTextScale = 0.4;
inline constexpr double kMinTextPx = 4.0;   // smaller texts are not drawn
inline constexpr double kMaxTextPx = 96.0;  // larger ones are capped

/// The pixel size of a text of y scale `scale` at `zoom` (pixels per
/// schematic unit), at most kMaxTextPx; nullopt below kMinTextPx (not drawn).
[[nodiscard]] std::optional<double> scene_text_px(double scale, double zoom) noexcept;

}  // namespace hefesto::shell
