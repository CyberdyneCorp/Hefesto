// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The library writer (gds-write "Library header and trailer", "Structure
// ordering", "Structure body order", "Mask geometry for each structure").
// Ports Magic 8.3.694 (fc5daa9e) calma/CalmaWrite.c CalmaWrite (after the
// prepare phase), calmaOutHeader, calmaProcessDef and calmaOutFunc,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Differences from Magic, none of them visible in the bytes of a write that
// succeeds:
// - Nothing in the layout changes (Q-MASK-208): Magic marks copied cells
//   CDVENDORGDS here; Hefesto's read-only cells already carry the vendor
//   flag (GDS_FILE), and an unavailable cell is not read again (the prepare
//   phase read every cell it could).
// - The traversal is recursive, as Magic's, with the depth limit of
//   kMaxSubtreeDepth (GDS-WRITE-014).
// - Read-only cells with content are copied by vendor.cpp (verbatim copy)
//   and dump.cpp (full dump); the forward references no dump defined are
//   appended at the end of the write in the order they were recorded
//   (Magic: calmaUndefHash order), with Q-MASK-214's naming.
// Owner: GDSW.

#include <chrono>
#include <cstdio>
#include <string>

#include "codes.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/write/labels.hpp"
#include "hefesto/gds/write/polygons.hpp"
#include "vendor.hpp"
#include "writer.hpp"

namespace hefesto::gds::detail {

namespace {

constexpr std::string_view kUnnamed = "(UNNAMED)";

// "polygon<n>" cells made by `gds polygon subcells` inside a vendor cell
// exist only in the layout and are not written.
bool vendor_polygon_cell(const db::CellDef& def) {
  const std::string& name = def.name();
  if (name.rfind("polygon", 0) != 0) {
    return false;
  }
  int index = 0;
  if (std::sscanf(name.c_str() + 7, "%d", &index) != 1) {  // NOLINT(cert-err34-c): Magic's sscanf test
    return false;
  }
  const std::vector<db::CellUse*>& parents = def.instances();
  if (parents.empty() || parents.front()->parent == nullptr) {
    return false;
  }
  return parents.front()->parent->properties().find("GDS_FILE") != nullptr;
}

}  // namespace

LibraryWriter::LibraryWriter(const PreparedWrite& prepared, RecordWriter& records, ByteSink& raw,
                             diag::DiagnosticSink& sink, mask::GenerationContext& ctx)
    : prepared_(prepared),
      records_(records),
      sink_(sink),
      ctx_(ctx),
      vendor_(std::make_unique<VendorIo>(VendorIo{prepared, records, raw, sink, names_,
                                                  [this](const db::CellDef& def) { return id_of(def); }})),
      dump_(std::make_unique<FullDump>()) {}

LibraryWriter::~LibraryWriter() = default;

std::int64_t LibraryWriter::now() const {
  if (prepared_.options.now) {
    return *prepared_.options.now;
  }
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::int64_t LibraryWriter::id_of(const db::CellDef& def) const {
  const auto it = ids_.find(&def);
  return it == ids_.end() ? 0 : it->second;
}

void LibraryWriter::print(std::string text) {
  sink_.emit(diag::make(codes::kProgress, std::move(text)));
}

StructureContext LibraryWriter::context_for(const db::CellDef& def, const OutputScale& scale) const {
  const tech::Technology* technology = prepared_.editor != nullptr ? prepared_.editor->technology.get() : nullptr;
  return StructureContext{&def, prepared_.style.get(), &prepared_.options.settings, prepared_.compat, scale,
                          technology};
}

// calmaOutHeader: HEADER, BGNLIB, LIBNAME, UNITS.
diag::Status LibraryWriter::header() {
  const std::array<std::int16_t, 1> version{3};
  if (diag::Status s = records_.write_int2(RecordType::header, version); !s.ok()) {
    return s;
  }
  const GdsSettings& settings = prepared_.options.settings;
  const db::CellDef& root = *prepared_.root;
  const std::array<std::int16_t, 12> fields = structure_dates(
      settings.datestamp.value_or(root.timestamp()), now(), prepared_.compat, prepared_.options.time_zone);
  if (diag::Status s = records_.write_int2(RecordType::bgnlib, fields); !s.ok()) {
    return s;
  }
  ElementWriter out(records_, context_for(root, {}), &sink_);
  ids_[&root] = -1;
  if (diag::Status s = names_.write(out, RecordType::libname, root, 1); !s.ok()) {
    return s;
  }
  // Q-MASK-201: from the current style on every write, in both modes.
  const bool angstroms = prepared_.style->has_option(mask::style_option::angstroms);
  const std::array<double, 2> units{angstroms ? 0.0001 : 0.001, angstroms ? 1e-10 : 1e-9};
  return records_.write_real8(RecordType::units, units);
}

LibraryWriter::Result LibraryWriter::write() {
  Result result;
  if (diag::Status s = header(); !s.ok()) {
    result.status = s;
    return result;
  }
  next_id_ = -2;
  const Visit visit = process(*prepared_.root, prepared_.options.settings.library, prepared_.hierarchy, 0);
  result.good = visit == Visit::done;
  if (error_.ok() && !append_undefined()) {
    result.good = false;
  }
  if (!error_.ok()) {
    result.good = false;
    result.status = error_;
    if (error_.error().code == codes::kCancelled) {
      return result;
    }
  }
  if (diag::Status s = records_.write_empty(RecordType::endlib); !s.ok() && result.status.ok()) {
    result.status = s;
  }
  return result;
}

// The abstract-view rule of calmaProcessDef (Q-MASK-226 ON: the write
// fails here; OFF the prepare phase has refused the write already).
LibraryWriter::Visit LibraryWriter::check_kind(const db::CellDef& def, const CellKind& kind) {
  if (kind.abstract && !kind.read_only) {
    if (!prepared_.options.settings.allow_abstract) {
      sink_.emit(diag::make(codes::kAbstract,
                            "Error:  Cell \"" + def.name() + "\" is an abstract view;  cannot write GDS."));
      return Visit::failed;
    }
    sink_.emit(diag::make(codes::kAbstractAllowed, "Warning:  Writing abstract view of \"" + def.name() + "\" to GDS."));
  }
  return Visit::done;
}

// calmaProcessVendorUse: a magnified copy in a vendor cell is written as
// the structure it magnifies.
db::CellDef& LibraryWriter::magnified_source(db::CellDef& def) {
  const db::PropertyValue* value = def.properties().find("GDS_MAG_SOURCE");
  const auto* name = value != nullptr ? std::get_if<std::string>(value) : nullptr;
  if (name == nullptr) {
    return def;
  }
  if (db::CellDef* source = prepared_.editor->cells.find(*name)) {
    return *source;
  }
  sink_.emit(diag::make(codes::kVendorMissing, "Calma output error:  Cell \"" + def.name() +
                                                   "\" is a magnified copy of \"" + *name +
                                                   "\", which is not in memory."));
  return def;
}

LibraryWriter::Visit LibraryWriter::process_children(const db::CellDef& def, const mask::HierarchyOptions& options,
                                                     std::size_t depth, bool vendor) {
  // DBCellEnum: the whole-plane enumeration, which never rebuilds.
  for (db::CellUse* use : db::uses_in_search_order(def, prepared_.compat)) {
    if (use->def == nullptr) {
      continue;
    }
    db::CellDef& child = vendor ? magnified_source(*use->def) : *use->def;
    if (process(child, false, options, depth + 1) == Visit::failed) {
      return Visit::failed;
    }
  }
  return Visit::done;
}

// The full dump of `file` (once per write; Q-MASK-212: a refused library
// follows the `undefined` rule).
LibraryWriter::VendorVisit LibraryWriter::full_dump(const db::CellDef& def, ByteSource& source,
                                                    const std::string& file) {
  const bool allow = prepared_.options.settings.allow_undefined;
  if (dump_->refused(file)) {
    return allow ? VendorVisit::done : VendorVisit::failed;
  }
  if (dump_->seen(file)) {
    return VendorVisit::done;
  }
  vendor_->now = now();
  const bool dumped = dump_->dump(*vendor_, def, source, file);
  if (!vendor_->error.ok()) {
    error_ = vendor_->error;
    return VendorVisit::failed;
  }
  return dumped || allow ? VendorVisit::done : VendorVisit::failed;
}

// The copy branch of calmaProcessDef.
LibraryWriter::VendorVisit LibraryWriter::vendor_structure(const db::CellDef& def, const CellKind& kind) {
  const auto* file = std::get_if<std::string>(def.properties().find("GDS_FILE"));
  sink_.emit(diag::make(codes::kVendorCopy, "   Copying output for cell " + def.name() + " from " + *file));
  std::optional<VendorFile> opened = open_vendor_gds(*vendor_, def, *file);
  if (!opened) {
    const std::vector<db::CellUse*>& parents = def.instances();
    const db::CellDef* parent =
        !parents.empty() && parents.front()->parent != nullptr ? parents.front()->parent : &def;
    if (parent->properties().find("GDS_FILE") != nullptr) {
      return VendorVisit::done;  // inside another vendor cell: not written
    }
    sink_.emit(diag::make(codes::kVendorMissing, "Calma output error:  Can't find GDS file \"" + *file +
                                                     "\" for vendor cell \"" + def.name() +
                                                     "\".  It will not be output."));
    return prepared_.options.settings.allow_undefined ? VendorVisit::done : VendorVisit::failed;
  }
  if (kind.abstract || !kind.has_end) {
    return full_dump(def, *opened->source, opened->path.generic_string());
  }
  vendor_->now = now();
  const CopyResult copied = copy_vendor_structure(*vendor_, def, *opened->source);
  if (!vendor_->error.ok()) {
    error_ = vendor_->error;
  }
  switch (copied) {
    case CopyResult::copied:
      return VendorVisit::done;
    case CopyResult::fallback:
      return VendorVisit::fallback;
    default:
      return VendorVisit::failed;
  }
}

// The structures of the forward references no dump defined (CalmaWrite's
// calmaUndefHash pass, Q-MASK-214).
bool LibraryWriter::append_undefined() {
  bool good = true;
  for (const FullDump::Undefined& u : dump_->undefined()) {
    db::CellDef* def = prepared_.editor->cells.find(u.name);
    if (def == nullptr) {
      sink_.emit(diag::make(codes::kDumpError, "Error:  Cell " + u.recorded + " is not defined in the output file!"));
      continue;
    }
    if (!prepared_.compat.magic_compat) {
      ElementWriter out(records_, context_for(*def, {}), &sink_);
      names_.assign(out, *def, u.recorded);
    } else if (u.recorded != def->name()) {
      sink_.emit(diag::make_native(codes::kDumpDangling, "Warning: cell " + def->name() +
                                                             " is written under its own name, so references to \"" +
                                                             u.recorded + "\" in the copied vendor structures dangle."));
    }
    if (process(*def, false, prepared_.hierarchy, 0) == Visit::failed) {
      good = false;
    }
  }
  return good;
}

// calmaProcessDef.
LibraryWriter::Visit LibraryWriter::process(db::CellDef& def, bool library, const mask::HierarchyOptions& inherited,
                                            std::size_t depth) {
  std::int64_t& id = ids_[&def];
  if (id > 0) {
    return Visit::done;
  }
  if (id == 0) {
    id = next_id_--;
  }
  id = -id;
  if (!error_.ok()) {
    return Visit::failed;
  }
  if (depth > kMaxSubtreeDepth) {
    error_ = diag::make(codes::kDepth, "Calma output error:  the hierarchy under cell " + prepared_.root->name() +
                                           " is deeper than " + std::to_string(kMaxSubtreeDepth) + " levels.");
    return Visit::failed;
  }
  if (!def.flags().available) {
    return Visit::done;  // DBCellRead failed: not written, references stay
  }
  if (def.name() == kUnnamed) {
    sink_.emit(diag::make(codes::kUnnamed, "Error:  Cell has the default name \"(UNNAMED)\"!"));
  }
  const CellKind kind = cell_kind(def);
  if (kind.copied() && prepared_.options.settings.addendum) {
    return Visit::done;
  }
  if (check_kind(def, kind) == Visit::failed) {
    return Visit::failed;
  }
  const mask::HierarchyOptions options = mask::hierarchy_options(def, inherited, true, prepared_.compat);
  if (kind.traverses_children() && process_children(def, options, depth, kind.copied()) == Visit::failed) {
    return Visit::failed;
  }
  return emit(def, kind, options, library);
}

// The rest of calmaProcessDef: the vendor copy or calmaOutFunc.
LibraryWriter::Visit LibraryWriter::emit(const db::CellDef& def, const CellKind& kind,
                                         const mask::HierarchyOptions& options, bool library) {
  bool read_only = kind.read_only;
  if (kind.copied()) {
    const VendorVisit visit = vendor_structure(def, kind);
    if (visit != VendorVisit::fallback) {
      return visit == VendorVisit::done ? Visit::done : Visit::failed;
    }
    read_only = false;  // a failed sanity check: generated from the layout
  } else {
    print("   Generating output for cell " + def.name());
  }
  if (read_only || vendor_polygon_cell(def) || library) {
    return Visit::done;
  }
  if (diag::Status s = structure(def, options); !s.ok()) {
    error_ = s;
    return Visit::failed;
  }
  return Visit::done;
}

// calmaOutFunc.
diag::Status LibraryWriter::structure(const db::CellDef& def, const mask::HierarchyOptions& options) {
  const GdsSettings& settings = prepared_.options.settings;
  const std::array<std::int16_t, 12> fields =
      structure_dates(settings.datestamp.value_or(def.timestamp()), now(), prepared_.compat, prepared_.options.time_zone);
  if (diag::Status s = records_.write_int2(RecordType::bgnstr, fields); !s.ok()) {
    return s;
  }
  ElementWriter named(records_, context_for(def, {}), &sink_);
  if (diag::Status s = names_.write(named, RecordType::strname, def, id_of(def)); !s.ok()) {
    return s;
  }
  const ScaleResult scale = output_scale(*prepared_.style, prepared_.compat);
  for (const diag::Diagnostic& e : scale.errors) {
    sink_.emit(e);
  }
  ElementWriter out(records_, context_for(def, scale.scale), &sink_);
  if (diag::Status s = structure_body(def, out, options); !s.ok()) {
    return s;
  }
  return records_.write_empty(RecordType::endstr);
}

diag::Status LibraryWriter::structure_body(const db::CellDef& def, ElementWriter& out,
                                           const mask::HierarchyOptions& options) {
  // The SREF order is sampled before the cell's CIFGenSubcells.
  for (const db::CellUse* use : db::uses_in_search_order(def, prepared_.compat)) {
    if (use->def == nullptr) {
      continue;
    }
    if (diag::Status s = write_reference(out, names_, *use, id_of(*use->def), ctx_); !s.ok()) {
      return s;
    }
  }
  if (diag::Status s = mask_layers(def, out, options); !s.ok()) {
    return s;
  }
  if (!prepared_.options.settings.labels) {
    return diag::Done{};
  }
  return emit_labels(def, out, port_warnings_);
}

diag::Status LibraryWriter::mask_layers(const db::CellDef& def, ElementWriter& out,
                                        const mask::HierarchyOptions& options) {
  const mask::OutputStyle& style = *prepared_.style;
  const mask::LayerPlanes planes =
      mask::generate_hierarchical(mask::generation_source(*prepared_.editor), def, style, ctx_, options);
  if (ctx_.cancelled()) {
    return diag::make(codes::kCancelled, "GDS write cancelled.");
  }
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    const mask::OutputLayer& layer = style.layers[i];
    const mask::MaskPlane* plane = planes[i];
    if (!layer.gds_written() || plane == nullptr) {
      continue;
    }
    diag::Status s = diag::Done{};
    if (layer.label_layer) {
      s = emit_label_layer(*plane, i, out);
    } else if (prepared_.options.settings.merge) {
      s = emit_merged_polygons(*plane, *layer.gds, out);
    } else {
      s = emit_tile_boundaries(*plane, *layer.gds, out);
    }
    if (!s.ok()) {
      return s;
    }
  }
  return diag::Done{};
}

}  // namespace hefesto::gds::detail
