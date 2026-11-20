// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// oasis::read (capability oasis-read; openspec/changes/m6-lef-def-oasis/
// design.md D4, D12): pass 1 (index.cpp), pass 2 (transcode*.cpp), then
// gds::read of the GDS-equivalent stream with the session's shared read
// settings (internal.hpp says why). After it:
// - the read-only cells' GDS_FILE/GDS_START/GDS_END become OASIS_FILE and
//   OASIS_CELL ("Read-only OASIS vendor views"), and their timestamps the
//   OASIS content stamps ("Content-hash timestamps for read-only OASIS
//   cells");
// - the messages get their OASIS positions and the `oasis warning` level
//   (messages.cpp).
// The whole read holds the exclusive database lock and records no undo
// events ("Read command and file lookup").
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include "hefesto/oasis/read/read.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/read/read.hpp"
#include "hefesto/gds/read/vendor.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/read/limits.hpp"
#include "hefesto/oasis/read/stamps.hpp"
#include "hefesto/oasis/read/vendor.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

namespace {

using read_detail::FileIndex;
using read_detail::ReadMessages;
using read_detail::Transcoded;

/// The GDS_FILE value the GDS reader gives read-only cells of this read,
/// replaced by OASIS_FILE afterwards; no path holds the control bytes.
constexpr std::string_view kVendorMarker = "\x01oasis read\x01";

/// Detaches the cell library's change recorder (UndoDisable).
class UndoDisabled {
 public:
  explicit UndoDisabled(db::CellLibrary& cells) : cells_(cells), recorder_(cells.recorder()) {
    cells_.set_recorder(nullptr);
  }
  ~UndoDisabled() { cells_.set_recorder(recorder_); }
  UndoDisabled(const UndoDisabled&) = delete;
  UndoDisabled& operator=(const UndoDisabled&) = delete;

 private:
  db::CellLibrary& cells_;
  db::ChangeRecorder* recorder_;
};

cmd::CommandResult failed(std::vector<diag::Diagnostic> messages) {
  cmd::CommandResult result;
  result.status = cmd::Status::failed;
  result.diagnostics = std::move(messages);
  return result;
}

/// The options of the GDS-equivalent read: the shared read settings and
/// the OASIS timestamp rule (Q-OASIS-30: a cell's date is unknown, so its
/// stamp is 0, or `datestamp <N>`, fixed when N != 0).
gds::GdsReadOptions gds_options(const OasisReadOptions& o, std::uint64_t bytes) {
  gds::GdsReadOptions g;
  g.stream_read = o.stream_read;
  g.settings.datestamp = o.settings.datestamp.value_or(0);
  g.settings.library = false;  // `oasis library` has no effect on reading
  g.rescale_allowed = o.rescale_allowed;
  g.typed_name = o.typed_name;
  g.files = o.files;
  g.gds_file = std::string{kVendorMarker};
  g.time_zone = "UTC0";
  g.source_bytes = bytes;
  return g;
}

/// The OASIS cell name of the structure whose STRNAME ends at `start`.
std::optional<std::string> structure_cell(const Transcoded& t, std::int64_t start) {
  const auto it = std::lower_bound(t.structures.begin(), t.structures.end(), start,
                                   [](const auto& s, std::int64_t v) { return static_cast<std::int64_t>(s.first) < v; });
  if (it == t.structures.end() || static_cast<std::int64_t>(it->first) != start) {
    return std::nullopt;
  }
  return it->second;
}

/// GDS_FILE/GDS_START/GDS_END of this read's read-only cells (and of the
/// abstract views it annotated) replaced by OASIS_FILE and OASIS_CELL, and
/// the content stamp assigned when `stamps` is given.
void convert_vendor_cells(editor::EditorState& editor, const Transcoded& t, const std::string& oasis_file,
                          const gds::StampTable* stamps) {
  std::vector<db::CellDef*> marked;
  editor.cells.for_each([&marked](db::CellDef& def) {
    const db::PropertyValue* file = def.properties().find("GDS_FILE");
    const auto* text = file != nullptr ? std::get_if<std::string>(file) : nullptr;
    if (text != nullptr && *text == kVendorMarker) {
      marked.push_back(&def);
    }
  });
  for (db::CellDef* def : marked) {
    const std::optional<std::int64_t> start = gds::property_integer(*def, "GDS_START");
    const std::string name = (start ? structure_cell(t, *start) : std::nullopt).value_or(def->name());
    for (const char* gone : {"GDS_START", "GDS_END", "GDS_FILE"}) {
      db::set_property(*def, gone, std::nullopt);
    }
    db::set_property(*def, kOasisFile, db::PropertyValue{oasis_file});
    db::set_property(*def, kOasisCell, db::PropertyValue{name});
    if (stamps != nullptr && def->flags().vendor_gds && !def->flags().fixed_timestamp) {
      const std::uint32_t stamp = lookup_stamp(*stamps, name);
      def->set_timestamp(stamp);
      def->flags().needs_new_timestamp = stamp == 0 && def->flags().needs_new_timestamp;
    }
  }
}

/// The `oasis warning redirect <file>` text written out.
void write_redirect(const OasisReadOptions& o, platform::FileSystem& fs, const ReadMessages& messages,
                    std::vector<diag::Diagnostic>& out) {
  if (o.warnings.level != mask::WarningLevel::redirect || o.warnings.redirect_file.empty()) {
    return;
  }
  if (auto written = fs.write_atomic(o.warnings.redirect_file, messages.redirected()); !written.ok()) {
    out.push_back(written.error());
  }
}

/// Pass 1's messages; false when no cell can be read.
bool report_index(const FileIndex& index, ReadMessages& messages) {
  for (const diag::Diagnostic& note : index.notes) {
    messages.print(note);
  }
  if (index.fatal) {
    diag::Diagnostic d = *index.fatal;
    if (index.started) {
      d.code = "OASIS-READ-003";
      d.severity = diag::Severity::error;
    }
    messages.print(std::move(d));
  }
  return index.started && !(index.fatal && index.cells.empty());
}

}  // namespace

cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path, const OasisReadOptions& options,
                        const mask::Progress* progress) {
  return read(editor, path, options, ReadLimits{}, progress);
}

cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path, const OasisReadOptions& options,
                        const ReadLimits& limits, const mask::Progress* progress) {
  if (editor.mask.input_styles.current_style() == nullptr || editor.technology == nullptr) {
    return failed({diag::make("OASIS-READ-001", "Nothing in \"cifinput\" section of tech file.")});
  }
  platform::LocalFileSystem local;
  platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
  auto source = gds::open_source(fs, path);
  if (!source.ok()) {
    return failed({diag::make("OASIS-READ-002", "Cannot open " + path.string() + " to read OASIS input."),
                   source.error()});
  }
  const db::DatabaseLock::Exclusive lock(editor.database_lock);
  ReadMessages messages(options.warnings);
  const FileIndex index = read_detail::index_file(*source.value());
  if (!report_index(index, messages) || !source.value()->seek(0).ok()) {
    return failed(messages.take());
  }
  const std::optional<std::string> top = read_detail::top_cell(index);
  const read_detail::TranscodeOptions transcode_options{options.settings.circlepoints,
                                                        editor.mask.input_styles.current_style(), top.value_or(""),
                                                        limits};
  Transcoded t = read_detail::transcode(*source.value(), index, transcode_options, messages);
  OasisReadOptions effective = options;
  effective.files = &fs;
  const gds::GdsReadOptions g = gds_options(effective, t.gds.size());
  const bool rewound = editor.mask.gds.rewound;  // an OASIS read never rewinds (Q-MASK-111's session flag)
  gds::MemorySource stream{std::move(t.gds)};
  cmd::CommandResult result = gds::read(editor, stream, g, progress);
  editor.mask.gds.rewound = rewound;
  messages.forward(std::move(result.diagnostics), t.positions);
  {
    const UndoDisabled no_undo(editor.cells);
    const bool stamp_default = options.settings.datestamp.value_or(0) == 0;
    std::optional<gds::StampTable> stamps;
    if (options.stream_read.readonly && stamp_default) {
      stamps = oasis::content_stamps(*source.value());
    }
    const std::string file = options.oasis_file.empty() ? gds::gds_file_value(path.string(), editor) : options.oasis_file;
    convert_vendor_cells(editor, t, file, stamps ? &*stamps : nullptr);
  }
  result.diagnostics = messages.take();
  write_redirect(options, fs, messages, result.diagnostics);
  return result;
}

}  // namespace hefesto::oasis
