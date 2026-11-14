// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF parser's cell commands: DS, DF, DD (CIFParseStart, CIFParseFinish,
// CIFParseDelete, cif/CIFrdcl.c:421-500, 1163-1230), C (CIFParseCall,
// :1334-1430), the user extensions 9 and 91 (:1250-1330, 1400-1430) and the
// end of the read (CIFReadCellCleanup, :1734-1841). Symbol numbers map to
// cells through the per-read SymbolTable (Q-MASK-318 fix). Owner: CIF.

#include <algorithm>

#include "hefesto/db/geometry.hpp"
#include "parser_impl.hpp"

namespace hefesto::cif::detail {

namespace {

bool in_plane(std::int64_t v) noexcept {
  return v >= db::kPlaneMin && v <= db::kPlaneMax;
}

}  // namespace

void Parser::report_symbol_cap() {
  if (!symbol_cap_reported_) {
    symbol_cap_reported_ = true;
    messages_.diagnostic(diag::make_native(read_codes::kSymbolLimit,
                                           "CIF file read: more than " + std::to_string(options_.max_symbols) +
                                               " cells; the remaining symbols are ignored."));
  }
}

void Parser::grow_graph() {
  const std::size_t n = session_.symbols.cells().size();
  children_.resize(n);
  has_parent_.resize(n, false);
  visited_.resize(n, 0);
}

// --- DS, DF, DD -------------------------------------------------------------

void Parser::start_definition() {
  if (in_definition_) {
    error("definition start inside other definition; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  discard_pending_id();
  lex_.take();  // the 'S'
  const auto number = unsigned_integer();
  if (!number) {
    error("definition start, but no symbol number; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  if (*number < 0) {
    error("illegal negative symbol number; definition ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  read_symbol_scale();
  open_definition(*number);
}

void Parser::read_symbol_scale() {
  scale_n_ = 1;
  scale_d_ = 1;
  if (const auto a = unsigned_integer()) {
    scale_n_ = mul(*a, multiplier_);  // the units are not centimicrons
    if (const auto b = unsigned_integer()) {
      scale_d_ = *b;
    } else {
      error("only one of two scale factors given; ignored.");
      scale_n_ = 1;
      scale_d_ = 1;
    }
  }
  if (scale_n_ <= 0 || scale_d_ <= 0) {
    error("Illegal scale " + std::to_string(scale_n_) + " / " + std::to_string(scale_d_) + " changed to 1 / 1");
    scale_n_ = 1;
    scale_d_ = 1;
  }
}

void Parser::open_definition(std::int64_t number) {
  const auto def = session_.symbols.define(number);
  in_definition_ = true;
  saved_layer_ = layer_;
  layer_.reset();
  current_.reset();
  if (!def) {
    report_symbol_cap();
    return;
  }
  grow_graph();
  if (def->renamed_to) {
    check(target_.rename_cell(def->renamed_from, *def->renamed_to));
    error("Warning: cell definition " + std::to_string(number) + " reused.");
  }
  current_ = def->cell;
  check(target_.begin_cell(session_.symbols.cell(def->cell).name));
}

void Parser::finish_definition() {
  if (!in_definition_) {
    error("definition finish without definition start; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  discard_pending_id();
  lex_.take();  // the 'F'
  close_definition();
}

void Parser::close_definition() {
  if (current_) {
    check(target_.end_cell());  // the engine processes the cell's layers
  }
  layer_ = saved_layer_;
  scale_n_ = 1;
  scale_d_ = 1;
  in_definition_ = false;
  current_.reset();
}

void Parser::delete_definition() {
  lex_.take();  // the second 'D'
  const auto number = unsigned_integer();
  if (!number) {
    error("definition delete, but no symbol number; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  session_.symbols.forget(*number, options_.magic_compat);
  lex_.skip_to_semicolon();
}

// --- C ----------------------------------------------------------------------

void Parser::call() {
  lex_.take();  // the 'C'
  const auto called = unsigned_integer();
  if (!called) {
    error("call, but no symbol number; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  Transform64 t;
  const bool well_formed = transform(t);
  if (!well_formed && !options_.magic_compat) {
    error("call ignored.");  // Q-MASK-335 OFF
    pending_id_.reset();
    return;
  }
  if (in_definition_ && !current_) {
    pending_id_.reset();  // the definition was dropped by the symbol cap
    return;
  }
  const auto child = session_.symbols.call(*called);
  if (!child) {
    report_symbol_cap();
    lex_.skip_to_semicolon();
    return;
  }
  grow_graph();
  if (current_ && is_ancestor(*child, *current_)) {
    error("attempt to place cell use inside its own definition!");
    lex_.skip_to_semicolon();
    return;
  }
  place(*child, t, well_formed);
}

void Parser::place(SymbolTable::CellId child, const Transform64& t, bool well_formed) {
  const ReadCell& cell = session_.symbols.cell(child);
  if (!options_.magic_compat && style_.has_copyup && !cell.defined && !copyup_warned_) {
    copyup_warned_ = true;  // Q-MASK-309 OFF: a forward call copies nothing
    warning("call to symbol " + std::to_string(cell.symbol) +
            " precedes its definition; its copyup layers are not copied.");
  }
  const std::string id = pending_id_.value_or("");
  pending_id_.reset();
  if (range_error_ || !in_plane(t.c) || !in_plane(t.f)) {
    outside_plane("call");
    return;
  }
  const db::Transform transform{static_cast<std::int32_t>(t.a), static_cast<std::int32_t>(t.b),
                                static_cast<db::Coord>(t.c),    static_cast<std::int32_t>(t.d),
                                static_cast<std::int32_t>(t.e), static_cast<db::Coord>(t.f)};
  check(target_.place(CifCall{cell.name, transform, well_formed, id, cell.defined}));
  if (current_) {
    children_[*current_].push_back(child);
    has_parent_[child] = true;
  }
}

bool Parser::is_ancestor(SymbolTable::CellId child, SymbolTable::CellId cell) {
  // DBIsAncestor(child, cell): does child contain cell? Only cells of this
  // read can contain the cell being defined, and only if it was called
  // before its definition, so most calls need no search.
  if (child == cell) {
    return true;
  }
  if (!has_parent_[cell]) {
    return false;
  }
  if (++visit_epoch_ == 0) {
    std::fill(visited_.begin(), visited_.end(), 0);
    visit_epoch_ = 1;
  }
  std::vector<SymbolTable::CellId> stack{child};
  visited_[child] = visit_epoch_;
  while (!stack.empty()) {
    const SymbolTable::CellId node = stack.back();
    stack.pop_back();
    for (const SymbolTable::CellId kid : children_[node]) {
      if (kid == cell) {
        return true;
      }
      if (visited_[kid] != visit_epoch_) {
        visited_[kid] = visit_epoch_;
        stack.push_back(kid);
      }
    }
  }
  return false;
}

// --- 9, 91 ------------------------------------------------------------------

void Parser::user9() {
  const std::string name = lex_.name();
  if (in_definition_) {
    rename_current(name);
    return;
  }
  if (!options_.magic_compat) {
    warning("9 " + name + " outside a symbol definition; ignored.");  // Q-MASK-317 OFF
    return;
  }
  // Q-MASK-317 ON: a top-level 9 renames the edit cell, as Magic does.
  const std::string old = target_.edit_cell_name();
  if (session_.symbols.name_in_use(name) || !target_.rename_cell(old, name).ok()) {
    error(name + " already exists, so cell from CIF is named " + old + ".");
    return;
  }
  session_.symbols.taken_outside(name);
}

void Parser::rename_current(const std::string& name) {
  if (!current_) {
    return;
  }
  const std::string old = session_.symbols.cell(*current_).name;
  if (session_.symbols.name_in_use(name) || !target_.rename_cell(old, name).ok()) {
    error(name + " already exists, so cell from CIF is named " + old + ".");
    return;
  }
  session_.symbols.rename(*current_, name);
}

void Parser::user91() {
  if (pending_id_) {
    error("91 command with identifier " + *pending_id_ + " pending; " + *pending_id_ + " discarded.");
  }
  pending_id_ = lex_.name();
}

void Parser::discard_pending_id() {
  if (pending_id_) {
    error("pending call identifier " + *pending_id_ + " discarded.");
    pending_id_.reset();
  }
}

// --- end of the read --------------------------------------------------------

void Parser::cleanup() {
  if (in_definition_) {
    error("CIF ended partway through a symbol definition.");
    discard_pending_id();
    close_definition();
  }
  for (const ReadCell& cell : session_.symbols.cells()) {
    if (!cell.defined) {
      error("cell " + cell.name + " was used but not defined.");
    }
  }
  check(target_.end_top());
}

}  // namespace hefesto::cif::detail
