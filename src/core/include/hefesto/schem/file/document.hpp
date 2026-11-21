// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// schem::Document: one open schematic (`.sch`) or symbol (`.sym`)
// (openspec/changes/m8-schematic/design.md D4; capabilities
// schematic-file-format "Record grammar", "Global property records",
// "Embedded symbols" and schematic-editing "Document model").
//
// Each record kind is one list in read/creation order; the writer regroups
// lines, boxes, arcs and polygons by layer ("Canonical write order"), so the
// model keeps the order of the file within each kind. Text and property
// fields hold the decoded brace-string contents ("Brace strings": escapes
// removed, CR dropped); an empty string is "no value". Coordinates are
// binary64 ("Numeric fields"). The document is independent of the layout
// database.
//
// Owner: FORMAT (the record model); EDIT changes documents only through the
// schematic command API.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"

namespace hefesto::schem {

/// The two file kinds share one grammar ("File kinds and character handling").
enum class FileKind : std::uint8_t { schematic, symbol };

/// Layers are 0..21 ("Layers"); the reader enforces the range.
inline constexpr int kLayerCount = 22;

struct Point {
  double x = 0.0;
  double y = 0.0;

  friend bool operator==(const Point&, const Point&) = default;
};

/// `L` and `B` records: layer, two corners, attributes.
struct LineRecord {
  int layer = 0;
  Point p1;
  Point p2;
  std::string props;

  friend bool operator==(const LineRecord&, const LineRecord&) = default;
};
using BoxRecord = LineRecord;

/// `A`: centre, radius, start angle and sweep angle in degrees.
struct ArcRecord {
  int layer = 0;
  Point centre;
  double radius = 0.0;
  double start_angle = 0.0;
  double sweep_angle = 0.0;
  std::string props;

  friend bool operator==(const ArcRecord&, const ArcRecord&) = default;
};

/// `P`: a polygon with exactly `points.size()` coordinate pairs.
struct PolygonRecord {
  int layer = 0;
  std::vector<Point> points;
  std::string props;

  friend bool operator==(const PolygonRecord&, const PolygonRecord&) = default;
};

/// `T`: text, origin, rotation 0..3, flip 0..1, x and y scale, attributes.
struct TextRecord {
  std::string text;
  Point origin;
  int rotation = 0;
  int flip = 0;
  double x_scale = 0.0;
  double y_scale = 0.0;
  std::string props;

  friend bool operator==(const TextRecord&, const TextRecord&) = default;
};

/// `N`: a wire segment.
struct WireRecord {
  Point p1;
  Point p2;
  std::string props;

  friend bool operator==(const WireRecord&, const WireRecord&) = default;
};

/// `C`: an instance of the symbol named by `symbol` (as written, after the
/// file-version rules of "Version record and header text").
struct InstanceRecord {
  std::string symbol;
  Point origin;
  int rotation = 0;
  int flip = 0;
  std::string props;

  friend bool operator==(const InstanceRecord&, const InstanceRecord&) = default;
};

/// The six global property records (`K`, `G`, `V`, `S`, `F`, `E`); the last
/// one read wins ("Record grammar").
struct GlobalProperties {
  std::string k;  // symbol property
  std::string g;  // VHDL text (symbol property before 1.2)
  std::string v;  // Verilog text
  std::string s;  // SPICE text
  std::string f;  // Spectre text
  std::string e;  // tEDAx text

  friend bool operator==(const GlobalProperties&, const GlobalProperties&) = default;
};

struct EmbeddedSymbol;

struct Document {
  FileKind kind = FileKind::schematic;
  /// Where the document was read from or last saved to; empty for a new,
  /// unsaved document. Relative symbol references resolve against its
  /// folder (schematic-symbols-and-libraries "Symbol references", Q-SCHEM-30).
  platform::Path path;
  /// `file_version` as read ("1.0" when absent); Hefesto always writes 1.3.
  std::string file_version = "1.0";
  /// The `v` string without the `xschem`, `version` and `file_version` keys.
  std::string header_text;
  GlobalProperties globals;
  std::vector<LineRecord> lines;
  std::vector<BoxRecord> boxes;
  std::vector<ArcRecord> arcs;
  std::vector<PolygonRecord> polygons;
  std::vector<TextRecord> texts;
  std::vector<WireRecord> wires;
  std::vector<InstanceRecord> instances;
  /// `[` ... `]` blocks read from this file, in read order ("Embedded symbols").
  std::vector<EmbeddedSymbol> embedded;
  /// Set by editing operations, cleared by a save; netlisting never sets it.
  bool modified = false;
};

/// A symbol definition read from a `[` ... `]` block. `reference` is the
/// symbol reference of the instance it followed.
struct EmbeddedSymbol {
  std::string reference;
  Document symbol;
};

}  // namespace hefesto::schem
