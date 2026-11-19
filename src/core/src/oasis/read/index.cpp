// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pass 1 of the OASIS reader (capability oasis-read "Two-pass decoding and
// name resolution", "Top cell selection"): the whole file is decoded once,
// without interpreting geometry, so that every name and reference number
// is known before any cell is read. A cell is complete once a record that
// is not cell content (CELL, END or a name record) follows it; a fatal
// decoding error leaves the cell in progress, and every later one, out of
// the read (rule 2).
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <algorithm>
#include <unordered_set>

#include "hefesto/gds/stream/io.hpp"
#include "internal.hpp"

namespace hefesto::oasis::read_detail {

namespace {

/// Records that end the content of a cell.
bool ends_cell(RecordId id) {
  switch (id) {
    case RecordId::start:
    case RecordId::end:
    case RecordId::cell_by_reference:
    case RecordId::cell_by_name:
    case RecordId::xname_implicit:
    case RecordId::xname:
      return true;
    default:
      return id >= RecordId::cellname_implicit && id <= RecordId::layername_text;
  }
}

bool is_cell(RecordId id) {
  return id == RecordId::cell_by_reference || id == RecordId::cell_by_name;
}

struct PendingCell {
  NameRef ref;
  StreamPosition position;
  std::vector<NameRef> children;
};

/// The records of pass 1 before name resolution.
class Indexer {
 public:
  void take(const Record& r) {
    if (r.id == RecordId::start && r.start) {
      index_.started = true;
      index_.unit = r.start->unit;
    }
    if (ends_cell(r.id)) {
      complete_ += open_ ? 1 : 0;
      open_ = false;
    }
    if (is_cell(r.id)) {
      cells_.push_back(PendingCell{NameRef{r.name, r.reference}, StreamPosition::of(r), {}});
      open_ = true;
      return;
    }
    if (open_ && r.element && r.element->kind == ElementKind::placement) {
      cells_.back().children.push_back(NameRef{r.element->name, r.element->reference});
    }
    if (r.property && r.target == PropertyTarget::file) {
      file_properties_.push_back(*r.property);
    }
  }

  void stop(diag::Diagnostic error, std::uint64_t position) {
    stopped_ = std::move(error);
    stopped_at_ = position;
  }

  FileIndex finish(const NameTables& names, std::vector<diag::Diagnostic> notes) {
    index_.names = names;
    index_.notes = std::move(notes);
    for (std::size_t i = 0; i < complete_ && i < cells_.size(); ++i) {
      IndexedCell cell{cell_name(names, cells_[i].ref), cells_[i].position, {}};
      for (const NameRef& child : cells_[i].children) {
        cell.children.push_back(cell_name(names, child));
      }
      index_.cells.push_back(std::move(cell));
    }
    for (const Property& p : file_properties_) {
      if (property_name(names, p) == "S_TOP_CELL" && !p.values.empty()) {
        if (auto value = string_value(names, p.values.front())) {
          index_.top_cells.push_back(std::move(*value));
        }
      }
    }
    if (stopped_) {
      const std::string cell = open_ && !cells_.empty() ? cell_name(names, cells_.back().ref) : std::string{};
      diag::Diagnostic d = std::move(*stopped_);
      if (index_.started) {
        d.text = "Error while reading cell \"" + cell + "\" (byte position " + std::to_string(stopped_at_) +
                 "): " + d.text;
      }
      index_.fatal = std::move(d);
    }
    return std::move(index_);
  }

 private:
  FileIndex index_;
  std::vector<PendingCell> cells_;
  std::vector<Property> file_properties_;
  std::size_t complete_ = 0;
  bool open_ = false;
  std::optional<diag::Diagnostic> stopped_;
  std::uint64_t stopped_at_ = 0;
};

}  // namespace

std::string position_text(const StreamPosition& p) {
  std::string text = std::to_string(p.offset);
  if (p.in_cblock) {
    text += "+" + std::to_string(p.inner);
  }
  return text;
}

std::string cell_name(const NameTables& names, const NameRef& ref) {
  if (!ref.reference) {
    return ref.name;
  }
  const auto found = names.cellnames.find(*ref.reference);
  return found != names.cellnames.end() ? found->second : "#" + std::to_string(*ref.reference);
}

std::optional<std::string> string_value(const NameTables& names, const PropertyValue& value) {
  using Kind = PropertyValue::Kind;
  switch (value.kind) {
    case Kind::a_string:
    case Kind::b_string:
    case Kind::n_string:
      return value.string;
    case Kind::a_reference:
    case Kind::b_reference:
    case Kind::n_reference: {
      const auto found = names.propstrings.find(value.reference);
      return found != names.propstrings.end() ? std::optional<std::string>{found->second} : std::nullopt;
    }
    default:
      return std::nullopt;
  }
}

std::string property_name(const NameTables& names, const Property& property) {
  if (!property.name_reference) {
    return property.name;
  }
  const auto found = names.propnames.find(*property.name_reference);
  return found != names.propnames.end() ? found->second : std::string{};
}

FileIndex index_file(gds::ByteSource& source) {
  Decoder decoder(source);
  Indexer indexer;
  for (;;) {
    auto record = decoder.next();
    if (!record.ok()) {
      indexer.stop(record.error(), decoder.source().position());
      break;
    }
    if (!record.value()) {
      break;
    }
    indexer.take(*record.value());
  }
  return indexer.finish(decoder.names(), decoder.take_diagnostics());
}

std::optional<std::string> top_cell(const FileIndex& index) {
  std::unordered_set<std::string> defined;
  for (const IndexedCell& c : index.cells) {
    defined.insert(c.name);
  }
  for (const std::string& name : index.top_cells) {
    if (defined.contains(name)) {
      return name;
    }
  }
  std::unordered_set<std::string> placed;
  for (const IndexedCell& c : index.cells) {
    for (const std::string& child : c.children) {
      if (child != c.name) {
        placed.insert(child);
      }
    }
  }
  std::optional<std::string> only;
  std::unordered_set<std::string> seen;
  for (const IndexedCell& c : index.cells) {
    if (placed.contains(c.name) || !seen.insert(c.name).second) {
      continue;
    }
    if (only) {
      return std::nullopt;
    }
    only = c.name;
  }
  return only;
}

}  // namespace hefesto::oasis::read_detail
