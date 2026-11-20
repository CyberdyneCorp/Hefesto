// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The single-cell transcoders (see hefesto/oasis/write/transcode.hpp).
// Owner: OASW.

#include "hefesto/oasis/write/transcode.hpp"

#include "hefesto/oasis/write/vendor_cells.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

namespace {

diag::Diagnostic element_failure(std::string_view name) {
  return diag::make(detail::codes::kEncoding, "Error: cell " + std::string{name} + " cannot be written to OASIS.");
}

}  // namespace

diag::Status transcode_gds_cell(gds::ByteSource& gds_stream, std::uint64_t offset, std::string_view name,
                                Encoder& out, diag::DiagnosticSink* sink) {
  if (diag::Status s = gds_stream.seek(offset); !s.ok()) {
    return s;
  }
  detail::GdsItems items(gds_stream);
  diag::Outcome<std::optional<detail::GdsItem>> first = items.next();
  if (!first.ok()) {
    return first.error();
  }
  if (!first.value() || first.value()->kind != detail::GdsItem::Kind::structure) {
    return diag::make(detail::codes::kGdsStream, "OASIS write: no GDS structure at byte " + std::to_string(offset) + ".");
  }
  if (diag::Status s = out.begin_cell(name); !s.ok()) {
    return s;
  }
  const compat::CompatSettings compat;
  const gds::CharTable table(gds::CharTable::Kind::strict, compat);
  detail::OutputNames names(sink);
  detail::TranslateContext context;
  context.names = &names;
  context.sink = sink;
  context.cell = std::string{name};
  context.vendor = true;
  context.table = &table;
  context.compat = compat;
  bool failed = false;
  for (;;) {
    diag::Outcome<std::optional<detail::GdsItem>> item = items.next();
    if (!item.ok()) {
      return item.error();
    }
    if (!item.value() || item.value()->kind != detail::GdsItem::Kind::element) {
      break;
    }
    failed = !detail::write_items(detail::translate(item.value()->element, context), out).ok() || failed;
  }
  if (failed || context.failed) {
    return element_failure(name);
  }
  return diag::Done{};
}

diag::Status reencode_oasis_cell(gds::ByteSource& oasis_stream, std::string_view cell, std::string_view name,
                                 Encoder& out, diag::DiagnosticSink* sink) {
  diag::Outcome<VendorLibrary> library = decode_vendor_library(oasis_stream, {std::string{cell}});
  if (!library.ok()) {
    return library.error();
  }
  const auto found = library.value().cells.find(cell);
  if (found == library.value().cells.end()) {
    return diag::make(detail::codes::kVendorOasis, "Error: the OASIS file does not define cell " + std::string{cell} + ".");
  }
  if (diag::Status s = out.begin_cell(name); !s.ok()) {
    return s;
  }
  const NameTables& names = library.value().names;
  bool failed = false;
  for (const Record& r : found->second) {
    diag::Status s = diag::Done{};
    if (r.element) {
      Element e = *r.element;
      if (e.kind == ElementKind::placement || e.kind == ElementKind::text) {
        e.name = resolved_name(e.kind == ElementKind::placement ? names.cellnames : names.textstrings, e);
      }
      e.reference.reset();
      s = out.element(e);
    } else {
      s = out.property(resolved_property(*r.property, names));
    }
    if (!s.ok() && sink != nullptr) {
      sink->emit(s.error());
    }
    failed = failed || !s.ok();
  }
  return failed ? diag::Status{element_failure(name)} : diag::Status{diag::Done{}};
}

}  // namespace hefesto::oasis
