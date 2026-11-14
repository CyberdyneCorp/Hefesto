// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the GDS writer (src/core/src/gds/write/): the structures of
// read-only vendor cells (gds-write "Verbatim copy of read-only cells",
// "Full dump of a vendor library"). LibraryWriter calls them from its
// calmaProcessDef port:
// - open_vendor_gds: CalmaOpenVendorGDS, the file named by GDS_FILE on the
//   cell search path and the library path, with `.gz`, then relative to
//   the cell's .mag directory;
// - copy_vendor_structure: the new-style (GDS_START) or old-style
//   (GDS_BEGIN) copy of [start, end) with Magic's sanity checks; a failed
//   check makes the caller generate the cell from its layout;
// - FullDump: calmaFullDump and calmaDumpStructure with the write's
//   library, prefix and undefined-reference tables.
// The vendor file is only read, and nothing in the layout changes
// (Q-MASK-208): Magic's CDVENDORGDS mark is already implied by GDS_FILE.
// Owner: GDSW.

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/gds/write/write.hpp"
#include "writer.hpp"

namespace hefesto::gds::detail {

/// What the vendor copies need from the write in progress.
struct VendorIo {
  const PreparedWrite& prepared;
  RecordWriter& records;
  ByteSink& raw;  // the stream under `records` (flushed before raw bytes)
  diag::DiagnosticSink& sink;
  StructureNames& names;
  std::function<std::int64_t(const db::CellDef&)> id_of;
  std::int64_t now = 0;
  /// The first failure that stops the write (a stream error).
  diag::Status error = diag::Done{};

  [[nodiscard]] platform::FileSystem& files() const;
  [[nodiscard]] StructureContext context(const db::CellDef& def) const;
  void report(std::string_view code, std::string text) const;
  /// A Hefesto-only message (no Magic counterpart).
  void report_native(std::string_view code, std::string text) const;
};

/// An opened vendor GDS file.
struct VendorFile {
  platform::Path path;  // the file opened (Magic's retfilename)
  std::unique_ptr<ByteSource> source;
};

/// CalmaOpenVendorGDS(def, name): nullopt when no candidate opens.
[[nodiscard]] std::optional<VendorFile> open_vendor_gds(VendorIo& io, const db::CellDef& def,
                                                        const std::string& name);

/// The result of a verbatim copy.
enum class CopyResult : std::uint8_t {
  copied,    // the structure is in the output
  fallback,  // a sanity check failed before anything was written
  failed,    // the output is invalid (a short read or write after the header)
};

/// The verbatim copy of read-only cell `def` (GDS_START/GDS_BEGIN to
/// GDS_END of `source`). SNAME records are kept (magic-compat ON, with a
/// warning for an SNAME that names no child's structure) or rewritten to
/// the children's output names (OFF), Q-MASK-216.
[[nodiscard]] CopyResult copy_vendor_structure(VendorIo& io, const db::CellDef& def, ByteSource& source);

/// calmaFullDump and the write's tables it shares across dumps.
class FullDump {
 public:
  /// A library whose dump started (calmaLibHash) or was refused for its
  /// units in this write (Q-MASK-212: reported once).
  [[nodiscard]] bool seen(const std::string& file) const;
  /// A library refused for its units in this write.
  [[nodiscard]] bool refused(const std::string& file) const;

  /// The dump of `file` triggered by `def`. False when the library's units
  /// are incompatible (Q-MASK-212, both modes): nothing is written.
  [[nodiscard]] bool dump(VendorIo& io, const db::CellDef& def, ByteSource& source, const std::string& file);

  /// A forward reference no dump defined (calmaUndefHash, flag '0'), in
  /// the order it was recorded.
  struct Undefined {
    std::string name;      // the structure name in the vendor file
    std::string recorded;  // the SNAME written for it (prefix + name, or the [[0]] form)
  };
  [[nodiscard]] std::vector<Undefined> undefined() const;

  /// One dump in progress (dump.cpp).
  struct Dump;

 private:
  [[nodiscard]] bool dump_structure(Dump& d);
  [[nodiscard]] bool copy_elements(Dump& d, const std::optional<std::string>& prefix);
  [[nodiscard]] std::string choose_prefix(VendorIo& io, const db::CellDef& def, const std::string& file);
  [[nodiscard]] std::optional<std::string> undefined_prefix(VendorIo& io, const std::string& name);
  void record_undefined(const std::map<std::string, std::string>& names);
  [[nodiscard]] bool is_undefined(const std::string& name) const;

  std::vector<std::pair<std::string, std::string>> libraries_;  // file -> prefix, in dump order
  std::set<std::string> refused_;
  std::set<std::string> prefixes_;
  // calmaUndefHash in record order: a removed entry stays as a dead slot
  // (`live` false) so the indexes of the others hold; a name recorded again
  // after its removal is appended.
  struct UndefinedEntry {
    std::string name;
    std::string recorded;  // '0'/'1' + the SNAME written
    bool live = true;
  };
  std::vector<UndefinedEntry> undefined_;
  std::unordered_map<std::string, std::size_t> undefined_index_;  // live name -> slot in undefined_
};

}  // namespace hefesto::gds::detail
