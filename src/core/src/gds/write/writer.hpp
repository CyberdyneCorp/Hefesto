// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the GDS writer (src/core/src/gds/write/): the cell kinds of
// calmaProcessDef, structure naming (Q-MASK-209), cell references and the
// library writer that walks the hierarchy and writes each structure.
// Owner: GDSW.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/gds/write/emit.hpp"
#include "hefesto/gds/write/labels_text.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/mask/gen/context.hpp"

namespace hefesto::gds::detail {

struct VendorIo;
class FullDump;

/// The properties calmaProcessDef reads (LEFview, GDS_START, GDS_END,
/// CIFhier, GDS_FILE as a string).
struct CellKind {
  bool abstract = false;
  bool has_content = false;  // GDS_START
  bool has_end = false;      // GDS_END
  bool read_only = false;    // GDS_FILE holds a string
  bool needs_hier = false;   // CIFhier

  /// A full dump (GDS_START without GDS_END) writes no children.
  [[nodiscard]] bool traverses_children() const noexcept { return !has_content || has_end; }
  /// A read-only cell with content is copied from its vendor file.
  [[nodiscard]] bool copied() const noexcept { return read_only && has_content; }
};
[[nodiscard]] CellKind cell_kind(const db::CellDef& def);

/// "Structure naming": the STRNAME, SNAME and LIBNAME text of each cell of
/// one write. A legal name is written as it is (the character table maps it
/// on output), an illegal one as XXXXX<id>; when the encoded name is
/// already another cell's in this write, the later cell becomes XXXXX<id>,
/// then XXXXX<id>_<k> with the smallest free k (Q-MASK-209, both modes).
class StructureNames {
 public:
  /// Writes the name record `type` of `def`, whose id number is `id`.
  [[nodiscard]] diag::Status write(ElementWriter& out, RecordType type, const db::CellDef& def, std::int64_t id);
  /// The structure name text of `def` (chosen now when it has none yet).
  [[nodiscard]] std::string name_of(ElementWriter& out, const db::CellDef& def, std::int64_t id);
  /// The name name_of would return now, without choosing it (no name is
  /// claimed and no warning is printed).
  [[nodiscard]] std::string preview(const ElementWriter& out, const db::CellDef& def, std::int64_t id) const;
  /// Gives `def` the name `text` unless it has one already (Q-MASK-214
  /// OFF: an appended structure takes the name its references use).
  void assign(ElementWriter& out, const db::CellDef& def, const std::string& text);

 private:
  std::string choose(ElementWriter& out, const db::CellDef& def, std::int64_t id, bool legal);
  [[nodiscard]] std::string candidate(const ElementWriter& out, const db::CellDef& def, std::int64_t id,
                                      bool legal) const;
  [[nodiscard]] bool taken_by_other(const ElementWriter& out, const db::CellDef& def, const std::string& text) const;

  std::unordered_map<const db::CellDef*, std::string> chosen_;
  std::unordered_map<std::string, const db::CellDef*> taken_;  // encoded payload -> cell
};

/// True when `name` is a legal structure name for `out`'s table and style.
[[nodiscard]] bool legal_name(const ElementWriter& out, std::string_view name);

/// "Cell reference elements" and "Array flattening on output": the
/// element(s) of `use`, whose child is named through `names` with id `id`.
/// `cancelled` is polled while a flattened array is written.
[[nodiscard]] diag::Status write_reference(ElementWriter& out, StructureNames& names, const db::CellUse& use,
                                           std::int64_t id, const mask::GenerationContext& ctx);

/// Writes the library of a prepared write to `records`: header, the
/// structures in post-order from the root ("Structure ordering"), ENDLIB.
class LibraryWriter {
 public:
  /// `raw` is the stream under `records` (verbatim vendor copies).
  LibraryWriter(const PreparedWrite& prepared, RecordWriter& records, ByteSink& raw, diag::DiagnosticSink& sink,
                mask::GenerationContext& ctx);
  ~LibraryWriter();
  LibraryWriter(const LibraryWriter&) = delete;
  LibraryWriter& operator=(const LibraryWriter&) = delete;

  /// The whole library. `good` is false when a traversal failed (the
  /// stream is still terminated); an error status is a stream, range or
  /// cancellation failure that stopped the write.
  struct Result {
    bool good = true;
    diag::Status status = diag::Done{};
  };
  [[nodiscard]] Result write();

 private:
  enum class Visit : std::uint8_t { done, failed };
  enum class VendorVisit : std::uint8_t { done, failed, fallback };

  [[nodiscard]] diag::Status header();
  [[nodiscard]] Visit process(db::CellDef& def, bool library, const mask::HierarchyOptions& inherited,
                              std::size_t depth);
  [[nodiscard]] Visit process_children(const db::CellDef& def, const mask::HierarchyOptions& options,
                                       std::size_t depth, bool vendor);
  [[nodiscard]] Visit emit(const db::CellDef& def, const CellKind& kind, const mask::HierarchyOptions& options,
                           bool library);
  [[nodiscard]] VendorVisit vendor_structure(const db::CellDef& def, const CellKind& kind);
  [[nodiscard]] VendorVisit full_dump(const db::CellDef& def, ByteSource& source, const std::string& file);
  [[nodiscard]] db::CellDef& magnified_source(db::CellDef& def);
  [[nodiscard]] bool append_undefined();
  [[nodiscard]] Visit check_kind(const db::CellDef& def, const CellKind& kind);
  [[nodiscard]] diag::Status structure(const db::CellDef& def, const mask::HierarchyOptions& options);
  [[nodiscard]] diag::Status structure_body(const db::CellDef& def, ElementWriter& out,
                                            const mask::HierarchyOptions& options);
  [[nodiscard]] diag::Status mask_layers(const db::CellDef& def, ElementWriter& out,
                                         const mask::HierarchyOptions& options);
  [[nodiscard]] std::int64_t id_of(const db::CellDef& def) const;
  [[nodiscard]] std::int64_t now() const;
  [[nodiscard]] StructureContext context_for(const db::CellDef& def, const OutputScale& scale) const;
  void print(std::string text);

  const PreparedWrite& prepared_;
  RecordWriter& records_;
  diag::DiagnosticSink& sink_;
  mask::GenerationContext& ctx_;
  mask::GenerationSource source_;  // the editor's, with the prepared compat and unfracture
  StructureNames names_;
  PortLabelWarnings port_warnings_;  // Q-MASK-206: once per type and write
  std::unique_ptr<VendorIo> vendor_;
  std::unique_ptr<FullDump> dump_;  // calmaLibHash, calmaPrefixHash, calmaUndefHash
  // Magic's cd_client: 0 not yet numbered, negative numbered, positive
  // visited (in progress or written).
  std::unordered_map<const db::CellDef*, std::int64_t> ids_;
  std::int64_t next_id_ = -2;
  // A failure that stops the write (stream, range, cancellation).
  diag::Status error_ = diag::Done{};
};

}  // namespace hefesto::gds::detail
