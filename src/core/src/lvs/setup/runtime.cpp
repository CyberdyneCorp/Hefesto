// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The setup runtime: netlist numbering and the cell reference syntax
// (lvs-setup-file "Cell reference syntax").
//
// Owner: LVSSETUP.

#include "runtime.hpp"

#include <algorithm>
#include <set>
#include <utility>

#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/setup/codes.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::lvs::setup {

namespace {

constexpr std::string_view kNoSuchCell = "No such cell!";
constexpr std::string_view kNoSuchCellname = "No such cellname!";
constexpr std::string_view kNotInFile = "Cell is not in file!";
constexpr std::string_view kNoSuchFile = "No such file number!";
constexpr std::string_view kNoCircuit = "No circuit has been declared for comparison";
constexpr std::string_view kWildcardAlone = "Wildcards must be used with a valid cellname";
// The list forms with a file number (capture of NG-SET-03): an unknown cell
// or an unknown number.
constexpr std::string_view kNoCellOrFile = "No such cell or bad file number!";

diag::Diagnostic message(std::string_view text) {
  return diag::make(setup_codes::kCommandError, std::string{text});
}

bool is_circuit_keyword(std::string_view word) noexcept {
  return word == "-circuit1" || word == "-circuit2" || word == "-current";
}

}  // namespace

Runtime::Runtime(SetupContext& context, SetupState& state) : context_(context), state_(state), rng_(context.seed) {
  number_netlists();
}

void Runtime::number_netlists() {
  std::set<FileNumber> used;
  FileNumber highest = -1;
  const auto add = [this](Circuit* circuit, int side, FileNumber offset) {
    for (const auto& file : circuit->db.files) {
      netlists_.push_back(Netlist{circuit, side, file.number, file.number + offset, file.name, file.top});
    }
  };
  if (context_.first != nullptr) {
    add(context_.first, 1, 0);
    for (const auto& file : context_.first->db.files) {
      used.insert(file.number);
      highest = std::max(highest, file.number);
    }
  }
  if (context_.second == nullptr || context_.second == context_.first) {
    return;
  }
  const bool collide = std::any_of(context_.second->db.files.begin(), context_.second->db.files.end(),
                                   [&used](const LoadedFile& file) { return used.contains(file.number); });
  FileNumber offset = 0;
  if (collide) {
    FileNumber lowest = 0;
    for (const auto& file : context_.second->db.files) {
      lowest = std::min(lowest, file.number);
    }
    offset = highest + 1 - lowest;
  }
  add(context_.second, 2, offset);
}

std::vector<Circuit*> Runtime::circuits() const {
  std::vector<Circuit*> out;
  for (Circuit* c : {context_.first, context_.second}) {
    if (c != nullptr && std::find(out.begin(), out.end(), c) == out.end()) {
      out.push_back(c);
    }
  }
  return out;
}

Circuit* Runtime::circuit(int side) const {
  Circuit* c = side == 1 ? context_.first : context_.second;
  return c != nullptr && c->top.has_value() ? c : nullptr;
}

const Netlist* Runtime::netlist_of(const Circuit& circuit, CellId cell) const {
  const FileNumber file = circuit.db.cells.at(cell).file;
  for (const auto& netlist : netlists_) {
    if (netlist.circuit == &circuit && netlist.local == file) {
      return &netlist;
    }
  }
  return nullptr;
}

const Netlist* Runtime::netlist_by_number(FileNumber number) const {
  for (const auto& netlist : netlists_) {
    if (netlist.number == number) {
      return &netlist;
    }
  }
  return nullptr;
}

const Netlist* Runtime::netlist_by_file_name(std::string_view name) const {
  for (const auto& netlist : netlists_) {
    if (netlist.name == name) {
      return &netlist;
    }
  }
  return nullptr;
}

const Netlist* Runtime::netlist_by_top_cell(std::string_view name) const {
  for (const auto& netlist : netlists_) {
    const auto& db = netlist.circuit->db;
    const auto id = db.find_cell(name, netlist.local);
    if (id && (db.cells.at(*id).flags.top || *id == netlist.top)) {
      return &netlist;
    }
  }
  return nullptr;
}

diag::Outcome<Resolved> Runtime::resolve(std::string_view reference) const {
  auto words = split_list(reference);
  if (!words) {
    return words.error();
  }
  const auto& w = words.value();
  if (w.size() == 1) {
    return resolve_one(w[0]);
  }
  if (w.size() == 2) {
    return resolve_pair(w[0], w[1]);
  }
  return message(w.empty() ? kNoSuchCell : kNoSuchCellname);
}

diag::Outcome<Target> Runtime::circuit_top(std::string_view keyword) const {
  int side = keyword == "-circuit1" ? 1 : 2;
  if (keyword == "-current") {
    side = circuit(2) != nullptr ? 2 : 1;
  }
  Circuit* c = circuit(side);
  if (c == nullptr) {
    return message(kNoCircuit);
  }
  const Netlist* netlist = netlist_of(*c, *c->top);
  return Target{c, side, *c->top, netlist != nullptr ? netlist->number : 0};
}

diag::Outcome<Resolved> Runtime::resolve_one(std::string_view word) const {
  if (is_circuit_keyword(word)) {
    auto top = circuit_top(word);
    if (!top) {
      return top.error();
    }
    return Resolved{{top.value()}, false};
  }
  if (word == "*") {
    return message(kWildcardAlone);
  }
  if (const auto number = parse_integer(word)) {
    const Netlist* netlist = netlist_by_number(static_cast<FileNumber>(*number));
    if (netlist == nullptr) {
      return message(kNoSuchFile);
    }
    return Resolved{{Target{netlist->circuit, netlist->side, netlist->top, netlist->number}}, false};
  }
  if (const Netlist* netlist = netlist_by_top_cell(word)) {
    return in_netlist(*netlist, word);
  }
  Resolved all = everywhere(word);
  if (all.targets.empty()) {
    return message(kNoSuchCell);
  }
  return all;
}

diag::Outcome<Resolved> Runtime::in_netlist(const Netlist& netlist, std::string_view cell) const {
  const auto id = netlist.circuit->db.find_cell(cell, netlist.local);
  if (!id) {
    return message(everywhere(cell).targets.empty() ? kNoSuchCellname : kNotInFile);
  }
  return Resolved{{Target{netlist.circuit, netlist.side, *id, netlist.number}}, false};
}

Resolved Runtime::everywhere(std::string_view cell) const {
  Resolved out;
  out.wildcard = true;
  for (const auto& netlist : netlists_) {
    if (const auto id = netlist.circuit->db.find_cell(cell, netlist.local)) {
      out.targets.push_back(Target{netlist.circuit, netlist.side, *id, netlist.number});
    }
  }
  return out;
}

diag::Outcome<Resolved> Runtime::resolve_pair(std::string_view a, std::string_view b) const {
  if (a == "*" || b == "*") {
    Resolved all = everywhere(a == "*" ? b : a);
    if (all.targets.empty()) {
      return message(kNoSuchCellname);
    }
    return all;
  }
  if (is_circuit_keyword(a) || is_circuit_keyword(b)) {
    return is_circuit_keyword(a) ? in_circuit(a, b) : in_circuit(b, a);
  }
  const auto na = parse_integer(a);
  const auto nb = parse_integer(b);
  if (na || nb) {
    return na ? in_numbered(static_cast<FileNumber>(*na), b) : in_numbered(static_cast<FileNumber>(*nb), a);
  }
  return in_named_file(a, b);
}

diag::Outcome<Resolved> Runtime::in_circuit(std::string_view keyword, std::string_view cell) const {
  auto top = circuit_top(keyword);
  if (!top) {
    return top.error();
  }
  const Netlist* netlist = netlist_of(*top.value().circuit, top.value().cell);
  if (netlist == nullptr) {  // a top cell outside every loaded file
    return message(kNoCircuit);
  }
  return in_netlist(*netlist, cell);
}

diag::Outcome<Resolved> Runtime::in_numbered(FileNumber number, std::string_view cell) const {
  const Netlist* netlist = netlist_by_number(number);
  if (netlist == nullptr || !netlist->circuit->db.find_cell(cell, netlist->local)) {
    return message(kNoCellOrFile);
  }
  return in_netlist(*netlist, cell);
}

diag::Outcome<Resolved> Runtime::in_named_file(std::string_view a, std::string_view b) const {
  // A loaded file's name first, then a top cell's name, names the file.
  const Netlist* by_a = netlist_by_file_name(a);
  const Netlist* by_b = by_a != nullptr ? nullptr : netlist_by_file_name(b);
  if (by_a == nullptr && by_b == nullptr) {
    by_a = netlist_by_top_cell(a);
    by_b = by_a != nullptr ? nullptr : netlist_by_top_cell(b);
  }
  if (by_a != nullptr) {
    return in_netlist(*by_a, b);
  }
  if (by_b != nullptr) {
    return in_netlist(*by_b, a);
  }
  return message(kNoSuchCellname);
}

std::uint64_t Runtime::next_random() {
  // SplitMix64 (Steele, Lea and Flood), the generator of Q-LVS-4.
  rng_ += 0x9e3779b97f4a7c15ULL;
  std::uint64_t z = rng_;
  z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31U);
}

void Runtime::print(std::string text) {
  state_.log.push_back(SetupMessage{SetupMessage::Stream::out, std::move(text)});
}

void Runtime::print_error(std::string text) {
  state_.log.push_back(SetupMessage{SetupMessage::Stream::err, std::move(text)});
}

bool Runtime::cancelled() {
  if (!context_.cancel.requested()) {
    return false;
  }
  if (interpreter_ != nullptr) {
    interpreter_->request_interrupt();
  }
  return true;
}

void Runtime::time_out() {
  timed_out_ = true;
  if (interpreter_ != nullptr) {
    interpreter_->request_interrupt();
  }
}

}  // namespace hefesto::lvs::setup
