// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDS structures (capability gds-read "Structure processing and cell
// creation", "Structure timestamps", "Duplicate structures within one
// file", "Cells already in memory before reading", "Element dispatch and
// unsupported content", "Post-order reading and file repositioning",
// "Flattening small cells on input", "End-of-structure processing",
// "Read-only vendor views", "Mask hints on input"). Ported from Magic
// 8.3.694 calma/CalmaRdcl.c (calmaParseStructure, calmaParseElement,
// calmaSetPosition, calmaNextCell, calmaFlattenPolygonFunc and the
// look-ahead of calmaElementSref) and cif/CIFrdcl.c (the `gds maskhints`
// step of CIFPaintCurrent); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Compat decisions: Q-MASK-110 (a malformed structure's planes leak into
// the next structure ON, are cleared OFF), Q-MASK-111 (the duplicate
// messages are suppressed after a rewind in this session ON, in this read
// OFF; the rewound flag is GdsSettings::rewound), Q-MASK-126 (fix: the
// look-ahead returns to the BGNSTR record's own offset).
//
// Deviations (X-2 tier 1): the look-ahead and magnified-copy recursion is
// capped at kMaxNesting (GDS-READ-011; the reference is then placed without
// reading ahead). `gds maskhints` compares the input planes as read, before
// the templayers of the cifinput style replace them (the read engine clears
// its planes after paint_current).
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>
#include <utility>

#include "gds/read/reader.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/hints/hints.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/tech/commands.hpp"

namespace hefesto::gds::read_detail {

namespace {

constexpr std::string_view kCapReached = "GDS-READ-011";
constexpr std::string_view kCompatNote = "GDS-READ-012";

std::uint8_t type_of(RecordType t) {
  return static_cast<std::uint8_t>(t);
}

bool matches_any(const std::vector<std::string>& patterns, std::string_view name) {
  for (const std::string& pattern : patterns) {
    if (db::glob_match(pattern, name)) {
      return true;
    }
  }
  return false;
}

std::optional<std::size_t> hint_output_layer(const mask::OutputStyle& out, std::string_view name) {
  for (std::size_t j = 0; j < out.layers.size(); ++j) {
    if (out.layers[j].name != name) {
      continue;
    }
    for (const mask::LayerOp& op : out.layers[j].ops) {
      if (op.kind == mask::OpKind::mask_hints) {
        return j;
      }
    }
    return std::nullopt;
  }
  return std::nullopt;
}

db::Rect scaled_rect(const db::Rect& r, std::int64_t num, std::int64_t den) {
  const auto scale = [num, den](db::Coord v) {
    return static_cast<db::Coord>(std::clamp<std::int64_t>(tech::scale_value(v, num, den), db::kPlaneMin, db::kPlaneMax));
  };
  return db::Rect{scale(r.x0), scale(r.y0), scale(r.x1), scale(r.y1)};
}

}  // namespace

// --- positioning ----------------------------------------------------------------

void Reader::next_cell() {
  // calmaNextCell: up to (not into) the next BGNSTR or ENDLIB.
  for (;;) {
    std::optional<Record> record = next();
    if (!record || is_padding(*record)) {
      return;
    }
    if (record->type == type_of(RecordType::bgnstr) || record->type == type_of(RecordType::endlib)) {
      unread(std::move(*record));
      return;
    }
  }
}

Reader::Search Reader::find_structure(const std::string& name) {
  Search out;
  for (;;) {
    std::optional<Record> record = next();
    if (!record) {
      return out;
    }
    if (is_padding(*record)) {
      out.padding = true;  // Magic's search ends here without reaching EOF
      return out;
    }
    if (record->type != type_of(RecordType::bgnstr)) {
      continue;
    }
    const std::optional<std::string> strname = read_string(RecordType::strname);
    if (strname && *strname == name) {
      out.offset = record->offset;  // Q-MASK-126 (fix): the BGNSTR record's own offset
      return out;
    }
  }
}

Reader::Position Reader::set_position(const std::string& name) {
  Position out;
  out.original = records_.position();
  Search search = find_structure(name);
  if (!search.offset && out.original != 0 && records_.seek(0).ok()) {
    rewound_ = true;
    editor_.mask.gds.rewound = true;  // CalmaRewound (Q-MASK-111 ON)
    search = find_structure(name);
    if (!search.offset && editor_.cells.find(name) == nullptr) {
      error("Cell \"" + name + "\" is used but not defined in this file.");
    }
    if (!options_.settings.ordering) {
      error("Rewinding input.  Cells may have been instanced before they were defined.  Consider using \"gds "
            "ordering on\".");
    }
  } else if (!search.offset && editor_.cells.find(name) == nullptr) {
    error("Cell \"" + name + "\" is used but not defined in this file.");
  }
  out.found = search.offset && records_.seek(*search.offset).ok();
  out.padding = !out.found && search.padding;
  return out;
}

db::CellDef* Reader::read_ahead(const std::string& name) {
  if (nesting_ >= kMaxNesting) {
    counted(diag::make(kCapReached, "look-ahead of cell " + name + " not made: more than " +
                                        std::to_string(kMaxNesting) + " nested structure reads"));
    return nullptr;
  }
  const Position position = set_position(name);
  if (!position.found) {
    (void)records_.seek(position.original);
    db::CellDef* def = find_cell(name, nullptr, nullptr);
    if (def != nullptr) {
      def->flags().dereference = true;  // CDDEREFERENCE until a definition clears it
    }
    return def;
  }
  db::CellDef* reading = current_;
  PlaneSet saved = save_planes(true);
  saved_sets_.push_back(&saved);
  const auto saved_errors = layer_errors_;
  ++nesting_;
  (void)parse_structure();
  --nesting_;
  saved_sets_.pop_back();
  (void)records_.seek(position.original);
  current_ = reading;
  restore_planes(saved);
  layer_errors_ = saved_errors;
  db::CellDef* def = look_cell(name);
  if (def != nullptr) {
    state(*def).preloaded = true;
  }
  return def;
}

// --- structures -----------------------------------------------------------------

bool Reader::duplicate_structure(std::string& name) {
  const auto it = def_init_.find(name);
  if (it == def_init_.end() || it->second == nullptr) {
    return false;
  }
  db::CellDef* def = it->second;
  CellState& st = state(*def);
  if (st.preloaded) {
    st.preloaded = false;  // read ahead: no complaint
    next_cell();
    return true;
  }
  if (st.processed) {
    const bool rewound = compat_.magic_compat ? editor_.mask.gds.rewound : rewound_;
    if (!options_.settings.ordering && !rewound) {
      current_ = def;
      error("Cell \"" + name + "\" was already defined in this file.");
      error("Ignoring duplicate definition");
    }
    next_cell();
    return true;
  }
  current_ = def;
  error("Cell \"" + name + "\" was already defined in this file.");
  std::string renamed;
  for (std::int64_t suffix = 1;; ++suffix) {
    renamed = name + "_" + std::to_string(suffix);
    const auto taken = def_init_.find(renamed);
    if (taken == def_init_.end() || taken->second == nullptr) {
      break;
    }
  }
  error("Giving this cell a new name: " + renamed);
  name = std::move(renamed);
  return false;
}

void Reader::begin_definition(db::CellDef& def, std::int64_t date, bool magnified) {
  clear_definition(def);
  def.flags().available = true;  // DBCellSetAvail
  def.flags().dereference = false;
  const CellStamp stamp = cell_stamp(date, options_.settings.datestamp);
  def.set_timestamp(stamp.value);
  if (stamp.fixed) {
    def.flags().fixed_timestamp = true;
  }
  if (options_.settings.readonly && !magnified) {
    def.flags().vendor_gds = true;  // CDVENDORGDS
  }
}

void Reader::annotate_vendor(db::CellDef& def, std::uint64_t start, std::uint64_t end) {
  db::set_property(def, "GDS_START", db::PropertyValue{db::DoubleList{{static_cast<std::int64_t>(start)}}});
  db::set_property(def, "GDS_END", db::PropertyValue{db::DoubleList{{static_cast<std::int64_t>(end)}}});
  db::set_property(def, "GDS_FILE", db::PropertyValue{options_.gds_file});
}

bool Reader::flatten_decision(const std::string& name, bool was_called, const ElementCounts& counts) const {
  if (was_called) {
    return false;
  }
  if (matches_any(options_.settings.flatglob, name)) {
    return true;
  }
  return options_.settings.flatten && counts.shapes < options_.settings.flatten_limit && counts.references == 0;
}

bool Reader::read_elements(ElementCounts& counts) {
  skip_set({RecordType::strclass, RecordType::strtype});
  layer_errors_.clear();
  non_manhattan_ = false;
  while (parse_element(counts)) {
    if (cancelled()) {
      interrupted_ = true;
      return false;
    }
    if (counts.references > 0 && counts.references % 5000 == 0 && counts.references != last_reported_) {
      last_reported_ = counts.references;
      info("    " + std::to_string(counts.references) + " uses");
    }
    non_manhattan_ = false;
  }
  return true;
}

bool Reader::parse_structure() {
  const std::optional<Record> head = peek();
  if (!head || head->type != type_of(RecordType::bgnstr)) {
    return false;
  }
  const std::int64_t polygons_before = polygon_count_;
  const bool magnified = mag_name_.has_value();
  const Record bgnstr = *next();
  const StructureDate date = structure_date(bgnstr, options_.time_zone, compat_.magic_compat);
  if (!date.known_format) {
    counted(unknown_timestamp_format());
  }
  std::optional<std::string> strname = read_string(RecordType::strname);
  if (!strname) {
    return skip_to(RecordType::endstr);
  }
  std::string name = magnified ? *mag_name_ : *strname;
  mag_name_.reset();
  info("Reading \"" + name + "\".");
  const std::uint64_t start = records_.position();
  if (duplicate_structure(name)) {
    return true;
  }
  if (options_.settings.unique) {
    unique_cell(name);
  }
  bool was_called = false;
  bool predefined = false;
  db::CellDef* def = find_cell(name, &was_called, &predefined);
  if (def == nullptr) {
    return skip_to(RecordType::endstr);
  }
  current_ = def;
  def_init_[name] = def;
  predefined = predefined && !magnified;
  return predefined ? predefined_structure(*def, start) : structure_body(*def, name, was_called, magnified, date.time,
                                                                         start, polygons_before);
}

bool Reader::predefined_structure(db::CellDef& def, std::uint64_t start) {
  const db::PropertyValue* lef = def.properties().find("LEFview");
  if (lef == nullptr) {
    next_cell();
    return true;
  }
  (void)skip_to(RecordType::endstr);
  annotate_vendor(def, start, records_.position());
  return true;
}

bool Reader::structure_body(db::CellDef& def, const std::string& name, bool was_called, bool magnified,
                            std::int64_t date, std::uint64_t start, std::int64_t polygons_before) {
  begin_definition(def, date, magnified);
  ElementCounts counts;
  if (!read_elements(counts)) {
    return true;  // interrupted (Magic's `goto done`)
  }
  if (options_.settings.readonly && !magnified) {
    // Magic's FTELL with the ENDSTR header read ahead: the end of that header.
    const std::optional<Record> ahead = peek();
    annotate_vendor(def, start, ahead ? ahead->offset + 4 : records_.position());
  }
  const bool flatten = flatten_decision(name, was_called, counts);
  if (!skip_exact(RecordType::endstr)) {
    malformed_structure();
    return skip_to(RecordType::endstr);
  }
  if (flatten) {
    save_flatten_planes(def);
  } else {
    paint_structure(def);
  }
  end_structure(def, polygons_before);
  return true;
}

void Reader::malformed_structure() {
  if (compat_.magic_compat) {
    return;  // Q-MASK-110 ON: the planes leak into the next structure, as in Magic
  }
  bool discarded = false;
  for (mask::MaskPlane* plane : planes()) {
    if (plane != nullptr && !plane->empty()) {
      discarded = true;
      plane->clear();
    }
  }
  if (discarded) {
    counted(diag::make_native(kCompatNote, "the geometry of the malformed structure is discarded (Q-MASK-110)"));
  }
}

void Reader::save_flatten_planes(db::CellDef& def) {
  info("Saving contents of cell " + def.name());
  flat_planes_[&def] = save_planes(true);
  state(def).flattened = false;
  if (!def.labels().empty()) {
    def.labels().clear();  // DBEraseLabel over the whole plane and every type
    db::mark_modified(def);
  }
}

void Reader::paint_structure(db::CellDef& def) {
  std::optional<PlaneSet> hint_inputs;
  if (!options_.settings.maskhints.empty()) {
    hint_inputs = save_planes(false);
  }
  engine_->paint_current(planes(), def);
  if (hint_inputs) {
    mask_hints(def, *hint_inputs);
  }
}

void Reader::end_structure(db::CellDef& def, std::int64_t polygons_before) {
  std::erase_if(def.labels(), [](const db::Label& l) { return l.text.empty(); });
  if (options_.settings.polygon_subcells == PolygonSubcells::temporary && polygons_before < polygon_count_) {
    flatten_polygon_cells(def);
  }
  adjust_labels(def);
  db::recompute_bbox(def, compat_.magic_compat);
  if (!options_.settings.readonly && options_.settings.drccheck) {
    drc::check_this(editor_, def, def.bbox());
  }
  db::mark_modified(def);
  generate_ids(def);
  state(def).processed = true;
}

void Reader::generate_ids(db::CellDef& def) {
  (void)db::make_use_ids_unique(def, compat_.magic_compat);
}

void Reader::flatten_polygon_cells(db::CellDef& def) {
  // calmaFlattenPolygonFunc: every use of a "polygon*" cell is copied into
  // `def` and the cell deleted.
  std::vector<db::CellUse*> doomed;
  for (const auto& use : def.uses()) {
    if (use->def != nullptr && use->def->name().starts_with("polygon")) {
      doomed.push_back(use.get());
    }
  }
  const tech::Technology& technology = *editor_.technology;
  for (db::CellUse* use : doomed) {
    db::CellDef* child = use->def;
    copy_paint(*child, def, technology);
    (void)db::remove_use(*use);
    forget_cell(*child);
    (void)editor_.drc.pending.remove(*child);  // DRCRemovePending before the definition goes
    (void)editor_.cells.remove(*child);
  }
}

bool Reader::parse_element(ElementCounts& counts) {
  std::optional<Record> record = next();
  if (!record) {
    error("Unexpected EOF.");
    return false;
  }
  switch (static_cast<RecordType>(record->type)) {
    case RecordType::aref:
    case RecordType::sref: {
      const std::int64_t made = element_sref();
      if (made >= 0) {
        counts.references += made;
      }
      break;
    }
    case RecordType::boundary:
      element_boundary();
      ++counts.shapes;
      break;
    case RecordType::box:
      element_box();
      ++counts.shapes;
      break;
    case RecordType::path:
      element_path();
      ++counts.shapes;
      break;
    case RecordType::text:
      element_text();
      break;
    case RecordType::node:
      error("NODE elements not supported: skipping.");
      skip_set({RecordType::elflags, RecordType::plex, RecordType::layer, RecordType::nodetype, RecordType::xy});
      break;
    default:
      unread(std::move(*record));
      return false;
  }
  return skip_to(RecordType::endel);
}

// --- mask hints on input ------------------------------------------------------------

void Reader::mask_hints(db::CellDef& def, const PlaneSet& inputs) {
  const mask::OutputStyle* out = editor_.mask.output_styles.current_style();
  if (out == nullptr) {
    return;
  }
  const mask::InputStyle& in = engine_->style();
  std::vector<std::pair<std::size_t, std::size_t>> pairs;  // input layer -> output layer
  for (std::size_t i = 0; i < in.cif_layers.size(); ++i) {
    if (!hint_requested(in.cif_layers[i])) {
      continue;
    }
    if (const auto j = hint_output_layer(*out, in.cif_layers[i])) {
      pairs.emplace_back(i, *j);
    }
  }
  if (pairs.empty()) {
    return;
  }
  mask::GenerationContext ctx;
  const mask::LayerPlanes generated = mask::generate_flat(editor_, def, db::whole_plane(), *out, ctx);
  const std::int64_t s_out = std::max<std::int32_t>(out->scale.scale_factor, 1);
  const std::int64_t s_in = scale_factor();
  for (const auto& [i, j] : pairs) {
    hints::HintRegion input;
    if (i < inputs.size() && inputs[i]) {
      for (const db::TileView& tile : inputs[i]->solid_tiles()) {
        input.paint(scaled_rect(tile.area, s_out, s_in));
      }
    }
    hints::HintRegion output;
    if (j < generated.planes.size() && generated.planes[j] != nullptr) {
      output = hints::HintRegion{std::span<const db::Rect>{generated.planes[j]->solid_rects()}};
    }
    hints::add_input_region(def, in.cif_layers[i],
                            hints::gds_maskhints_region(input, output, static_cast<std::int32_t>(s_out)));
  }
}

bool Reader::hint_requested(std::string_view layer) const {
  const std::string& list = options_.settings.maskhints;
  if (list == "yes" || list == "all" || list == "*") {
    return true;
  }
  std::size_t from = 0;
  while (from <= list.size()) {
    const std::size_t comma = list.find(',', from);
    const std::string_view item = std::string_view{list}.substr(from, comma == std::string::npos ? std::string::npos : comma - from);
    if (item == layer) {
      return true;
    }
    if (comma == std::string::npos) {
      break;
    }
    from = comma + 1;
  }
  return false;
}

}  // namespace hefesto::gds::read_detail
