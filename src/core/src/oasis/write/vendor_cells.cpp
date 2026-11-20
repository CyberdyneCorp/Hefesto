// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only OASIS cells in OASIS output (see
// hefesto/oasis/write/vendor_cells.hpp; oasis-write "Read-only OASIS
// vendor cells", Q-OASIS-40): every element is re-encoded with its record
// type, resolved values and repetition, names and strings go to the output
// tables, every PROPERTY is kept, XNAME records are not copied, and
// XELEMENT/XGEOMETRY payloads are copied unchanged (with a warning when the
// source file has XNAME records).
// Owner: OASW.

#include "hefesto/oasis/write/vendor_cells.hpp"

#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/values.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

namespace {

namespace codes = detail::codes;

thread_local VendorCells* g_active = nullptr;

const std::string* string_property(const db::CellDef& def, std::string_view name) {
  const db::PropertyValue* value = def.properties().find(std::string{name});
  return value != nullptr ? std::get_if<std::string>(value) : nullptr;
}

bool ends_cell(RecordId id) {
  switch (id) {
    case RecordId::end:
    case RecordId::cellname_implicit:
    case RecordId::cellname:
    case RecordId::textstring_implicit:
    case RecordId::textstring:
    case RecordId::propname_implicit:
    case RecordId::propname:
    case RecordId::propstring_implicit:
    case RecordId::propstring:
    case RecordId::layername_geometry:
    case RecordId::layername_text:
    case RecordId::xname_implicit:
    case RecordId::xname:
      return true;
    default:
      return false;
  }
}

bool cell_record(RecordId id) {
  return id == RecordId::cell_by_reference || id == RecordId::cell_by_name;
}

/// Whether `r` belongs to the body of the cell before it.
bool body_record(const Record& r) {
  if (r.element) {
    return true;
  }
  return r.property && (r.target == PropertyTarget::cell || r.target == PropertyTarget::element);
}

// Pass 1: START, the name tables and whether XNAMEs exist.
diag::Status read_names(gds::ByteSource& source, VendorLibrary& library) {
  Decoder decoder(source);
  for (;;) {
    diag::Outcome<std::optional<Record>> next = decoder.next();
    if (!next.ok()) {
      return next.error();
    }
    if (!next.value()) {
      break;
    }
    if (next.value()->start) {
      library.unit = next.value()->start->unit;
    }
  }
  library.names = decoder.names();
  library.has_xnames = !library.names.xnames.empty();
  return diag::Done{};
}

std::string cell_name(const Record& r, const NameTables& names) {
  if (!r.reference) {
    return r.name;
  }
  const auto it = names.cellnames.find(*r.reference);
  return it != names.cellnames.end() ? it->second : "#" + std::to_string(*r.reference);
}

// Pass 2: the records of the wanted cells.
diag::Status read_cells(gds::ByteSource& source, const std::set<std::string, std::less<>>& wanted,
                        VendorLibrary& library) {
  Decoder decoder(source);
  std::vector<Record>* current = nullptr;
  for (;;) {
    diag::Outcome<std::optional<Record>> next = decoder.next();
    if (!next.ok()) {
      return next.error();
    }
    if (!next.value()) {
      return diag::Done{};
    }
    Record& r = *next.value();
    if (cell_record(r.id)) {
      const std::string name = cell_name(r, library.names);
      current = wanted.contains(name) ? &library.cells[name] : nullptr;
    } else if (ends_cell(r.id)) {
      current = nullptr;
    } else if (current != nullptr && body_record(r)) {
      current->push_back(std::move(r));
    }
  }
}

PropertyValue resolved_value(const PropertyValue& v, const NameTables& names) {
  using Kind = PropertyValue::Kind;
  const auto kind_of = [](Kind k) {
    return k == Kind::a_reference ? Kind::a_string : (k == Kind::b_reference ? Kind::b_string : Kind::n_string);
  };
  if (v.kind != Kind::a_reference && v.kind != Kind::b_reference && v.kind != Kind::n_reference) {
    return v;
  }
  const auto it = names.propstrings.find(v.reference);
  return PropertyValue::of_string(kind_of(v.kind),
                                  it != names.propstrings.end() ? it->second : "#" + std::to_string(v.reference));
}

std::string key_of(const std::vector<std::byte>& payload) {
  std::string text = detail::payload_text(payload);
  return text.substr(0, text.find('\0'));
}

}  // namespace

bool is_oasis_vendor_cell(const db::CellDef& def) {
  return string_property(def, "OASIS_FILE") != nullptr && string_property(def, "OASIS_CELL") != nullptr;
}

diag::Outcome<VendorLibrary> decode_vendor_library(gds::ByteSource& source,
                                                   const std::set<std::string, std::less<>>& wanted) {
  const std::uint64_t start = source.position();
  VendorLibrary library;
  if (diag::Status s = read_names(source, library); !s.ok()) {
    return s.error();
  }
  if (diag::Status s = source.seek(start); !s.ok()) {
    return s.error();
  }
  if (diag::Status s = read_cells(source, wanted, library); !s.ok()) {
    return s.error();
  }
  return library;
}

std::string resolved_name(const std::map<std::uint64_t, std::string>& table, const Element& e) {
  if (!e.reference) {
    return e.name;
  }
  const auto it = table.find(*e.reference);
  return it != table.end() ? it->second : "#" + std::to_string(*e.reference);
}

Property resolved_property(const Property& property, const NameTables& names) {
  Property p = property;
  if (p.name_reference) {
    const auto it = names.propnames.find(*p.name_reference);
    p.name = it != names.propnames.end() ? it->second : "#" + std::to_string(*p.name_reference);
    p.name_reference.reset();
  }
  for (PropertyValue& v : p.values) {
    v = resolved_value(v, names);
  }
  return p;
}

// --- VendorCells ---------------------------------------------------------------

struct VendorCells::Impl {
  Impl(const gds::PreparedWrite& write, diag::DiagnosticSink& diagnostics)
      : prepared(write),
        sink(diagnostics),
        table(gds::char_table(*write.style, write.compat)),
        strings(gds::string_options(*write.style, write.options.settings)),
        output_unit(write.style->has_option(mask::style_option::angstroms) ? 10000.0 : 1000.0) {
    if (prepared.editor == nullptr) {
      return;
    }
    prepared.editor->cells.for_each([this](db::CellDef& def) {
      if (is_oasis_vendor_cell(def)) {
        by_payload.emplace(payload_key(def), &def);
        by_file[*string_property(def, "OASIS_FILE")][*string_property(def, "OASIS_CELL")] = &def;
      }
    });
  }

  struct Loaded {
    std::set<std::string, std::less<>> wanted;
    std::optional<VendorLibrary> library;  // nullopt: the file cannot be decoded
  };
  struct Ready {
    const VendorLibrary* library = nullptr;
    std::string cell;
    const std::string* file = nullptr;
  };

  [[nodiscard]] std::string payload_key(const db::CellDef& def) const {
    return key_of(gds::encode_string(def.name(), table, strings, prepared.compat).data);
  }

  void error(std::string text) const { sink.emit(diag::make(codes::kVendorOasis, std::move(text))); }

  Loaded& load(const std::string& path, const std::string& file, const std::string& cell, gds::ByteSource& source) {
    Loaded& loaded = libraries[path];
    if (loaded.wanted.contains(cell)) {
      return loaded;
    }
    for (const auto& [name, def] : by_file[file]) {
      loaded.wanted.insert(name);
    }
    loaded.wanted.insert(cell);
    (void)source.seek(0);
    diag::Outcome<VendorLibrary> decoded = decode_vendor_library(source, loaded.wanted);
    if (decoded.ok()) {
      loaded.library = std::move(decoded.value());
    } else {
      loaded.library.reset();
      error("Error: the OASIS file " + path + " cannot be decoded: " + decoded.error().text);
    }
    return loaded;
  }

  [[nodiscard]] std::string child_payload(const Ready& cell, const std::string& name) const {
    const auto children = by_file.find(*cell.file);
    if (children != by_file.end()) {
      if (const auto child = children->second.find(name); child != children->second.end()) {
        return payload_key(*child->second);
      }
    }
    return name;
  }

  const gds::PreparedWrite& prepared;
  diag::DiagnosticSink& sink;
  gds::CharTable table;
  gds::StringOptions strings;
  double output_unit;
  std::unordered_map<std::string, const db::CellDef*> by_payload;
  std::map<std::string, std::map<std::string, const db::CellDef*>, std::less<>> by_file;
  std::map<std::string, Loaded, std::less<>> libraries;
  std::unordered_map<const db::CellDef*, Ready> prepared_cells;
};

VendorCells::VendorCells(const gds::PreparedWrite& prepared, const OasisSettings& /*settings*/,
                         diag::DiagnosticSink& sink)
    : impl_(std::make_unique<Impl>(prepared, sink)) {}

VendorCells::~VendorCells() = default;

VendorCells::Scope::Scope(VendorCells& cells) : previous_(g_active) {
  g_active = &cells;
}

VendorCells::Scope::~Scope() {
  g_active = previous_;
}

VendorCells* VendorCells::active() noexcept {
  return g_active;
}

VendorCells::Check VendorCells::prepare(const db::CellDef& def, gds::ByteSource& source, const std::string& path) {
  const std::string* file = string_property(def, "OASIS_FILE");
  const std::string* cell = string_property(def, "OASIS_CELL");
  if (file == nullptr || cell == nullptr) {
    return Check::fallback;
  }
  Impl::Loaded& loaded = impl_->load(path, *file, *cell, source);
  if (!loaded.library) {
    return Check::fallback;
  }
  if (loaded.library->unit != impl_->output_unit) {
    impl_->error("Error: the OASIS file " + path + " of read-only cell " + def.name() + " has unit " +
                 std::to_string(loaded.library->unit) + ", not the output unit " +
                 std::to_string(impl_->output_unit) + "; the cell is generated from its layout.");
    return Check::fallback;
  }
  if (!loaded.library->cells.contains(*cell)) {
    return Check::missing;
  }
  impl_->prepared_cells[&def] = Impl::Ready{&*loaded.library, *cell, file};
  return Check::ready;
}

std::optional<diag::Status> VendorCells::reencode(std::string_view payload, const std::string& output, Encoder& out,
                                                  const std::function<std::string(std::string_view)>& name_of) {
  const auto def = impl_->by_payload.find(std::string{payload.substr(0, payload.find('\0'))});
  if (def == impl_->by_payload.end()) {
    return std::nullopt;
  }
  const auto found = impl_->prepared_cells.find(def->second);
  if (found == impl_->prepared_cells.end()) {
    return std::nullopt;
  }
  const Impl::Ready& ready = found->second;
  const NameTables& names = ready.library->names;
  diag::Status status = diag::Done{};
  bool extensions = false;
  for (const Record& r : ready.library->cells.at(ready.cell)) {
    diag::Status s = diag::Done{};
    if (r.element) {
      Element e = *r.element;
      if (e.kind == ElementKind::placement) {
        e.name = name_of(impl_->child_payload(ready, resolved_name(names.cellnames, e)));
      } else if (e.kind == ElementKind::text) {
        e.name = resolved_name(names.textstrings, e);
      }
      e.reference.reset();
      extensions = extensions || e.kind == ElementKind::xelement || e.kind == ElementKind::xgeometry;
      s = out.element(e);
    } else {
      s = out.property(resolved_property(*r.property, names));
    }
    if (!s.ok()) {
      impl_->error("Error: an element of read-only cell " + output + " cannot be written to OASIS: " +
                   s.error().text);
      status = s;
    }
  }
  if (extensions && ready.library->has_xnames) {
    impl_->sink.emit(diag::make_native(codes::kExtensions,
                                       "Warning: read-only cell " + output +
                                           " carries XELEMENT or XGEOMETRY records whose payloads may refer to "
                                           "XNAME records of its source file, which are not written."));
  }
  return status;
}

}  // namespace hefesto::oasis
