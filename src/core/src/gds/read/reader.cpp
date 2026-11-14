// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The GDS reader's library level, messages, record helpers, cell tables,
// planes and end-of-read cleanup (capability gds-read "Library grammar and
// header processing", "Units and input scale", "Coordinate conversion and
// automatic rescaling", "Cells already in memory before reading",
// "End-of-read cleanup", "Read diagnostics", "Content-hash timestamps for
// read-only cells"). Ported from Magic 8.3.694 calma/CalmaRead.c
// (CalmaReadFile, calmaParseUnits, CalmaReadError, calmaUnexpected),
// calma/CalmaRdio.c (calmaReadI2Record, calmaReadI4Record,
// calmaReadStringRecord, calmaSkipSet, calmaSkipTo, calmaSkipExact),
// calma/CalmaRdcl.c (calmaFindCell, calmaLookCell, calmaUniqueCell,
// calmaExact), calma/CalmaRdpt.c (calmaInputRescale, calmaReadX,
// calmaReadY) and cif/CIFrdcl.c (CIFReadCellCleanup); Magic is (c) the
// Regents of the University of California, distributed under its permissive
// license (THIRD_PARTY_NOTICES.md).
//
// Deviations (X-2 tier 1): the per-cell read flags (CDPRELOADED,
// CDPROCESSEDGDS) live for one read, where Magic leaves stale bits on the
// definitions; "used but not defined" is reported in creation order (Magic's
// hash order is not specified).
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "gds/read/reader.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/read/vendor.hpp"
#include "hefesto/gds/stream/values.hpp"
#include "hefesto/tech/commands.hpp"

namespace hefesto::gds::read_detail {

namespace {

constexpr std::string_view kReadError = "GDS-READ-006";
constexpr std::string_view kErrorLimit = "GDS-READ-007";
constexpr std::string_view kProgress = "GDS-READ-008";
constexpr std::string_view kNoStyle = "GDS-READ-009";

/// The first technology-dependent plane (PL_TECHDEPBASE).
constexpr tech::PlaneId kTechPlaneBase = tech::kPlaneRHint + 1;

std::uint8_t type_of(RecordType t) {
  return static_cast<std::uint8_t>(t);
}

/// Forwards the read engine's messages through the read counter.
class EngineSink final : public diag::DiagnosticSink {
 public:
  explicit EngineSink(Reader& reader) : reader_(reader) {}
  void emit(const diag::Diagnostic& d) override { reader_.engine_message(d); }

 private:
  Reader& reader_;
};

/// dbScalePlane by an integer factor (CIFScalePlanes(r, 1)).
void scale_plane(mask::MaskPlane& plane, std::int64_t r, bool magic_compat) {
  const std::vector<db::TileView> tiles = plane.solid_tiles();
  plane.clear();
  const auto scale = [r](db::Coord v) {
    return static_cast<db::Coord>(std::clamp<std::int64_t>(std::int64_t{v} * r, db::kPlaneMin, db::kPlaneMax));
  };
  for (const db::TileView& t : tiles) {
    const db::Rect area{scale(t.area.x0), scale(t.area.y0), scale(t.area.x1), scale(t.area.y1)};
    if (t.split) {
      (void)plane.paint_triangle(area, t.diagonal, t.right_side, magic_compat);
    } else {
      (void)plane.paint(area);
    }
  }
}

void scale_set(PlaneSet& set, std::int64_t r, bool magic_compat) {
  for (auto& plane : set) {
    if (plane) {
      scale_plane(*plane, r, magic_compat);
    }
  }
}

}  // namespace

// --- messages ---------------------------------------------------------------------

bool Messages::admit(std::int64_t count, std::string_view limit_text) {
  if (warnings_.level == mask::WarningLevel::none) {
    return false;
  }
  if (count < 100 || warnings_.level != mask::WarningLevel::limit) {
    return true;
  }
  if (count == 100) {
    printed_.push_back(diag::make(kErrorLimit, std::string{limit_text}));
  }
  return false;
}

void Messages::emit(diag::Diagnostic d) {
  if (warnings_.level == mask::WarningLevel::redirect) {
    if (!warnings_.redirect_file.empty()) {
      redirected_ += d.text;
      if (!d.no_newline) {
        redirected_ += "\n";
      }
    }
    return;
  }
  printed_.push_back(std::move(d));
}

void Messages::counted(std::string_view cell, std::uint64_t position, diag::Diagnostic d, bool no_newline) {
  if (!admit(++calma_errors_, "Error limit set:  Remaining errors will not be reported.")) {
    return;
  }
  d.text = "Error while reading cell \"" + std::string{cell} + "\" (byte position " + std::to_string(position) +
           "): " + d.text;
  d.no_newline = no_newline;
  emit(std::move(d));
}

void Messages::detail(diag::Diagnostic d) {
  const bool limited = warnings_.level == mask::WarningLevel::limit && calma_errors_ >= 100;
  if (warnings_.level != mask::WarningLevel::none && !limited) {
    emit(std::move(d));
  }
}

void Messages::cif_error(diag::Diagnostic d) {
  if (admit(++cif_errors_, "Error limit set:  Remaining errors will not be reported.")) {
    d.text = "CIF file read error: " + d.text;
    printed_.push_back(std::move(d));  // CIFReadError always prints to the console
  }
}

void Messages::cif_warning(diag::Diagnostic d) {
  if (admit(++cif_warnings_, "Warning limit set:  Remaining warnings will not be reported.")) {
    d.text = "CIF file read warning: " + d.text;
    printed_.push_back(std::move(d));
  }
}

void Messages::print(diag::Diagnostic d) {
  printed_.push_back(std::move(d));
}

void Reader::error(std::string text, bool no_newline) {
  const db::CellDef* cell = current_ != nullptr ? current_ : editor_.edit_cell;
  messages_.counted(cell != nullptr ? cell->name() : std::string{}, records_.position(),
                    diag::make(kReadError, std::move(text)), no_newline);
}

void Reader::counted(diag::Diagnostic d) {
  const db::CellDef* cell = current_ != nullptr ? current_ : editor_.edit_cell;
  messages_.counted(cell != nullptr ? cell->name() : std::string{}, records_.position(), std::move(d));
}

void Reader::engine_message(diag::Diagnostic d) {
  // The engine's grid and unit messages are CIFReadWarnings; the snap
  // message is given once per session (cifSeenSnapWarning, which only a
  // CIF read resets); the boundary redefinition is a CalmaReadError.
  if (d.code == "MASK-INPUT-010" || d.code == "MASK-INPUT-011") {
    if (editor_.mask.gds.snap_warned) {
      return;
    }
    editor_.mask.gds.snap_warned = true;
    messages_.cif_warning(std::move(d));
  } else if (d.code == "MASK-INPUT-012") {
    messages_.cif_warning(std::move(d));
  } else if (d.code == "MASK-INPUT-013") {
    counted(std::move(d));
  } else {
    messages_.print(std::move(d));
  }
}

void Reader::info(std::string text) {
  messages_.print(diag::make(kProgress, std::move(text)));
}

void Reader::unexpected(RecordType wanted, std::uint8_t got) {
  // calmaUnexpected: the counted line, then the detail line unprefixed.
  error("Unexpected record type in input: ");
  messages_.detail(diag::make(kReadError, "    Expected " + record_name(static_cast<std::uint8_t>(wanted)) +
                                              " record but got " + record_name(got) + "."));
}

void Reader::header_error(diag::Diagnostic d) {
  const std::string_view text = d.text;
  const std::size_t expected = text.find("Expected ");
  if (d.code != "GDS-READ-001" || expected == std::string_view::npos) {
    counted(std::move(d));
    return;
  }
  error("Unexpected record type in input: ");
  messages_.detail(diag::make(kReadError, "    " + std::string{text.substr(expected)}));
}

// --- construction and the library ---------------------------------------------

Reader::Reader(editor::EditorState& editor, ByteSource& source, const GdsReadOptions& options,
               const mask::Progress* progress)
    : editor_(editor),
      source_(source),
      records_(source),
      options_(options),
      progress_(progress),
      compat_(editor.drc.compat_settings()),
      messages_(options.warnings) {}

Reader::~Reader() = default;

void Reader::run() {
  const mask::InputStyle* style = editor_.mask.input_styles.current_style();
  if (style == nullptr || editor_.technology == nullptr) {
    messages_.print(diag::make(kNoStyle, "Don't know how to read GDS-II:\nNothing in \"cifinput\" section of tech file."));
    no_style_ = true;
    return;
  }
  info("Warning: Calma reading is not undoable!  I hope that's OK.");
  engine_sink_ = std::make_unique<EngineSink>(*this);
  mask::ReadEngine::Options engine_options;
  engine_options.rescale_allowed = options_.settings.rescale;
  engine_options.copyup_on_call = true;
  engine_options.sink = engine_sink_.get();
  engine_ = std::make_unique<mask::ReadEngine>(editor_, *style, compat_, engine_options);
  if (read_header()) {
    while (parse_structure()) {
      if (cancelled()) {
        interrupted_ = true;
        break;
      }
    }
    if (!interrupted_) {
      (void)skip_exact(RecordType::endlib);
    }
  }
  if (libname_ && look_cell(*libname_) != nullptr) {
    library_cell_ = libname_;
  }
  end_of_read();
  (void)engine_->finish();
  editor_.mask.input_styles.replace_current(std::make_shared<const mask::InputStyle>(engine_->style()));
}

bool Reader::read_header() {
  diag::CollectingSink header_messages;
  auto header = read_library_header(records_, header_messages);
  for (diag::Diagnostic& d : header_messages.take()) {
    messages_.print(std::move(d));
  }
  if (!header.ok()) {
    header_error(header.error());
    return false;
  }
  const LibraryName name = library_cell_name(header.value().libname, options_.settings.library);
  if (name.name) {
    if (name.modified) {
      info("Library name modified to make legal cell name syntax.");
    }
    info("Library name: " + *name.name);
    libname_ = name.name;
  }
  const double mpd = header.value().meters_per_unit;
  if (options_.settings.readonly) {
    const mask::OutputStyle* out = editor_.mask.output_styles.current_style();
    const bool angstroms = out != nullptr && out->has_option(mask::style_option::angstroms);
    if (auto refused = readonly_units_error(mpd, angstroms)) {
      counted(std::move(*refused));
      messages_.print(diag::make("GDS-READ-003", "Cannot read this file in read-only mode."));
      readonly_refused_ = true;
      return false;
    }
  }
  const std::int64_t limit = editor_.mask.cif.limit > 0 && editor_.mask.cif.limit <= CoordinateScale::kDefaultLimit
                                 ? editor_.mask.cif.limit
                                 : CoordinateScale::kDefaultLimit;
  scale_ = std::make_unique<CoordinateScale>(units_scale(mpd, engine_->style().multiplier), limit);
  return true;
}

void Reader::end_of_read() {
  const bool drc = options_.settings.drccheck;
  for (db::CellDef* def : cell_order_) {
    if (!def->flags().available) {
      error("cell " + def->name() + " was used but not defined.");
    }
    if (drc) {
      drc::check_this(editor_, *def, def->bbox());
    }
    db::mark_modified(*def);
    if (def->timestamp() != 0) {
      def->flags().needs_new_timestamp = false;
    }
  }
  for (db::CellDef* def : cell_order_) {
    const auto& uses = def->uses();
    if (std::any_of(uses.begin(), uses.end(), [](const auto& u) { return u->id.empty(); })) {
      (void)db::make_use_ids_unique(*def, compat_.magic_compat);  // a structure that ended before its ids were made
    }
  }
  if (db::CellDef* edit = editor_.edit_cell) {
    (void)db::adjust_labels(*edit, *editor_.technology, db::whole_plane());
    db::recompute_bbox(*edit, compat_.magic_compat);
    db::mark_modified(*edit);
  }
  flat_planes_.clear();
  const bool stamp_default = !options_.settings.datestamp || *options_.settings.datestamp == 0;
  if (options_.settings.readonly && !interrupted_ && !readonly_refused_ && stamp_default &&
      source_.seek(0).ok()) {
    const StampTable table = content_stamps(source_);
    assign_content_stamps(table, cell_order_);
  }
}

// --- records ----------------------------------------------------------------------

std::optional<Record> Reader::next() {
  auto record = records_.next();
  if (!record.ok()) {
    if (!stream_error_reported_) {
      stream_error_reported_ = true;
      counted(record.error());
    }
    return std::nullopt;
  }
  return std::move(record.value());
}

std::optional<Record> Reader::peek() {
  auto record = records_.peek();
  if (!record.ok()) {
    if (!stream_error_reported_) {
      stream_error_reported_ = true;
      counted(record.error());
    }
    return std::nullopt;
  }
  return std::move(record.value());
}

void Reader::unread(Record record) {
  records_.unread(std::move(record));
}

std::optional<Record> Reader::expect(RecordType type) {
  std::optional<Record> record = next();
  if (!record) {
    error("Unexpected EOF.");
    return std::nullopt;
  }
  if (record->type != type_of(type)) {
    unexpected(type, record->type);
    return std::nullopt;
  }
  return record;
}

void Reader::skip_set(std::initializer_list<RecordType> types) {
  for (;;) {
    const std::optional<Record> record = peek();
    if (!record || std::none_of(types.begin(), types.end(), [&](RecordType t) { return record->type == type_of(t); })) {
      return;
    }
    (void)next();
  }
}

bool Reader::skip_to(RecordType type) {
  for (;;) {
    const std::optional<Record> record = next();
    if (!record || is_padding(*record)) {
      return false;
    }
    if (record->type == type_of(type)) {
      return true;
    }
  }
}

bool Reader::skip_exact(RecordType type) {
  const std::optional<Record> record = next();
  if (!record) {
    error("Unexpected EOF.");
    return false;
  }
  if (record->type != type_of(type)) {
    unexpected(type, record->type);
    return false;
  }
  return true;
}

std::optional<std::int32_t> Reader::read_i2(RecordType type) {
  const std::optional<Record> record = expect(type);
  if (!record) {
    return std::nullopt;
  }
  const auto value = i2_at(record->data, 0);
  if (!value) {
    error("Unexpected EOF.");
    return std::nullopt;
  }
  return *value;
}

std::optional<std::int32_t> Reader::read_i4(RecordType type) {
  const std::optional<Record> record = expect(type);
  if (!record) {
    return std::nullopt;
  }
  const auto value = i4_at(record->data, 0);
  if (!value) {
    error("Unexpected EOF.");
    return std::nullopt;
  }
  return *value;
}

std::optional<std::string> Reader::read_string(RecordType type) {
  const std::optional<Record> record = expect(type);
  if (!record) {
    return std::nullopt;
  }
  return string_value(record->data);
}

// --- cell tables --------------------------------------------------------------------

db::CellDef* Reader::look_cell(std::string_view name) const {
  const auto it = cif_cells_.find(std::string{name});
  return it == cif_cells_.end() ? nullptr : it->second;
}

db::CellDef* Reader::create_cell(const std::string& name) {
  auto created = editor_.cells.create(name, editor_.technology->plane_count());
  if (!created.ok()) {
    counted(created.error());
    return nullptr;
  }
  db::recompute_bbox(*created.value(), compat_.magic_compat);
  return created.value();
}

bool Reader::note_existing_cell(const std::string& name) {
  if (!options_.settings.noduplicates) {
    info("Warning:  cell " + name + " already existed before reading GDS!");
    return false;
  }
  info("Note:  cell " + name + " already existed before reading GDS.");
  info("Using pre-existing cell definition");
  return true;
}

db::CellDef* Reader::find_cell(const std::string& name, bool* was_called, bool* predefined) {
  // calmaFindCell: `was_called` is whether the name was known (in this
  // read's table, or in memory) before; `predefined` whether `gds
  // noduplicates` keeps the existing definition.
  auto [it, inserted] = cif_cells_.try_emplace(name, nullptr);
  bool called = it->second != nullptr;
  if (!called) {
    db::CellDef* def = editor_.cells.find(name);
    called = def != nullptr;
    if (def == nullptr) {
      def = create_cell(name);
    } else if (note_existing_cell(name) && predefined != nullptr) {
      *predefined = true;
    }
    if (def == nullptr) {
      cif_cells_.erase(it);
      return nullptr;
    }
    it->second = def;
    if (in_order_.insert(def).second) {
      cell_order_.push_back(def);
    }
  }
  if (was_called != nullptr) {
    *was_called = called;
  }
  return it->second;
}

void Reader::unique_cell(const std::string& name) {
  const auto it = cif_cells_.find(name);
  if (it != cif_cells_.end() && it->second == nullptr) {
    return;  // the existing cell came from this file
  }
  db::CellDef* def = editor_.cells.find(name);
  if (def == nullptr || !def->flags().available) {
    return;
  }
  std::string renamed;
  for (std::int64_t n = 1;; ++n) {
    renamed = name + "_" + std::to_string(n);
    if (editor_.cells.find(renamed) == nullptr) {
      break;
    }
  }
  (void)editor_.cells.rename_definition(*def, renamed);
  cif_cells_[name] = nullptr;
  error("Warning: cell definition \"" + name + "\" reused.");
}

void Reader::forget_cell(db::CellDef& def) {
  std::erase_if(cif_cells_, [&](const auto& entry) { return entry.second == &def; });
  std::erase_if(def_init_, [&](const auto& entry) { return entry.second == &def; });
  std::erase(cell_order_, &def);
  in_order_.erase(&def);
  states_.erase(&def);
  flat_planes_.erase(&def);
}

// --- planes ---------------------------------------------------------------------

std::vector<mask::MaskPlane*>& Reader::planes() {
  return engine_->read_planes();
}

PlaneSet Reader::save_planes(bool clear) {
  PlaneSet saved;
  for (mask::MaskPlane* plane : planes()) {
    if (plane == nullptr || plane->empty()) {
      saved.emplace_back();
      continue;
    }
    auto copy = std::make_unique<mask::MaskPlane>();
    copy->copy_from(*plane);
    saved.push_back(std::move(copy));
    if (clear) {
      plane->clear();
    }
  }
  return saved;
}

void Reader::restore_planes(const PlaneSet& saved) {
  std::vector<mask::MaskPlane*>& current = planes();
  for (std::size_t i = 0; i < current.size(); ++i) {
    if (current[i] == nullptr) {
      continue;
    }
    current[i]->clear();
    if (i < saved.size() && saved[i]) {
      current[i]->copy_from(*saved[i]);
    }
  }
}

void Reader::rescale_read_units(std::int64_t r) {
  engine_->rescale(r, 1);
  for (auto& [def, set] : flat_planes_) {
    scale_set(set, r, compat_.magic_compat);
  }
  for (PlaneSet* set : saved_sets_) {
    scale_set(*set, r, compat_.magic_compat);
  }
}

std::int64_t Reader::scale_factor() const {
  return std::max<std::int64_t>(engine_->style().scale_factor, 1);
}

std::int64_t Reader::convert(std::int32_t v, std::int64_t k, std::int64_t& rescale) {
  // calmaReadX / calmaReadY.
  const Conversion c = scale_->convert(v, k);
  rescale = c.rescale;
  if (c.rounded) {
    error("Warning:  calma units at max scale; value rounded");
  }
  if (c.rescale > 1) {
    rescale_read_units(c.rescale);
  }
  return c.value;
}

// --- shared helpers -------------------------------------------------------------

void clear_definition(db::CellDef& def) {
  while (!def.uses().empty()) {
    (void)db::remove_use(*def.uses().back());
  }
  def.clear_use_ids();
  const std::size_t planes = def.plane_count();
  def.set_plane_count(0);
  def.set_plane_count(planes);
  def.set_bboxes(db::Rect{0, 0, 1, 1}, db::Rect{0, 0, 1, 1});
  def.labels().clear();
  def.elements().clear();
  db::clear_properties(def);
}

bool is_ancestor(const db::CellDef& cell, const db::CellDef& of) {
  std::vector<const db::CellDef*> stack{&of};
  std::unordered_set<const db::CellDef*> seen{&of};
  while (!stack.empty()) {
    const db::CellDef* def = stack.back();
    stack.pop_back();
    if (def == &cell) {
      return true;
    }
    for (const db::CellUse* use : def->instances()) {
      if (use->parent != nullptr && seen.insert(use->parent).second) {
        stack.push_back(use->parent);
      }
    }
  }
  return false;
}

bool has_contents(const db::CellDef& def) {
  if (!def.uses().empty()) {
    return true;
  }
  tech::TypeMask paint;
  for (std::size_t t = tech::kMagnet; t < tech::kMaxTileTypes; ++t) {
    paint.set(static_cast<tech::TileType>(t));
  }
  for (std::size_t p = kTechPlaneBase; p < def.plane_count(); ++p) {
    bool found = false;
    def.plane(static_cast<tech::PlaneId>(p)).enumerate(db::whole_plane(), paint, [&](const db::TileView&) {
      found = true;
      return false;
    });
    if (found) {
      return true;
    }
  }
  return false;
}

}  // namespace hefesto::gds::read_detail
