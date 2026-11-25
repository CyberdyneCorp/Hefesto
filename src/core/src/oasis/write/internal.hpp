// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the OASIS writer (src/core/src/oasis/write/; capability
// oasis-write; openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D13). An OASIS
// write translates the GDS element stream that gds-write produces for the
// same layout, record by record ("GDS-consistent content"):
// - gds_elements.cpp reads that stream (or a vendor GDS structure) as
//   structures and elements;
// - translate.cpp maps one GDS element to its OASIS element(s)
//   ("Boundary translation", "Text translation", "Placements", "Array
//   placements", "Reference properties", and the vendor rules of
//   "Read-only GDS vendor cells");
// - names.cpp gives each structure name its OASIS output name ("Cell
//   names") and maps strings ("String mapping");
// - repetitions.cpp groups the shapes of one mask layer ("Shape repetition
//   detection");
// - library.cpp drives the translation of a whole stream into an
//   oasis::Encoder (file layout, standard properties, layer names);
// - reencode.cpp copies read-only OASIS cells from their source file.
// Owner: OASW (design D2).

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/stream/encode.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/gds/stream/values.hpp"
#include "hefesto/gds/write/emit.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/encoder.hpp"
#include "hefesto/oasis/codec/property.hpp"
#include "hefesto/oasis/command/settings.hpp"

namespace hefesto::oasis::detail {

namespace codes {
inline constexpr std::string_view kOutputFile = "OASIS-WRITE-001";
inline constexpr std::string_view kFontLabels = "OASIS-WRITE-002";
inline constexpr std::string_view kCellName = "OASIS-WRITE-003";
inline constexpr std::string_view kOddPath = "OASIS-WRITE-004";
inline constexpr std::string_view kVendorElement = "OASIS-WRITE-005";
inline constexpr std::string_view kExtensions = "OASIS-WRITE-006";
inline constexpr std::string_view kVendorOasis = "OASIS-WRITE-007";
inline constexpr std::string_view kLayerName = "OASIS-WRITE-008";
inline constexpr std::string_view kEncoding = "OASIS-WRITE-009";
inline constexpr std::string_view kCap = "OASIS-WRITE-010";
inline constexpr std::string_view kCancelled = "OASIS-WRITE-011";
inline constexpr std::string_view kGdsStream = "OASIS-WRITE-012";
}  // namespace codes

/// The most explicit displacements one vendor AREF may expand to (a
/// repetition of type 10); above it the element is an error (approved
/// resource cap, E3 of 2026-11-20, Q-OASIS-53; GDS COLROW allows 32767 x 32767).
inline constexpr std::uint64_t kMaxExplicitOffsets = std::uint64_t{1} << 24;

// --- the GDS element stream (gds_elements.cpp) ------------------------------

enum class GdsKind : std::uint8_t { boundary, path, sref, aref, text, node, box };

/// One PROPATTR/PROPVALUE pair; `value` is the PROPVALUE payload.
struct GdsProperty {
  std::int32_t attribute = 0;
  std::string value;
};

/// One GDS element with the records the translation reads. Strings are
/// record payloads (padding included); LAYER and the second number
/// (DATATYPE, TEXTTYPE, BOXTYPE, NODETYPE) are read as unsigned 16 bits.
struct GdsElement {
  GdsKind kind = GdsKind::boundary;
  std::int32_t layer = 0;
  std::int32_t datatype = 0;
  std::vector<Point64> points;
  std::string sname;
  std::uint16_t strans = 0;
  std::optional<double> magnification;
  std::optional<double> angle;
  std::int32_t columns = 0;
  std::int32_t rows = 0;
  std::int32_t width = 0;
  std::int32_t pathtype = 0;
  std::optional<std::int32_t> begin_extension;
  std::optional<std::int32_t> end_extension;
  std::string string;
  bool presentation = false;  // a PRESENTATION record (a font label)
  std::vector<GdsProperty> properties;
};

/// One step of a GDS stream.
struct GdsItem {
  enum class Kind : std::uint8_t { library_name, structure, element, structure_end, library_end, other };
  Kind kind = Kind::other;
  std::string name;  // LIBNAME or STRNAME payload
  GdsElement element;
};

/// Reads a GDS stream as library names, structures and elements.
class GdsItems {
 public:
  explicit GdsItems(gds::ByteSource& source) : reader_(source) {}

  /// The next item; nullopt at the end of the stream.
  [[nodiscard]] diag::Outcome<std::optional<GdsItem>> next();

 private:
  [[nodiscard]] diag::Outcome<GdsElement> element(GdsKind kind);
  [[nodiscard]] diag::Outcome<GdsItem> structure();

  gds::RecordReader reader_;
};

/// The payload of a record as a byte string.
[[nodiscard]] std::string payload_text(std::span<const std::byte> data);

// --- strings and names (names.cpp) ------------------------------------------

/// The OASIS text of a string payload that gds-write wrote: without the
/// padding byte, a byte gds-write maps to 0x00 written as `_`.
[[nodiscard]] std::string unpadded(std::string_view payload);

/// The string a gds-write STRING or PROPVALUE record would hold for the
/// vendor text `payload` (mapped with `table` and `options`), as OASIS text.
[[nodiscard]] std::string mapped_text(std::string_view payload, const gds::CharTable& table,
                                      const gds::StringOptions& options, const compat::CompatSettings& compat);

/// "Cell names": the OASIS output name of each GDS structure name of one
/// write. A legal, free name is kept; an empty name, one with a byte
/// outside 0x21-0x7E, or one already given to another structure becomes
/// XXXXX<n> (then XXXXX<n>_<k>), where n is the name's position in order
/// of first need.
class OutputNames {
 public:
  explicit OutputNames(diag::DiagnosticSink* sink) : sink_(sink) {}

  /// The output name of the GDS name payload `payload`.
  [[nodiscard]] const std::string& of(std::string_view payload);

 private:
  [[nodiscard]] std::string fresh(std::string text);

  diag::DiagnosticSink* sink_;
  std::unordered_map<std::string, std::string> by_payload_;
  std::unordered_set<std::string> taken_;
};

// --- element translation (translate.cpp) ------------------------------------

/// How one structure is translated.
struct TranslateContext {
  OutputNames* names = nullptr;
  diag::DiagnosticSink* sink = nullptr;
  std::string cell;  // the structure's output name, for messages
  /// Vendor content ("Read-only GDS vendor cells"): degenerate boundaries
  /// are dropped and strings are mapped as gds-write maps them.
  bool vendor = false;
  const gds::CharTable* table = nullptr;
  gds::StringOptions strings;
  compat::CompatSettings compat;
  bool failed = false;   // an element made the write fail
  bool node_warned = false;
};

/// The translation of one GDS element, in output order, each element
/// followed by the properties attached to it.
struct Translated {
  struct Item {
    Element element;
    std::vector<Property> properties;
  };
  std::vector<Item> items;
};

/// "Boundary translation" of the vertices `v` (no closing point).
[[nodiscard]] Element shape_element(std::uint64_t layer, std::uint64_t datatype, std::span<const Point64> v);

/// The vertices of a BOUNDARY's XY points: the closing point dropped.
[[nodiscard]] std::vector<Point64> boundary_vertices(std::span<const Point64> points);

/// The repetition of an AREF with counts `columns`, `rows` and the exact
/// column and row vectors `a`, `b` ("Array placements"); nullopt for 1 x 1.
[[nodiscard]] std::optional<Repetition> array_repetition(std::uint64_t columns, std::uint64_t rows, Point64 a,
                                                         Point64 b);

/// The S_GDS_PROPERTY of a PROPATTR/PROPVALUE pair.
[[nodiscard]] Property gds_property(std::int32_t attribute, std::string value);

/// The OASIS element(s) of `element`.
[[nodiscard]] Translated translate(const GdsElement& element, TranslateContext& context);

// --- repetition detection (repetitions.cpp) ---------------------------------

/// "Shape repetition detection" over the candidates of one mask layer
/// (RECTANGLE and POLYGON elements without repetition, in GDS order): the
/// grouped elements in output order.
[[nodiscard]] std::vector<Element> group_shapes(std::span<const Element> candidates);

// --- the library translation (library.cpp) ----------------------------------

/// What the translation of a write needs besides the stream.
struct LibraryContext {
  const OasisSettings* settings = nullptr;
  const mask::OutputStyle* style = nullptr;
  compat::CompatSettings compat;
  diag::DiagnosticSink* sink = nullptr;
  /// The STRNAME payloads of the structures gds-write generates from the
  /// layout (repetition candidates); every other structure is vendor
  /// content.
  std::unordered_set<std::string> generated;
  /// The (layer, datatype, STRING payload) of the TEXTs of `labellayer`
  /// layers, which stay among the mask layers.
  std::set<std::tuple<std::int32_t, std::int32_t, std::string>> label_layer_texts;
  /// Writes the body of the read-only OASIS cell of STRNAME payload
  /// `payload` under `output`, when it is one ("Read-only OASIS vendor
  /// cells"); nullopt when the structure is not a read-only OASIS cell.
  std::function<std::optional<diag::Status>(std::string_view payload, const std::string& output, Encoder& out,
                                            OutputNames& names)>
      reencode;
};

/// The translation's outcome.
struct LibraryResult {
  bool failed = false;          // an OASIS-specific element error (the write fails)
  std::uint64_t font_labels = 0;  // font labels written without presentation
  diag::Status status = diag::Done{};  // a stream or encoding error
};

/// Translates the GDS stream `gds_stream` into `out` (begin to finish).
[[nodiscard]] LibraryResult translate_library(gds::ByteSource& gds_stream, const LibraryContext& context,
                                              Encoder& out);

/// The LAYERNAME pairs of `style` ("Layer names") written to `out`.
void write_layer_names(const mask::OutputStyle& style, const compat::CompatSettings& compat, Encoder& out,
                       diag::DiagnosticSink* sink);

/// Writes the items of `translated` to `out`.
[[nodiscard]] diag::Status write_items(const Translated& translated, Encoder& out);

}  // namespace hefesto::oasis::detail
