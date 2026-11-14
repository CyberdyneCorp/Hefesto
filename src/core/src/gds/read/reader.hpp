// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSR-private: the GDS reader's state and handlers (capability gds-read;
// openspec/changes/m5-mask-gds-cif/design.md D11). One Reader is one
// CalmaReadFile: the library header, the structures (post-order reading,
// duplicates, cells already in memory, flattening, magnified copies) and
// their elements, and the end-of-read cleanup. Magic's session globals of a
// read (calmaReadScale1/2, calmaDefInitHash, CifCellTable, cifReadCellDef,
// the cd_client planes, CalmaPolygonCount, ...) are members.
//
// Planes. The current structure's input-layer planes are the read engine's
// top-level planes (ReadEngine::read_planes() with no engine cell open),
// processed by ReadEngine::paint_current at the end of the structure. A
// nested read (look-ahead, a magnified copy) or a polygon/path subcell
// saves copies of them first and restores them after (calmaExact). The
// flatten planes of flatten-marked cells are copies held here (cd_client
// with CDFLATGDS); the engine holds the copyup planes it saves itself. A
// read-unit rescale (calmaInputRescale) scales all of them.
//
// Messages follow CalmaReadError: counted, with the cell and byte position
// prefix, under the shared read warning level (`gds warning`).
//
// The handlers are split over reader.cpp (the library, messages, records,
// cell tables, planes, the end of the read), structure.cpp (structures,
// look-ahead, the end of a structure, `gds maskhints`), shapes.cpp
// (BOUNDARY, BOX, PATH), text.cpp (TEXT, pin shapes, label adjustment) and
// sref.cpp (SREF, AREF, magnified copies, flatten copies).
//
// Owner: GDSR (design D2).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/read/elements.hpp"
#include "hefesto/gds/read/library.hpp"
#include "hefesto/gds/read/read.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/mask/input/engine.hpp"
#include "hefesto/mask/plane/plane.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::gds::read_detail {

/// Resource caps (design D21): exceeding one is GDS-READ-011.
inline constexpr std::size_t kMaxNesting = 512;                          // look-ahead and magnified-copy reads
inline constexpr std::size_t kMaxMagnifyDepth = 32;                      // nested magnified copies
inline constexpr std::int64_t kMaxArrayCopies = std::int64_t{1} << 20;  // flatten copies of one AREF

/// CalmaReadError, CIFReadError / CIFReadWarning and TxPrintf under the
/// read warning level. Each kind has its own counter, as in Magic.
class Messages {
 public:
  explicit Messages(const mask::WarningSettings& warnings) : warnings_(warnings) {}

  /// CalmaReadError: counted; "Error while reading cell "<cell>" (byte
  /// position <position>): <text>" unless the level suppresses it.
  /// `no_newline`: Magic's text has no line end (the next message follows
  /// on the same line).
  void counted(std::string_view cell, std::uint64_t position, diag::Diagnostic d, bool no_newline = false);
  /// A continuation line of the last read error (calmaUnexpected's
  /// "    Expected ..."), printed when that error was.
  void detail(diag::Diagnostic d);
  /// CIFReadError ("CIF file read error: ") and CIFReadWarning ("CIF file
  /// read warning: "), which the read engine and the polygon converter use.
  void cif_error(diag::Diagnostic d);
  void cif_warning(diag::Diagnostic d);
  /// TxPrintf / TxError outside the counters.
  void print(diag::Diagnostic d);

  [[nodiscard]] std::vector<diag::Diagnostic>& printed() noexcept { return printed_; }
  /// The text for the redirect file (`gds warning redirect <file>`).
  [[nodiscard]] const std::string& redirected() const noexcept { return redirected_; }

 private:
  /// Whether the `count`th message of a kind is printed; the 100th under
  /// `limit` prints `limit_text` instead.
  [[nodiscard]] bool admit(std::int64_t count, std::string_view limit_text);
  void emit(diag::Diagnostic d);

  mask::WarningSettings warnings_;
  std::int64_t calma_errors_ = 0;  // calmaTotalErrors
  std::int64_t cif_errors_ = 0;    // cifTotalErrors
  std::int64_t cif_warnings_ = 0;  // cifTotalWarnings
  std::vector<diag::Diagnostic> printed_;
  std::string redirected_;
};

/// A saved set of input-layer planes (calmaExact), indexed by CIF layer;
/// empty layers are null.
using PlaneSet = std::vector<std::unique_ptr<mask::MaskPlane>>;

/// The per-read flags Magic keeps in cd_flags.
struct CellState {
  bool preloaded = false;  // CDPRELOADED: read by look-ahead
  bool processed = false;  // CDPROCESSEDGDS
  bool flattened = false;  // CDFLATTENED: copied into a parent at least once
};

/// What one structure counts (calmaParseStructure's nsrefs, npaths).
struct ElementCounts {
  std::int64_t references = 0;  // uses made
  std::int64_t shapes = 0;      // BOUNDARY, BOX, PATH
};

struct Reference;

class Reader {
 public:
  Reader(editor::EditorState& editor, ByteSource& source, const GdsReadOptions& options,
         const mask::Progress* progress);
  ~Reader();
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  /// CalmaReadFile from the HEADER record to the end-of-read cleanup.
  void run();

  [[nodiscard]] Messages& messages() noexcept { return messages_; }
  /// The library cell name when a cell of that name was read (DBWloadWindow
  /// is the caller's: it needs the window).
  [[nodiscard]] const std::optional<std::string>& library_cell() const noexcept { return library_cell_; }
  /// No cifinput style: nothing was read.
  [[nodiscard]] bool no_style() const noexcept { return no_style_; }

  /// Counts and prints a diagnostic as a read error (also the read engine's
  /// messages).
  void counted(diag::Diagnostic d);
  /// The read engine's messages, routed as Magic prints them.
  void engine_message(diag::Diagnostic d);

 private:
  struct LayerPair {
    std::int32_t layer = 0;
    std::int32_t datatype = 0;
  };
  struct Position {
    std::uint64_t original = 0;
    bool found = false;    // positioned at the structure's BGNSTR
    bool padding = false;  // the search stopped at block padding (a zero header), not at EOF
  };
  struct Search {
    std::optional<std::uint64_t> offset;
    bool padding = false;
  };
  struct Strans {
    db::Transform transform;
    double mag = 1.0;
    double angle = 0.0;
    bool ok = true;
  };
  struct TextFields {
    std::optional<std::size_t> cif;
    std::int32_t type = 0;
    std::int32_t font = -1;
    std::int32_t size = 0;
    std::int32_t rotation = 0;
    db::Position position = db::kSouthEast;
  };

  // --- messages and records (reader.cpp) ---
  /// Magic's nbytes <= 0: a zero record header, as the padding after
  /// ENDLIB (or a damaged record) reads.
  [[nodiscard]] static bool is_padding(const Record& r) noexcept {
    return r.type == 0 && r.data_type == 0 && r.data.empty();
  }
  void error(std::string text, bool no_newline = false);
  void info(std::string text);
  void unexpected(RecordType wanted, std::uint8_t got);
  void header_error(diag::Diagnostic d);
  [[nodiscard]] std::optional<Record> next();
  [[nodiscard]] std::optional<Record> peek();
  void unread(Record record);
  [[nodiscard]] std::optional<Record> expect(RecordType type);
  void skip_set(std::initializer_list<RecordType> types);
  bool skip_to(RecordType type);
  bool skip_exact(RecordType type);
  [[nodiscard]] std::optional<std::int32_t> read_i2(RecordType type);
  [[nodiscard]] std::optional<std::int32_t> read_i4(RecordType type);
  [[nodiscard]] std::optional<std::string> read_string(RecordType type);

  // --- library and end of read (reader.cpp) ---
  bool read_header();
  void end_of_read();
  [[nodiscard]] bool cancelled() const noexcept { return progress_ != nullptr && progress_->cancelled(); }

  // --- cell tables (reader.cpp) ---
  [[nodiscard]] CellState& state(const db::CellDef& def) { return states_[&def]; }
  [[nodiscard]] db::CellDef* look_cell(std::string_view name) const;
  db::CellDef* create_cell(const std::string& name);
  db::CellDef* find_cell(const std::string& name, bool* was_called, bool* predefined);
  bool note_existing_cell(const std::string& name);
  void unique_cell(const std::string& name);
  void forget_cell(db::CellDef& def);

  // --- planes and units (reader.cpp) ---
  [[nodiscard]] std::vector<mask::MaskPlane*>& planes();
  [[nodiscard]] PlaneSet save_planes(bool clear);
  void restore_planes(const PlaneSet& saved);
  void rescale_read_units(std::int64_t r);
  [[nodiscard]] std::int64_t scale_factor() const;
  [[nodiscard]] std::int64_t convert(std::int32_t v, std::int64_t k, std::int64_t& rescale);
  [[nodiscard]] const GdsSettings& settings() const noexcept { return options_.settings; }

  // --- structures (structure.cpp) ---
  bool parse_structure();
  bool duplicate_structure(std::string& name);
  bool predefined_structure(db::CellDef& def, std::uint64_t start);
  bool structure_body(db::CellDef& def, const std::string& name, bool was_called, bool magnified, std::int64_t date,
                      std::uint64_t start, std::int64_t polygons_before);
  void begin_definition(db::CellDef& def, std::int64_t date, bool magnified);
  bool read_elements(ElementCounts& counts);
  bool parse_element(ElementCounts& counts);
  void annotate_vendor(db::CellDef& def, std::uint64_t start, std::uint64_t end);
  [[nodiscard]] bool flatten_decision(const std::string& name, bool was_called, const ElementCounts& counts) const;
  void malformed_structure();
  void save_flatten_planes(db::CellDef& def);
  void paint_structure(db::CellDef& def);
  void end_structure(db::CellDef& def, std::int64_t polygons_before);
  void generate_ids(db::CellDef& def);
  void flatten_polygon_cells(db::CellDef& def);
  void next_cell();
  [[nodiscard]] Search find_structure(const std::string& name);
  Position set_position(const std::string& name);
  [[nodiscard]] db::CellDef* read_ahead(const std::string& name);
  void mask_hints(db::CellDef& def, const PlaneSet& inputs);
  [[nodiscard]] bool hint_requested(std::string_view layer) const;

  // --- BOUNDARY, BOX, PATH (shapes.cpp) ---
  [[nodiscard]] std::optional<LayerPair> read_layer_pair(RecordType datatype, std::string_view missing);
  void layer_error(std::string_view message, std::int32_t layer, std::int32_t datatype);
  [[nodiscard]] std::optional<std::vector<PathPoint>> read_path(std::int64_t k, std::int64_t& rescaled);
  void paint_polygon(std::size_t cif_layer, const mask::Polygon& polygon, db::Rect* last_strip);
  [[nodiscard]] db::CellDef* begin_subcell(const std::string& name);
  void end_subcell(db::CellDef& sub, db::CellDef* parent, const PlaneSet& saved);
  void element_boundary();
  void boundary_polygon(std::size_t cif_layer, mask::Polygon polygon);
  void element_box();
  void element_path();
  [[nodiscard]] std::int32_t path_type();
  [[nodiscard]] std::optional<std::int64_t> path_width();
  [[nodiscard]] std::int64_t path_extension(RecordType type);
  void path_geometry(std::size_t cif_layer, const std::vector<PathPoint>& points, std::int64_t width, bool endcap);
  void record_path(db::CellDef& def, const std::vector<PathPoint>& points);

  // --- TEXT, pins, labels (text.cpp) ---
  void pin_shape(std::size_t cif_layer, const db::Rect& last_strip);
  [[nodiscard]] std::int32_t default_text_size(std::int32_t type) const;
  void skip_width_records();
  [[nodiscard]] std::optional<std::int64_t> text_width(std::int32_t& size);
  bool text_presentation(TextFields& f);
  bool text_width_run(TextFields& f);
  void text_transform(TextFields& f);
  [[nodiscard]] std::optional<db::Point> text_point();
  void element_text();
  void place_text(const TextFields& f, const LayerPair& pair, const db::Point& point, const std::string& text);
  void put_text_label(const TextFields& f, const mask::CifLayerLabels& labels, const db::Point& point,
                      const std::string& text);
  void consume_placeholder(db::CellDef& def, db::Label& lab);
  [[nodiscard]] std::optional<tech::TileType> pick_label_layer(const db::CellDef& def, const db::Label& lab) const;
  void label_cover(const db::CellDef& def, const db::Label& lab, tech::TypeMask& covers,
                   tech::TypeMask& components) const;
  void point_cover(const db::CellDef& def, const db::Rect& r, tech::TypeMask& touching) const;
  void two_area_cover(const db::CellDef& def, const db::Rect& a, const db::Rect& b, tech::TypeMask& covers,
                      tech::TypeMask& components) const;
  void area_cover(const db::CellDef& def, const db::Rect& area, tech::TypeMask& covers,
                  tech::TypeMask& components) const;
  void adjust_labels(db::CellDef& def);

  // --- SREF, AREF (sref.cpp) ---
  std::int64_t element_sref();
  [[nodiscard]] db::CellDef* reference_cell(const std::string& sname);
  bool ancestor_refused(const db::CellDef& def);
  [[nodiscard]] Strans read_transform(const std::string& name);
  bool read_magnification(const std::string& name, Strans& out);
  bool read_angle(Strans& out);
  [[nodiscard]] db::CellDef* magnified_cell(const std::string& sname, double mag);
  void read_magnified(const std::string& name, std::int64_t num, std::int64_t den, const MagFraction& fraction);
  bool read_colrow(Reference& ref);
  bool read_reference_points(Reference& ref);
  void read_reference_point(Reference& ref, const Record& xy, std::size_t n, bool rotated);
  void scale_reference_points(Reference& ref, std::size_t count);
  bool read_instance_properties(Reference& ref);
  bool instance_property(Reference& ref, std::int32_t attr);
  void skip_property_value();
  [[nodiscard]] std::string instance_name(std::string id);
  void array_limits(Reference& ref, const std::string& text);
  db::CellUse* place_use(db::CellDef& parent, db::CellDef& child, const db::Transform& transform,
                         const db::ArrayInfo& array, const std::string& id, bool check_duplicate);
  void array_spacing(const Reference& ref, const db::Transform& trans, db::ArrayInfo& array);
  [[nodiscard]] bool is_flat(const db::CellDef& def) const;
  void copy_flattened(const Reference& ref, const db::Transform& trans, const db::ArrayInfo& array);
  bool place_reference(const Reference& ref, bool unscaled, std::int64_t& made);

  editor::EditorState& editor_;
  ByteSource& source_;
  RecordReader records_;
  GdsReadOptions options_;
  const mask::Progress* progress_;
  compat::CompatSettings compat_;
  Messages messages_;
  std::unique_ptr<diag::DiagnosticSink> engine_sink_;
  std::unique_ptr<mask::ReadEngine> engine_;
  std::unique_ptr<CoordinateScale> scale_;

  // Magic's session globals of one read.
  db::CellDef* current_ = nullptr;                                // cifReadCellDef
  std::unordered_map<std::string, db::CellDef*> def_init_;        // calmaDefInitHash
  std::unordered_map<std::string, db::CellDef*> cif_cells_;       // CifCellTable (null: renamed by `unique`)
  std::vector<db::CellDef*> cell_order_;                          // CifCellTable's cells, as created
  std::unordered_set<const db::CellDef*> in_order_;               // the members of cell_order_
  std::unordered_map<const db::CellDef*, CellState> states_;
  std::unordered_map<const db::CellDef*, PlaneSet> flat_planes_;  // cd_client of CDFLATGDS cells
  std::vector<PlaneSet*> saved_sets_;                             // nested saves, scaled by a rescale
  std::unordered_set<std::uint64_t> layer_errors_;                // calmaLayerHash
  std::int64_t polygon_count_ = 0;                                // CalmaPolygonCount
  std::int64_t path_count_ = 0;                                   // CalmaPathCount
  std::int64_t name_conversions_ = 0;                             // NameConvertErrors
  std::int64_t last_reported_ = 0;                                // "    <n> uses"
  bool non_manhattan_ = false;                                    // calmaNonManhattan
  bool non_manhattan_path_ = false;                               // the last path read had a diagonal
  bool rewound_ = false;                                          // a rewind in this read (Q-MASK-111 OFF)
  std::int64_t mag_num_ = 1;                                      // calmaMagNum
  std::int64_t mag_den_ = 1;                                      // calmaMagDen
  std::optional<std::string> mag_name_;                           // calmaMagName
  std::size_t mag_depth_ = 0;
  std::size_t nesting_ = 0;
  std::optional<std::string> libname_;
  std::optional<std::string> library_cell_;
  bool interrupted_ = false;
  bool no_style_ = false;
  bool readonly_refused_ = false;
  bool stream_error_reported_ = false;
};

/// The (layer, datatype) key of calmaLayerHash.
[[nodiscard]] inline std::uint64_t layer_key(std::int32_t layer, std::int32_t datatype) noexcept {
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(layer)) << 32U) |
         static_cast<std::uint32_t>(datatype);
}

/// DBCellClearDef: uses, paint, labels, properties and elements go; the
/// bounding boxes are reset.
void clear_definition(db::CellDef& def);

/// DBIsAncestor(cell, of): `cell` is `of` or one of its ancestors.
[[nodiscard]] bool is_ancestor(const db::CellDef& cell, const db::CellDef& of);

/// Whether `def` has paint on a technology plane (calmaEnumFunc) or a use
/// (gdsHasUses).
[[nodiscard]] bool has_contents(const db::CellDef& def);

/// DBCellCopyAllPaint with the identity transform: `from`'s paint into `to`.
void copy_paint(const db::CellDef& from, db::CellDef& to, const tech::Technology& technology);

}  // namespace hefesto::gds::read_detail
